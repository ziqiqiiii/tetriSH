# deploy/

> The public half of a deployed server's credentials — what a player needs to
> verify it, and nothing else.

`scripts/play.sh --host <address>` verifies the server's certificate chain
against `deploy/vps-ca.crt` and refuses the session without it. That file is
the `certs/ca.crt` of the machine running `tetrisd`, copied here and committed:
every player needs it, and nothing in it is secret.

`certs/` itself is git-ignored and holds whatever the local machine last
minted, so it can only ever vouch for a server that machine is also running.
This directory is the deployed server's, which is a different CA.

| File | Contents | Tracked |
|---|---|---|
| `vps-ca.crt` | The deployed server's CA certificate | yes |
| `ca.key`, `server.key` | The private halves | **never** — they stay on the server |

Minting it, publishing it and rolling it are in
[`../docs/deployment.md`](../docs/deployment.md).

`[pending: vps-ca.crt lands here once the Droplet exists]`
