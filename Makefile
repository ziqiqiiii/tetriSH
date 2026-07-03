# tetriSH umbrella Makefile
#
# Recurses into the self-contained libraries and the vendored shell, then
# provides a shell-driven `run` entry point. Daemons are launched from inside
# the shell via dspawn (see .tetrishrc), not from here.
#
# Targets:
#   make / make all   install missing dependencies, then build everything
#   make deps         check/install dependencies for this OS
#   make check-deps   verify dependencies without changing the system
#   make run          build, then launch the shell (sources .tetrishrc)
#   make stack        build, then launch the daemons headless (integration tests)
#   make clean        recurse `clean` into every component
#   make fclean       recurse `fclean` and drop ./bin
#   make re           fclean + all

SHELL_DIR  := src/tetrish
SHELL_BIN  := $(SHELL_DIR)/macmini_shell
BIN        := bin

UNAME_S           := $(shell uname -s)
AUTO_INSTALL_DEPS ?= 1
REQUIRE_VALGRIND  ?= 0

# Source-built dependencies usually install pkg-config metadata under
# /usr/local; keep that visible before falling back to distro/Homebrew paths.
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

# Homebrew keeps these libraries keg-only on some macOS releases. Export their
# pkg-config metadata so this Makefile and every recursive component build use
# the same headers and libraries on both Intel and Apple Silicon Macs.
ifeq ($(UNAME_S),Darwin)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifneq ($(BREW_PREFIX),)
export PKG_CONFIG_PATH := $(BREW_PREFIX)/opt/openssl@3/lib/pkgconfig:$(BREW_PREFIX)/opt/readline/lib/pkgconfig:$(BREW_PREFIX)/opt/ncurses/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif
endif

# Build only the components that exist yet — the project is in early dev, so
# the daemon directories are filled in over time. Match Makefiles rather than
# directories so ignored build artefacts cannot be mistaken for components.
LIB_MAKEFILES := $(wildcard lib/lib*/Makefile)
LIB_DIRS      := $(patsubst %/,%,$(dir $(LIB_MAKEFILES)))

COMPONENT_MAKEFILES := $(wildcard src/tetrisd/Makefile \
                                  src/tetrislogd/Makefile \
                                  src/tetrisctl/Makefile \
                                  src/tetrisu/Makefile \
                                  src/chatd/Makefile \
                                  src/chatctl/Makefile \
                                  src/marketd/Makefile \
                                  src/marketctl/Makefile)
DAEMON_DIRS := $(patsubst %/,%,$(dir $(COMPONENT_MAKEFILES)))

.PHONY: all deps install-deps check-deps deps-info libs shell daemons \
        bin-link run stack clean fclean reset re

all: deps libs shell daemons

# `make` installs only when the compile/link probe fails. Set
# AUTO_INSTALL_DEPS=0 in CI or managed environments to make this check-only.
deps:
	@ if $(MAKE) --no-print-directory check-deps >/dev/null 2>&1; then \
		echo "Dependencies ready ($(UNAME_S))."; \
	elif [ "$(AUTO_INSTALL_DEPS)" = "1" ]; then \
		$(MAKE) --no-print-directory install-deps; \
		$(MAKE) --no-print-directory check-deps; \
	else \
		$(MAKE) --no-print-directory check-deps; \
		echo "Missing dependencies; automatic installation is disabled."; \
		exit 1; \
	fi

# Root deps are shared by multiple components: compiler toolchain,
# pkg-config, OpenSSL, Readline, and ncurses. Component-only render/audio
# packages belong in that component's Makefile.
install-deps:
	@ set -eu; \
	case "$(UNAME_S)" in \
		Linux) \
			if grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease \
					/proc/version 2>/dev/null; then \
				echo "Installing tetriSH dependencies for WSL..."; \
			else \
				echo "Installing tetriSH dependencies for Linux..."; \
			fi; \
			if [ "$$(id -u)" -eq 0 ]; then \
				SUDO=""; \
			elif command -v sudo >/dev/null 2>&1; then \
				SUDO="sudo"; \
			else \
				echo "Root access or sudo is required to install packages."; \
				exit 1; \
			fi; \
			if command -v apt-get >/dev/null 2>&1; then \
				$$SUDO apt-get update; \
				$$SUDO apt-get install -y build-essential pkg-config \
					libssl-dev libreadline-dev libncurses-dev; \
				if apt-cache show valgrind >/dev/null 2>&1; then \
					$$SUDO apt-get install -y valgrind libc6-dbg || \
						echo "Warning: Valgrind install failed; build dependencies are installed."; \
				else \
					echo "Warning: Valgrind package not found; build can continue without it."; \
				fi; \
			elif command -v dnf >/dev/null 2>&1; then \
				$$SUDO dnf install -y gcc make binutils pkgconf-pkg-config \
					openssl-devel readline-devel ncurses-devel; \
				$$SUDO dnf install -y valgrind || \
					echo "Warning: Valgrind install failed; build dependencies are installed."; \
			elif command -v yum >/dev/null 2>&1; then \
				$$SUDO yum install -y gcc make binutils pkgconfig \
					openssl-devel readline-devel ncurses-devel; \
				$$SUDO yum install -y valgrind || \
					echo "Warning: Valgrind install failed; build dependencies are installed."; \
			elif command -v pacman >/dev/null 2>&1; then \
				$$SUDO pacman -S --needed --noconfirm base-devel pkgconf \
					openssl readline ncurses; \
				$$SUDO pacman -S --needed --noconfirm valgrind || \
					echo "Warning: Valgrind install failed; build dependencies are installed."; \
			elif command -v zypper >/dev/null 2>&1; then \
				$$SUDO zypper --non-interactive install gcc make binutils \
					pkg-config libopenssl-devel readline-devel \
					ncurses-devel; \
				$$SUDO zypper --non-interactive install valgrind || \
					echo "Warning: Valgrind install failed; build dependencies are installed."; \
			elif command -v apk >/dev/null 2>&1; then \
				$$SUDO apk add build-base pkgconf openssl-dev readline-dev \
					ncurses-dev; \
				$$SUDO apk add valgrind || \
					echo "Warning: Valgrind install failed; build dependencies are installed."; \
			else \
				echo "Unsupported Linux package manager."; \
				echo "Install GCC, make, binutils, pkg-config, OpenSSL,"; \
				echo "Readline, and ncurses development headers."; \
				exit 1; \
			fi \
			;; \
		Darwin) \
			echo "Installing tetriSH dependencies for macOS..."; \
			if ! xcrun --find cc >/dev/null 2>&1; then \
				echo "Starting the Xcode Command Line Tools installer..."; \
				xcode-select --install; \
				echo "Finish that installer, then run make again."; \
				exit 1; \
			fi; \
			if ! command -v brew >/dev/null 2>&1; then \
				echo "Homebrew is required: https://brew.sh"; \
				echo "Install it, then run make again."; \
				exit 1; \
			fi; \
			brew install pkgconf openssl@3 readline ncurses \
			;; \
		*) \
			echo "Unsupported operating system: $(UNAME_S)"; \
			exit 1 \
			;; \
	esac

# Compile and link one small program instead of trusting package database state.
# This catches missing headers, libraries, and unusable pkg-config search paths.
check-deps:
	@ set -eu; \
	probe="$$(mktemp /tmp/tetrish-deps-check.XXXXXX)"; \
	trap 'rm -f "$$probe"' EXIT HUP INT TERM; \
	missing=""; \
	for tool in gcc make ar pkg-config; do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			missing="$$missing $$tool"; \
		fi; \
	done; \
	if [ "$(UNAME_S)" = "Linux" ] \
			&& [ "$(REQUIRE_VALGRIND)" = "1" ] \
			&& ! command -v valgrind >/dev/null 2>&1; then \
		missing="$$missing valgrind"; \
	fi; \
	if [ -n "$$missing" ]; then \
		echo "Missing tools:$$missing"; \
		exit 1; \
	fi; \
	pkg-config --exists openssl readline ncursesw || { \
		echo "Missing development packages: OpenSSL, Readline, or ncurses."; \
		exit 1; \
	}; \
	printf '%s\n' \
		'#include <openssl/evp.h>' \
		'#include <readline/readline.h>' \
		'#include <ncurses.h>' \
		'int main(void) { return 0; }' \
		| gcc -x c - $$(pkg-config --cflags --libs \
			openssl readline ncursesw) -o "$$probe"
	@ if [ "$(UNAME_S)" = "Linux" ] \
			&& [ "$(REQUIRE_VALGRIND)" != "1" ] \
			&& ! command -v valgrind >/dev/null 2>&1; then \
		echo "Warning: Valgrind is not installed. Build is OK, but PR/checkoff memory-safety runs need it."; \
	fi

deps-info:
	@ echo "OS: $(UNAME_S)"
	@ if [ "$(UNAME_S)" = "Linux" ] \
			&& grep -Eqi '(microsoft|wsl)' /proc/sys/kernel/osrelease \
				/proc/version 2>/dev/null; then \
		echo "Environment: WSL"; \
	else \
		echo "Environment: native"; \
	fi
	@ echo "Auto-install: $(AUTO_INSTALL_DEPS)"
	@ echo "Required: GCC, make, binutils, pkg-config, OpenSSL, Readline, ncurses"
	@ if [ "$(UNAME_S)" = "Darwin" ]; then \
		echo "Valgrind: use Linux/WSL for the mandatory memory-safety run"; \
	elif [ "$(REQUIRE_VALGRIND)" = "1" ]; then \
		echo "Valgrind: required by REQUIRE_VALGRIND=1"; \
	else \
		echo "Valgrind: optional for build, required for PR/checkoff memory-safety runs"; \
	fi

libs: | deps
	@for d in $(LIB_DIRS); do $(MAKE) -C $$d || exit 1; done

shell: | deps
	@$(MAKE) -C $(SHELL_DIR) DEPS_READY=1

daemons: libs | deps
	@for d in $(DAEMON_DIRS); do \
		$(MAKE) -C $$d DEPS_READY=1 || exit 1; \
	done

# Collect every built binary into a single ./bin. The shell prepends $PWD/bin
# to PATH, so dspawn/dcheck and the daemons resolve by name from the prompt.
bin-link: shell daemons
	@mkdir -p $(BIN)
	@ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true

# Idiomatic launch: the shell sources .tetrishrc, which dspawns the daemons.
run: all bin-link
	@TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

# Headless stack for integration tests (no interactive shell). Launches only
# the daemons that have been built.
stack: all bin-link
	@for d in tetrislogd tetrisd marketd chatd; do \
		if [ -x $(BIN)/$$d ]; then $(BIN)/$$d & fi; \
	done; \
	echo "Started available daemons; inspect with 'dcheck' or tmp/daemons.reg"

clean:
	@for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d clean >/dev/null 2>&1 || true; \
	done

fclean:
	@for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@rm -rf $(BIN)

# Like fclean but also wipes daemon runtime state: delegates to the shell's
# own `reset` (drops its tmp/ and archive/) and clears the repo-level bin/tmp.
reset:
	@$(MAKE) -C $(SHELL_DIR) reset >/dev/null 2>&1 || true
	@for d in $(LIB_DIRS) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@rm -rf $(BIN) tmp

re: fclean all
