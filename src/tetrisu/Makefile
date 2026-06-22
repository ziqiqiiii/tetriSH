#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################
#
# Self-contained binary (same self-contained pattern as the libraries under
# lib/, applied to a binary instead of an archive). Until the umbrella
# Makefile lands, build/test this directly:
#     make -C src/tetrisu
#     make -C src/tetrisu test
#     make -C src/tetrisu run

NAME		:= tetrisu
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
RM			:= rm -rf

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
CYAN		:= \033[1;36m
BLUE		:= \033[1;34m

################################################################################
#                                DIRECTORIES                                   #
################################################################################

OBJ_DIR		:= obj
BIN_DIR		:= bin
NAME_PATH	:= $(BIN_DIR)/$(NAME)

HEADER		:= tetrisu.h
ASSET_DIR	:= $(CURDIR)/assets

NOTCURSES_CFLAGS	:= $(shell pkg-config --cflags notcurses)
NOTCURSES_LIBS		:= $(shell pkg-config --libs notcurses)

FLAGS		+= -I. $(NOTCURSES_CFLAGS) -DASSET_DIR=\"$(ASSET_DIR)\" -DTETRISU_BIN_PATH=\"$(NAME_PATH)\"

LDLIBS		:= $(NOTCURSES_LIBS)

################################################################################
#                                  SOURCES                                     #
################################################################################

LOGIC_SRC	:= app_state.c
MENU_SRC	:= render_background.c render_menu.c
SRC			:= main.c $(LOGIC_SRC) $(MENU_SRC)
OBJ			:= $(SRC:%.c=$(OBJ_DIR)/%.o)

LOGIC_OBJ	:= $(LOGIC_SRC:%.c=$(OBJ_DIR)/%.o)
LOGIC_LIB	:= $(OBJ_DIR)/logic.a

################################################################################
#                                   BUILD                                      #
################################################################################

all: $(NAME_PATH)

$(OBJ_DIR)/%.o: %.c $(HEADER)
	@ mkdir -p $(OBJ_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) -c $< -o $@
	@ echo "ok"

$(NAME_PATH): $(OBJ)
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(OBJ) $(LDLIBS) -o $@
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) built"

run: $(NAME_PATH)
	@ ./$(NAME_PATH)

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
TEST_BIN_DIR	:= $(TESTS_DIR)/bin
TEST_RUNNER		:= ./scripts/run_tests.sh

$(LOGIC_LIB): $(LOGIC_OBJ)
	@ ar rcs $@ $(LOGIC_OBJ)

UNIT_SRC		:= $(TESTS_DIR)/test_app_state.c
UNIT_BINS		:= $(UNIT_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.c $(LOGIC_LIB) $(HEADER)
	@ mkdir -p $(TEST_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(FLAGS) $< $(LOGIC_LIB) -o $@
	@ echo "ok"

# No automated integration test: notcurses queries the terminal for
# capabilities (palette, pixel size, Kitty/Sixel support, ...) and blocks
# indefinitely waiting for replies. A real terminal answers these instantly;
# a scripted pty (forkpty) does not, and there is no notcurses option to
# skip the probe. Verified manually that it hangs regardless of TERM value.
# Manual check via `make run` is the verification path for this binary.

test: $(UNIT_BINS)
	@ echo "\n$(CYAN)==> Running tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(UNIT_BINS)

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(TEST_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs"

fclean: clean
	@ $(RM) $(BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) binary"

re: fclean all

.PHONY:		all run test clean fclean re
