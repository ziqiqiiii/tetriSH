#    \  |         |
#  |\/ |   _  |  |  /   _
#  |   |  (   |    <    __/
# _|  _| \__,_| _|\_\ \___|
#
################################################################################
#                                    CONFIG                                    #
################################################################################

NAME		:= macmini_shell
CC			:= gcc
FLAGS		:= -Wall -Wextra -Werror
RM			:= rm -rf

CLR_RMV		:= \033[0m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN		:= \033[1;36m

################################################################################
#                              PLATFORM SPECIFICS                              #
################################################################################

UNAME		:= $(shell uname)

# Readline + AddressSanitizer for Linux
ifeq ($(UNAME), Linux)
	READLINE	:= -lreadline
	INC_RL		:= -I/usr/include/readline
	FSAN		:= -fsanitize=address -g3
endif

# Readline for macOS (RL_PREFIX is lazily expanded so it resolves correctly
# even when readline is installed by the check-readline target during a build).
ifeq ($(UNAME), Darwin)
	RL_PREFIX	 = $(shell brew --prefix readline)
	READLINE	 = -lreadline -L$(RL_PREFIX)/lib
	INC_RL		 = -I$(RL_PREFIX)/include
endif


LSAN_SUPP	:= $(CURDIR)/readline.supp
LSAN_ENV	:= LSAN_OPTIONS=suppressions=$(LSAN_SUPP)

################################################################################
#                                DIRECTORIES                                   #
################################################################################

SRC_DIR		:= src
OBJ_DIR		:= obj
BIN_DIR		:= bin
INC_DIR		:= includes

LIBFT_DIR	:= libft
LIBFT		:= $(LIBFT_DIR)/libft.a
LIB			:= -lft -L./$(LIBFT_DIR)

COMMON_LIB	:= $(OBJ_DIR)/libcommon.a

# Header search path shared by every target
INC			:= -I./$(INC_DIR) -I./$(LIBFT_DIR)/$(INC_DIR) $(INC_RL)

################################################################################
#                                  SOURCES                                     #
################################################################################

# Shell  : every .c under src/shell        -> macmini_shell
SHELL_SRC	:= $(wildcard $(SRC_DIR)/shell/*.c)
SHELL_OBJ	:= $(SHELL_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Common : helpers shared by system programs -> libcommon.a
COMMON_SRC	:= $(wildcard $(SRC_DIR)/common/*.c)
COMMON_OBJ	:= $(COMMON_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# System : every .c under src/system        -> one binary each in bin/
SYS_SRC		:= $(wildcard $(SRC_DIR)/system/*.c)
# sys.c relies on Linux-only APIs (e.g. sysinfo); skip it on macOS.
ifeq ($(UNAME), Darwin)
	SYS_SRC	:= $(filter-out $(SRC_DIR)/system/sys.c, $(SYS_SRC))
endif
SYS_BINS	:= $(SYS_SRC:$(SRC_DIR)/system/%.c=$(BIN_DIR)/%)

################################################################################
#                                   BUILD                                      #
################################################################################

all: check-dependencies $(NAME) system-programs

# A direct `make -C src/tetrish` still gets the cross-platform dependency
# installer. The umbrella build sets DEPS_READY=1 because it has already run it.
check-dependencies:
	@ if [ "$(DEPS_READY)" != "1" ]; then \
		$(MAKE) --no-print-directory -s -C ../.. deps; \
	fi

run: all
	@ $(LSAN_ENV) ./$(NAME)

# --- shell object rule (ASan-instrumented, mirrors src/ tree under obj/) -----
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | check-dependencies
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(FSAN) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

# --- common object rule (no ASan: linked into the plain system binaries) -----
$(OBJ_DIR)/common/%.o: $(SRC_DIR)/common/%.c | check-dependencies
	@ mkdir -p $(dir $@)
	@ $(CC) $(FLAGS) $(INC) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

# --- libft ------------------------------------------------------------------
$(LIBFT): | check-dependencies
	@ echo "$(GREEN)Making $(CYAN)libft$(CLR_RMV) library..."
	@ $(MAKE) -C $(LIBFT_DIR)

# --- libcommon (helpers shared by the system programs) ----------------------
$(COMMON_LIB): $(COMMON_OBJ)
	@ ar rcs $@ $(COMMON_OBJ)
	@ echo "\n$(GREEN)[Success] $(BLUE)libcommon.a$(CLR_RMV) created ✔️"

# --- shell binary -----------------------------------------------------------
$(NAME): $(LIBFT) $(COMMON_LIB) $(SHELL_OBJ)
	@ echo "\n$(GREEN)Compilation $(CLR_RMV)of $(BLUE)$(NAME)$(CLR_RMV)..."
	@ $(CC) $(FLAGS) $(FSAN) $(SHELL_OBJ) $(COMMON_LIB) $(LIBFT) $(LIB) $(READLINE) -o $(NAME)
	@ echo "$(GREEN)[Success] $(BLUE)$(NAME)$(CLR_RMV) created ✔️"

################################################################################
#                              SYSTEM PROGRAMS                                 #
################################################################################

# Each system program is a standalone binary, linked against libcommon + libft.
$(BIN_DIR)/%: $(SRC_DIR)/system/%.c $(COMMON_LIB) $(LIBFT) | check-dependencies
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(FLAGS) $(INC) $< $(COMMON_LIB) $(LIBFT) -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(BIN_DIR)/logo.txt: $(SRC_DIR)/system/logo.txt
	@ mkdir -p $(BIN_DIR)
	@ cp $< $@

system-programs: $(SYS_BINS) $(BIN_DIR)/logo.txt
	@ echo "\n$(GREEN)[Success] $(BLUE)system programs$(CLR_RMV) created ✔️"

################################################################################
#                                    TESTS                                     #
################################################################################

TESTS_DIR		:= tests
UNIT_DIR		:= $(TESTS_DIR)/unit
INTEGRATION_DIR	:= $(TESTS_DIR)/integration
UNITY_DIR		:= $(TESTS_DIR)/unity
UNIT_BIN_DIR	:= $(UNIT_DIR)/bin

UNIT_SOURCES	:= $(wildcard $(UNIT_DIR)/test_*.c)
UNIT_BINS		:= $(UNIT_SOURCES:$(UNIT_DIR)/%.c=$(UNIT_BIN_DIR)/%)

# Shell objects minus main(), so each test can provide its own entry point.
TEST_SHELL_OBJ	:= $(filter-out $(OBJ_DIR)/shell/00_main.o, $(SHELL_OBJ))
TEST_CFLAGS		:= $(INC) -I$(UNITY_DIR) -Wall -Wextra -DUNIT_TEST $(FSAN)

$(UNIT_BIN_DIR)/test_%: $(UNIT_DIR)/test_%.c $(UNITY_DIR)/unity.c \
                        $(TEST_SHELL_OBJ) $(COMMON_LIB) | $(LIBFT)
	@ mkdir -p $(UNIT_BIN_DIR)
	@ printf "$(YELLOW)$<$(CLR_RMV)... "
	@ $(CC) $(TEST_CFLAGS) $< $(UNITY_DIR)/unity.c $(TEST_SHELL_OBJ) \
		$(shell find $(SRC_DIR)/system -name "*$*.c" 2>/dev/null) \
		$(COMMON_LIB) $(LIB) $(READLINE) \
		-o $@

TEST_RUNNER := ./scripts/run_tests.sh

unit: $(NAME) $(UNIT_BINS)
	@ echo "$(CYAN)==> Running unit tests$(CLR_RMV)"
	@ $(TEST_RUNNER) unit $(UNIT_BINS)

integration: all
	@ echo "$(CYAN)==> Running integration tests$(CLR_RMV)"
	@ $(TEST_RUNNER) integration $(INTEGRATION_DIR)/*.sh

test: unit integration

################################################################################
#                                  AI TESTS                                    #
################################################################################

ai-unit-tests ai-builtin-tests:
	@ if [ -z "$(MODULE)" ]; then \
		echo "Usage: make $@ MODULE=name"; exit 1; fi
	@ MACMINI_AGENT_CMD='claude -p --allowedTools ""' \
		bash ./scripts/gen_$(if $(filter ai-builtin-tests,$@),builtin,unit)_tests.sh \
		$(MODULE) > $(UNIT_DIR)/test_$(MODULE).c
	@ echo "$(GREEN)Generated$(CLR_RMV) $(UNIT_DIR)/test_$(MODULE).c"

################################################################################
#                                   CLEANUP                                    #
################################################################################

clean:
	@ $(RM) $(OBJ_DIR) $(UNIT_BIN_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) objs ✔️"

fclean: clean
	@ $(RM) $(NAME) $(SYS_BINS) $(BIN_DIR)
	@ $(MAKE) fclean -C $(LIBFT_DIR)
	@ echo "$(RED)Deleting $(BLUE)$(NAME)$(CLR_RMV) binary ✔️"

reset: fclean
	@ echo "$(RED)Deleting $(BLUE)./tmp$(CLR_RMV) dir ✔️"
	@ $(RM) ./tmp
	@ echo "$(RED)Deleting $(BLUE)./archive$(CLR_RMV) dir ✔️"
	@ $(RM) ./archive
	@ echo "$(RED)Resetting project... All generated files deleted ✔️"

re: fclean all

################################################################################
#                              PHONY && PRECIOUS                               #
################################################################################

.PHONY:		all run system-programs unit integration test \
			ai-unit-tests ai-builtin-tests clean fclean re \
			reset check-dependencies

.PRECIOUS:	$(OBJ_DIR)/%.o
