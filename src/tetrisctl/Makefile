#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Admin CLI that owns the daemons' lifecycle. Built by the umbrella Makefile,
# or on its own with:
#     $(MAKE) -C src/tetrisctl
#
# It links the CoreStack archives listed under LIB_NAMES; each is built in
# place by its own Makefile before any tetrisctl object is compiled.
#
# It also carries its own tests:  $(MAKE) -C src/tetrisctl test

NAME		:= tetrisctl
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
RM			:= rm -rf
MAKE_FLAGS	:= --no-print-directory -s

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
LIB_ROOT	:= ../../lib
INC_DIR		:= include

# One archive per CoreStack library, each with the include path that goes with
# it. tetrisctl drives the daemons by pidfile and signal, so libcoredaemon is
# all it needs; the control socket that would pull in libhtttp is a later step.
LIB_NAMES	:= libcoredaemon
LIB_DIRS	:= $(LIB_NAMES:%=$(LIB_ROOT)/%)
LIB_ARCS	:= $(foreach l,$(LIB_NAMES),$(LIB_ROOT)/$(l)/$(l).a)
LIB_INC		:= $(LIB_DIRS:%=-I%/include)

INC			:= -I./$(INC_DIR) $(LIB_INC)

################################################################################
#                                  SOURCES                                     #
################################################################################

# Every .c under src/ is part of the CLI -> tetrisctl
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME)

# --- corestack archives -------------------------------------------------------
# An order-only gate: the archives must exist before compiling, but a sibling
# library's timestamp must not force every tetrisctl object to rebuild.
libs:
	@ for d in $(LIB_DIRS); do $(MAKE) $(MAKE_FLAGS) -C $$d || exit 1; done

# --- cli binary ---------------------------------------------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | libs
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(NAME): $(OBJ) | libs
	@ echo "\n$(GREEN)Compilation $(CLR_RMV)of $(BLUE)$(NAME)$(CLR_RMV)..."
	@ $(CC) $(FLAGS) $(OBJ) $(LIB_ARCS) -o $(NAME)
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

run: $(NAME)
	@ ./$(NAME) status

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin

TEST_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS		:= $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

TEST_RUNNER		:= ./scripts/run_tests.sh

# main.o is excluded so each test can provide its own main().
TEST_OBJ		:= $(filter-out $(OBJ_DIR)/main.o, $(OBJ))

# The stand-in daemon the lifecycle suite really forks and signals. It is
# installed under the name of a managed daemon because tetrisctl resolves what
# to run through PATH, exactly as dspawn did - the suite puts tests/bin first.
FAKE_NAMES		:= tetrislogd tetrisd
FAKE_DAEMONS	:= $(FAKE_NAMES:%=$(TEST_BIN_DIR)/%)

$(TEST_BIN_DIR)/%: $(TESTS_DIR)/fakedaemon.c | libs
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$< -> $@$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) $< $(LIB_ARCS) -o $@

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(wildcard $(TESTS_DIR)/*.h) \
						$(TEST_OBJ)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) -I$(TESTS_DIR) $< $(TEST_OBJ) $(LIB_ARCS) -o $@

# Filter a subset with: make test FILTER=cfg
test: $(TEST_BINS) $(FAKE_DAEMONS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

# The CoreStack archives are shared with the daemons and owned by their own
# directories, so cleanup here never recurses into them.
clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) binary ✔️"

re: fclean all

################################################################################
#                              PHONY && PRECIOUS                               #
################################################################################

.PHONY:		all libs run test clean fclean re

.PRECIOUS:	$(OBJ_DIR)/%.o
