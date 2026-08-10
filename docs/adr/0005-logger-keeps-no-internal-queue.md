# The logger keeps no internal queue

`tetrislogd` receives a datagram, validates it, formats it, and writes it —
on one thread, in one loop, with nothing buffered in between. The kernel's
socket receive buffer is the only queue in the design.

This is worth writing down because the other end of the same log path is built
the opposite way. `tetrisd` has a `libcoreipc` ring buffer and a dedicated
shipper thread, for a reason that does not apply here: a game thread must never
block to log, so it pushes into a ring and moves on, and a full ring drops the
record rather than stalling a room's ticker. A reader who has just finished
`src/tetrisd/src/log.c` will reasonably expect the logger to mirror it, and
will wonder where the ring went.

The rejected alternative was exactly that mirror — a receiver thread draining
the socket into an `rb_*` ring, and a writer thread draining the ring to disk.
It does not move the overflow point, it only moves *who loses*. Records still
arrive faster than the disk accepts them; the ring still fills; and when it
does, the logger discards records the kernel has already accepted and the
sender believes were delivered. Those records exist nowhere else.

Without the ring, the same pressure fills the socket receive buffer instead.
`us_dgram_send_nb` then returns `EAGAIN` to `tetrisd`'s shipper, which prints
the record to stderr rather than losing it (`ship_one`). The pressure is
answered by the one party still holding a copy. Backpressure that reaches the
sender is strictly better than a drop that does not, and a single-threaded
loop with no shared state is also the smaller thing to get right: no lock
order, no shutdown ordering between two threads, no records stranded in a ring
at exit.

## Consequences

- The loop must drain the socket until `EAGAIN` on each wake-up, not one
  record per iteration, or a burst can fill the receive buffer for no reason.
- No work may be added to the receive path that can block for long. Durability
  is therefore one `write(2)` per record with `fdatasync` on the idle tick —
  syncing per record would make disk latency the ceiling on log throughput and
  push `tetrisd` into stderr fallback under ordinary load.
- If the sink is ever unavailable the loop keeps draining the socket anyway,
  writing to stderr and counting the records as Degraded. Stalling instead
  would convert a local disk problem into backpressure on the game server.
- `tetrisd`'s `EAGAIN` branch is the Degraded case on the producer side and
  should be counted there too; today it falls back to stderr silently, so
  `tetrisctl dropped-logs` can read zero while records are pouring into the
  stderr stream.
- The stderr fallback is a bring-up and terminal convenience, not a durable
  second sink. `dspawn` daemonises before exec and points stderr at
  `tmp/<registered-name>.err` (`dspawn.c`, `redirect_stderr`), so a record
  degraded under the shell is readable rather than gone — but that file is
  inside the `tmp/` that `make reset` wipes, and nothing reclaims the
  descriptor the way the sink reclaims its own, so a degraded record is
  recoverable, not retained. What is guaranteed is the counter, and the report
  line the logger writes when the sink comes back. Anything that must not be
  lost belongs in the sink, not in the fallback.
