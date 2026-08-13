#  _         _        _
# | |_  ___ | |_  _ _ (_) ___ _  _
# |  _|/ -_)|  _|| '_|| |(_-<| || |
#  \__|\___| \__||_|  |_|/__/ \_,_|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

# Self-contained binary — the lib/ pattern applied to a binary, so
# `make -C src/tetrisu [test|run]` works without the umbrella.

NAME		:= tetrisu
BOT_NAME	:= tetrisu-bot
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
# openssl is keg-only there — its libraries are not on the linker's default
# search path at all — so its .pc file is what turns -lssl into a -L that
# resolves.
ifeq ($(UNAME), Darwin)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifneq ($(BREW_PREFIX),)
export PKG_CONFIG_PATH := $(BREW_PREFIX)/opt/notcurses/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2/lib/pkgconfig:$(BREW_PREFIX)/opt/sdl2_mixer/lib/pkgconfig:$(BREW_PREFIX)/opt/ncurses/lib/pkgconfig:$(BREW_PREFIX)/opt/openssl@3/lib/pkgconfig:$(BREW_PREFIX)/opt/openssl/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif
endif

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SRC_DIR		:= src
OBJ_DIR		:= obj
BIN_DIR		:= bin
NAME_PATH	:= $(BIN_DIR)/$(NAME)
BOT_PATH	:= $(BIN_DIR)/$(BOT_NAME)

HEADER		:= tetrisu.h
NET_HEADER	:= tetrisu_net.h
BOT_HEADER	:= tetrisu_bot.h
ASSET_DIR	:= $(CURDIR)/assets

# libtetrisbrain is local prediction and the shared piece/board types; the
# other three are the session tetrisu speaks to tetrisd over.
LIB_ROOT	:= ../../lib
LIB_NAMES	:= libtetrisbrain libtetrissh libhtttp libstatusbody
LIB_DIRS	:= $(LIB_NAMES:%=$(LIB_ROOT)/%)
LIB_ARCS	:= $(foreach l,$(LIB_NAMES),$(LIB_ROOT)/$(l)/$(l).a)
LIB_INC		:= $(LIB_DIRS:%=-I%/include)

BRAIN_DIR	:= $(LIB_ROOT)/libtetrisbrain
BRAIN_LIB	:= $(BRAIN_DIR)/libtetrisbrain.a
BRAIN_INC	:= -I$(BRAIN_DIR)/include
BRAIN_HEADER	:= $(BRAIN_DIR)/include/tetrisbrain.h

# notcurses (render) is required; SDL2 + SDL2_mixer (audio) compile out when
# absent. Lazily expanded (=) because deps may be installed mid-build, after
# this file is parsed but before the first object.
PKG_CONFIG		:= pkg-config
AUDIO_DEPS		:= sdl2 SDL2_mixer
NOTCURSES_CFLAGS	 = $(shell $(PKG_CONFIG) --cflags notcurses 2>/dev/null)
NOTCURSES_LIBS		 = $(shell $(PKG_CONFIG) --libs notcurses 2>/dev/null)
AUDIO_CFLAGS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --cflags $(AUDIO_DEPS); fi)
AUDIO_LIBS		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then $(PKG_CONFIG) --libs $(AUDIO_DEPS); fi)
AUDIO_DEFINE		 = $(shell if $(PKG_CONFIG) --exists $(AUDIO_DEPS) 2>/dev/null; then printf '%s' -DTETRISU_ENABLE_AUDIO=1; else printf '%s' -DTETRISU_ENABLE_AUDIO=0; fi)

# OpenSSL the same way libtetrissh asks for it: pkg-config first, bare -l flags
# as the fallback on a distribution that keeps it on the default paths. Asking
# pkg-config is not decoration — on Homebrew the library lives under a keg the
# linker does not search, and the bare flags alone fail with `library 'ssl' not
# found`.
OPENSSL_LIBS		 = $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null || printf '%s' "-lssl -lcrypto")

# Header search path + generated defines shared by every target.
# DEPFLAGS makes each object record the headers it actually included, so a
# CoreStack header that changes the shape of a struct rebuilds every object
# holding one instead of leaving stale ones to be linked against fresh code.
INC			:= -I. $(LIB_INC)
DEPFLAGS	:= -MMD -MP
FLAGS		 = $(BASE_FLAGS) $(INC) $(NOTCURSES_CFLAGS) $(AUDIO_CFLAGS) $(AUDIO_DEFINE) -DASSET_DIR=\"$(ASSET_DIR)\" -DTETRISU_BIN_PATH=\"$(NAME_PATH)\"
LDLIBS		 = $(NOTCURSES_LIBS) $(AUDIO_LIBS) $(OPENSSL_LIBS) -lpthread

################################################################################
#                                  SOURCES                                     #
################################################################################

# Binary : every .c under src/            -> bin/tetrisu
#
# bot_main.c is the one exception: it carries a main() of its own, so leaving
# it in would link two into tetrisu. It is the entry point of the second
# binary below.
SRC			:= $(filter-out $(SRC_DIR)/bot_main.c, $(wildcard $(SRC_DIR)/*.c))
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEP			:= $(OBJ:.o=.d)

# Audio is a compile-time define, and make compares timestamps, not flags: SDL2
# appearing later would leave audio compiled out until an fclean nobody knew to
# run. The stamp puts the flag in the dependency graph, rewritten only when its
# value changes, so it forces exactly one rebuild.
AUDIO_STAMP	:= $(OBJ_DIR)/.audio-config

# Logic  : pure app-state, no notcurses/SDL -> logic.a (linked into unit tests).
# The filtered-out sources pull in notcurses/SDL, which no unit test can link.
LOGIC_SRC	:= $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/solo_mode.c \
				   $(SRC_DIR)/multiplayer_match_mode.c \
				   $(SRC_DIR)/render_%.c $(SRC_DIR)/audio.c, $(SRC))
LOGIC_OBJ	:= $(LOGIC_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LOGIC_LIB	:= $(OBJ_DIR)/logic.a

################################################################################
#                                   BUILD                                      #
################################################################################

all: check-dependencies libs $(NAME_PATH) $(BOT_PATH)

# A standalone build asks root for shared deps, then handles render/audio here.
# Root passes DEPS_READY=1 to avoid recursion.
check-dependencies:
	@ if [ "$(DEPS_READY)" != "1" ]; then \
		$(MAKE) --no-print-directory -s -C ../.. deps; \
	fi
	@ $(MAKE) --no-print-directory -s deps

# --- audio configuration stamp ----------------------------------------------
# Order-only on check-dependencies so the value recorded is the one that holds
# after whatever that step installed.
$(AUDIO_STAMP): FORCE | check-dependencies
	@ mkdir -p $(dir $@)
	@ printf '%s' "$(AUDIO_DEFINE)" | cmp -s - $@ 2>/dev/null \
		|| printf '%s' "$(AUDIO_DEFINE)" > $@

FORCE:

# --- object rule (header stays at the tetrisu root, reached via -I.) ---------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(AUDIO_STAMP) | check-dependencies libs
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

# --- tetrisu-bot binary -----------------------------------------------------
# The opponent a player adds to their own room, as its own process. It links
# the logic archive and the CoreStack libraries and deliberately not
# NOTCURSES_LIBS or AUDIO_LIBS: a bot has no screen, and a binary that pulled
# in a terminal library would be one more thing to install on a machine that
# only ever wanted to fill a seat.
#
# BOT_LDLIBS is therefore LDLIBS without the two render/audio groups. The
# archive is the same one the unit tests link, so nothing here is a second
# copy of the network layer.
#
# It has to name $(OPENSSL_LIBS) rather than -lssl -lcrypto: dropping the audio
# group drops the -L that formerly carried OpenSSL in by accident on macOS.
# Lazily expanded for the same reason the groups above are.
BOT_LDLIBS	 = $(OPENSSL_LIBS) -lpthread

$(BOT_PATH): $(SRC_DIR)/bot_main.c $(LOGIC_LIB) $(LIB_ARCS) $(BOT_HEADER) $(NET_HEADER)
	@ mkdir -p $(BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(BOT_LDLIBS) -o $@
	@ echo "ok"

run: $(NAME_PATH)
	@ ./$(NAME_PATH)

################################################################################
#                                DEPENDENCIES                                  #
################################################################################

# Render/audio dependency orchestration lives in scripts/ so this Makefile
# stays declarative and each shell failure has one owner.
deps:
	@ UNAME_S=$(UNAME) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		INSTALL_NOTCURSES_FROM_SOURCE=$(INSTALL_NOTCURSES_FROM_SOURCE) \
		NOTCURSES_VERSION=$(NOTCURSES_VERSION) \
		NOTCURSES_MIN_VERSION=$(NOTCURSES_MIN_VERSION) \
		NOTCURSES_PREFIX=$(NOTCURSES_PREFIX) CC=$(CC) \
		PKG_CONFIG=$(PKG_CONFIG) AUDIO_DEPS='$(AUDIO_DEPS)' \
		GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)' \
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

# Each archive is built in place by its own Makefile. Cleanup never recurses
# into lib/ — the archives are shared with the daemons.
libs:
	@ for d in $(LIB_DIRS); do $(MAKE) --no-print-directory -s -C $$d; done

$(LIB_ARCS): libs
	@ :

brain: libs

# The script compiles and links against the actual headers and libraries
# rather than trusting package-manager state.
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
$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(BOT_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# --- integration binaries ---------------------------------------------------
# Not unit tests: each needs a live tetrisd, started by its integration script.
# All link the whole app archive, since the network layer writes into the same
# view models the renderer reads.

# The session: connect, auth, join, play, STATE decoded into the Solo model.
$(TEST_BIN_DIR)/net_smoke: $(TESTS_DIR)/net_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The provider vtable: CHECK SERVER, SIGN UP, LOGIN, profile, lobby, room.
$(TEST_BIN_DIR)/net_provider_smoke: $(TESTS_DIR)/net_provider_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# Marketplace and Settings: prices, wallet, ownership and the loadout, all of
# which live in the account rather than the client's view model.
$(TEST_BIN_DIR)/store_smoke: $(TESTS_DIR)/store_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The layer solo_mode.c calls: who owns the board, and the countdown hold that
# reconciles the client's 3-2-1 with the server's gravity.
$(TEST_BIN_DIR)/solo_authority_smoke: $(TESTS_DIR)/solo_authority_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# The room feed: server narration and player chat both reach the ring, whether
# they arrive on their own or across a reply.
$(TEST_BIN_DIR)/chat_smoke: $(TESTS_DIR)/chat_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) $(LIB_ARCS) $(LDLIBS) -o $@
	@ echo "ok"

# Two clients in one process, on two sockets — the only way one program can
# assert on both halves of the same instant. Two processes could agree only by
# talking to each other, which is the thing under test.
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

# One person and three bots in a Battle Royale, against a live tetrisd. It
# spawns processes as well as talking to a server, which is why it is a client
# binary and not a unit test: what it proves is that bot_proc.c's children
# really do reach a seat, play, and leave when they are let go.
$(TEST_BIN_DIR)/bots_smoke: $(TESTS_DIR)/bots_smoke.c $(LOGIC_LIB) $(LIB_ARCS) $(HEADER) $(NET_HEADER) $(BOT_HEADER) $(BOT_PATH)
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

bot: $(BOT_PATH)

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
			 $(TEST_BIN_DIR)/match_smoke $(TEST_BIN_DIR)/arena_smoke \
			 $(TEST_BIN_DIR)/bots_smoke
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
			stress bot clean fclean re FORCE

.PRECIOUS:	$(OBJ_DIR)/%.o
