# tetriSH development environment.
#
#   make docker-build   build this image
#   make docker-run     interactive shell + daemons, port 4242 published
#   make docker-test    run every component suite
#
FROM ubuntu:24.04

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
        kitty-terminfo \
        locales \
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

RUN sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen && locale-gen
ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    TERM=xterm-256color

WORKDIR /tetrish

ENV AUTO_INSTALL_DEPS=0 DEPS_READY=1

COPY scripts/ ./scripts/
COPY src/tetrisu/scripts/ ./src/tetrisu/scripts/
RUN bash ./scripts/check_deps.sh \
    && bash ./src/tetrisu/scripts/check_deps.sh \
    && printf '%s\n' \
        '#include <notcurses/notcurses.h>' \
        'int main(void) { return (int)NCBLIT_4x2; }' \
       | gcc -x c - $(pkg-config --cflags --libs notcurses) \
         -o /tmp/tetrisu-probe \
    && rm -f /tmp/tetrisu-probe

COPY . .

RUN make all bin-link && make certs

ENV TETRISHRC=/tetrish/.tetrishrc

EXPOSE 4242

CMD ["make", "run"]
