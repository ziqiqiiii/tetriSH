#  _         _        _ ___  _  _
# | |_  ___ | |_  _ _ (_) __|| || |
# |  _|/ -_)|  _|| '_|| \__ \| __ |
#  \__|\___| \__||_|  |_|___/|_||_|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

# tetriSH umbrella Makefile: recurses into the libraries and the vendored shell.
# Daemons are launched by tetrisctl from inside the shell (.tetrishrc), not here.
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
#   make play         install + compile on this host, then play in kitty
#   make play-image   the same, built in a container instead of on this host

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

# Source-built dependencies land their pkg-config metadata under /usr/local;
# keep it visible before the distro/Homebrew paths.
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

# Homebrew keeps these keg-only on some macOS releases; exporting their
# metadata keeps every recursive build on the same headers and libraries.
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

# Build only the components that exist yet. Match Makefiles rather than
# directories, so ignored build artefacts are not mistaken for components.
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

# Collect every built binary into one ./bin: the shell prepends it to PATH and
# tetrisctl resolves daemons through PATH, so one missing here cannot be
# launched by name at all. Each binary is named after its directory, and both
# layouts are searched - daemons build beside their Makefile, tetrisu into bin/.
bin-link: shell daemons
	@ mkdir -p $(BIN)
	@ ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true
	@ for d in $(DAEMON_DIRS); do \
		n=`basename $$d`; \
		for p in $$d/$$n $$d/$(BIN)/$$n; do \
			if [ -x $$p ]; then ln -sf $(CURDIR)/$$p $(BIN)/; break; fi; \
		done; \
	done

# The shell sources .tetrishrc, whose last line is `tetrisctl start` - so the
# daemons come up before the first prompt. certs is a prerequisite because that
# boots tetrisd, which treats missing certificates as fatal, and certs/ is
# git-ignored: on a fresh clone it decides whether there is a server at all.
run: all bin-link certs
	@ TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

# Development credentials for the secure session, which tetrisd refuses to boot
# without. The script is a no-op while the certificate is still valid.
certs:
	@ bash ./scripts/generate_certs.sh $(CERT_DIR)

# Headless stack for integration tests. tetrisctl reads the roster and its order
# from .tetrishrc; each daemon detaches, so this returns only once they are up -
# and non-zero if one is not.
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

# Installs only when the compile/link probe fails; AUTO_INSTALL_DEPS=0 makes it
# check-only (CI). Outsourced to scripts/check_deps.sh + install_deps.sh.
deps:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)' \
		bash ./scripts/deps.sh

# Root deps are the ones several components share: toolchain, pkg-config,
# OpenSSL, readline, ncurses. A component's own packages belong in its Makefile.
install-deps:
	@ bash ./scripts/install_deps.sh

# Verify without changing the system: the script compiles and links a probe
# rather than trusting the package database.
check-deps:
	@ UNAME_S=$(UNAME_S) REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		bash ./scripts/check_deps.sh

# Read-only summary of the dependency situation.
deps-info:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) \
		bash ./scripts/deps_info.sh

################################################################################
#                                    PLAY                                      #
################################################################################

# Two routes from a fresh clone to a playable client, differing only in where it
# is built. Both install what is missing, compile, and open kitty on the game;
# play.sh checks before every step, so re-running either restarts the client.
#
#   play        built on this host
#   play-image  built in a container carrying the toolchain and notcurses, so
#               this host installs none of it
#
# The container never draws: the board is Kitty-graphics escape sequences, bytes
# on the pty the host terminal renders either way. That is what lets one Linux
# image serve macOS, where `make` cannot run at all - libcoreipc's mqueue module
# does not compile on Darwin, and the recursion stops long before tetrisu.
#
# Both play on the shared tetriSH server by default - the address is in
# scripts/play.sh as DEFAULT_HOST, named once there rather than here, because
# the script is what has to pair it with the matching CA. Neither target starts
# a server now; `bash scripts/play.sh --local` is the one that does.
#
# HOST= plays on another server:           make play HOST=tetrish.dev
# PLAY_ARGS= passes anything else through: make play-image PLAY_ARGS=--rebuild
PLAY_HOST_ARG	 = $(if $(HOST),--host $(HOST))

play:
	@ bash ./scripts/play.sh --native $(PLAY_HOST_ARG) $(PLAY_ARGS)

PLAY_REBUILD_ARG	 = $(if $(REBUILD),--rebuild)

play-image:
	@ bash ./scripts/play.sh --container $(PLAY_HOST_ARG) $(PLAY_REBUILD_ARG) $(PLAY_ARGS)

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

# fclean plus daemon runtime state: stops what is running, delegates to the
# shell's own `reset` (its tmp/ and archive/), then clears bin/ and tmp/.
#
# Stopping comes first because tetrisctl blocks until each daemon has torn down,
# so the wipe cannot delete tmp/ out from under a logger still writing into it.
# Reversing these lines is the wound tetrislogd's sink reclaim was written to
# survive - reclaim stays for hand-rotated logs, but is not a patch for this.
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
			bin-link run certs stack test play play-local play-image \
			clean fclean reset re
