#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Standalone logger daemon. Built by the umbrella Makefile, or on its own with:
#     $(MAKE) -C src/tetrislogd
#
# It links the CoreStack archives listed under LIB_NAMES; each is built in
# place by its own Makefile before any tetrislogd object is compiled.
#
# It also carries its own tests:  $(MAKE) -C src/tetrislogd test

NAME		:= tetrislogd
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
LDLIBS		:= -lpthread -lrt
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
# it. Adding a library here wires up its build, its -I and its link in one step.
# The logger is the log path itself, so it links only what it needs to receive
# records (libcoreipc) and to detach and hold its pidfile (libcoredaemon).
LIB_NAMES	:= libcoreipc libcoredaemon
LIB_DIRS	:= $(LIB_NAMES:%=$(LIB_ROOT)/%)
LIB_ARCS	:= $(foreach l,$(LIB_NAMES),$(LIB_ROOT)/$(l)/$(l).a)
LIB_INC		:= $(LIB_DIRS:%=-I%/include)

INC			:= -I./$(INC_DIR) $(LIB_INC)

################################################################################
#                                  SOURCES                                     #
################################################################################

# Every .c under src/ is part of the daemon -> tetrislogd
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME)

# --- corestack archives -------------------------------------------------------
# An order-only gate: the archives must exist before compiling, but a sibling
# library's timestamp must not force every tetrislogd object to rebuild.
libs:
	@ for d in $(LIB_DIRS); do $(MAKE) $(MAKE_FLAGS) -C $$d || exit 1; done

# --- daemon binary ------------------------------------------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | libs
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(NAME): $(OBJ) | libs
	@ echo "\n$(GREEN)Compilation $(CLR_RMV)of $(BLUE)$(NAME)$(CLR_RMV)..."
	@ $(CC) $(FLAGS) $(OBJ) $(LIB_ARCS) $(LDLIBS) -o $(NAME)
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

run: $(NAME)
	@ ./$(NAME)

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
INTEGRATION_DIR	:= $(TESTS_DIR)/integration

TEST_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS		:= $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

# The fixture every suite builds its throwaway socket and log file with:
# tests/*.c that are not themselves a suite are linked into all of them.
TEST_HELPERS	:= $(filter-out $(TEST_SRC), $(wildcard $(TESTS_DIR)/*.c))

TEST_RUNNER		:= ./scripts/run_tests.sh

# main.o is excluded so each test can provide its own main().
TEST_OBJ		:= $(filter-out $(OBJ_DIR)/main.o, $(OBJ))

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(TEST_HELPERS) \
						$(wildcard $(TESTS_DIR)/*.h) $(TEST_OBJ)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) -I$(TESTS_DIR) $< $(TEST_HELPERS) $(TEST_OBJ) \
		$(LIB_ARCS) $(LDLIBS) -o $@

# Filter a subset with: make test FILTER=sink
unit: $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

# The rotation rule is shell against a real logrotate, not a linked suite: what
# it guards is the handover between logrotate and the daemon, and neither half
# of that exists inside a test binary.
integration:
	@ echo "\n$(CYAN)==> Running integration tests$(CLR_RMV)"
	@ $(TEST_RUNNER) integration $(INTEGRATION_DIR)/*.sh

test: unit integration

################################################################################
#                                   CLEANUP                                    #
################################################################################

# The CoreStack archives are shared with the other daemons and owned by their
# own directories, so cleanup here never recurses into them.
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

.PHONY:		all libs run unit integration test clean fclean re

.PRECIOUS:	$(OBJ_DIR)/%.o
