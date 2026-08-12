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

MAKE_FLAGS	:= --no-print-directory -s
RM			:= rm -rf

AUTO_INSTALL_DEPS	:= 1
REQUIRE_VALGRIND	:= 0

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
DAEMON_DIRS			:= $(patsubst %/,%,$(dir $(COMPONENT_MAKEFILES)))
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

# --- load ------------------------------------------------------------------
# Deliberately outside `test`: it reports what the server cost rather than
# whether it was right, and takes as long as it is told to. STRESS_ARGS= passes
# the fleet's shape through, HOST= drives a server that is already running:
#   make stress STRESS_ARGS="--players 50 --seconds 30"
#   make stress HOST=10.27.229.33
STRESS_HOST_ENV	 = $(if $(HOST),HOST=$(HOST))

stress:
	@ $(STRESS_HOST_ENV) bash ./scripts/stress.sh $(STRESS_ARGS)

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# `make` installs only when the compile/link probe fails. Set
# AUTO_INSTALL_DEPS=0 in CI or managed environments to make this check-only.
# Outsourced to scripts/; it delegates to check_deps.sh / install_deps.sh.
deps:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
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
		bash ./scripts/check_deps.sh

# Read-only summary of the dependency situation. Outsourced to scripts/.
deps-info:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		bash ./scripts/deps_info.sh

################################################################################
#                                    PLAY                                      #
################################################################################

# Build the client and play. This is the target for the machine that is only ever
# a client, which on macOS is every machine: `make` cannot run here at all -
# libcoreipc's mqueue module fails to compile on Darwin before the recursion ever
# reaches tetrisu - so this recurses straight into src/tetrisu and touches
# nothing else. It deliberately does not depend on bin-link, which builds the
# shell and every daemon to populate ./bin and would fail for the same reason.
#
# Deliberately no TETRISU_HOST. The server's address changes - a hotspot hands
# out a new one - and a value baked in here would be a second place to remember
# to edit. The sign-in screen's SERVER ID field is the one place it is typed, and
# it wins over the environment anyway.
#
# TETRISU_CA_PATH is left alone too, because its default is already right:
# certs/demo-ca.crt is committed, so a fresh clone verifies the demo server with
# nothing configured. Only a server on this machine needs the override, and
# `make play-local` passes it.
#
# TETRISU_NET is the one thing that must be set. Without it the client builds its
# fixture provider instead of a session, and CHECK SERVER reports offline without
# opening a socket - the same screen a wrong address gives, for a reason no
# address can fix.
play:
	@ $(MAKE) $(MAKE_FLAGS) -C src/tetrisu
	@ echo "\n$(CYAN)==> Launching $(BLUE)tetrisu$(CLR_RMV) - type the server's address in $(YELLOW)SERVER ID$(CLR_RMV)"
	@ TETRISU_NET=1 ./src/tetrisu/bin/tetrisu

# The other half: bring a server up on this machine and play against it.
# scripts/play.sh installs what is missing, waits for the port and launches the
# client against it.
#
# HOST= plays on somebody else's server instead of starting one here, checking it
# is reachable first and verifying it against the committed demo CA:
#   make play-local HOST=10.27.229.33
# PLAY_ARGS= passes anything else through: make play-local PLAY_ARGS=--rebuild
PLAY_HOST_ARG	 = $(if $(HOST),--host $(HOST))

play-local:
	@ bash ./scripts/play.sh $(PLAY_HOST_ARG) $(PLAY_ARGS)

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
			bin-link run certs stack test play play-local \
			clean fclean reset re
