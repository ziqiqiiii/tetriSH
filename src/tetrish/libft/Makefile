# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: lchew <lchew@student.42.fr>                +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2022/05/14 17:13:24 by lchew             #+#    #+#              #
#    Updated: 2022/05/14 22:01:24 by lchew            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = libft.a

# MANDATORY
SRC_DIR = ./src/
SRC = $(addprefix ft_, $(addsuffix .c, \
		isalpha isdigit isalnum isascii isprint toupper tolower \
		memset memcpy memmove memchr memcmp atoi bzero calloc\
		strlen strlcpy strlcat strncmp strchr strrchr strnstr strdup \
		substr strjoin strtrim split itoa itoa_base strmapi striteri \
		putchar_fd putstr_fd putendl_fd putnbr_fd \
		lstnew lstadd_front lstsize lstlast lstadd_back lstdelone \
		lstclear lstiter lstmap array2d_len str_to_lower)) get_next_line.c
OBJ_DIR = ./obj/
OBJ = $(SRC:%.c=$(OBJ_DIR)%.o)

# BONUS
B_SRC = $(addprefix $(SRC_DIR)ft_, $(addsuffix .c, \
		lstnew lstadd_front lstsize lstlast lstadd_back lstdelone \
		lstclear lstiter lstmap))
B_OBJ = $(B_SRC:%.c=$(OBJ_DIR)%.o)

# HEADER
INC = ./includes
HEAD = -I$(INC)

# COMPILER
#-L ../libft -l ft<-- make the lib file dir able to be accessed anywhere
CC = gcc
CFLAGS = -Wall -Werror -Wextra $(HEAD)

# LIBRARY
LIBCR = ar -rcs

# REMOVE FILES OR DIRECTORIES
RM = rm -f
RMDIR = rmdir

# PRETTY
CLR_RMV		:= \033[0m
RED		    := \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
CYAN 		:= \033[1;36m

all: $(NAME)
	@:

$(NAME): $(OBJ_DIR) $(OBJ)
	@ echo "\n$(GREEN)[Success] $(CYAN)libft $(CLR_RMV)object files generated ✔️"
	@ @ echo "$(GREEN)Generating $(CYAN)libft $(CLR_RMV)Library..."
	@ $(LIBCR) $(NAME) $(OBJ)
	@ echo "$(GREEN)[Success] $(CYAN)libft $(CLR_RMV)Library created ✔️"

$(OBJ_DIR)%.o: $(SRC_DIR)%.c
	@ $(CC) $(CFLAGS) $(HEAD) -c $< -o $@
	@ printf "$(YELLOW)$<$(CLR_RMV)... "

$(OBJ_DIR):
	@ echo "$(GREEN)Creating $(CYAN)libft $(CLR_RMV)object directory..."
	@ mkdir -p $(OBJ_DIR)
	@ echo "$(GREEN)[Success] $(CYAN)libft $(CLR_RMV)object directory created ✔️"
	@ echo "$(GREEN)Generating $(CYAN)libft $(CLR_RMV)object files..."

bonus: $(B_OBJ) $(OBJ)
	$(LIBCR) $(NAME) $(B_OBJ) $(OBJ)
	
clean:
	@ $(RM) $(OBJ) ${B_OBJ}
	@ $(RMDIR) $(OBJ_DIR)
	@ echo "$(RED)Deleting $(CYAN)libft $(CLR_RMV)objs ✔️"
	@ echo "$(RED)Deleting $(CYAN)libft $(CLR_RMV)obj dir ✔️"

fclean: clean
	@ $(RM) $(NAME)
	@ echo "$(RED)Deleting $(CYAN)libft $(CLR_RMV)library ✔️"

re: fclean all

norme:
	norminette -R CheckForbiddenSourceHeader $(SRC) $(B_SRC) ./libft.h

.PHONY: all clean fclean re norme bonus
