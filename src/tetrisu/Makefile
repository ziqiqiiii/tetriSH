#  _         _        _
# | |_  ___ | |_  _ _ (_) ___ _  _
# |  _|/ -_)|  _|| '_|| |(_-<| || |
#  \__|\___| \__||_|  |_|/__/ \_,_|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

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

AUTO_INSTALL_DEPS				?= 1
INSTALL_NOTCURSES_FROM_SOURCE	?= 1
NOTCURSES_VERSION				?= v3.0.17
NOTCURSES_PREFIX				?= /usr/local

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN		:= \033[1;36m

################################################################################
#                              PLATFORM SPECIFICS                              #
################################################################################

UNAME		:= $(shell uname -s)

# Source-built notcurses installs pkg-config metadata under /usr/local.
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

# Homebrew keeps each formula's pkg-config metadata under its own opt/ prefix.
ifeq ($(UNAME), Darwin)
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

# notcurses (render) is required; SDL2 + SDL2_mixer (audio) are optional and
# compiled out when absent. These are lazily expanded (=) so the recipes see
# post-install pkg-config metadata: a direct `make -C src/tetrisu` may install
# the packages before compiling the first object.
PKG_CONFIG		?= pkg-config
AUDIO_DEPS		:= sdl2 SDL2_mixer
NOTCURSES_CFLAGS	 = $(shell $(PKG_CONFIG) --cflags notcurses 2>/dev/null)
NOTCURSES_LIBS		 = $(shell $(PKG_CONFIG) --libs notcurses 2>/dev/null)
AUDIO_CFLAGS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --cflags $(AUDIO_DEPS); fi)
AUDIO_LIBS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --libs $(AUDIO_DEPS); fi)
AUDIO_DEFINE		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then printf '%s' -DTETRISU_ENABLE_AUDIO=1; else printf '%s' -DTETRISU_ENABLE_AUDIO=0; fi)

# Header search path + generated defines shared by every target.
INC			:= -I.
FLAGS		 = $(BASE_FLAGS) $(INC) $(NOTCURSES_CFLAGS) $(AUDIO_CFLAGS) $(AUDIO_DEFINE) -DASSET_DIR=\"$(ASSET_DIR)\" -DTETRISU_BIN_PATH=\"$(NAME_PATH)\"
LDLIBS		 = $(NOTCURSES_LIBS) $(AUDIO_LIBS)

################################################################################
#                                  SOURCES                                     #
################################################################################

# Binary : every .c under src/            -> bin/tetrisu
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Logic  : pure app-state, no notcurses/SDL -> logic.a (linked into unit tests).
# main.c and the render/audio sources pull in notcurses/SDL, so exclude them.
LOGIC_SRC	:= $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/render_%.c $(SRC_DIR)/audio.c, $(SRC))
LOGIC_OBJ	:= $(LOGIC_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
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

# --- object rule (header stays at the tetrisu root, reached via -I.) ---------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER) | check-dependencies
	@ mkdir -p $(dir $@)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) -c $< -o $@

# --- tetrisu binary ---------------------------------------------------------
$(NAME_PATH): $(OBJ)
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(OBJ) $(LDLIBS) -o $@
	@ echo
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) built ✔️"

run: $(NAME_PATH)
	@ ./$(NAME_PATH)

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# Component-only render/audio deps. Install is outsourced to scripts/; the
# check side is a compile/link probe kept in the Makefile.
deps:
	@ if $(MAKE) --no-print-directory check-tetrisu-deps >/dev/null 2>&1; then \
		echo "$(GREEN)tetrisu dependencies ready$(CLR_RMV) ($(UNAME))."; \
	elif [ "$(AUTO_INSTALL_DEPS)" = "1" ]; then \
		$(MAKE) --no-print-directory install-deps; \
		$(MAKE) --no-print-directory check-tetrisu-deps; \
	else \
		$(MAKE) --no-print-directory check-tetrisu-deps; \
		echo "tetrisu: missing dependencies; automatic installation is disabled."; \
		exit 1; \
	fi

install-deps:
	@ INSTALL_NOTCURSES_FROM_SOURCE=$(INSTALL_NOTCURSES_FROM_SOURCE) \
		NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) \
		bash ./scripts/install_deps.sh

install-notcurses-from-source:
	@ NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) \
		bash ./scripts/install_notcurses.sh

# Compile and link a tiny notcurses program instead of trusting package state;
# this catches missing headers, libraries, and unusable pkg-config paths.
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

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
TEST_RUNNER		:= ./scripts/run_tests.sh

UNIT_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
UNIT_BINS		:= $(UNIT_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

# --- logic archive (pure app-state, linked into every unit test) ------------
$(LOGIC_LIB): $(LOGIC_OBJ)
	@ ar rcs $@ $(LOGIC_OBJ)

# --- unit test binaries -----------------------------------------------------
$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(LOGIC_LIB) $(HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) -o $@
	@ echo "ok"

# No automated integration test: notcurses queries the terminal for
# capabilities (palette, pixel size, Kitty/Sixel support, ...) and blocks
# indefinitely waiting for replies. A real terminal answers these instantly;
# a scripted pty (forkpty) does not, and there is no notcurses option to
# skip the probe. Manual check via `make run` is the verification path.
test: $(UNIT_BINS)
	@ echo "\n$(CYAN)==> Running tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(UNIT_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) binary ✔️"

re: fclean all

################################################################################
#                              PHONY && PRECIOUS                               #
################################################################################

.PHONY:		all run deps install-deps install-notcurses-from-source \
			check-dependencies check-tetrisu-deps test clean fclean re

.PRECIOUS:	$(OBJ_DIR)/%.o
