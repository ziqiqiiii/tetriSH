# A component is a subsystem, not a process

_Status: accepted; the log line it describes is not yet implemented._

The `component` field on a log record names the part of the system that
produced the record, not the process it ran in. `tetrisd` serves room chat,
narration and the marketplace itself, and that code logs as `chat` and
`market` — three component names, one process, one pid.

This is worth writing down because the log then depicts an architecture the
project explicitly does not have. `CLAUDE.md` states there are no separate
social-layer daemons; a reader watching `[chat]` and `[market]` lines scroll
past will reasonably conclude the opposite, go looking for `chatd`, and find
nothing. The confusion is entirely self-inflicted and the log is where it
happens.

What makes it survivable is the pid column. Every line carries the pid of the
process that emitted it, so `[tetrisd]`, `[chat]` and `[market]` lines sharing
pid 4123 are visibly one program. The pid is what names the process; the
component is what names the part. Dropping pid from the line — which the
original sketch of the format did — would remove the only evidence that the
four-daemon reading is wrong, so the two decisions are one decision.

The alternative that keeps the component column honest by construction is a
compound name, `tetrisd/chat`, which nobody can misread even without this
file. It was rejected for width: the column has to fit the longest name on
every line forever, and `tetrisd/market` costs four characters against every
record to restate something the pid already answers. The other alternative —
component stays a process name and the subsystem rides as an ordinary
`subsys=chat` field — needs no new vocabulary at all, but it gives up
per-subsystem filtering and colouring, which is the main thing an operator
wants from a log that carries four subsystems in one process.

## Consequences

- Daemon names and component names are different namespaces that happen to
  overlap. `tetrisctl stop tetrisd` takes a daemon name and only `tetrisd` or
  `tetrislogd` are valid; `dlog -f chat` takes a component name and `chat` is
  valid while being nothing you can start or stop. Neither list is the other.
- A component is not a unit of lifecycle, ownership, or configuration. It is
  a label on a record. Nothing may key behaviour off it beyond filtering and
  display.
- New subsystems become new component names without any protocol change —
  `component` is a fixed 16-byte field in `t_log_record` and adding a value
  costs nothing. This is deliberately cheap, so the discipline has to come
  from review rather than from the wire format.
- `dlog` colours by component, so its palette needs a stable mapping and a
  fallback for names it has never seen.
