# Self-contained static library for durable account/market state.
# Build with:  make -C lib/libcoredb
# Test with:   make -C lib/libcoredb test

NAME        := libcoredb.a
CC          := gcc
CFLAGS      := -std=c11 -Wall -Wextra -Werror -pedantic -g -D_POSIX_C_SOURCE=200809L
AR          := ar
ARFLAGS     := rcs
RM          := rm -rf

CLR_RMV     := \033[0m
RED         := \033[1;31m
GREEN       := \033[1;32m
YELLOW      := \033[1;33m
BLUE        := \033[1;34m
CYAN        := \033[1;36m

SRC_DIR     := src
OBJ_DIR     := obj
INC_DIR     := include
INC         := -I$(INC_DIR)

HEADER      := $(INC_DIR)/coredb.h
SRC         := $(wildcard $(SRC_DIR)/*.c)
OBJ         := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@ mkdir -p $(OBJ_DIR)
	@ $(CC) $(CFLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(NAME): $(OBJ)
	@ $(AR) $(ARFLAGS) $@ $(OBJ)
	@ echo "\n$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created"

TESTS_DIR    := tests
TEST_BIN_DIR := $(TESTS_DIR)/bin
TEST_SRC     := $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS    := $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)
TEST_RUNNER  := sh ./scripts/run_tests.sh

$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(NAME) $(HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(CFLAGS) $(INC) $< $(NAME) -pthread -o $@

test: $(NAME) $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) library"

re: fclean all

.PHONY: all test clean fclean re
