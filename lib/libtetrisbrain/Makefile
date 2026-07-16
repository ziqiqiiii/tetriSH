#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Self-contained static library. Another Makefile builds it with:
#     $(MAKE) -C libtetrisbrain
# then links the archive + adds the header path when compiling its own code:
#     gcc ... libtetrisbrain/libtetrisbrain.a -I libtetrisbrain/include ...
#
# It also carries its own tests:  $(MAKE) -C libtetrisbrain test

NAME		:= libtetrisbrain.a
CC			:= gcc
FLAGS		:= -std=c11 -Wall -Wextra -Werror -pedantic
AR			:= ar
ARFLAGS		:= rcs
RM			:= rm -rf

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN		:= \033[1;36m

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SRC_DIR		:= src
OBJ_DIR		:= obj
INC_DIR		:= include
INC			:= -I$(INC_DIR)

HEADER		:= $(INC_DIR)/tetrisbrain.h

################################################################################
#                                  SOURCES                                     #
################################################################################

# Every .c under src/ is part of the library.
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@ mkdir -p $(OBJ_DIR)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(NAME): $(OBJ)
	@ $(AR) $(ARFLAGS) $@ $(OBJ)
	@ echo "\n$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin

TEST_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS		:= $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

TEST_RUNNER		:= ./scripts/run_tests.sh

# Each test provides its own main() and links against the library archive.
$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) $< $(NAME) -o $@

# Compile every test, then run them through the formatted runner.
# Filter a subset with: make test FILTER=abilities
test: $(NAME) $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) library ✔️"

re: fclean all

.PHONY:		all test clean fclean re
