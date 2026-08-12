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
#   AUTO_INSTALL_DEPS  1 to install a missing terminal, 0 to only report

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

# Where the chosen terminal actually is, or empty if it is not installed.
# macOS is the awkward one: kitty ships as a cask, so the binary lives inside
# the bundle and is not put on PATH by the installer.
terminal_binary() {
    local choice="$1"
    case "$choice" in
        kitty)
            if command -v kitty >/dev/null 2>&1; then
                command -v kitty
            elif [ -x /Applications/kitty.app/Contents/MacOS/kitty ]; then
                printf '%s' /Applications/kitty.app/Contents/MacOS/kitty
            elif [ -x "$HOME/Applications/kitty.app/Contents/MacOS/kitty" ]; then
                printf '%s' "$HOME/Applications/kitty.app/Contents/MacOS/kitty"
            fi
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

ensure_terminal() {
    local choice="$1" path
    path="$(terminal_binary "$choice")"
    [ -n "$path" ] && { printf '%s' "$path"; return 0; }

    if [ "$AUTO_INSTALL_DEPS" != "1" ]; then
        warn "$choice is not installed and automatic installation is disabled."
        return 1
    fi

    case "$choice" in
        kitty)
            say "installing kitty..." >&2
            if [ "$UNAME_S" = "Darwin" ]; then
                install_kitty_darwin >&2
            else
                install_kitty_linux >&2
            fi
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

    path="$(terminal_binary "$choice")"
    [ -n "$path" ] || { warn "$choice still not found after installing."; return 1; }
    printf '%s' "$path"
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

# No window to open: run here, and say what that costs if the surrounding
# terminal cannot draw bitmaps. This is not an error path — the game plays in
# compatibility mode, and saying so beats a board that silently looks wrong.
run_in_place() {
    if ! current_term_has_graphics; then
        warn "no display to open a terminal on, and ${BOLD}TERM=${TERM:-unset}${RST} does not"
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
