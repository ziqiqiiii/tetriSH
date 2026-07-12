# libtetrissh

The secure-session library for tetriSH. It authenticates the server, exchanges
a fresh AES-256 key, and carries application messages in length-prefixed,
authenticated AES-256-GCM frames.

## Secure Session Sequence

![libtetrissh secure-session sequence](assets/libtetrissh_secure_session.svg)

The connection has three stages:

1. **Connect** — `tetrisu` opens TCP and both endpoints start their respective
   handshake functions.
2. **Authenticate and exchange a key** — the client sends a fresh 32-byte
   nonce. The server returns its X.509 certificate and an RSA-PSS signature over
   that nonce. After verification, the client sends a fresh AES-256 key wrapped
   with RSA-OAEP.
3. **Exchange protected messages** — `session_send()` and `session_recv()` use
   AES-256-GCM frames for HTTTP commands, responses, and server-pushed `STATE`.

The editable diagram source is
[`assets/libtetrissh-sequence.puml`](assets/libtetrissh-sequence.puml).

## Frame Format

Each post-handshake message is encoded as:

```text
[4-byte big-endian frame length]
[12-byte GCM nonce][16-byte GCM tag][ciphertext]
```

The maximum plaintext size is 64 KiB. Each direction has an independent
sequence counter, and the counter plus client/server direction marker is bound
into the GCM authentication tag. A modified, replayed, reflected, oversized,
or truncated frame is rejected before plaintext is returned.

## Public API

| Function | Purpose |
|---|---|
| `session_handshake_server()` | Receive the nonce, send certificate and signature, then unwrap the session key |
| `session_handshake_client()` | Verify the server and send the wrapped session key |
| `session_send()` | Encrypt, authenticate, frame, and write one complete message |
| `session_recv()` | Read, authenticate, decrypt, and return one complete message |
| `session_close()` | Cleanse key material and reset session state |

## Ownership and Failure Behaviour

- The caller owns the connected socket descriptor; `session_close()` resets
  library state but does not close the descriptor.
- A handshake failure follows one cleanup path, frees partial OpenSSL state,
  wipes temporary secret buffers, and leaves the session unestablished.
- `session_recv()` returns `0` for clean EOF and `-1` for malformed,
  unauthenticated, oversized, truncated, or otherwise invalid frames.
- `common.c` and `common.h` are course-provided cryptographic primitives and
  remain unchanged.

## Build and Test

```bash
make -C lib/libtetrissh
make -C lib/libtetrissh test
```
