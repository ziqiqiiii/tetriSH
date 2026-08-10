#  _         _        _ ___  _  _
# | |_  ___ | |_  _ _ (_) __|| || |
# |  _|/ -_)|  _|| '_|| \__ \| __ |
#  \__|\___| \__||_|  |_|___/|_||_|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

# tetriSH umbrella Makefile. Recurses into the self-contained libraries and the
# vendored shell, then provides a shell-driven `run` entry point. Daemons are
# launched by tetrisctl from inside the shell (see .tetrishrc), not from here.
#
#   make / make all   install missing dependencies, then build everything
#   make deps         check/install dependencies for this OS
#   make check-deps   verify dependencies without changing the system
#   make run          build, then launch the shell (sources .tetrishrc)
#   make certs        generate the dev CA + server certificate tetrisd needs
#   make stack        build, then launch the daemons headless (integration tests)
#   make test         build, then run every available component test suite
#   make clean        recurse `clean` into every component
#   make fclean       recurse `fclean` and drop ./bin
#   make re           fclean + all
#
#   make play         set up everything and launch a client against a server
#
#   make docker-build build the full image (installs every dependency itself)
#   make docker-build-server build the server-only image (no tetrisu, no
#                     notcurses) - what a deployment and `play` run
#   make docker-run   run the shell + daemons in a container, publishing 4242
#   make docker-server run the daemons alone, detached, for a client on the host
#   make docker-logs  follow the detached server's daemon logs
#   make docker-test  run every test suite inside a container
#   make docker-shell open a bash prompt inside a container
#   make docker-stop  stop the running containers, freeing the port
#   make docker-clean remove the image
#   make docker-reset stop the containers and drop the state volume

MAKE_FLAGS	:= --no-print-directory -s
RM			:= rm -rf

AUTO_INSTALL_DEPS	:= 1
REQUIRE_VALGRIND	:= 0
REQUIRE_DOCKER		:= 0

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN		:= \033[1;36m

################################################################################
#                              PLATFORM SPECIFICS                              #
################################################################################

UNAME_S		:= $(shell uname -s)

# Source-built dependencies usually install pkg-config metadata under
# /usr/local; keep that visible before falling back to distro/Homebrew paths.
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

# Homebrew keeps these libraries keg-only on some macOS releases. Export their
# pkg-config metadata so this Makefile and every recursive component build use
# the same headers and libraries on both Intel and Apple Silicon Macs.
ifeq ($(UNAME_S), Darwin)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifneq ($(BREW_PREFIX),)
export PKG_CONFIG_PATH := $(BREW_PREFIX)/opt/openssl@3/lib/pkgconfig:$(BREW_PREFIX)/opt/readline/lib/pkgconfig:$(BREW_PREFIX)/opt/ncurses/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif
endif

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SHELL_DIR	:= src/tetrish
SHELL_BIN	:= $(SHELL_DIR)/macmini_shell
BIN			:= bin
CERT_DIR	:= certs

# Build only the components that exist yet — the project is in early dev, so
# the daemon directories are filled in over time. Match Makefiles rather than
# directories so ignored build artefacts cannot be mistaken for components.
LIB_MAKEFILES		:= $(wildcard lib/lib*/Makefile)
LIB_DIRS			:= $(patsubst %/,%,$(dir $(LIB_MAKEFILES)))

COMPONENT_MAKEFILES	:= $(wildcard src/tetrisd/Makefile \
							  src/tetrislogd/Makefile \
							  src/tetrisctl/Makefile \
							  src/tetrisu/Makefile)

# Components this build leaves out, even though their Makefile is present. The
# server container image sets SKIP_COMPONENTS=src/tetrisu: nothing in the server
# half needs notcurses, and building it from source is most of that image.
# Tests follow the same list, since a component that was not built cannot be
# tested.
SKIP_COMPONENTS		?=

DAEMON_DIRS			:= $(filter-out $(SKIP_COMPONENTS), \
							  $(patsubst %/,%,$(dir $(COMPONENT_MAKEFILES))))
TEST_DIRS			:= $(LIB_DIRS) $(filter src/tetrisu src/tetrisd \
							  src/tetrislogd src/tetrisctl,$(DAEMON_DIRS))

################################################################################
#                                   BUILD                                      #
################################################################################

all: deps libs shell daemons bin-link

# --- libraries (self-contained archives under lib/) -------------------------
libs: | deps
	@ for d in $(LIB_DIRS); do $(MAKE) $(MAKE_FLAGS) -C $$d || exit 1; done

# --- vendored shell ---------------------------------------------------------
shell: | deps
	@ $(MAKE) $(MAKE_FLAGS) -C $(SHELL_DIR) DEPS_READY=1

# --- daemons / client components (need the libraries first) -----------------
daemons: libs | deps
	@ for d in $(DAEMON_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d DEPS_READY=1 || exit 1; \
	done

# Collect every built binary into a single ./bin. The shell prepends $PWD/bin
# to PATH, and tetrisctl resolves each daemon through PATH exactly as execvp
# does, so a daemon missing from ./bin cannot be launched by name at all. Each
# component's binary is named after its directory; unbuilt ones are skipped.
# Both layouts are searched, because the daemons build their binary beside
# their Makefile and tetrisu builds its own into bin/ - looking only for the
# first is what left ./bin/tetrisu missing after a successful build.
bin-link: shell daemons
	@ mkdir -p $(BIN)
	@ ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true
	@ for d in $(DAEMON_DIRS); do \
		n=`basename $$d`; \
		for p in $$d/$$n $$d/$(BIN)/$$n; do \
			if [ -x $$p ]; then ln -sf $(CURDIR)/$$p $(BIN)/; break; fi; \
		done; \
	done

# Idiomatic launch: the shell sources .tetrishrc, whose last line is
# `tetrisctl start` - so the daemons come up before the first prompt.
#
# certs is a prerequisite for the same reason it is one of `stack`: that
# `tetrisctl start` boots tetrisd, and tetrisd treats missing certificates as
# a fatal boot error. certs/ is git-ignored, so on a fresh clone this is the
# difference between a shell with a game server behind it and one without.
run: all bin-link certs
	@ TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

# Development credentials for the secure session. tetrisd refuses to boot
# without them, so `run` and `stack` depend on this; the directory is
# git-ignored and the script is a no-op while the certificate is still valid.
certs:
	@ bash ./scripts/generate_certs.sh $(CERT_DIR)

# Headless stack for integration tests (no interactive shell). tetrisctl reads
# the roster and its order from .tetrishrc, and each daemon detaches itself, so
# this returns only once they are actually up - and non-zero if one is not.
stack: all bin-link certs
	@ PATH=$(CURDIR)/$(BIN):$$PATH TETRISHRC=$(CURDIR)/.tetrishrc \
		$(BIN)/tetrisctl start
	@ PATH=$(CURDIR)/$(BIN):$$PATH TETRISHRC=$(CURDIR)/.tetrishrc \
		$(BIN)/tetrisctl status

# --- component tests --------------------------------------------------------
test: all
	@ for d in $(TEST_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d test DEPS_READY=1 || exit 1; \
	done

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# `make` installs only when the compile/link probe fails. Set
# AUTO_INSTALL_DEPS=0 in CI or managed environments to make this check-only.
# Outsourced to scripts/; it delegates to check_deps.sh / install_deps.sh.
deps:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)' \
		bash ./scripts/deps.sh

# Root deps are shared by multiple components: compiler toolchain, pkg-config,
# OpenSSL, Readline, and ncurses. Install is outsourced to scripts/; component
# render/audio packages belong in that component's own Makefile.
install-deps:
	@ bash ./scripts/install_deps.sh

# Verify dependencies without changing the system. Outsourced to scripts/; the
# script compiles/links a probe rather than trusting the package database.
check-deps:
	@ UNAME_S=$(UNAME_S) REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		bash ./scripts/check_deps.sh

# Read-only summary of the dependency situation. Outsourced to scripts/.
deps-info:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		bash ./scripts/deps_info.sh

################################################################################
#                                    DOCKER                                    #
################################################################################

# Containerised build of the whole stack. Every dependency - the toolchain,
# OpenSSL, ncurses, SDL2 and the source-built notcurses - is installed by the
# Dockerfile, so there is no separate download step to run here.

DOCKER			:= docker
DOCKER_IMAGE	:= tetrish
DOCKER_TAG		:= dev
DOCKER_REF		:= $(DOCKER_IMAGE):$(DOCKER_TAG)
DOCKER_NAME		:= tetrish
DOCKER_SERVER	:= tetrish-server

# Two images from one Dockerfile (see its header). The server one is the
# deployable half and the only one `play` and `docker-server` want; the dev one
# carries tetrisu and the test tooling as well.
DOCKER_SRV_TAG	:= server
DOCKER_SRV_REF	:= $(DOCKER_IMAGE):$(DOCKER_SRV_TAG)
DOCKER_PORT		:= 4242

# Named volume for the daemons' runtime state, mounted where .tetrishrc points
# TETRISD_DATA_DIR and the logger's sink: tmp/. Without it the player store's
# append-only log is inside the container's writable layer, so every player,
# wallet and leaderboard row is lost the moment the container is replaced -
# which `docker run --rm` does on every single run.
DOCKER_STATE	:= tetrish-state

# -it only when there is a terminal to attach. make's own stdin says nothing
# about that, so the test opens /dev/tty on fd 3 - and stderr is dropped for the
# whole probe rather than for the test alone, because it is the *redirection*
# that fails where there is no controlling terminal, and that message escapes an
# inner 2>/dev/null. It surfaced as "/bin/sh: /dev/tty: Device not configured"
# on every docker target run from a script.
DOCKER_TTY		 = $(shell exec 2>/dev/null; { [ -t 3 ] \
					&& printf -- '-it' || printf -- '-i'; } 3</dev/tty \
					|| printf -- '-i')

# TETRISD_PORT travels with the published port. tetrisd lets the environment win
# over .tetrishrc, and without this line `DOCKER_PORT=5252` published 5252 to a
# server still listening on 4242 - a container that starts, reports itself up,
# and answers nothing.
DOCKER_ENV		 = -e TETRISD_PORT=$(DOCKER_PORT) -e TETRISU_PORT=$(DOCKER_PORT)

# --init, because both daemons double-fork: the intermediate parent exits and
# the detached daemon is reparented onto pid 1, which is a shell that never
# waited for it and so cannot reap it. tini can, and forwards TERM to the
# script's trap unchanged.
DOCKER_INIT		 = --init

# The host's TERM, forwarded when there is one. The image pins
# TERM=xterm-256color, which has no bitmap graphics protocol, so an in-container
# tetrisu could not draw a board no matter what terminal was actually attached;
# kitty-terminfo is installed for the same reason. Not passed to docker-server,
# which draws nothing.
DOCKER_TERM		 = $(if $(TERM),-e TERM=$(TERM))

docker-build:
	@ echo "\n$(CYAN)==> Building image$(CLR_RMV) $(BLUE)$(DOCKER_REF)$(CLR_RMV)..."
	@ $(DOCKER) build --target dev -t $(DOCKER_REF) .
	@ echo "$(GREEN)[Success] $(BLUE)$(DOCKER_REF)$(CLR_RMV) built ✔️"

# The server half alone: no notcurses, no SDL2, no valgrind, so this is both the
# faster build and the one small enough to build on the box that will serve it.
docker-build-server:
	@ echo "\n$(CYAN)==> Building image$(CLR_RMV) $(BLUE)$(DOCKER_SRV_REF)$(CLR_RMV)..."
	@ $(DOCKER) build --target server -t $(DOCKER_SRV_REF) .
	@ echo "$(GREEN)[Success] $(BLUE)$(DOCKER_SRV_REF)$(CLR_RMV) built ✔️"


docker-run: docker-stop
	@ $(DOCKER) run --rm $(DOCKER_TTY) $(DOCKER_TERM) $(DOCKER_INIT) \
		--name $(DOCKER_NAME) $(DOCKER_ENV) \
		-p $(DOCKER_PORT):$(DOCKER_PORT) \
		-v $(DOCKER_STATE):/tetrish/tmp $(DOCKER_REF)

# The server on its own, detached, for a client running on the host - the only
# way to play on macOS, where tetrisd cannot be built at all (epoll, timerfd,
# POSIX mqueue). certs/ is mounted rather than baked: the host's tetrisu has to
# verify the server against the same CA, and certificates minted inside the
# image would be in a filesystem the host cannot read. Read-only, because the
# host is what mints them - `certs` is a prerequisite here for exactly that
# reason. The image is the server target, so its CMD is already the entrypoint
# script and there is nothing to override on the command line.
docker-server: certs docker-stop
	@ echo "\n$(CYAN)==> Starting$(CLR_RMV) $(BLUE)$(DOCKER_SERVER)$(CLR_RMV) on port $(DOCKER_PORT)..."
	@ $(DOCKER) run -d $(DOCKER_INIT) --name $(DOCKER_SERVER) \
		$(DOCKER_ENV) -p $(DOCKER_PORT):$(DOCKER_PORT) \
		-v $(CURDIR)/$(CERT_DIR):/tetrish/certs:ro \
		-v $(DOCKER_STATE):/tetrish/tmp \
		$(DOCKER_SRV_REF) >/dev/null
	@ echo "$(GREEN)[Success] $(BLUE)$(DOCKER_SERVER)$(CLR_RMV) started ✔️"

docker-logs:
	@ $(DOCKER) logs -f $(DOCKER_SERVER)

docker-test:
	@ $(DOCKER) run --rm $(DOCKER_TTY) $(DOCKER_REF) make test

docker-shell:
	@ $(DOCKER) run --rm $(DOCKER_TTY) $(DOCKER_TERM) $(DOCKER_REF) bash

# Both names, because either can be holding the published port.
docker-stop:
	@ $(DOCKER) rm -f $(DOCKER_NAME) $(DOCKER_SERVER) >/dev/null 2>&1 || true

docker-clean:
	@ $(DOCKER) image rm -f $(DOCKER_REF) $(DOCKER_SRV_REF) >/dev/null 2>&1 || true
	@ echo "$(RED)Deleted $(BLUE)$(DOCKER_REF) $(DOCKER_SRV_REF)$(CLR_RMV) ✔️"

# Drops the players, wallets and leaderboard with the volume; the image stays.
docker-reset: docker-stop
	@ $(DOCKER) volume rm -f $(DOCKER_STATE) >/dev/null 2>&1 || true
	@ echo "$(RED)Deleted $(BLUE)$(DOCKER_STATE)$(CLR_RMV) ✔️"

# One command from a fresh clone to a playable client: scripts/play.sh installs
# what is missing, starts the engine, brings the server up and launches tetrisu
# against it. It is the only target that spans host and container, which is why
# it is a script and not a recipe.
#
# The server image, not the dev one: the client half of `play` is native on both
# platforms, so nothing it starts in a container ever draws a board.
#
# HOST= plays on somebody else's server instead of starting one here, verifying
# it against the committed demo CA:  make play HOST=10.27.229.33
# PLAY_ARGS= passes anything else through: make play PLAY_ARGS=--rebuild
PLAY_HOST_ARG	 = $(if $(HOST),--host $(HOST))

play:
	@ DOCKER_REF=$(DOCKER_SRV_REF) DOCKER_SERVER=$(DOCKER_SERVER) \
		bash ./scripts/play.sh $(PLAY_HOST_ARG) $(PLAY_ARGS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d clean >/dev/null 2>&1 || true; \
	done
	@ echo "$(RED)Cleaned $(BLUE)component objs$(CLR_RMV) ✔️"

fclean:
	@ for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@ $(RM) $(BIN)
	@ echo "$(RED)Deleted $(BLUE)component binaries$(CLR_RMV) ✔️"

# Like fclean but also wipes daemon runtime state: stops whatever is still
# running, delegates to the shell's own `reset` (drops its tmp/ and archive/),
# and clears the repo-level bin/tmp.
#
# The daemons are stopped first, and stopping them is what this target owes
# them: tetrisctl blocks until each has finished tearing down, so the wipe
# cannot delete tmp/ out from under a logger that is still writing into it.
# Reversing these lines is the self-inflicted wound tetrislogd's sink reclaim
# was written to survive - reclaim stays, because a log file can still be
# rotated or removed by hand, but it stops being a patch for this.
reset:
	@ if [ -x $(BIN)/tetrisctl ]; then \
		PATH=$(CURDIR)/$(BIN):$$PATH TETRISHRC=$(CURDIR)/.tetrishrc \
			$(BIN)/tetrisctl stop >/dev/null 2>&1 || true; \
	fi
	@ bash $(SHELL_DIR)/daemons_killer.sh >/dev/null 2>&1 || true
	@ $(MAKE) $(MAKE_FLAGS) -C $(SHELL_DIR) reset >/dev/null 2>&1 || true
	@ for d in $(LIB_DIRS) $(DAEMON_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@ $(RM) $(BIN) tmp
	@ echo "$(RED)Reset $(BLUE)project state$(CLR_RMV) ✔️"

re: fclean all

################################################################################
#                              PHONY && PRECIOUS                               #
################################################################################

.PHONY:		all deps install-deps check-deps deps-info libs shell daemons \
			bin-link run certs stack test docker-build \
			docker-build-server docker-run \
			docker-server docker-logs docker-test docker-shell \
			docker-stop docker-clean docker-reset play \
			clean fclean reset re
