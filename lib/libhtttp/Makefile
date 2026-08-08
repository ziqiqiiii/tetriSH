# Self-contained HTTTP static library and unit tests.

NAME			:= libhtttp.a
CC				:= gcc
FLAGS			:= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
AR				:= ar
ARFLAGS			:= rcs
RM				:= rm -rf
VALGRIND		?= valgrind

CLR_RMV			:= \033[0m
RED				:= \033[1;31m
GREEN			:= \033[1;32m
YELLOW			:= \033[1;33m
BLUE			:= \033[1;34m
CYAN			:= \033[1;36m

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
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(NAME): FORCE $(OBJ)
	@ $(RM) $@
	@ $(AR) $(ARFLAGS) $@ $(OBJ)
	@ echo "\n$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) $< $(NAME) $(TEST_LDFLAGS_$*) -o $@

test: $(NAME) $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ bash $(TEST_RUNNER) unit $(TEST_BINS)

memcheck: $(NAME) $(TEST_BINS)
	@ matched=0; status=0; \
	for test_bin in $(TEST_BINS); do \
		case "$$test_bin" in \
			*"$(FILTER)"*) \
				matched=$$((matched + 1)); \
				$(VALGRIND) --leak-check=full --show-leak-kinds=all \
					--errors-for-leak-kinds=all --error-exitcode=1 \
					"$$test_bin" || status=1 ;; \
		esac; \
	done; \
	if [ $$matched -eq 0 ]; then \
		printf 'No tests matched FILTER="%s"\n' "$(FILTER)"; \
		exit 1; \
	fi; \
	exit $$status

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) library ✔️"

re: fclean all

FORCE:

.PHONY: all test memcheck clean fclean re FORCE

.PRECIOUS: $(OBJ_DIR)/%.o
