#!/usr/bin/env bash
# Generate the logrotate rule for the files the daemons write.
#
# Usage:  scripts/logrotate.sh                     print the rule to stdout
#         scripts/logrotate.sh --install           write /etc/logrotate.d/tetrish
#         scripts/logrotate.sh --install PATH      write it somewhere else
#         scripts/logrotate.sh --check             ask logrotate to parse it
#
#         --rc PATH      read paths from this rc instead of ./.tetrishrc
#         --root PATH    resolve those paths against this root
#         --no-su        omit the su directive (for a non-root logrotate)
#
# tetrislogd never truncates its own sink: it appends until something else
# moves the file, and reopens the configured path only when SIGHUP arrives
# (sink_reopen). Nothing in the daemon caps the file, so without this rule the
# log grows for as long as the server runs — one Double game-hour is ~7 MB of
# access lines at the rate the load generator plays at.
#
# The two halves of the rule are not interchangeable:
#
#   - The **sink** rotates by rename, then postrotate SIGHUPs the pid in the
#     pidfile so the daemon lets go of the renamed inode. copytruncate would
#     lose whatever was written between the copy and the truncate, and is the
#     one thing that must never appear here.
#   - The **error files** are the daemons' stderr, redirected there after boot.
#     No signal reopens them — tetrisd's SIGHUP re-reads .tetrishrc and does
#     not touch stderr (server_reload) — so copytruncate is the only correct
#     tool for those, and it is used deliberately rather than by omission.
#
# Paths come from .tetrishrc and are made absolute here, because logrotate has
# no notion of the repository root: a root-relative path matches nothing and
# rotates nothing, silently.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RC=""
DEST=""
ACTION=print
WANT_SU=1

# Retention. Overridable from the environment because the right answer is a
# function of the disk and how many players the box is serving, neither of
# which this script can see.
ROTATE_KEEP="${ROTATE_KEEP:-14}"
ROTATE_MAXSIZE="${ROTATE_MAXSIZE:-100M}"

while [ $# -gt 0 ]; do
	case "$1" in
		--install)
			ACTION=install
			if [ $# -gt 1 ] && [ "${2#-}" = "$2" ]; then
				DEST="$2"
				shift
			fi
			;;
		--check)  ACTION=check ;;
		--no-su)  WANT_SU=0 ;;
		--rc)     RC="${2:?--rc needs a path}"; shift ;;
		--root)   ROOT="${2:?--root needs a path}"; shift ;;
		-h|--help)
			sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
			exit 0
			;;
		*)
			echo "logrotate.sh: unknown argument '$1'" >&2
			exit 1
			;;
	esac
	shift
done

ROOT=$(cd "$ROOT" && pwd)
RC="${RC:-$ROOT/.tetrishrc}"
DEST="${DEST:-/etc/logrotate.d/tetrish}"

if [ ! -f "$RC" ]; then
	echo "logrotate.sh: no rc file at $RC" >&2
	exit 1
fi

# The same sed idiom play.sh reads TETRISD_PORT with: the rc is shell, but
# sourcing it to read four paths would also run everything else in it.
rc_get() {
	sed -n "s/^[[:space:]]*export[[:space:]]*$1=\(.*\)/\1/p" "$RC" \
		| tail -1 | tr -d "\"'"
}

# A key may legitimately be absent; only the sink and its pidfile are required,
# since those two are what the postrotate handover is made of.
abs() {
	case "$1" in
		"")  return 1 ;;
		/*)  printf '%s\n' "$1" ;;
		*)   printf '%s\n' "$ROOT/$1" ;;
	esac
}

SINK=$(abs "$(rc_get TETRISLOGD_LOG_PATH)") || true
PIDFILE=$(abs "$(rc_get TETRISLOGD_PID_PATH)") || true
LOGD_ERR=$(abs "$(rc_get TETRISLOGD_ERR_PATH)") || true
TETRISD_ERR=$(abs "$(rc_get TETRISD_ERR_PATH)") || true

if [ -z "$SINK" ] || [ -z "$PIDFILE" ]; then
	echo "logrotate.sh: $RC names no TETRISLOGD_LOG_PATH/TETRISLOGD_PID_PATH" >&2
	exit 1
fi

# Who the rotated files belong to. This is never root by default: `create`
# hands the fresh sink to whoever is named, and tetrislogd opens it O_APPEND as
# the player — a root-owned 0644 sink is one the daemon cannot write, so it
# degrades to stderr from the first rotation onward and never recovers.
#
# The log directory is the best answer when it exists. It often does not at
# install time (make reset wipes tmp/), so fall back to the human behind the
# install — SUDO_USER before the effective user, since the whole point of the
# --install path is that it is run under sudo.
SINK_DIR=$(dirname "$SINK")
if [ -d "$SINK_DIR" ]; then
	OWNER=$(stat -c '%U' "$SINK_DIR" 2>/dev/null || true)
	GROUP=$(stat -c '%G' "$SINK_DIR" 2>/dev/null || true)
fi
if [ -z "${OWNER:-}" ] || [ "$OWNER" = "UNKNOWN" ]; then
	OWNER="${SUDO_USER:-$(id -un)}"
	GROUP=$(id -gn "$OWNER" 2>/dev/null || id -gn)
fi

# su is what lets a root logrotate work inside a directory it does not own. It
# is a privileged directive: logrotate refuses the config outright when it is
# not running as root, which is why the suite generates with --no-su.
if [ "$WANT_SU" -eq 1 ]; then
	SU_USER=$OWNER
	SU_GROUP=$GROUP
else
	SU_USER=""
	SU_GROUP=""
fi

emit_su() {
	[ -n "$SU_USER" ] && printf '\tsu %s %s\n' "$SU_USER" "$SU_GROUP"
	return 0
}

generate() {
	cat <<EOF
# /etc/logrotate.d/tetrish
#
# Generated by scripts/logrotate.sh from $RC — regenerate rather than edit,
# or the paths drift from the ones the daemons are actually configured with.

$SINK {
$(emit_su)	daily
	maxsize $ROTATE_MAXSIZE
	rotate $ROTATE_KEEP
	missingok
	notifempty
	compress
	delaycompress
	create 0644 $OWNER $GROUP
	postrotate
		# The daemon holds the renamed inode until this reaches it. A
		# stopped daemon is not a failure: the pidfile is stale or gone,
		# rotation still happened, and a non-zero exit here would have
		# logrotate report failure on every run once the server is down.
		if [ -r $PIDFILE ]; then
			kill -HUP "\$(cat $PIDFILE)" 2>/dev/null || true
		fi
	endscript
}
EOF

	# stderr, not the sink: no signal reopens these, so they are copied and
	# truncated in place. The window that loses a byte is real and accepted -
	# these files carry boot failures and the odd Degraded record, not the feed.
	if [ -n "$LOGD_ERR" ] || [ -n "$TETRISD_ERR" ]; then
		cat <<EOF

$LOGD_ERR $TETRISD_ERR {
$(emit_su)	weekly
	maxsize 20M
	rotate 8
	missingok
	notifempty
	compress
	copytruncate
}
EOF
	fi
}

case "$ACTION" in
	print)
		generate
		;;
	check)
		TMP=$(mktemp) && trap 'rm -f "$TMP"' EXIT
		generate > "$TMP"
		STATE=$(mktemp) && trap 'rm -f "$TMP" "$STATE"' EXIT
		if logrotate --debug --state "$STATE" "$TMP"; then
			echo "logrotate accepted the rule."
		else
			echo "logrotate.sh: logrotate rejected the generated rule" >&2
			exit 1
		fi
		;;
	install)
		if ! generate > "$DEST" 2>/dev/null; then
			echo "logrotate.sh: cannot write $DEST — run as root, or pass a path" >&2
			exit 1
		fi
		chmod 0644 "$DEST"
		echo "Wrote $DEST"
		echo "Verify with: logrotate --debug $DEST"
		;;
esac
