#!/usr/bin/env bash
# daemons_killer.sh
#
# Stops every daemon dspawn/dplant left running, and does not return until they
# are gone. Used by the root `make reset`, which wipes the runtime state those
# daemons are writing into.
#
# Killing by name does not work here. dspawn execs its target, so a daemon
# launched as `dspawn tetrislogd -- tetrislogd` is running under the target's
# name and a match on "dspawn" never sees it — the two daemons that matter are
# exactly the two that get missed. The registry is the only record tying the
# registered name to the pid, so it is read first and dkill does the killing:
# it sends SIGTERM by pid, records each daemon in the graveyard, and truncates
# the registry. Feeding it "a" selects "all".
#
# Waiting is the point of the rest. dkill signals and returns, while a daemon
# shutting down still writes: tetrislogd reclaims its sink on the way out and
# recreates tmp/ to do it. Returning early lets that land *after* the caller's
# wipe, leaving behind the state reset was asked to remove.
#
# Must run before anything deletes tmp/ or bin/: it needs the registry it reads
# and, preferably, the dkill binary that reads it.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REG="$ROOT/tmp/daemons.reg"

# Collected before dkill runs, because dkill truncates the registry on success
# and these are the pids the wait below needs.
pids=()
if [ -f "$REG" ]; then
	while read -r _name pid _rest; do
		case "${pid:-}" in
			'' | *[!0-9]*) continue ;;
		esac
		if kill -0 "$pid" 2>/dev/null; then
			pids+=("$pid")
		fi
	done < "$REG"
fi

if [ -x "$ROOT/bin/dkill" ]; then
	printf 'a\n' | "$ROOT/bin/dkill"
else
	# After an fclean there is no dkill to delegate to, and leaving the daemons
	# running would be the worse failure. The graveyard entry is what is lost.
	for pid in "${pids[@]:-}"; do
		[ -n "$pid" ] && kill "$pid" 2>/dev/null
	done
fi

# Stragglers the registry cannot account for: a dspawn that died before it
# registered, or one still running its built-in loop under its own name.
#
# -x matches the process name, not the command line. Matching the command line
# (-f) makes this script a hazard rather than a cleanup: "dspawn" appears in the
# argv of the make that called it, of an editor holding dspawn.c, and of the
# shell running this very line, and pkill kills all of them.
pkill -x dspawn 2>/dev/null
pkill -x dplant 2>/dev/null

# Up to five seconds for a clean exit, then insist. A daemon that ignores
# SIGTERM must not turn `make reset` into a hang.
for _ in $(seq 1 50); do
	alive=0
	for pid in "${pids[@]:-}"; do
		if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
			alive=1
			break
		fi
	done
	[ "$alive" -eq 0 ] && break
	sleep 0.1
done
for pid in "${pids[@]:-}"; do
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		kill -9 "$pid" 2>/dev/null
	fi
done

exit 0
