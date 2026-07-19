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
# launched from inside the shell via dspawn (see .tetrishrc), not from here.
#
#   make / make all   install missing dependencies, then build everything
#   make deps         check/install dependencies for this OS
#   make check-deps   verify dependencies without changing the system
#   make run          build, then launch the shell (sources .tetrishrc)
#   make stack        build, then launch the daemons headless (integration tests)
#   make test         build, then run every available component test suite
#   make clean        recurse `clean` into every component
#   make fclean       recurse `fclean` and drop ./bin
#   make re           fclean + all

MAKE_FLAGS	:= --no-print-directory
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

# Build only the components that exist yet — the project is in early dev, so
# the daemon directories are filled in over time. Match Makefiles rather than
# directories so ignored build artefacts cannot be mistaken for components.
LIB_MAKEFILES		:= $(wildcard lib/lib*/Makefile)
LIB_DIRS			:= $(patsubst %/,%,$(dir $(LIB_MAKEFILES)))

COMPONENT_MAKEFILES	:= $(wildcard src/tetrisd/Makefile \
							  src/tetrislogd/Makefile \
							  src/tetrisctl/Makefile \
							  src/tetrisu/Makefile \
							  src/chatd/Makefile \
							  src/chatctl/Makefile \
							  src/marketd/Makefile \
							  src/marketctl/Makefile)
DAEMON_DIRS			:= $(patsubst %/,%,$(dir $(COMPONENT_MAKEFILES)))
TEST_DIRS			:= $(LIB_DIRS) $(filter src/tetrisu,$(DAEMON_DIRS))

################################################################################
#                                   BUILD                                      #
################################################################################

all: deps libs shell daemons

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
# to PATH, so dspawn/dcheck and the daemons resolve by name from the prompt.
bin-link: shell daemons
	@ mkdir -p $(BIN)
	@ ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true

# Idiomatic launch: the shell sources .tetrishrc, which dspawns the daemons.
run: all bin-link
	@ TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

# Headless stack for integration tests (no interactive shell). Launches only
# the daemons that have been built.
stack: all bin-link
	@ for d in tetrislogd tetrisd marketd chatd; do \
		if [ -x $(BIN)/$$d ]; then $(BIN)/$$d & fi; \
	done; \
	echo "Started available daemons; inspect with 'dcheck' or tmp/daemons.reg"

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

# Like fclean but also wipes daemon runtime state: delegates to the shell's
# own `reset` (drops its tmp/ and archive/) and clears the repo-level bin/tmp.
reset:
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
			bin-link run stack test clean fclean reset re
