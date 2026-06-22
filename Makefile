# tetriSH / TetriSocial — top-level Makefile
#
# Library build order (dependency-ordered, see docs/corestack.md SS12):
#   libcoreipc -> libtetrissh -> libhtttp -> libtetrisbrain
#   -> libchatcore -> libmarketcore -> binaries
#
# Only libtetrisbrain exists so far (Week 4). Other libraries and binaries
# are listed below as not-yet-implemented placeholders - wire each one in by
# mirroring the libtetrisbrain pattern as its source lands.

CC       ?= gcc
AR       ?= ar
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -pedantic -g -D_POSIX_C_SOURCE=200809L
OPTFLAGS ?= -O2

BIN_DIR := bin
LIB_DIR := lib
OBJ_DIR := obj

.PHONY: all libs bins test test-unit test-integration test-system clean

all: libs bins

# ---------------------------------------------------------------------------
# libtetrisbrain - pure game logic (board, pieces, gravity, line clear,
# scoring, abilities). No I/O, no POSIX - statically linked into tetrisd
# and (optionally) tetrisu for client-side prediction.
# ---------------------------------------------------------------------------
TETRISBRAIN_SRC := $(wildcard libtetrisbrain/*.c)
TETRISBRAIN_OBJ := $(patsubst libtetrisbrain/%.c,$(OBJ_DIR)/libtetrisbrain/%.o,$(TETRISBRAIN_SRC))
TETRISBRAIN_LIB := $(LIB_DIR)/libtetrisbrain.a

$(OBJ_DIR)/libtetrisbrain/%.o: libtetrisbrain/%.c include/tetrisbrain.h | $(OBJ_DIR)/libtetrisbrain
	$(CC) $(CFLAGS) $(OPTFLAGS) -I include -c $< -o $@

$(TETRISBRAIN_LIB): $(TETRISBRAIN_OBJ) | $(LIB_DIR)
	$(AR) rcs $@ $^

# ---------------------------------------------------------------------------
# Not yet implemented - add a block mirroring libtetrisbrain above as each
# library's source lands:
#   libcoreipc     ring_buffer.c, mq_helpers.c, unix_socket.c
#   libtetrissh    handshake.c, session.c
#   libhtttp       parser.c, serialiser.c
#   libchatcore    room_registry.c, rate_limiter.c, role.c, event_formatter.c
#   libmarketcore  ledger.c, inventory.c, catalogue.c, theme_loader.c
# ---------------------------------------------------------------------------

libs: $(TETRISBRAIN_LIB)

# ---------------------------------------------------------------------------
# Not yet implemented:
#   tetrish tetrisd tetrislogd tetrisctl tetrisu chatd chatctl marketd marketctl
# ---------------------------------------------------------------------------
bins:
	@echo "no binaries implemented yet"

# ---------------------------------------------------------------------------
# Tests - see docs/requirements.md SS"Test pyramid"
# ---------------------------------------------------------------------------
TETRISBRAIN_TESTS := test_board test_pieces test_gravity test_lineclear test_scoring test_abilities

# libtetrisbrain, libhtttp, libchatcore, libmarketcore unit tests.
# Only libtetrisbrain tests exist so far.
test-unit: $(TETRISBRAIN_LIB) | $(BIN_DIR)
	@for t in $(TETRISBRAIN_TESTS); do \
		echo "--- $$t ---"; \
		$(CC) $(CFLAGS) $(OPTFLAGS) -I include tests/$$t.c $(TETRISBRAIN_LIB) -lm -o $(BIN_DIR)/$$t || exit 1; \
		$(BIN_DIR)/$$t || exit 1; \
	done

# game_event.h publish -> consume pipeline across real SOCK_DGRAM sockets.
# Needs libcoreipc + tetrisd/event_pub.c - not yet implemented.
test-integration:
	@echo "no integration tests yet"

# Full clean build + scripted tetrisu sessions - not yet implemented.
test-system:
	@echo "no system tests yet"

test: test-unit test-integration test-system

$(OBJ_DIR)/libtetrisbrain $(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)
