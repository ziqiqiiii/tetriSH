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
#     $(MAKE) -C libtetrissh
# then links the archive + adds the header path when compiling its own code:
#     gcc ... libtetrissh/libtetrissh.a -I libtetrissh/include -lssl -lcrypto ...
#
# It also carries its own tests:  $(MAKE) -C libtetrissh test

NAME			:= libtetrissh.a
CC				:= gcc
FLAGS			:= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
ifeq ($(shell uname -s),Darwin)
FLAGS			+= -D_DARWIN_C_SOURCE
endif
AR				:= ar
ARFLAGS			:= rcs
RM				:= rm -rf

OPENSSL_CFLAGS		:= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS		:= $(shell pkg-config --libs openssl 2>/dev/null)
INC				:= -Iinclude $(OPENSSL_CFLAGS)
LDLIBS			:= $(if $(OPENSSL_LIBS),$(OPENSSL_LIBS),-lssl -lcrypto)

# common.c is the frozen course-provided crypto helper; it is built without
# -Werror -pedantic so its warnings do not fail this library's strict build.
COMMON_FLAGS	:= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2

CLR_RMV			:= \033[0m
RED				:= \033[1;31m
GREEN			:= \033[1;32m
YELLOW			:= \033[1;33m
BLUE			:= \033[1;34m
CYAN			:= \033[1;36m

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SRC_DIR			:= src
OBJ_DIR			:= obj
INC_DIR			:= include

HEADER			:= $(INC_DIR)/tetrissh.h $(INC_DIR)/internal.h

################################################################################
#                                  SOURCES                                     #
################################################################################

# Library : every .c under src/ except the frozen common.c -> libtetrissh.a
# Common  : frozen course helper, compiled with relaxed flags -> common.o
COMMON_SRC		:= $(SRC_DIR)/common.c
COMMON_OBJ		:= $(OBJ_DIR)/common.o

SRC				:= $(filter-out $(COMMON_SRC), $(wildcard $(SRC_DIR)/*.c))
OBJ				:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(COMMON_OBJ)

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME)

# --- library objects --------------------------------------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

# --- frozen common helper ---------------------------------------------------
# Given its own recipe (relaxed flags) rather than an if inside the shared rule.
$(COMMON_OBJ): $(COMMON_SRC) $(INC_DIR)/libs/common.h
	@ mkdir -p $(dir $@)
	@ $(CC) $(COMMON_FLAGS) $(INC) -c $< -o $@
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

# Each test provides its own main(), links the archive + OpenSSL, and needs
# pthreads for the socketpair integration cases.
$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $(INC) $< $(NAME) $(LDLIBS) -pthread -o $@

# Compile every test, then run them through the formatted runner.
# Filter a subset with: make test FILTER=io
test: $(NAME) $(TEST_BINS)
	@ echo "\n$(CYAN)==> Running libtetrissh tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(TEST_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR) $(TESTS_DIR)/tmp
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) library ✔️"

re: fclean all

################################################################################
#                               PHONY && PRECIOUS                              #
################################################################################

.PHONY:		all test clean fclean re

.PRECIOUS:	$(OBJ_DIR)/%.o
