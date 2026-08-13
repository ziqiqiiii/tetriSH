# tetriSH client environment.
#
# This image builds and runs `tetrisu` only. It deliberately does not draw
# anything itself: the board is Kitty-graphics-protocol escape sequences, and
# those travel out over the pty `docker run -t` allocates and are rendered by
# the terminal on the *host*. The container never needs a display, which is
# what lets one Linux image serve a Linux, macOS, and WSL client alike.
#
# It is not a server image. tetrisd needs epoll, timerfd and POSIX mqueue, and
# is run natively on the Linux host beside this (see scripts/play.sh).
#
#   built and run by: scripts/container.sh (via `make play`)
#
FROM ubuntu:24.04

# Two groups: what tetrisu and the CoreStack archives link (OpenSSL, ncurses,
# SDL2 for audio), and what building notcurses from source needs (cmake, git,
# and its own dependency set). kitty-terminfo is not optional — notcurses reads
# the terminal's capabilities from terminfo, and without an xterm-kitty entry
# inside the container it cannot know the host terminal speaks the graphics
# protocol, so the board silently degrades to cells.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        binutils \
        pkg-config \
        cmake \
        git \
        ca-certificates \
        libssl-dev \
        libncurses-dev \
        libsdl2-dev \
        libsdl2-mixer-dev \
        libavdevice-dev \
        libdeflate-dev \
        libgpm-dev \
        libswscale-dev \
        libunistring-dev \
        kitty-terminfo \
        ncurses-term \
        locales \
    && rm -rf /var/lib/apt/lists/*

# The notcurses version is read out of src/tetrisu/Makefile rather than written
# here, so the pin has one owner: bumping NOTCURSES_VERSION there rebuilds this
# image to match instead of leaving the two to drift. No distro package works —
# Ubuntu's predates the NCBLIT_4x2 blitter tetrisu uses, and Debian ships none.
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

# notcurses decodes the board's artwork as UTF-8 and draws box-drawing glyphs;
# under the default POSIX locale both come out as question marks.
RUN sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen && locale-gen
ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

WORKDIR /tetrish

# The host has already resolved its own dependencies by the time this builds,
# and everything this image needs is installed above; DEPS_READY stops
# src/tetrisu/Makefile recursing into the root `deps` target, and
# AUTO_INSTALL_DEPS keeps the probe from trying to apt-get inside the build.
ENV AUTO_INSTALL_DEPS=0 DEPS_READY=1

# .dockerignore keeps src/tetrisu/assets out of this: 183 MB of artwork that
# would be copied into a layer only to be shadowed by the read-only bind mount
# scripts/container.sh puts at the same path. The compiled-in ASSET_DIR is
# /tetrish/src/tetrisu/assets either way, so the mount lands where the binary
# already looks.
COPY . .

# Objects and binary go to container-only directories so a host-native build of
# the same checkout can coexist: bin/tetrisu on the host is linked against the
# host's notcurses, bin-container/tetrisu against this image's, and neither
# overwrites the other.
RUN make -C src/tetrisu OBJ_DIR=obj-container BIN_DIR=bin-container

ENTRYPOINT ["/tetrish/src/tetrisu/bin-container/tetrisu"]
