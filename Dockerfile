# tetriSH development environment.
#
# notcurses is built from source, pinned to the v3.0.17 the project targets
# (NOTCURSES_VERSION in src/tetrisu/Makefile). This is deliberate and not an
# oversight: Ubuntu 24.04 packages notcurses 3.0.7, which predates the
# NCBLIT_4x2 blitter that src/tetrisu/src/render_background.c selects, so the
# distro package fails the client build under -Werror. Debian ships no
# notcurses package at all. Building the pinned tag is therefore the only way
# the client compiles, and it is exactly what the project's own
# src/tetrisu/scripts/install_notcurses.sh does on a bare Linux host.
#
#   docker build -t tetrish .
#   docker run --rm -it tetrish              # interactive shell, daemons up
#   docker run --rm -it tetrish make test    # run every component suite
#
# Both daemons and the client are terminal programs, so run with -it; notcurses
# needs a real TTY and a TERM it recognises.
FROM ubuntu:24.04

# apt must never open a prompt during a build.
ENV DEBIAN_FRONTEND=noninteractive

# The repo's own dependency scripts are the source of truth for what is
# needed, but running them here would re-fetch the apt lists on every source
# change. Install the same package set directly so it lands in one cached
# layer; the check_deps.sh probe below proves the two agree.
#
#   root  (scripts/install_deps.sh):    toolchain, pkg-config, OpenSSL,
#                                       Readline, ncurses, Valgrind
#   tetrisu (src/tetrisu/scripts/...):  SDL2 + SDL2_mixer (optional audio);
#                                       notcurses is built from source below
#   notcurses build prerequisites:      cmake, FFmpeg (avdevice/swscale),
#                                       deflate, gpm, unistring
#
# ca-certificates is needed for the HTTPS apt/git traffic; locales and
# ncurses-term give notcurses a UTF-8 locale and a full terminfo database.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libssl-dev \
        libreadline-dev \
        libncurses-dev \
        valgrind \
        libc6-dbg \
        libsdl2-dev \
        libsdl2-mixer-dev \
        ca-certificates \
        ncurses-term \
        locales \
        git \
        cmake \
        libavdevice-dev \
        libdeflate-dev \
        libgpm-dev \
        libswscale-dev \
        libunistring-dev \
    && rm -rf /var/lib/apt/lists/*

# Build the pinned notcurses. Kept in its own layer, before the source copy,
# so this several-minute step is cached across ordinary code edits. The
# version and prefix mirror src/tetrisu/Makefile; ldconfig at the end of the
# script makes the new shared library visible.
ARG NOTCURSES_VERSION=v3.0.17
ARG NOTCURSES_PREFIX=/usr/local
COPY src/tetrisu/scripts/install_notcurses.sh /tmp/install_notcurses.sh
RUN NOTCURSES_VERSION="$NOTCURSES_VERSION" \
    NOTCURSES_PREFIX="$NOTCURSES_PREFIX" \
    bash /tmp/install_notcurses.sh \
    && rm -f /tmp/install_notcurses.sh

# Source-built pkg-config metadata lives under /usr/local; the root Makefile
# exports the same path for its recursive builds.
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig

# notcurses draws Unicode box/blocks; without a UTF-8 locale it falls back to
# ASCII and the intro/menu render wrong.
RUN sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen && locale-gen
ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    TERM=xterm-256color

WORKDIR /tetrish

# Dependencies are already installed above, so keep the build check-only:
# a missing package should fail loudly here rather than have a recipe try to
# apt-get inside a layer that has no apt lists left.
ENV AUTO_INSTALL_DEPS=0 \
    DEPS_READY=1

# Fail the build now, not at `make`, if the package set above ever drifts from
# what the project's own probe requires. check_deps.sh compiles and links a
# real program rather than trusting the package database.
COPY scripts/ ./scripts/
RUN bash ./scripts/check_deps.sh \
    && pkg-config --exists notcurses sdl2 SDL2_mixer \
    && printf '%s\n' \
        '#include <notcurses/notcurses.h>' \
        '#include <SDL_mixer.h>' \
        'int main(void) { return (int)NCBLIT_4x2; }' \
       | gcc -x c - $(pkg-config --cflags --libs notcurses sdl2 SDL2_mixer) \
         -o /tmp/tetrisu-probe \
    && rm -f /tmp/tetrisu-probe

# Source last: everything above is cached across code edits.
COPY . .

# Build libraries, shell, and daemons, then collect the binaries into ./bin —
# the shell prepends $PWD/bin to PATH and tetrisctl resolves each daemon
# through PATH, so a daemon missing from ./bin cannot be launched by name.
# Certificates are minted at build time because tetrisd refuses to boot
# without them; they are development credentials, valid ~365 days, and the
# certs/ directory is excluded from the build context so these are always
# freshly generated rather than copied from the host.
RUN make all bin-link && make certs

# TETRISHRC only. `bin/` is deliberately NOT put on the global PATH: the shell's
# system programs include `find` and `ld`, which would shadow /usr/bin/find and
# the system linker for every process in the image — that silently breaks gcc,
# and with it every `make test` run. The shell prepends $PWD/bin to PATH itself
# at start-up (init_root), and tetrisctl resolves the daemons through the PATH
# the shell builds, so nothing needs it exported here.
ENV TETRISHRC=/tetrish/.tetrishrc

# The game server's TCP port (TETRISD_PORT in .tetrishrc).
EXPOSE 4242

# `make run` launches the shell, which sources .tetrishrc — whose last line is
# `tetrisctl start`, bringing up tetrislogd then tetrisd before the first
# prompt. Override with e.g. `make test` or `make stack`.
CMD ["make", "run"]
