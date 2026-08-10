# Deployment

> Running the server half of tetriSH on a DigitalOcean Droplet, so that players
> reach it with `scripts/play.sh --host <address>` from their own terminals.

This is the deployed counterpart to `make play`, which puts both halves on one
machine. Nothing about the client changes: it is native on every platform, and
only the address and the CA it trusts move.

---

## Table of Contents

- [Why a Droplet](#why-a-droplet)
- [Prerequisites](#prerequisites)
- [Deploy](#deploy)
- [Publish the CA](#publish-the-ca)
- [Play against it](#play-against-it)
- [Operating](#operating)
- [Certificates](#certificates)

---

## Why a Droplet

App Platform cannot serve `tetrisd`, at any tier. Its ingress is HTTP/HTTPS
routed by hostname — Host header or TLS SNI — which is why the Static Ingress
IPs are Cloudflare anycast and are shared between customers. `libtetrissh`
speaks no TLS: the first bytes on the wire are a bespoke RSA-OAEP key exchange,
with no SNI and no ALPN, so a shared ingress has nothing to demultiplex on and
cannot know which app a connection belongs to. Raw TCP needs an address that is
yours.

A Droplet is that address. It also runs `make docker-server` unchanged, because
the server half is already a container with a read-only `certs/` mount.

| Choice | Value | Why |
|---|---|---|
| Image | Ubuntu 24.04 LTS | What the `Dockerfile` builds `FROM` |
| Plan | Basic, 1 vCPU / 1 GB | Enough once the image is the `server` target |
| Region | `SGP1` | Input latency is the feel of the game; the players are in Singapore |
| Firewall | Cloud Firewall + `ufw` | The Cloud Firewall sits outside the Droplet, so a `ufw` mistake cannot expose it |

The 1 GB plan holds only because the server image leaves `src/tetrisu` out of
the build. The `dev` image builds notcurses from source against the ffmpeg
headers and will exhaust that box — build `--target server`, or build the `dev`
image somewhere else.

---

## Prerequisites

On the Droplet: Docker, `git`, and `make`. On DigitalOcean's side: a Cloud
Firewall with these inbound rules, and everything else denied.

| Type | Protocol | Port | Sources |
|---|---|---|---|
| SSH | TCP | `22` | your own addresses |
| Custom | TCP | `4242` | all IPv4, all IPv6 |

---

## Deploy

**1. Install Docker and the build tools:**

```bash
apt-get update && apt-get install -y docker.io git make
systemctl enable --now docker
```

**2. Clone the repository:**

```bash
git clone https://github.com/ziqiqiiii/MacMini_tetriSH.git
cd MacMini_tetriSH
```

**3. Mint the server's certificates:**

```bash
make certs
```

This writes `certs/{ca.crt,server.crt,server.key}`. They are minted here, on the
machine that serves, and never baked into the image — `docker-server` mounts the
directory read-only.

**4. Build the server image:**

```bash
make docker-build-server
```

**5. Start it:**

```bash
make docker-server
docker update --restart unless-stopped tetrish-server
```

The second line is the deployment's own: `make docker-server` is written for a
laptop and does not ask for a restart policy, so without it a reboot leaves the
port dark.

**6. Confirm it is listening:**

```bash
make docker-logs
```

`tetrisd is listening on port 4242 inside the container` is the line to wait
for. `Ctrl-C` stops following, not the server.

Set `DOCKER_PORT` on both the build and the run to serve a different port; it
travels into the container as `TETRISD_PORT` and into the published mapping
together, so the two cannot drift.

---

## Publish the CA

Players verify the server's certificate chain against the CA that signed it, and
refuse the session without it. `certs/` is git-ignored and holds whatever the
local machine last minted, so a deployed server's CA is tracked separately:

```bash
cat certs/ca.crt
```

Copy that into `deploy/vps-ca.crt` in the repository and commit it. It is the
public half of the CA — every player needs it and nothing in it is secret.
`certs/ca.key` and `certs/server.key` are secret, stay on the Droplet, and are
`chmod 600` where `generate_certs.sh` left them.

---

## Play against it

From any player's own terminal, on macOS or Linux, with no container engine
involved:

```bash
bash scripts/play.sh --host <droplet-ip>
```

`--host` skips every step but the client, and defaults the CA to
`deploy/vps-ca.crt`. Pass `--ca PATH` for a server whose CA is not the tracked
one, and `--port N` if it is not on 4242.

Verification is chain-only, so the certificate does not have to name the address
being dialled: the Droplet can be rebuilt at a new IP, or put behind a DNS name,
without reissuing anything.

`CHECK SERVER` on the sign-in screen must report `SERVER ONLINE`. Without it
Solo silently plays the local rules instead, and looks identical.

---

## Operating

| Task | Command |
|---|---|
| Follow the logs | `make docker-logs` |
| Stop the server | `make docker-stop` |
| Restart it | `make docker-server` |
| Drop players, wallets and the leaderboard | `make docker-reset` |
| Deploy a new build | `git pull && make docker-build-server && make docker-server` |

`docker stop` reaches `scripts/docker_server.sh` as pid 1, which traps `TERM`
and stops the daemons through `tetrisctl` — logger last, so `tetrisd`'s shutdown
reaches the log rather than its error file.

Player state lives in the `tetrish-state` Docker volume mounted at
`/tetrish/tmp`, not in the container, so rebuilding the image keeps every wallet
and leaderboard row. `make docker-reset` is the only command here that discards
them.

---

## Certificates

`make certs` mints for **365 days** and is a no-op while the existing
certificate is still valid, so re-running it after a `git pull` reissues
nothing. When it does expire, every client fails the handshake at once.

To roll them:

```bash
FORCE=1 bash scripts/generate_certs.sh certs
make docker-server
```

Then republish `certs/ca.crt` as `deploy/vps-ca.crt` — the new CA is a different
CA, and clients carrying the old one are refused until they pull.

These are the same self-signed development credentials `make certs` mints
everywhere else: no passphrase on the key, and a CA nobody else has any reason
to trust. That is the right shape for a coursework deployment and the wrong one
for anything holding real accounts.
