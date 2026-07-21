# Self-contained HTTTP static library and unit tests.

NAME			:= libhtttp.a
CC				:= gcc
FLAGS			:= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
AR				:= ar
ARFLAGS			:= rcs
RM				:= rm -rf
VALGRIND		?= valgrind

INC				:= -Iinclude

SRC_DIR			:= src
OBJ_DIR			:= obj
INC_DIR			:= include
HEADER			:= $(INC_DIR)/htttp.h

SRC				:= $(wildcard $(SRC_DIR)/*.c)
OBJ				:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
TEST_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS		:= $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)
TEST_RUNNER		:= ./scripts/run_tests.sh
TEST_LDFLAGS_message	:= -Wl,--wrap=malloc -Wl,--wrap=realloc
TEST_LDFLAGS_parser	:= -Wl,--wrap=malloc -Wl,--wrap=realloc
TEST_LDFLAGS_serialiser	:= -Wl,--wrap=malloc

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@

$(NAME): $(OBJ)
	@ $(AR) $(ARFLAGS) $@ $(OBJ)
	@ printf '[Success] %s created\n' "$(NAME)"

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@ mkdir -p $(TEST_BIN_DIR)
	@ $(CC) $(FLAGS) $(INC) $< $(NAME) $(TEST_LDFLAGS_$*) -o $@

test: $(NAME) $(TEST_BINS)
	@ bash $(TEST_RUNNER) unit $(TEST_BINS)

memcheck: $(NAME) $(TEST_BINS)
	@ matched=0; status=0; \
	for test_bin in $(TEST_BINS); do \
		case "$$test_bin" in \
			*"$(FILTER)"*) \
				matched=$$((matched + 1)); \
				$(VALGRIND) --leak-check=full --show-leak-kinds=all \
					--error-exitcode=1 "$$test_bin" || status=1 ;; \
		esac; \
	done; \
	if [ $$matched -eq 0 ]; then \
		printf 'No tests matched FILTER="%s"\n' "$(FILTER)"; \
		exit 1; \
	fi; \
	exit $$status

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)

fclean: clean
	@ $(RM) $(NAME)

re: fclean all

.PHONY: all test memcheck clean fclean re

.PRECIOUS: $(OBJ_DIR)/%.o
