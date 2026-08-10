# tetriSH container images. One file, two targets, because the two halves of
# this system want different machines.
#
#   make docker-build-server   the server half alone   -> tetrish:server
#   make docker-build          the full environment    -> tetrish:dev
#
#   make docker-run     interactive shell + daemons, port 4242 published
#   make docker-test    run every component suite
#
# The `server` target is what a deployment runs (docs/deployment.md) and what
# `make play` starts on macOS. It carries the daemons, the shell and tetrisctl,
# and nothing else: no notcurses, no SDL2, no valgrind. tetrisu is never run
# from a container - the board is Kitty-protocol bitmaps and it wants the
# player's own terminal - so building it here would buy nothing and cost the
# source build of notcurses plus the ffmpeg headers behind it, which is most of
# this image's build time and most of its size. Leaving it out is what lets the
# image be built on the same 1 GB droplet that then serves it.
#
# The `dev` target is the original single image - everything, tetrisu included -
# for docker-run, docker-test and docker-shell. It is last so that a bare
# `docker build .` with no --target still means "the whole environment", as it
# did when there was only one image to mean.

################################################################################
#                    base - what both halves need to compile                   #
################################################################################

FROM ubuntu:24.04 AS base

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libssl-dev \
        libreadline-dev \
        libncurses-dev \
        ca-certificates \
        locales \
    && rm -rf /var/lib/apt/lists/*

RUN sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen && locale-gen
ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    TERM=xterm-256color

WORKDIR /tetrish

ENV AUTO_INSTALL_DEPS=0 DEPS_READY=1
ENV TETRISHRC=/tetrish/.tetrishrc

# The dependency probe before the source, so a missing package fails on its own
# layer and stays cached across every later edit to the tree.
COPY scripts/ ./scripts/
RUN bash ./scripts/check_deps.sh

################################################################################
#                     server - the deployable half, headless                   #
################################################################################

FROM base AS server

COPY . .

# SKIP_COMPONENTS drops src/tetrisu from the umbrella build. Its own Makefile
# is what pulls notcurses in, and none of it is installed in this stage.
RUN make all bin-link SKIP_COMPONENTS=src/tetrisu

# No `make certs` here, deliberately. docker-server bind-mounts the host's
# certs/ read-only over this path, because the host's tetrisu has to verify the
# server against the same CA - so a baked set would be shadowed in the normal
# case and would surface only when the mount was missing, letting tetrisd boot
# against a CA no client can verify instead of failing where somebody notices.
# scripts/docker_server.sh mints them when the directory is writable and reports
# it when it is not.

EXPOSE 4242

CMD ["bash", "scripts/docker_server.sh"]

################################################################################
#                    dev - the whole environment, tetrisu too                  #
################################################################################

FROM base AS dev

RUN apt-get update && apt-get install -y --no-install-recommends \
        valgrind \
        libc6-dbg \
        libsdl2-dev \
        libsdl2-mixer-dev \
        ncurses-term \
        kitty-terminfo \
        git \
        cmake \
        libavdevice-dev \
        libdeflate-dev \
        libgpm-dev \
        libswscale-dev \
        libunistring-dev \
    && rm -rf /var/lib/apt/lists/*

ARG NOTCURSES_VERSION=
ARG NOTCURSES_PREFIX=/usr/local
COPY src/tetrisu/Makefile /tmp/tetrisu.mk
COPY src/tetrisu/scripts/install_notcurses.sh /tmp/install_notcurses.sh
RUN set -eu; \
    version="${NOTCURSES_VERSION:-$(sed -n \
        's/^NOTCURSES_VERSION[[:space:]]*:=[[:space:]]*\([^[:space:]]*\).*/\1/p' \
        /tmp/tetrisu.mk | head -n 1)}"; \
    if [ -z "$version" ]; then \
        echo "Could not read NOTCURSES_VERSION from src/tetrisu/Makefile" >&2; \
        exit 1; \
    fi; \
    echo "Building notcurses $version"; \
    NOTCURSES_VERSION="$version" NOTCURSES_PREFIX="$NOTCURSES_PREFIX" \
        bash /tmp/install_notcurses.sh; \
    rm -f /tmp/install_notcurses.sh /tmp/tetrisu.mk

ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig

COPY src/tetrisu/scripts/ ./src/tetrisu/scripts/
RUN bash ./src/tetrisu/scripts/check_deps.sh \
    && printf '%s\n' \
        '#include <notcurses/notcurses.h>' \
        'int main(void) { return (int)NCBLIT_4x2; }' \
       | gcc -x c - $(pkg-config --cflags --libs notcurses) \
         -o /tmp/tetrisu-probe \
    && rm -f /tmp/tetrisu-probe

COPY . .

RUN make all bin-link && make certs

EXPOSE 4242

CMD ["make", "run"]