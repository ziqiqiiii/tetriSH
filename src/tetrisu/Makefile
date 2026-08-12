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
BASE_FLAGS	:= -std=c11 -Wall -Wextra -Werror -pedantic -D_POSIX_C_SOURCE=200809L
RM			:= rm -rf

AUTO_INSTALL_DEPS				:= 1
INSTALL_NOTCURSES_FROM_SOURCE	:= 1
NOTCURSES_VERSION				:= v3.0.12
NOTCURSES_MIN_VERSION			:= 3.0.5
NOTCURSES_PREFIX				:= /usr/local

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
NET_HEADER	:= tetrisu_net.h
ASSET_DIR	:= $(CURDIR)/assets

# One archive per CoreStack library, each with the include path that goes with
# it. libtetrisbrain is the local prediction and the shared piece/board types;
# the other three are the session tetrisu speaks to tetrisd over — the
# handshake and framing, the HTTTP messages, and the bodies inside them.
LIB_ROOT	:= ../../lib
LIB_NAMES	:= libtetrisbrain libtetrissh libhtttp libstatusbody
LIB_DIRS	:= $(LIB_NAMES:%=$(LIB_ROOT)/%)
LIB_ARCS	:= $(foreach l,$(LIB_NAMES),$(LIB_ROOT)/$(l)/$(l).a)
LIB_INC		:= $(LIB_DIRS:%=-I%/include)

BRAIN_DIR	:= $(LIB_ROOT)/libtetrisbrain
BRAIN_LIB	:= $(BRAIN_DIR)/libtetrisbrain.a
BRAIN_INC	:= -I$(BRAIN_DIR)/include
BRAIN_HEADER	:= $(BRAIN_DIR)/include/tetrisbrain.h

# notcurses (render) is required; SDL2 + SDL2_mixer (audio) are optional and
# compiled out when absent. These are lazily expanded (=) so the recipes see
# post-install pkg-config metadata: a direct `make -C src/tetrisu` may install
# the packages before compiling the first object.
PKG_CONFIG		:= pkg-config
AUDIO_DEPS		:= sdl2 SDL2_mixer
NOTCURSES_CFLAGS	 = $(shell $(PKG_CONFIG) --cflags notcurses 2>/dev/null)
NOTCURSES_LIBS		 = $(shell $(PKG_CONFIG) --libs notcurses 2>/dev/null)
AUDIO_CFLAGS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --cflags $(AUDIO_DEPS); fi)
AUDIO_LIBS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --libs $(AUDIO_DEPS); fi)
AUDIO_DEFINE		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then printf '%s' -DTETRISU_ENABLE_AUDIO=1; else printf '%s' -DTETRISU_ENABLE_AUDIO=0; fi)

# Header search path + generated defines shared by every target.
# DEPFLAGS makes each object record the headers it actually included, so a
# CoreStack header that changes the shape of a struct rebuilds every object
# holding one instead of leaving stale ones to be linked against fresh code.
INC			:= -I. $(LIB_INC)
DEPFLAGS	:= -MMD -MP
FLAGS		 = $(BASE_FLAGS) $(INC) $(NOTCURSES_CFLAGS) $(AUDIO_CFLAGS) $(AUDIO_DEFINE) -DASSET_DIR=\"$(ASSET_DIR)\" -DTETRISU_BIN_PATH=\"$(NAME_PATH)\"
LDLIBS		 = $(NOTCURSES_LIBS) $(AUDIO_LIBS) -lssl -lcrypto -lpthread

################################################################################
#                                  SOURCES                                     #
################################################################################

# Binary : every .c under src/            -> bin/tetrisu
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEP			:= $(OBJ:.o=.d)

# Logic  : pure app-state, no notcurses/SDL -> logic.a (linked into unit tests).
# main.c, solo_mode.c, and render/audio sources pull in notcurses/SDL, so the
# unit-test archive keeps only state and gameplay logic.
LOGIC_SRC	:= $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/solo_mode.c \
				   $(SRC_DIR)/multiplayer_match_mode.c \
				   $(SRC_DIR)/render_%.c $(SRC_DIR)/audio.c, $(SRC))
LOGIC_OBJ	:= $(LOGIC_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LOGIC_LIB	:= $(OBJ_DIR)/logic.a

################################################################################
#                                   BUILD                                      #
################################################################################

all: check-dependencies libs $(NAME_PATH)

# A standalone tetrisu build can ask root for shared deps, then this Makefile
# owns render/audio deps. Root passes DEPS_READY=1 to avoid recursion.
check-dependencies:
	@ if [ "$(DEPS_READY)" != "1" ]; then \
		$(MAKE) --no-print-directory -s -C ../.. deps; \
	fi
	@ $(MAKE) --no-print-directory -s deps

# --- object rule (header stays at the tetrisu root, reached via -I.) ---------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | check-dependencies libs
	@ mkdir -p $(dir $@)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(DEPFLAGS) -c $< -o $@

# Absent on a first build, when everything compiles anyway.
-include $(DEP)

# --- tetrisu binary ---------------------------------------------------------
$(NAME_PATH): $(OBJ) $(LIB_ARCS)
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(OBJ) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) built ✔️"

run: $(NAME_PATH)
	@ ./$(NAME_PATH)

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# Component-only render/audio dependency orchestration lives in scripts/ so
# this Makefile remains declarative and each shell failure has one owner.
deps:
	@ UNAME_S=$(UNAME) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		INSTALL_NOTCURSES_FROM_SOURCE=$(INSTALL_NOTCURSES_FROM_SOURCE) \
		NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_MIN_VERSION=$(NOTCURSES_MIN_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) CC=$(CC) \
		PKG_CONFIG=$(PKG_CONFIG) GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)' \
		bash ./scripts/deps.sh

install-deps:
	@ INSTALL_NOTCURSES_FROM_SOURCE=$(INSTALL_NOTCURSES_FROM_SOURCE) \
		NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_MIN_VERSION=$(NOTCURSES_MIN_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) \
		bash ./scripts/install_deps.sh

install-notcurses-from-source:
	@ NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) \
		bash ./scripts/install_notcurses.sh

# Every CoreStack archive tetrisu links, built in place by its own Makefile.
# Cleanup never recurses into lib/ — the archives are shared with the daemons
# and owned by their own directories.
libs:
	@ for d in $(LIB_DIRS); do $(MAKE) --no-print-directory -s -C $$d; done

$(LIB_ARCS): libs
	@ :

brain: libs

# Compile/link probing is a shell responsibility; the script checks the actual
# headers and libraries rather than trusting package-manager state.
check-tetrisu-deps:
	@ CC=$(CC) PKG_CONFIG=$(PKG_CONFIG) AUDIO_DEPS='$(AUDIO_DEPS)' \
		NOTCURSES_MIN_VERSION=$(NOTCURSES_MIN_VERSION) \
		bash ./scripts/check_deps.sh

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
TEST_RUNNER		:= ./scripts/run_tests.sh

UNIT_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
UNIT_BINS		:= $(UNIT_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)
INTEGRATION_SRC	:= $(wildcard $(TESTS_DIR)/integration/*.sh)

# --- logic archive (pure app-state, linked into every unit test) ------------
$(LOGIC_LIB): $(LOGIC_OBJ)
	@ ar rcs $@ $(LOGIC_OBJ)

# --- unit test binaries -----------------------------------------------------
$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# --- network smoke binary ---------------------------------------------------
# Not a unit test: it needs a tetrisd, so the integration script starts one and
# runs this against it. It links the whole app archive because the network
# layer writes into the same Solo view model the renderer reads.
$(TEST_BIN_DIR)/net_smoke: $(TESTS_DIR)/net_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The network app data provider driven against a live tetrisd: the acceptance
# test for the net-mode wiring (CHECK SERVER, SIGN UP, LOGIN, profile, lobby,
# create/join room). Same link line as net_smoke - it uses the provider vtable
# which lives in the app archive alongside the network layer.
$(TEST_BIN_DIR)/net_provider_smoke: $(TESTS_DIR)/net_provider_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The Marketplace and Settings against a live tetrisd: prices, wallet,
# ownership and the loadout, all of which moved out of the client's view model
# and into the account. Same link line - the store calls live in the app
# archive beside the rest of the network layer.
$(TEST_BIN_DIR)/store_smoke: $(TESTS_DIR)/store_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The layer solo_mode.c calls, driven against a live tetrisd: who owns the
# board, and the countdown hold that reconciles the client's 3-2-1 with the
# server's gravity. Same link line - solo_authority.c is in the app archive.
$(TEST_BIN_DIR)/solo_authority_smoke: $(TESTS_DIR)/solo_authority_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The room feed against a live tetrisd: narration the server wrote, chat a
# player sent, and the fact that both reach the ring whether they arrive on
# their own or across a reply. Same link line - net_chat.c is in the app
# archive beside the rest of the network layer.
$(TEST_BIN_DIR)/chat_smoke: $(TESTS_DIR)/chat_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# Two clients in one process, on two sockets. That is what lets one program
# assert on both halves of the same instant - that the board one player steers
# is the board the other is shown, in the same frame. Two processes could only
# agree by talking to each other, which is the thing under test.
$(TEST_BIN_DIR)/match_smoke: $(TESTS_DIR)/match_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# Four clients in one Battle Royale. Separate from match_smoke because what it
# proves is a different channel: Double's rival rides the opponents section at
# full fidelity, and ninety-eight rivals cannot ride it at all - they ride an
# arena of thumbnails on a clock of its own, and the things that have to be
# true of that are about the roster, the filing and the cadence rather than
# about one board.
$(TEST_BIN_DIR)/arena_smoke: $(TESTS_DIR)/arena_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The load generator. Not a test and deliberately not in `integration`: it
# reports what the server cost rather than whether it was right, and a fleet
# of fifty is far too slow and far too machine-dependent to sit in `make test`.
# ../../scripts/stress.sh starts a server and drives this against it.
$(TEST_BIN_DIR)/stress_client: $(TESTS_DIR)/stress_client.c $(TESTS_DIR)/stress.h $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

stress: $(TEST_BIN_DIR)/stress_client

# The renderer has no automated test: notcurses queries the terminal for
# capabilities (palette, pixel size, Kitty/Sixel support, ...) and blocks
# indefinitely waiting for replies. A real terminal answers these instantly;
# a scripted pty (forkpty) does not, and there is no notcurses option to
# skip the probe. Manual check via `make run` is the verification path.
#
# The network layer is a different matter, and is covered: it needs no
# terminal, so tests/integration/ drives it against a real tetrisd.
test: unit integration

unit: $(UNIT_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(UNIT_BINS)

integration: $(TEST_BIN_DIR)/net_smoke $(TEST_BIN_DIR)/net_provider_smoke \
			 $(TEST_BIN_DIR)/solo_authority_smoke $(TEST_BIN_DIR)/store_smoke \
			 $(TEST_BIN_DIR)/chat_smoke $(TEST_BIN_DIR)/session_smoke \
			 $(TEST_BIN_DIR)/match_smoke $(TEST_BIN_DIR)/arena_smoke
	@ echo "\n$(CYAN)==> Running integration tests$(CLR_RMV)"
	@ $(TEST_RUNNER) integration $(INTEGRATION_SRC)

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

.PHONY:		all run deps install-deps install-notcurses-from-source brain libs \
			check-dependencies check-tetrisu-deps test unit integration \
			stress clean fclean re

.PRECIOUS:	$(OBJ_DIR)/%.o
