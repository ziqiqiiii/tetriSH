#include "minishell.h"

/**
 * @brief Creates a new history node.
 *
 * Allocates and initializes a new history node with the given index and
 * command. The 'next' and 'prev' pointers are set to NULL.
 *
 * @param index The index for the new history node.
 * @param cmd The command for the new history node.
 * @return Pointer to the new history node, or NULL if allocation fails.
 */
t_history	*history_node_new(int index, void *cmd)
{
	t_history	*history;

	history = ft_calloc(1, sizeof(t_history));
	if (!history)
		return (NULL);
	history->id = index;
	history->cmd = ft_strdup(cmd);
	history->next = NULL;
	history->prev = NULL;
	return (history);
}

/**
 * @brief Frees all nodes in the history list and clears readline history.
 *
 * This function walks the doubly-linked list, freeing each node and its
 * command string, then resets the head pointer to NULL and calls
 * clear_history() to flush the readline library's internal history.
 *
 * @param history Pointer to the head of the doubly-linked history list.
 */
void	history_clear(t_history **history)
{
	t_history	*tmp;
	t_history	*current;

	if (!history)
		return ;
	current = *history;
	while (current)
	{
		tmp = current;
		current = current->next;
		free(tmp->cmd);
		free(tmp);
	}
	*history = NULL;
	rl_clear_history();
	free(rl_line_buffer);
	rl_line_buffer = NULL;
}

/**
 * @brief Appends a new command entry to the history list.
 *
 * If cmd is NULL the function returns immediately. Otherwise it increments
 * the internal index, creates a new node, and links it to the tail of the
 * list, updating the prev pointer of the new node accordingly.
 *
 * @param history Pointer to the head of the doubly-linked history list.
 * @param cmd The command string to record.
 */
void	history_add(t_history **history, char *cmd)
{
	t_history	*new;
	t_history	*tmp;
	static int	index;

	if (!cmd)
		return ;
	add_history(cmd);
	new = history_node_new(++index, cmd);
	if (!(*history))
	{
		*history = new;
	}
	else
	{
		tmp = *history;
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
		new->prev = tmp;
	}
}

/**
 * @brief Prints the history of commands.
 *
 * Iterates through a doubly-linked list of history nodes, printing the
 * index and command of each node.
 *
 * @param history A pointer to the head of the doubly-linked list of history 
 * 					nodes.
 * @return EXIT_SUCCESS, indicating successful execution.
 */
int	history_print(t_history *history)
{
	t_history	*tmp;

	tmp = history;
	while (tmp)
	{
		printf("%4d  %s\n", tmp->id, tmp->cmd);
		tmp = tmp->next;
	}
	return (EXIT_SUCCESS);
}
