#ifndef MINISHELL_H
# define MINISHELL_H

# include <stdio.h>				/* printf */
# include <stdlib.h>			/* malloc, free, exit, getenv */
# include <unistd.h>			/* access, chdir, close, dup, dup2, fork,
									getcwd,isatty, open, pipe, read, unlink,
									write, execve, ttyname, ttyslot, opendir,
									readdir, closedir */
# include <readline/readline.h>	/* readline, rl_clear_history, rl_on_new_line,
									rl_replace_line, rl_redisplay,
									add_history */
# include <readline/history.h>
# include <sys/wait.h>			/* wait, waitpid, wait3, wait4 */
# include <signal.h>			/* signal, sigaction, sigemptyset, sigaddset,
									kill */
# include <sys/stat.h>			/* stat, lstat, fstat */
# include <termios.h>			/* tcsetattr, tcgetattr */
# include <errno.h>				/* perror, strerror */
# include <sys/ioctl.h>			/* ioctl */
# include <curses.h>			/* tgetent, tgetflag, tgetnum, tgetstr, tgoto,
									tputs */
# include <limits.h> 			/* PATH_MAX */
# include <string.h>
# include <fcntl.h>				/* open flags */
# include <libgen.h>			/* dirname */

# include "libft.h"
# include "get_next_line.h"
# include "common.h"

# define GREEN	"\033[1;32m"
# define BLUE	"\033[1;34m"
# define RESET	"\033[0m"

# define EXIT	"exit"
# define EXIT_MISUSE 2		/* Misused of shell built-ins */
# define EXIT_NO_INVOKE 126	/* Command invoked cannot execute */
# define EXIT_NO_CMD 127	/* Command not found */
# define EXIT_BAD_EXIT 128	/* Invalid exit argument */
# define EXIT_CTRL_C 130	/* Ctrl-C */
# define EXIT_SIGNAL 137	/* Signal */
# define EXIT_RANGE 255		/* Exit Status out of range */

# define PIPE_OP	"|"
# define RDIN_OP	"<"
# define HEREDOC_OP	"<<"
# define RDOUT_OP	">"
# define RDAPP_OP	">>"

# define SINGLE_QUOTE	'\''
# define DOUBLE_QUOTE	'\"'
# define SPACE			' '
# define EQUAL			'='
# define DOLLAR			'$'

extern int	g_exit_status;

typedef enum e_rc_line_type
{
	RC_LINE_EMPTY = 0,
	RC_LINE_PATH,
	RC_LINE_COMMAND
}	t_rc_line_type;

typedef struct s_history
{
	int					id;
	char				*cmd;
	struct s_history	*next;
	struct s_history	*prev;
}	t_history;

typedef enum e_token
{
	END,
	RDAPP,
	HEREDOC,
	RDIN,
	RDOUT,
	PIPE,
	COMMAND,
	NO_OF_TOKEN_TYPES
}	t_token;

typedef struct s_token_check
{
	char	*op;
	t_token	token;
}	t_token_check;

typedef struct s_tree
{
	t_token			token;
	char			*value;
	struct s_tree	*left;
	struct s_tree	*right;
}	t_tree;

typedef struct s_env
{
	char			*key;
	char			*value;
}	t_env;

typedef struct s_echo_var
{
	int	i;
	int	flag;
	int	flag2;
}	t_echo_var;

typedef struct s_expand_var
{
	char	*n_cmd;
	char	*substring;
	char	*dollar_ptr;
	char	*single_quote_ptr;
	char	*double_quote_ptr;
	char	*exit_status_str;
	char	*start;
	char	*key;
	char	*value;
	int		count;
	int		len;
}	t_expand_var;

typedef struct s_usage
{
    const char  *name;
    const char  *detail;
}   t_usage;

typedef struct s_help
{
    const char  *name;
    const char  *desc;
}   t_help;

typedef struct s_root
{
	t_history		*history;
	t_token_check	tkchk[NO_OF_TOKEN_TYPES];
	char			*tree_arg_value;
	int				stdin_tmp;
	int				stdout_tmp;
	t_list			*env_list;
	int				*pipe;
	struct termios	previous;
	struct termios	current;
	int				heredoc_flag;
	int				exit_cmd_flag;
	char			*current_dir;
}	t_root;


/* 01_INIT */

int					init_root(t_root *sh, char **envp);

/* 01_RC */

void				source_rc(t_root *sh, char **envp);
void				get_rc_paths(t_root *sh, char *rc_path, char *home_path);
void				run_rc(int fd, t_root *sh, char **envp);
void				create_empty_rc(const char *path);
t_rc_line_type	    classify_rc_line(const char *line, const char **value_out);
void				exec_rc_cmd(t_root *sh, char **envp, char *line);
void				set_path(t_root *sh, const char *value);

/* 01_BANNER */

void				print_banner(t_root *sh);


/* 02_PROMPT */

void				prompt(t_root *sh, char **envp);

/* 03_EXPAND */

char				*expand(char *cmd, t_list **env_list);
void				single_quote(t_expand_var *data);
void				join_dollar_ptr(t_expand_var *data, t_list **env_list);
void				replace_exit_status(t_expand_var *data);
char				*sub_or_join(char *cmd, char *start, int len, char *substring);
char				*key_check(char *input);

/* 04_LEXER */

t_list				*lexer(char *cmd);
int					count_token(char *cmd);
int					count_sp_char(char *cmd);
int					count_char(char *cmd);
void				cmd_modifier(char *cmd, char **tokens);

/* 05_PARSER */

t_tree				*parser(t_list *lexer, int n_token, t_root *sh);
t_tree				*tree_node_new(t_token type, char *value, \
							t_tree *left, t_tree *right);
void				print_tree(t_tree *root, int b);

/* 06_EXECUTE */

void				recurse_bst(t_tree *node, char **envp, t_root *sh);
char				**cmd_join(char **res, t_root *sh);
char				**find_path(t_list **env_list);
char				*join_path(t_list **env_list, char *cmd);
char				*get_exe_path(char *argv, t_list **env_list);

/* 07_PIPE */

void				pipe_handler(t_tree *node, char **envp, t_root *sh);

/* 08_REDIRECTION */

void				rdin_handler(t_tree *node, char **envp, t_root *sh);
void				rdout_handler(t_tree *node, char **envp, t_root *sh);
void				rdapp_handler(t_tree *node, char **envp, t_root *sh);
void				heredoc_handler(t_tree *node, char **envp, t_root *sh);
int					rdin_fd(char *node_value, t_root *sh);
int					rdout_fd(char *node_value, t_root *sh);
int					rdapp_fd(char *node_value, t_root *sh);
char				*find_file(char *value);
int					heredoc_fd(char *node_value, t_root *sh);

/* 09_BUILTIN */

int					builtin(char **cmd, t_root *sh);
int					echo_command(char **cmd);
int					cd(char **value, t_list **env_list);
int					pwd(void);
int					get_pwd(char cwd[256]);
int					export(char **cmd, t_list **env_list);
int					invalid_identifier(char *input);
int					unset(char **key, t_list **env_list);
int					get_env(t_list **env_list);
int					env_link_list(char **envp, t_list **env_list);
char				*existed_env(char *key, t_list **env_list);
void				creat_new_env_node(char *key, char	*input, t_list **env_list);
t_history			*history_node_new(int index, void *cmd);
void				history_clear(t_history **history);
void				history_add(t_history **history, char *cmd);
int					history_print(t_history *history);
int					exit_status(int status);
int					exit_command(char **cmd, t_root *sh);
void				exit_prompt(t_root *sh);
int					usage(char **cmd);
int					help(void);
int					set_env(char **cmd, t_list **env_list);
int					unset_env(char **cmd, t_list **env_list);

/* 10_QUOTE */

char				**cmd_quote_handler(char const *s, char c);
int					is_quote(char c);
int					quote_count(char *cmd);
char				*remove_quote(char *str);

/* 11_SIGNAL */

void				shell_ignore_signals(void);
void				child_restore_signals(void);
void				heredoc_restore_signals(void);
void				sigint_ignore(struct sigaction *old);
void				sigint_restore(struct sigaction *old);

/* 12_FREE */

void				del_data(void	*content);
void				reset_data(t_root *sh, t_list **cmd_lexer, t_tree **head);
void				free_2d(char **str);
void				free_tree(t_tree *node);

/* 13_MINISHELL_UTILS */

int					ft_tcgetattr(int fd, struct termios *termios_p);
int					ft_tcsetattr(int fd, int optional_actions, \
							struct termios *termios_p);

void				print_exec_cmd(char **cmd);

#endif
