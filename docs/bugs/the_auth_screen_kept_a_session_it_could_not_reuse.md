# Bug Note — The auth screen kept a session it could not reuse

## What the bug was

Sign in, play, back out to the auth screen, press SIGN IN again: refused, every
time, with the username and password still filled in. Retyping the server
address into the domain field fixed it — which is the tell, because that field
does not authenticate anything. It triggers CHECK SERVER, and CHECK SERVER
dials a new socket.

Identity belongs to the connection,
so `login_handler` opens with:

```c
if (ctx->cli->state == CLI_AUTHED)
	return (409);
```

Nothing on the client side ever un-authenticated a session. `run_auth_flow`
re-entered with `t_auth_form` still reading `AUTH_SERVER_ONLINE` and
`t_app_net_session` still bound to a player, so `net_login_action` sent a
`LOGIN` down an authenticated socket and got the 409 it asks for. The provider
mapped that to `APP_PROVIDER_INVALID`, and the screen said the credentials were
wrong.

A second, quieter version of the same shape: `solo_authority.c` calls
`net_disconnect` when a session is lost mid-game. That leaves
`session->net.state == NET_OFFLINE` while the form still reads ONLINE, because
nothing tells the form. The next sign-in wrote to a socket that was not there.

And `net_connect` itself began with `memset(net, 0, sizeof(*net))` over a
live handle, so the CHECK SERVER that "fixed" it leaked the previous socket and
left tetrisd holding a connection nobody would ever read.

## Why it was not caught

`net_provider_smoke` signed in once. Every check after it reused the session
that first `LOGIN` established, which is the correct thing for a smoke test of
the vtable to do and the exact case that works. Nothing exercised the second
sign-in, because in the fixture provider — the only provider the UI tests use —
there is no socket to be already-bound.

## The fix

`session_ready_for_credentials` in `net_provider.c`, called by both
`net_login_action` and `net_sign_up_action`:

```c
if (session->net.state == NET_CONNECTED)
	return (true);
if (session->cfg.host[0] == '\0')
	net_config_load(&session->cfg);
session->connected = net_connect(&session->net, &session->cfg) == 0;
return (session->connected);
```

`NET_CONNECTED` is the one state credentials can be sent from: dialled, not yet
anybody. Everything else — offline, authed, in a room, in a game — is a socket
that has to be thrown away first. `net_connect` now disconnects a live handle
before blanking it, so the throw-away is real and not a leak.

The regression tests are `signing in again from the auth screen works` and
`a lost session redials on the next sign-in` in
`src/tetrisu/tests/net_provider_smoke.c`.

## What to take from this

1. **Two places holding the same fact will disagree, and the UI is the one that
   will be wrong.** `t_auth_form.server_state` and
   `t_app_net_session.net.state` both answered "is there a server?". Only one of
   them was ever updated by the network going away. The repair was not to sync
   them but to stop the form's copy from being load-bearing: the provider now
   makes the connection true at the moment it is used.

2. **A server-side invariant is a client-side obligation.** "One connection per
   player" is stated in an ADR and enforced with a 409. Nothing on the client
   read that as "you must reconnect to sign in again" until a player found it.

3. **When the workaround is unrelated to the symptom, the diagnosis is wrong.**
   Retyping a hostname should have nothing to do with whether a password is
   accepted. That it did was the whole answer.
