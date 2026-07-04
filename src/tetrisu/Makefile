#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Self-contained binary (same self-contained pattern as the libraries under
# lib/, applied to a binary instead of an archive). Until the umbrella
# Makefile lands, build/test this directly:
#     make -C src/tetrisu
#     make -C src/tetrisu test
#     make -C src/tetrisu run

NAME		:= tetrisu
CC			:= gcc
BASE_FLAGS	:= -Wall -Wextra -Werror
RM			:= rm -rf
UNAME_S		:= $(shell uname -s)
AUTO_INSTALL_DEPS ?= 1
INSTALL_NOTCURSES_FROM_SOURCE ?= 1
NOTCURSES_VERSION ?= v3.0.17
NOTCURSES_PREFIX  ?= /usr/local

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
CYAN		:= \033[1;36m
BLUE		:= \033[1;34m

# Source-built notcurses installs pkg-config metadata under /usr/local.
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

ifeq ($(UNAME_S),Darwin)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifneq ($(BREW_PREFIX),)
export PKG_CONFIG_PATH := $(BREW_PREFIX)/opt/notcurses/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2_mixer/lib/pkgconfig:$(BREW_PREFIX)/opt/ncurses/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif
endif

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SRC_DIR		:= src
OBJ_DIR		:= obj
BIN_DIR		:= bin
NAME_PATH	:= $(BIN_DIR)/$(NAME)

HEADER		:= tetrisu.h
ASSET_DIR	:= $(CURDIR)/assets

PKG_CONFIG		?= pkg-config
NOTCURSES_CFLAGS	= $(shell $(PKG_CONFIG) --cflags notcurses 2>/dev/null)
NOTCURSES_LIBS		= $(shell $(PKG_CONFIG) --libs notcurses 2>/dev/null)
AUDIO_DEPS		:= sdl2 SDL2_mixer
AUDIO_CFLAGS	= $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --cflags $(AUDIO_DEPS); fi)
AUDIO_LIBS		= $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --libs $(AUDIO_DEPS); fi)
AUDIO_DEFINE	= $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then printf '%s' -DTETRISU_ENABLE_AUDIO=1; else printf '%s' -DTETRISU_ENABLE_AUDIO=0; fi)

# Keep pkg-config flags lazy: direct `make -C src/tetrisu` may install packages
# before compiling the first object, so recipes must see post-install metadata.
FLAGS		= $(BASE_FLAGS) -I. $(NOTCURSES_CFLAGS) $(AUDIO_CFLAGS) $(AUDIO_DEFINE) -DASSET_DIR=\"$(ASSET_DIR)\" -DTETRISU_BIN_PATH=\"$(NAME_PATH)\"
LDLIBS		= $(NOTCURSES_LIBS) $(AUDIO_LIBS)

################################################################################
#                                  SOURCES                                     #
################################################################################

LOGIC_SRC	:= app_state.c
MENU_SRC	:= render_background.c render_menu.c render_intro.c audio.c
SRC			:= main.c $(LOGIC_SRC) $(MENU_SRC)
OBJ			:= $(SRC:%.c=$(OBJ_DIR)/%.o)

LOGIC_OBJ	:= $(LOGIC_SRC:%.c=$(OBJ_DIR)/%.o)
LOGIC_LIB	:= $(OBJ_DIR)/logic.a

################################################################################
#                                   BUILD                                      #
################################################################################

all: check-dependencies $(NAME_PATH)

# A standalone tetrisu build can ask root for shared deps, then this Makefile
# owns render/audio deps. Root passes DEPS_READY=1 to avoid recursion.
check-dependencies:
	@ if [ "$(DEPS_READY)" != "1" ]; then \
		$(MAKE) --no-print-directory -C ../.. deps; \
	fi
	@ $(MAKE) --no-print-directory deps

deps:
	@ if $(MAKE) --no-print-directory check-tetrisu-deps >/dev/null 2>&1; then \
		echo "tetrisu dependencies ready ($(UNAME_S))."; \
	elif [ "$(AUTO_INSTALL_DEPS)" = "1" ]; then \
		$(MAKE) --no-print-directory install-deps; \
		$(MAKE) --no-print-directory check-tetrisu-deps; \
	else \
		$(MAKE) --no-print-directory check-tetrisu-deps; \
		echo "tetrisu: missing dependencies; automatic installation is disabled."; \
		exit 1; \
	fi

check-tetrisu-deps:
	@ set -eu; \
	probe="$$(mktemp /tmp/tetrisu-deps-check.XXXXXX)"; \
	trap 'rm -f "$$probe"' EXIT HUP INT TERM; \
	missing=""; \
	for tool in $(CC) $(PKG_CONFIG); do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			missing="$$missing $$tool"; \
		fi; \
	done; \
	if [ -n "$$missing" ]; then \
		echo "tetrisu: missing tools:$$missing"; \
		exit 1; \
	fi; \
	if ! $(PKG_CONFIG) --exists notcurses; then \
		echo "tetrisu: missing notcurses development files."; \
		echo "Run 'make -C src/tetrisu deps' or install notcurses for your distro/Homebrew."; \
		exit 1; \
	fi; \
	printf '%s\n' \
		'#include <notcurses/notcurses.h>' \
		'int main(void) { return 0; }' \
		| $(CC) -x c - $$($(PKG_CONFIG) --cflags --libs notcurses) -o "$$probe"; \
	if ! $(PKG_CONFIG) --exists $(AUDIO_DEPS); then \
		echo "Warning: tetrisu audio disabled; install SDL2 and SDL2_mixer development packages for intro audio."; \
	fi

install-deps:
	@ set -eu; \
	case "$(UNAME_S)" in \
		Linux) \
			echo "Installing tetrisu render/audio dependencies for Linux..."; \
			if [ "$$(id -u)" -eq 0 ]; then \
				SUDO=""; \
			elif command -v sudo >/dev/null 2>&1; then \
				SUDO="sudo"; \
			else \
				echo "Root access or sudo is required to install tetrisu packages."; \
				exit 1; \
			fi; \
			if command -v apt-get >/dev/null 2>&1; then \
				$$SUDO apt-get update; \
				$$SUDO apt-get install -y pkg-config libsdl2-dev libsdl2-mixer-dev; \
				NOTCURSES_PKG=""; \
				for pkg in libnotcurses-dev notcurses-dev; do \
					if apt-cache show $$pkg >/dev/null 2>&1; then \
						NOTCURSES_PKG="$$pkg"; \
						break; \
					fi; \
				done; \
				if [ -z "$$NOTCURSES_PKG" ] && [ -r /etc/os-release ]; then \
					. /etc/os-release; \
					if [ "$${ID:-}" = "ubuntu" ]; then \
						echo "Ubuntu keeps notcurses in universe; enabling universe if needed..."; \
						if ! command -v add-apt-repository >/dev/null 2>&1; then \
							$$SUDO apt-get install -y software-properties-common; \
						fi; \
						$$SUDO add-apt-repository -y universe || true; \
						$$SUDO apt-get update; \
						for pkg in libnotcurses-dev notcurses-dev; do \
							if apt-cache show $$pkg >/dev/null 2>&1; then \
								NOTCURSES_PKG="$$pkg"; \
								break; \
							fi; \
						done; \
					fi; \
				fi; \
				if [ -n "$$NOTCURSES_PKG" ]; then \
					$$SUDO apt-get install -y $$NOTCURSES_PKG; \
				elif [ "$(INSTALL_NOTCURSES_FROM_SOURCE)" = "1" ]; then \
					echo "No APT notcurses development package found; building notcurses $(NOTCURSES_VERSION) from source."; \
					$$SUDO apt-get install -y git cmake libavdevice-dev \
						libdeflate-dev libgpm-dev libswscale-dev libunistring-dev; \
					$(MAKE) --no-print-directory install-notcurses-from-source; \
				else \
					echo "No notcurses development package is available in configured APT repositories."; \
					echo "Enable Ubuntu universe/Debian testing, install notcurses manually, or run with INSTALL_NOTCURSES_FROM_SOURCE=1."; \
					exit 1; \
				fi; \
			elif command -v dnf >/dev/null 2>&1; then \
				$$SUDO dnf install -y pkgconf-pkg-config notcurses-devel \
					SDL2-devel SDL2_mixer-devel || { \
					echo "Enable Fedora/EPEL/CRB repos or install tetrisu render/audio packages manually."; \
					exit 1; \
				}; \
			elif command -v yum >/dev/null 2>&1; then \
				$$SUDO yum install -y pkgconfig notcurses-devel \
					SDL2-devel SDL2_mixer-devel || { \
					echo "Enable EPEL/CRB repos or install tetrisu render/audio packages manually."; \
					exit 1; \
				}; \
			elif command -v pacman >/dev/null 2>&1; then \
				$$SUDO pacman -S --needed --noconfirm pkgconf notcurses \
					sdl2 sdl2_mixer; \
			elif command -v zypper >/dev/null 2>&1; then \
				$$SUDO zypper --non-interactive install pkg-config \
					notcurses-devel libSDL2-devel libSDL2_mixer-devel || { \
					echo "Enable needed openSUSE repos or install tetrisu render/audio packages manually."; \
					exit 1; \
				}; \
			elif command -v apk >/dev/null 2>&1; then \
				$$SUDO apk add pkgconf notcurses-dev sdl2-dev sdl2_mixer-dev; \
			else \
				echo "Unsupported Linux package manager."; \
				echo "Install pkg-config, notcurses, SDL2, and SDL2_mixer development headers."; \
				exit 1; \
			fi \
			;; \
		Darwin) \
			echo "Installing tetrisu render/audio dependencies for macOS..."; \
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
			brew install pkgconf notcurses sdl2 sdl2_mixer \
			;; \
		*) \
			echo "Unsupported operating system: $(UNAME_S)"; \
			exit 1 \
			;; \
	esac

install-notcurses-from-source:
	@ set -eu; \
	if [ "$(UNAME_S)" != "Linux" ]; then \
		echo "Source fallback is only needed on Linux; use Homebrew on macOS."; \
		exit 1; \
	fi; \
	for tool in git cmake; do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			echo "Missing $$tool; install it before building notcurses from source."; \
			exit 1; \
		fi; \
	done; \
	if [ "$$(id -u)" -eq 0 ]; then \
		SUDO=""; \
	elif command -v sudo >/dev/null 2>&1; then \
		SUDO="sudo"; \
	else \
		echo "Root access or sudo is required to install source-built notcurses."; \
		exit 1; \
	fi; \
	tmp="$$(mktemp -d /tmp/notcurses-src.XXXXXX)"; \
	trap 'rm -rf "$$tmp"' EXIT HUP INT TERM; \
	jobs="$$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 2)"; \
	git clone --depth 1 --branch "$(NOTCURSES_VERSION)" \
		https://github.com/dankamongmen/notcurses.git "$$tmp/notcurses"; \
	cmake -S "$$tmp/notcurses" -B "$$tmp/build" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$(NOTCURSES_PREFIX)" \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DUSE_MULTIMEDIA=ffmpeg \
		-DUSE_DOCTEST=off \
		-DUSE_PANDOC=off \
		-DUSE_QRCODEGEN=off \
		-DBUILD_TESTING=off; \
	cmake --build "$$tmp/build" --parallel "$$jobs"; \
	$$SUDO cmake --install "$$tmp/build"; \
	if command -v ldconfig >/dev/null 2>&1; then \
		$$SUDO ldconfig; \
	fi

$(OBJ_DIR)/%.o: %.c $(HEADER) | check-dependencies
	@ mkdir -p $(OBJ_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) -c $< -o $@
	@ echo "ok"

$(NAME_PATH): $(OBJ)
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(OBJ) $(LDLIBS) -o $@
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) built"

run: $(NAME_PATH)
	@ ./$(NAME_PATH)

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
TEST_RUNNER		:= ./scripts/run_tests.sh

$(LOGIC_LIB): $(LOGIC_OBJ)
	@ ar rcs $@ $(LOGIC_OBJ)

UNIT_SRC		:= $(TESTS_DIR)/test_app_state.c
UNIT_BINS		:= $(UNIT_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(LOGIC_LIB) $(HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) -o $@
	@ echo "ok"

# No automated integration test: notcurses queries the terminal for
# capabilities (palette, pixel size, Kitty/Sixel support, ...) and blocks
# indefinitely waiting for replies. A real terminal answers these instantly;
# a scripted pty (forkpty) does not, and there is no notcurses option to
# skip the probe. Verified manually that it hangs regardless of TERM value.
# Manual check via `make run` is the verification path for this binary.

test: $(UNIT_BINS)
	@ echo "\n$(CYAN)==> Running tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(UNIT_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs"

fclean: clean
	@ $(RM) $(BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) binary"

re: fclean all

.PHONY:		all deps install-deps check-dependencies check-tetrisu-deps \
			install-notcurses-from-source run test clean fclean re
