NAME := libtetrissh.a
CC := gcc
AR := ar
ARFLAGS := rcs
RM := rm -rf

CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
COMMON_CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2
INC := -Iinclude
LDLIBS := -lssl -lcrypto

SRC_DIR := src
OBJ_DIR := obj
TESTS_DIR := tests
TEST_BIN_DIR := $(TESTS_DIR)/bin
COMMON_SRC := $(SRC_DIR)/common.c
COMMON_OBJ := $(OBJ_DIR)/common.o

SRC := $(filter-out $(COMMON_SRC),$(wildcard $(SRC_DIR)/*.c))
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(COMMON_OBJ)

TEST_SRC := $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS := $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

.PHONY: all test clean fclean re

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c include/tetrissh.h $(SRC_DIR)/internal.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(COMMON_OBJ): $(COMMON_SRC) include/libs/common.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(COMMON_CFLAGS) $(INC) -c $< -o $@

$(NAME): $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@mkdir -p $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(NAME) $(LDLIBS) -pthread -o $@

test: $(NAME) $(TEST_BINS)
	@sh ./scripts/run_tests.sh $(TEST_BINS)

clean:
	$(RM) $(OBJ_DIR) $(TEST_BIN_DIR) tests/tmp

fclean: clean
	$(RM) $(NAME)

re: fclean all
