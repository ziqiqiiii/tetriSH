#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Concurrent game daemon. Built by the umbrella Makefile, or on its own with:
#     $(MAKE) -C src/tetrisd
#
# It links the CoreStack archives listed under LIB_NAMES; each is built in
# place by its own Makefile before any tetrisd object is compiled.
#
# It also carries its own tests:  $(MAKE) -C src/tetrisd test

NAME		:= tetrisd
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
# Header prerequisites, generated per object. A struct in a CoreStack header is
# part of this daemon's ABI the moment it is compiled in, so a library header
# that changes shape without rebuilding its callers gets stale objects linked
# against fresh ones - which is memory corruption, not a build error, and it
# presented as a stack smash in a suite that had nothing to do with the edit.
DEPFLAGS	:= -MMD -MP
LDLIBS		:= -lssl -lcrypto -lpthread -lrt
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
HEADER		:= $(INC_DIR)/tetrisd.h


# One archive per CoreStack library, each with the include path that goes with
# it. Adding a library here wires up its build, its -I and its link in one step.
LIB_NAMES	:= libcoreipc libcoredaemon libhtttp libtetrisbrain libmacminidb \
			   libtetrissh libtetrisroom libstatusbody
LIB_DIRS	:= $(LIB_NAMES:%=$(LIB_ROOT)/%)
LIB_ARCS	:= $(foreach l,$(LIB_NAMES),$(LIB_ROOT)/$(l)/$(l).a)
LIB_INC		:= $(LIB_DIRS:%=-I%/include)

INC			:= -I./$(INC_DIR) $(LIB_INC)

################################################################################
#                                  SOURCES                                     #
################################################################################

# Every .c under src/ is part of the daemon -> tetrisd
SRC			:= $(wildcard $(SRC_DIR)/*.c)
OBJ			:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEP			:= $(OBJ:.o=.d)

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME)

# --- corestack archives -------------------------------------------------------
# Each archive is built by the library that owns it, and FORCE is what lets
# that sub-make decide whether it is stale - only it knows that library's
# sources. The archive's own timestamp is then what tells this Makefile whether
# to relink.
#
# That timestamp has to reach the link rules as a real prerequisite, not an
# order-only gate. As a gate it was invisible: a fix inside a library rebuilt
# the archive and nothing here relinked, so the daemon and - worse - every test
# binary kept asserting against the code as it was before the fix.
libs: $(LIB_ARCS)

$(LIB_ARCS): FORCE
	@ $(MAKE) $(MAKE_FLAGS) -C $(dir $@)

# --- daemon binary ------------------------------------------------------------
# The objects keep the order-only gate: the archives must exist before a
# tetrisd source compiles against their headers, but a sibling library's
# timestamp must not force every object to rebuild.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | libs
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(DEPFLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

# Every header each object actually included, tetrisd's and the libraries'.
# Absent on a first build, when everything compiles anyway.
-include $(DEP)

$(NAME): $(OBJ) $(LIB_ARCS)
	@ echo "\n$(GREEN)Compilation $(CLR_RMV)of $(BLUE)$(NAME)$(CLR_RMV)..."
	@ $(CC) $(FLAGS) $(OBJ) $(LIB_ARCS) $(LDLIBS) -o $(NAME)
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

# Every path in .tetrishrc is relative to the project root
run: $(NAME)
	@ cd ../.. && ./src/tetrisd/$(NAME)

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin

TEST_SRC		:= $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS		:= $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

# The headless harness client every integration test drives the server with:
# shared code, not a suite of its own, so it is compiled into each binary.
TEST_HELPERS	:= $(filter-out $(TEST_SRC), $(wildcard $(TESTS_DIR)/*.c))

TEST_RUNNER		:= ./scripts/run_tests.sh

# main.o is excluded so each test can provide its own main().
TEST_OBJ		:= $(filter-out $(OBJ_DIR)/main.o, $(OBJ))

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(TEST_HELPERS) $(HEADER) \
						$(wildcard $(TESTS_DIR)/*.h) $(TEST_OBJ) $(LIB_ARCS)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) -I$(TESTS_DIR) $< $(TEST_HELPERS) $(TEST_OBJ) \
		$(LIB_ARCS) $(LDLIBS) -o $@

# Filter a subset with: make test FILTER=outq
test: $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

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

.PHONY:		all libs run test clean fclean re FORCE

.PRECIOUS:	$(OBJ_DIR)/%.o
