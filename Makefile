#  _         _        _ ___  _  _
# | |_  ___ | |_  _ _ (_) __|| || |
# |  _|/ -_)|  _|| '_|| \__ \| __ |
#  \__|\___| \__||_|  |_|___/|_||_|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

# tetriSH umbrella: recurses into the libraries, the shell and the components.
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
#   make reset        fclean + wipe runtime state, keeping the player store
#   make del-db       drop the player store (the one target that deletes it)
#   make re           fclean + all
#
#   make play         install + compile on this host, then play; when the
#                     terminal already draws the board the game runs there
#                     instead of in a new window
#   make play-init    the same, with the terminal step skipped: the client
#                     runs in the terminal make was invoked in
#   make play-image   the same, built in a container instead of on this host
#
#   make play PLAY_ARGS=--local           play against a server on this machine
#   make play HOST=tetrish.dev            HOST= plays on another server
#   make play-image PLAY_ARGS=--rebuild   PLAY_ARGS= passes anything else through

MAKE_FLAGS	:= --no-print-directory -s
RM			:= rm -rf

AUTO_INSTALL_DEPS	:= 1
REQUIRE_VALGRIND	:= 0
REQUIRE_DOCKER		:= 0
REQUIRE_KITTY		:= 0

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

export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

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
TETRISHRC	:= .tetrishrc

DB_DIR_RC	:= $(shell sed -n 's/^export TETRISD_DATA_DIR=//p' $(TETRISHRC) 2>/dev/null | tail -n 1)
DB_DIR		:= $(if $(DB_DIR_RC),$(DB_DIR_RC),tmp/tetrisd)
DB_LOG		:= $(DB_DIR)/players.log

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

# One ./bin for every binary: the shell prepends it to PATH and tetrisctl
# resolves daemons through PATH, so one missing here cannot be launched at all.
# Each is named after its directory, in one of two layouts - daemons build
# beside their Makefile, tetrisu into bin/ - so both are searched.
bin-link: shell daemons
	@ mkdir -p $(BIN)
	@ ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true
	@ for d in $(DAEMON_DIRS); do \
		n=`basename $$d`; \
		for p in $$d/$$n $$d/$(BIN)/$$n; do \
			if [ -x $$p ]; then ln -sf $(CURDIR)/$$p $(BIN)/; break; fi; \
		done; \
	done

run: all bin-link certs
	@ TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

certs:
	@ bash ./scripts/generate_certs.sh $(CERT_DIR)

# Headless stack for integration tests; roster and order come from .tetrishrc.
# Each daemon detaches, so this returns only once they are up - non-zero if not.
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
# Outside `test`: it reports what the server cost, not whether it was right, and
# runs as long as it is told to. STRESS_ARGS= shapes the fleet, HOST= drives a
# server already running:
#   make stress STRESS_ARGS="--players 50 --seconds 30"
#   make stress HOST=10.27.229.33
STRESS_HOST_ENV	 = $(if $(HOST),HOST=$(HOST))

stress:
	@ $(STRESS_HOST_ENV) bash ./scripts/stress.sh $(STRESS_ARGS)

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# Installs only when the compile/link probe fails; AUTO_INSTALL_DEPS=0 makes it
# check-only (CI). The work is in scripts/check_deps.sh + install_deps.sh.
deps:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) REQUIRE_KITTY=$(REQUIRE_KITTY) \
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
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) REQUIRE_KITTY=$(REQUIRE_KITTY) \
		bash ./scripts/check_deps.sh

# Read-only summary of the dependency situation.
deps-info:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		REQUIRE_DOCKER=$(REQUIRE_DOCKER) REQUIRE_KITTY=$(REQUIRE_KITTY) \
		bash ./scripts/deps_info.sh

################################################################################
#                                    PLAY                                      #
################################################################################

# HOST= plays on another server, PLAY_ARGS= passes anything else through:
#   make play PLAY_ARGS=--local           play against a server on this machine
#   make play HOST=tetrish.dev
#   make play-image PLAY_ARGS=--rebuild
PLAY_HOST_ARG	 = $(if $(HOST),--host $(HOST))

play:
	@ bash ./scripts/play.sh --native $(PLAY_HOST_ARG) $(PLAY_ARGS)

# Never opens a window: TETRISU_TERMINAL is forced to none, so the terminal
# step is skipped and the client runs in the terminal make was invoked in.
# Everything else - dependencies, build, server handling - is the same play.sh
# path; forcing the variable here (rather than trusting the environment) is
# what overrides a TETRISU_TERMINAL the user may have set.
play-init:
	@ TETRISU_TERMINAL=none bash ./scripts/play.sh --native $(PLAY_HOST_ARG) $(PLAY_ARGS)

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

daemons-stop:
	@ if [ -x $(BIN)/tetrisctl ]; then \
		PATH=$(CURDIR)/$(BIN):$$PATH TETRISHRC=$(CURDIR)/$(TETRISHRC) \
			$(BIN)/tetrisctl stop >/dev/null 2>&1 || true; \
	fi
	@ bash $(SHELL_DIR)/daemons_killer.sh >/dev/null 2>&1 || true

# fclean + kill deamons
reset: daemons-stop
	@ $(MAKE) $(MAKE_FLAGS) -C $(SHELL_DIR) reset >/dev/null 2>&1 || true
	@ for d in $(LIB_DIRS) $(DAEMON_DIRS); do \
		$(MAKE) $(MAKE_FLAGS) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@ set -e; \
	stash=""; \
	if [ -f $(DB_LOG) ]; then stash=`mktemp`; cp -p $(DB_LOG) $$stash; fi; \
	$(RM) $(BIN) tmp; \
	if [ -n "$$stash" ]; then \
		mkdir -p $(DB_DIR); cp -p $$stash $(DB_LOG); $(RM) $$stash; \
		echo "$(GREEN)Kept $(BLUE)$(DB_LOG)$(CLR_RMV) ✔️"; \
	fi
	@ echo "$(RED)Reset $(BLUE)project state$(CLR_RMV) ✔️"

# delete db data
del-db: daemons-stop
	@ $(RM) $(DB_LOG)
	@ echo "$(RED)Deleted $(BLUE)$(DB_LOG)$(CLR_RMV) ✔️"

re: fclean all

################################################################################
#                              PHONY && PRECIOUS                               #
################################################################################

.PHONY:		all deps install-deps check-deps deps-info libs shell daemons \
			bin-link run certs stack test play play-init play-local play-image \
			clean fclean daemons-stop reset del-db re
