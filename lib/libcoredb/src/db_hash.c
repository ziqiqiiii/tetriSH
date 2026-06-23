#include "coredb_internal.h"

#include <stdlib.h>
#include <string.h>

static uint32_t	coredb_hash_string(const char *s)
{
	uint32_t	hash;

	hash = 2166136261u;
	while (*s != '\0')
	{
		hash ^= (uint8_t)*s;
		hash *= 16777619u;
		s++;
	}
	return (hash);
}

static uint32_t	coredb_hash_u32(uint32_t key)
{
	return (key * 2654435761u);
}

static int	coredb_hash_table_resize(t_coredb_hash_table *table)
{
	size_t				new_count;
	t_coredb_hash_node	**new_by_username;
	t_coredb_hash_node	**new_by_player_id;
	t_coredb_hash_node	*node;
	t_coredb_hash_node	*next;
	size_t				i;
	size_t				idx;

	new_count = table->bucket_count * 2;
	new_by_username = calloc(new_count, sizeof(*new_by_username));
	new_by_player_id = calloc(new_count, sizeof(*new_by_player_id));
	if (new_by_username == NULL || new_by_player_id == NULL)
	{
		free(new_by_username);
		free(new_by_player_id);
		return (EXIT_FAILURE);
	}
	i = 0;
	while (i < table->bucket_count)
	{
		node = table->buckets_by_username[i];
		while (node != NULL)
		{
			next = node->next_by_username;
			idx = coredb_hash_string(node->user.username) % new_count;
			node->next_by_username = new_by_username[idx];
			new_by_username[idx] = node;
			node = next;
		}
		i++;
	}
	i = 0;
	while (i < table->bucket_count)
	{
		node = table->buckets_by_player_id[i];
		while (node != NULL)
		{
			next = node->next_by_player_id;
			idx = coredb_hash_u32(node->user.player_id) % new_count;
			node->next_by_player_id = new_by_player_id[idx];
			new_by_player_id[idx] = node;
			node = next;
		}
		i++;
	}
	free(table->buckets_by_username);
	free(table->buckets_by_player_id);
	table->buckets_by_username = new_by_username;
	table->buckets_by_player_id = new_by_player_id;
	table->bucket_count = new_count;
	return (EXIT_SUCCESS);
}

int	coredb_hash_table_init(t_coredb_hash_table **out_table)
{
	t_coredb_hash_table	*table;

	if (out_table == NULL)
		return (EXIT_FAILURE);
	*out_table = NULL;
	table = calloc(1, sizeof(*table));
	if (table == NULL)
		return (EXIT_FAILURE);
	table->bucket_count = COREDB_HASH_INITIAL_BUCKETS;
	table->buckets_by_username = calloc(table->bucket_count,
			sizeof(*table->buckets_by_username));
	table->buckets_by_player_id = calloc(table->bucket_count,
			sizeof(*table->buckets_by_player_id));
	if (table->buckets_by_username == NULL
		|| table->buckets_by_player_id == NULL)
	{
		free(table->buckets_by_username);
		free(table->buckets_by_player_id);
		free(table);
		return (EXIT_FAILURE);
	}
	*out_table = table;
	return (EXIT_SUCCESS);
}

void	coredb_hash_table_destroy(t_coredb_hash_table *table)
{
	t_coredb_hash_node	*node;
	t_coredb_hash_node	*next;
	size_t				i;

	if (table == NULL)
		return ;
	i = 0;
	while (i < table->bucket_count)
	{
		node = table->buckets_by_username[i];
		while (node != NULL)
		{
			next = node->next_by_username;
			free(node);
			node = next;
		}
		i++;
	}
	free(table->buckets_by_username);
	free(table->buckets_by_player_id);
	free(table);
}

/* 75% load factor threshold, checked before the insert that would push
 * user_count past it (matches plan: (user_count + 1) * 4 >= bucket_count * 3). */
int	coredb_hash_insert(t_coredb_hash_table *table,
		const t_coredb_user *user, t_coredb_user **out_user)
{
	t_coredb_hash_node *node;
	size_t idx;

	if (table == NULL || user == NULL)
		return (EXIT_FAILURE);
	if ((table->user_count + 1) * 4 >= table->bucket_count * 3
		&& coredb_hash_table_resize(table) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	node = malloc(sizeof(*node));
	if (node == NULL)
		return (EXIT_FAILURE);
	node->user = *user;
	idx = coredb_hash_string(node->user.username) % table->bucket_count;
	node->next_by_username = table->buckets_by_username[idx];
	table->buckets_by_username[idx] = node;
	idx = coredb_hash_u32(node->user.player_id) % table->bucket_count;
	node->next_by_player_id = table->buckets_by_player_id[idx];
	table->buckets_by_player_id[idx] = node;
	table->user_count++;
	if (out_user != NULL)
		*out_user = &node->user;
	return (EXIT_SUCCESS);
}

t_coredb_user	*coredb_hash_find_by_username(t_coredb_hash_table *table,
		const char *username)
{
	t_coredb_hash_node *node;
	size_t idx;

	if (table == NULL || username == NULL)
		return (NULL);
	idx = coredb_hash_string(username) % table->bucket_count;
	node = table->buckets_by_username[idx];
	while (node != NULL)
	{
		if (strcmp(node->user.username, username) == 0)
			return (&node->user);
		node = node->next_by_username;
	}
	return (NULL);
}

t_coredb_user	*coredb_hash_find_by_player_id(t_coredb_hash_table *table,
		uint32_t player_id)
{
	t_coredb_hash_node *node;
	size_t idx;

	if (table == NULL)
		return (NULL);
	idx = coredb_hash_u32(player_id) % table->bucket_count;
	node = table->buckets_by_player_id[idx];
	while (node != NULL)
	{
		if (node->user.player_id == player_id)
			return (&node->user);
		node = node->next_by_player_id;
	}
	return (NULL);
}

size_t	coredb_hash_user_count(const t_coredb_hash_table *table)
{
	if (table == NULL)
		return (0);
	return (table->user_count);
}

size_t	coredb_hash_bucket_count(const t_coredb_hash_table *table)
{
	if (table == NULL)
		return (0);
	return (table->bucket_count);
}