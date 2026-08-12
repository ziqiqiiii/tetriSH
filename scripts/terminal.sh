#!/usr/bin/env bash
# Decide which terminal draws the board, make sure it is installed, and launch
# a command inside it.
#
#   bash scripts/terminal.sh check           name the terminal and report it
#   bash scripts/terminal.sh install         install it if it is missing
#   bash scripts/terminal.sh launch -- CMD   run CMD in it
#
# Why this is a script and not three lines in play.sh: the choice is not "which
# OS" but "is there a display to open a window on", and those two answers come
# apart in exactly the case that matters. A Linux VM reached over SSH from
# Windows Terminal is Linux with no display: there is no window to open, so the
# only terminal available is the one the user is already sitting in, on the
# other end of the SSH connection. Deciding that in one place keeps play.sh
# from having to know it.
#
# The board is Kitty-graphics-protocol escape sequences. They are bytes on the
# pty, so they survive a docker exec and an SSH hop alike — what matters is
# only whether whatever is on the far end understands them. kitty and WezTerm
# do; Windows Terminal does not, and gets the cell renderer.
#
# Environment:
#   TETRISU_TERMINAL   force a choice: kitty, wezterm, none, or a path
#   AUTO_INSTALL_DEPS  1 to install or repair the terminal, 0 to only report
#   KITTY_MIN_VERSION  lowest kitty that can draw the board (default 0.20.0)
#   KITTY_MAX_VERSION  highest kitty this machine can run (default: from glibc)

set -euo pipefail

BOLD=$(printf '\033[1m'); RED=$(printf '\033[1;31m')
GRN=$(printf '\033[1;32m'); YEL=$(printf '\033[1;33m')
BLU=$(printf '\033[1;34m'); RST=$(printf '\033[0m')

say()  { printf '%b\n' "${BLU}==>${RST} ${BOLD}$*${RST}"; }
ok()   { printf '%b\n' "    ${GRN}$*${RST}"; }
warn() { printf '%b\n' "    ${YEL}$*${RST}" >&2; }
die()  { printf '%b\n' "${RED}terminal.sh:${RST} $*" >&2; exit 1; }

UNAME_S="$(uname -s)"
AUTO_INSTALL_DEPS="${AUTO_INSTALL_DEPS:-1}"

# The window of kitty versions that can draw the board here, as [min, max].
# Both ends produce the same symptom - a window with no board in it - so both
# are checked before launching, and a kitty outside the window is reinstalled
# rather than merely complained about.
#
# The floor is the protocol. The placement-id (p=) and quiet (q=) graphics keys
# and the kitty keyboard protocol all arrive in 0.20.0; below that the client's
# escape sequences are not merely unsupported but unparseable, so the window
# fills with "Malformed GraphicsCommand" instead. Distro packages sit here:
# Ubuntu 20.04 still ships 0.15.0, from 2019.
#
# The ceiling is this machine's C library, so it is computed rather than fixed.
# kitty 0.48 bundles a libpython built against GLIBC_2.35, so on an older
# release it unpacks cleanly and then refuses to start at all. 0.47.0 is the
# last release that runs below that line.
KITTY_MIN_VERSION="${KITTY_MIN_VERSION:-0.20.0}"
KITTY_MODERN_GLIBC="2.35"
KITTY_LEGACY_VERSION="0.47.0"
KITTY_INSTALLER_URL="https://sw.kovidgoyal.net/kitty/installer.sh"
# Where the upstream installer puts it, which is also where this script looks
# first - a repaired kitty must win over the distro one that PATH may still
# resolve to.
KITTY_PREFIX="$HOME/.local/kitty.app"

################################################################################
#                                  DETECTION                                   #
################################################################################

is_wsl() {
    grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease /proc/version \
        2>/dev/null
}

# A display is what makes launching a window possible at all. Wayland and X11
# are both accepted; WSLg publishes a DISPLAY of its own, so it satisfies this
# the same way a native desktop does.
has_display() {
    [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]
}

is_ssh() {
    [ -n "${SSH_CONNECTION:-}" ] || [ -n "${SSH_TTY:-}" ]
}

# Does the terminal we are already inside speak the Kitty graphics protocol?
# Only consulted on the in-place path, to decide between proceeding quietly and
# warning that the pixel board is about to become a cell board.
current_term_has_graphics() {
    case "${TERM:-}" in
        *kitty*|*ghostty*) return 0 ;;
    esac
    case "${TERM_PROGRAM:-}" in
        WezTerm|ghostty|kitty) return 0 ;;
    esac
    [ -n "${KITTY_WINDOW_ID:-}" ] && return 0
    [ -n "${WEZTERM_PANE:-}" ] && return 0
    return 1
}

# Names the terminal to use: kitty, wezterm, or none.
#
# "none" is not a failure — it is the correct answer whenever there is no
# display to open a window on, and it means "run in the terminal the user
# already has". That is the Windows-Terminal-over-SSH case, where the terminal
# that matters lives on a machine this script cannot reach.
choose_terminal() {
    if [ -n "${TETRISU_TERMINAL:-}" ]; then
        printf '%s' "$TETRISU_TERMINAL"
        return 0
    fi
    if [ "$UNAME_S" = "Darwin" ]; then
        # A Mac reached over SSH has no window server to open onto either.
        if is_ssh && ! has_display; then
            printf 'none'
        else
            printf 'kitty'
        fi
        return 0
    fi
    if is_wsl; then
        # Under WSLg a Linux kitty is a native window and the simplest path.
        # Without it, the only GUI on this machine is Windows', so hand off to
        # a WezTerm installed there — it speaks the graphics protocol and can
        # re-enter the distro to run the command.
        if has_display; then
            printf 'kitty'
        elif command -v wezterm.exe >/dev/null 2>&1; then
            printf 'wezterm'
        else
            printf 'none'
        fi
        return 0
    fi
    if has_display; then
        printf 'kitty'
    else
        printf 'none'
    fi
}

# Every kitty this machine might have, most trustworthy first. There can be
# more than one and they are routinely different versions: the upstream
# installer writes into ~/.local and does not remove the distro package, and
# whether PATH resolves to the good one depends on the user's profile. So the
# candidates are ranked here rather than left to `command -v`, and the first
# *usable* one wins - see kitty_find.
#
# macOS is the awkward one: kitty ships as a cask, so the binary lives inside
# the bundle and is not put on PATH by the installer.
kitty_candidates() {
    printf '%s\n' "$KITTY_PREFIX/bin/kitty"
    command -v kitty 2>/dev/null || true
    printf '%s\n' /Applications/kitty.app/Contents/MacOS/kitty
    printf '%s\n' "$HOME/Applications/kitty.app/Contents/MacOS/kitty"
}

# The best kitty installed, or empty. $1 selects which question is being asked:
# "usable" is the one to launch, "any" is the one to diagnose or replace.
kitty_find() {
    local want="${1:-usable}" path
    while read -r path; do
        [ -n "$path" ] && [ -x "$path" ] || continue
        [ "$want" = "any" ] && { printf '%s' "$path"; return 0; }
        kitty_usable "$path" && { printf '%s' "$path"; return 0; }
    done <<< "$(kitty_candidates)"
    return 1
}

# Where the chosen terminal actually is, or empty if it is not installed.
terminal_binary() {
    local choice="$1"
    case "$choice" in
        kitty)
            # An unusable kitty is still reported, because "installed but
            # wrong" and "not installed" need different repairs.
            kitty_find usable || kitty_find any || true
            ;;
        wezterm)
            command -v wezterm.exe 2>/dev/null || command -v wezterm 2>/dev/null
            ;;
        none)
            printf 'none'
            ;;
        /*)
            [ -x "$choice" ] && printf '%s' "$choice"
            ;;
        *)
            command -v "$choice" 2>/dev/null
            ;;
    esac
}

# True when dotted version $1 is at least $2. Compared field by field rather
# than with `sort -V`, which is a GNU extension the macOS sort does not carry.
version_ge() {
    local i have want
    local -a hp wp
    IFS=. read -r -a hp <<< "$1"
    IFS=. read -r -a wp <<< "$2"
    for i in 0 1 2; do
        have="${hp[i]:-0}"; want="${wp[i]:-0}"
        have="${have%%[!0-9]*}"; want="${want%%[!0-9]*}"
        [ -n "$have" ] || have=0
        [ -n "$want" ] || want=0
        [ "$have" -gt "$want" ] && return 0
        [ "$have" -lt "$want" ] && return 1
    done
    return 0
}

# "2.31" on this machine's libc, or empty where there is no glibc to ask -
# macOS and musl both land there, and both mean "no ceiling" rather than
# "unknown, so refuse".
glibc_version() {
    local out
    out="$({ getconf GNU_LIBC_VERSION 2>/dev/null || true; } | awk '{ print $2 }')"
    [ -n "$out" ] || out="$({ ldd --version 2>/dev/null || true; } \
        | awk 'NR == 1 { print $NF }')"
    case "$out" in
        [0-9]*) printf '%s' "$out" ;;
    esac
}

# The newest kitty this machine can actually start, or empty for no ceiling.
kitty_max_version() {
    local glibc
    if [ -n "${KITTY_MAX_VERSION:-}" ]; then
        printf '%s' "$KITTY_MAX_VERSION"
        return 0
    fi
    glibc="$(glibc_version)"
    [ -n "$glibc" ] || return 0
    version_ge "$glibc" "$KITTY_MODERN_GLIBC" || printf '%s' "$KITTY_LEGACY_VERSION"
}

# Asked once, because it shells out and every candidate would ask it again.
KITTY_MAX_VERSION="$(kitty_max_version)"

# The version to install when repairing: the ceiling if there is one, and
# whatever upstream calls current otherwise.
kitty_target_version() {
    printf '%s' "${KITTY_MAX_VERSION:-latest}"
}

# "kitty 0.47.0 created by Kovid Goyal" -> "0.47.0". Empty when the binary
# cannot run at all, which is its own failure and reported separately.
kitty_version() {
    "$1" --version 2>/dev/null | awk 'NR == 1 { print $2; exit }'
}

# A kitty on PATH is not the same as a kitty that can draw. Quiet on purpose:
# this runs against every candidate in turn, so the diagnosis belongs to
# kitty_explain, which is called once against the one that was settled on.
kitty_usable() {
    local path="$1" have
    have="$(kitty_version "$path")" || have=""

    [ -n "$have" ] || return 1
    version_ge "$have" "$KITTY_MIN_VERSION" || return 1
    [ -z "$KITTY_MAX_VERSION" ] || version_ge "$KITTY_MAX_VERSION" "$have" || return 1
    return 0
}

# Why the kitty at $1 cannot draw. Three answers, and they are not the same
# repair - too old is a distro package, too new is a libc mismatch, and one
# that will not run at all has already printed its own reason.
kitty_explain() {
    local path="$1" have why
    have="$(kitty_version "$path")" || have=""

    if [ -z "$have" ]; then
        # The binary is expected to fail here, so its non-zero exit must not
        # discard the diagnosis it just printed — hence the inner `|| true`
        # rather than a `||` on the assignment.
        why="$({ "$path" --version 2>&1 || true; } | head -1)"
        warn "${BOLD}$path${RST} is installed but will not start:"
        [ -n "$why" ] && warn "    $why"
        warn "A kitty built for a newer glibc does this."
    elif ! version_ge "$have" "$KITTY_MIN_VERSION"; then
        warn "kitty ${BOLD}$have${RST} at $path is too old to draw the board"
        warn "(need ${BOLD}$KITTY_MIN_VERSION${RST} or newer). It cannot parse the graphics"
        warn "protocol the client speaks, so the window fills with"
        warn "${BOLD}Malformed GraphicsCommand${RST} errors and no board appears."
    else
        warn "kitty ${BOLD}$have${RST} at $path is newer than this system can run"
        warn "(glibc ${BOLD}$(glibc_version)${RST} needs kitty ${BOLD}$KITTY_MAX_VERSION${RST} or older)."
        warn "It unpacks cleanly and then refuses to start, so no window opens."
    fi
}

kitty_install_hint() {
    warn "Install a kitty this machine can run, for this user only:"
    warn "    ${BOLD}curl -fsSL $KITTY_INSTALLER_URL | sh /dev/stdin launch=n \\"
    warn "        installer=version-$(kitty_target_version)${RST}"
    warn "then put ${BOLD}~/.local/bin${RST} on PATH ahead of /usr/bin:"
    warn "    ${BOLD}ln -sf $KITTY_PREFIX/bin/kitty ~/.local/bin/kitty${RST}"
}

# Only kitty is version-gated. WezTerm has spoken the graphics protocol for as
# long as it has been an option here, and a path the caller named through
# TETRISU_TERMINAL is their choice to make.
verify_terminal() {
    case "$1" in
        kitty) kitty_usable "$2" ;;
        *)     return 0 ;;
    esac
}

################################################################################
#                                   INSTALL                                    #
################################################################################

resolve_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
    elif command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        die "root access or sudo is required to install a terminal."
    fi
}

install_kitty_linux() {
    resolve_sudo
    if command -v apt-get >/dev/null 2>&1; then
        $SUDO apt-get update && $SUDO apt-get install -y kitty
    elif command -v dnf >/dev/null 2>&1; then
        $SUDO dnf install -y kitty
    elif command -v pacman >/dev/null 2>&1; then
        $SUDO pacman -S --needed --noconfirm kitty
    elif command -v zypper >/dev/null 2>&1; then
        $SUDO zypper --non-interactive install kitty
    elif command -v apk >/dev/null 2>&1; then
        $SUDO apk add kitty
    else
        die "unsupported package manager; install kitty from https://sw.kovidgoyal.net/kitty/"
    fi
}

install_kitty_darwin() {
    command -v brew >/dev/null 2>&1 \
        || die "Homebrew is required to install kitty: https://brew.sh"
    brew install --cask kitty
}

# The repair path, and the only one that can choose a *version*. A package
# manager offers exactly one kitty and it is as likely to be four years old as
# current; upstream's installer takes the number, which is what makes a
# glibc ceiling actionable rather than just a diagnosis.
#
# Installed per-user under ~/.local, so it needs no privilege and cannot
# conflict with the distro package it is there to overrule - kitty_candidates
# looks here first precisely so PATH does not get a vote.
install_kitty_upstream() {
    local want
    want="$(kitty_target_version)"
    command -v curl >/dev/null 2>&1 \
        || { warn "curl is needed to install kitty $want."; return 1; }

    say "installing kitty $want under $KITTY_PREFIX..." >&2
    if [ "$want" = "latest" ]; then
        curl -fsSL "$KITTY_INSTALLER_URL" | sh /dev/stdin launch=n >&2 \
            || return 1
    else
        curl -fsSL "$KITTY_INSTALLER_URL" \
            | sh /dev/stdin launch=n "installer=version-$want" >&2 || return 1
    fi

    # A convenience, not the mechanism: this script launches kitty by absolute
    # path, so the game does not need PATH to agree. The symlink is for the
    # user's own shell, and only when nothing else already claims the name.
    if [ -x "$KITTY_PREFIX/bin/kitty" ] && [ ! -e "$HOME/.local/bin/kitty" ]; then
        mkdir -p "$HOME/.local/bin"
        ln -sf "$KITTY_PREFIX/bin/kitty" "$HOME/.local/bin/kitty"
    fi
}

# WezTerm is only ever chosen on WSL, and there it is a *Windows* application:
# installing it means driving winget across the WSL boundary, which prompts on
# the Windows side and puts software outside the environment `make` was invoked
# in. That is not a thing to do silently behind a build command, so this prints
# the one line to run rather than running it.
instruct_wezterm() {
    warn "WezTerm is not installed on the Windows side."
    warn "Install it from a Windows PowerShell prompt:"
    warn "    ${BOLD}winget install wez.wezterm${RST}"
    warn "then run 'make play' again."
    return 1
}

# Bring kitty to a version that can draw, from wherever it is now. Three
# starting points and they do not cost the same, so they are not attempted in
# the same order:
#
#   nothing installed   the package manager first - it is the cheapest, needs
#                       no network beyond its own mirrors, and on a current
#                       distro it is simply right
#   wrong version       straight to upstream. The package manager offers one
#                       kitty and it has already been established that it is
#                       not the one needed, so asking it again spends a sudo
#                       prompt to reinstall the same file
#   right version       nothing
#
# Either way the result is re-verified rather than assumed, because "installed"
# and "can draw the board" are the two different things this whole file exists
# to keep apart.
repair_kitty() {
    local path
    path="$(kitty_find any || true)"

    if [ -z "$path" ]; then
        say "installing kitty..." >&2
        if [ "$UNAME_S" = "Darwin" ]; then
            install_kitty_darwin >&2 || true
        else
            install_kitty_linux >&2 || true
        fi
        kitty_find usable && return 0
        path="$(kitty_find any || true)"
    fi

    [ -n "$path" ] && kitty_explain "$path"

    # macOS keeps its cask: brew's kitty is current, there is no glibc ceiling
    # to dodge, and the upstream tarball would leave a second kitty.app behind
    # the one Homebrew still thinks it owns.
    if [ "$UNAME_S" = "Darwin" ]; then
        say "upgrading kitty..." >&2
        brew upgrade --cask kitty >&2 || true
    else
        install_kitty_upstream || true
    fi
    kitty_find usable
}

ensure_terminal() {
    local choice="$1" path
    path="$(terminal_binary "$choice")"
    if [ -n "$path" ] && verify_terminal "$choice" "$path"; then
        printf '%s' "$path"
        return 0
    fi

    if [ "$AUTO_INSTALL_DEPS" != "1" ]; then
        if [ -n "$path" ]; then
            [ "$choice" = "kitty" ] && kitty_explain "$path"
            warn "$choice cannot draw the board and repair is disabled."
            [ "$choice" = "kitty" ] && kitty_install_hint
        else
            warn "$choice is not installed and automatic installation is disabled."
        fi
        return 1
    fi

    case "$choice" in
        kitty)
            if ! path="$(repair_kitty)"; then
                warn "kitty is still not able to draw the board."
                kitty_install_hint
                return 1
            fi
            printf '%s' "$path"
            return 0
            ;;
        wezterm)
            instruct_wezterm
            return 1
            ;;
        *)
            warn "don't know how to install '$choice'; install it and retry."
            return 1
            ;;
    esac
}

################################################################################
#                                    LAUNCH                                    #
################################################################################

# Runs the command in the chosen terminal, replacing this process so the
# caller's exit status is the terminal's.
#
# --hold on kitty keeps the window open after the command exits. Without it a
# client that dies during start-up closes its own window along with whatever it
# printed about why, which is indistinguishable from the window never opening.
launch_in_terminal() {
    local choice="$1" path="$2"; shift 2
    case "$choice" in
        kitty)
            exec "$path" --hold -e "$@"
            ;;
        wezterm)
            # Launched from Windows, so it has to re-enter this distro to reach
            # the command; WSL_DISTRO_NAME names the one we are running in.
            if [ "${path##*/}" = "wezterm.exe" ]; then
                exec "$path" start -- \
                    wsl.exe -d "${WSL_DISTRO_NAME:-}" -- "$@"
            fi
            exec "$path" start -- "$@"
            ;;
        *)
            exec "$path" -e "$@"
            ;;
    esac
}

# Run here, and say what that costs if the surrounding terminal cannot draw
# bitmaps. This is not an error path — the game plays in compatibility mode, and
# saying so beats a board that silently looks wrong. Reached two ways, so the
# message names neither cause: there may be no display to open a window on, or
# there may be one whose kitty was rejected as unable to draw.
run_in_place() {
    if ! current_term_has_graphics; then
        warn "running in this terminal, and ${BOLD}TERM=${TERM:-unset}${RST} does not"
        warn "speak the Kitty graphics protocol, so the board will draw in"
        warn "compatibility mode (cells, not pixel art)."
        if is_ssh; then
            warn "For the pixel board over SSH, connect from ${BOLD}WezTerm${RST} or"
            warn "${BOLD}kitty${RST} instead of Windows Terminal — the escape sequences"
            warn "survive the hop, so only the near end has to understand them."
        fi
    fi
    exec "$@"
}

################################################################################
#                                     MAIN                                     #
################################################################################

action="${1:-check}"
[ $# -gt 0 ] && shift

case "$action" in
    check)
        choice="$(choose_terminal)"
        path="$(terminal_binary "$choice")"
        if [ "$choice" = "none" ]; then
            echo "terminal: none (no display; will run in the current terminal)"
        elif [ -n "$path" ]; then
            if ! verify_terminal "$choice" "$path"; then
                echo "terminal: $choice ($path, cannot draw the board)"
                exit 1
            fi
            echo "terminal: $choice ($path)"
        else
            echo "terminal: $choice (not installed)"
            exit 1
        fi
        ;;
    install)
        choice="$(choose_terminal)"
        [ "$choice" = "none" ] && { ok "no terminal needed; running in place"; exit 0; }
        ensure_terminal "$choice" >/dev/null || exit 1
        ok "$choice is ready"
        ;;
    launch)
        [ "${1:-}" = "--" ] && shift
        [ $# -gt 0 ] || die "launch needs a command after --"
        choice="$(choose_terminal)"
        if [ "$choice" = "none" ]; then
            run_in_place "$@"
        fi
        if ! path="$(ensure_terminal "$choice")"; then
            warn "falling back to the current terminal."
            run_in_place "$@"
        fi
        launch_in_terminal "$choice" "$path" "$@"
        ;;
    *)
        die "unknown action '$action' (check, install, launch)"
        ;;
esac
