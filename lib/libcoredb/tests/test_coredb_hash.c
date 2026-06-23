#include "coredb.h"
#include "../src/coredb_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t	test_strlen(const char *s)
{
	size_t	len;

	len = 0;
	while (s[len] != '\0')
		len++;
	return (len);
}

static void	write_result(const char *prefix, const char *name)
{
	(void)write(STDOUT_FILENO, prefix, test_strlen(prefix));
	(void)write(STDOUT_FILENO, name, test_strlen(name));
	(void)write(STDOUT_FILENO, "\n", 1);
}

static int	assert_true(int condition, const char *name)
{
	if (condition)
	{
		write_result("PASS ", name);
		return (EXIT_SUCCESS);
	}
	write_result("FAIL ", name);
	return (EXIT_FAILURE);
}

static int	test_insert_then_find_both_paths(void)
{
	t_coredb_hash_table	*table;
	t_coredb_user		user;
	t_coredb_user		*inserted;
	t_coredb_user		*by_username;
	t_coredb_user		*by_player_id;
	int					ok;

	if (coredb_hash_table_init(&table) != EXIT_SUCCESS)
		return (assert_true(0, "hash table init succeeds"));
	memset(&user, 0, sizeof(user));
	user.player_id = 7;
	strcpy(user.username, "alice");
	user.points = 0;
	ok = assert_true(coredb_hash_insert(table, &user, &inserted) == EXIT_SUCCESS
		&& inserted != NULL, "insert succeeds and returns live pointer");
	by_username = coredb_hash_find_by_username(table, "alice");
	if (assert_true(by_username == inserted,
			"find_by_username returns the same live node") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	by_player_id = coredb_hash_find_by_player_id(table, 7);
	if (assert_true(by_player_id == inserted,
			"find_by_player_id returns the same live node") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(coredb_hash_find_by_username(table, "bob") == NULL,
			"find_by_username returns NULL for unknown user") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_hash_table_destroy(table);
	return (ok);
}

static int	test_find_returns_live_pointer_for_mutation(void)
{
	t_coredb_hash_table	*table;
	t_coredb_user		user;
	t_coredb_user		*live;
	int					ok;

	if (coredb_hash_table_init(&table) != EXIT_SUCCESS)
		return (assert_true(0, "hash table init succeeds for mutation test"));
	memset(&user, 0, sizeof(user));
	user.player_id = 1;
	strcpy(user.username, "carol");
	coredb_hash_insert(table, &user, NULL);
	live = coredb_hash_find_by_player_id(table, 1);
	live->points += 100;
	ok = assert_true(coredb_hash_find_by_player_id(table, 1)->points == 100,
		"mutating through a found pointer persists in the table");
	coredb_hash_table_destroy(table);
	return (ok);
}

static int	test_resize_keeps_every_user_findable(void)
{
	t_coredb_hash_table	*table;
	t_coredb_user		user;
	char				username[COREDB_USERNAME_MAX];
	size_t				n;
	size_t				i;
	int					ok;

	if (coredb_hash_table_init(&table) != EXIT_SUCCESS)
		return (assert_true(0, "hash table init succeeds for resize test"));
	n = 2000;
	i = 0;
	while (i < n)
	{
		memset(&user, 0, sizeof(user));
		user.player_id = (uint32_t)i + 1;
		snprintf(username, sizeof(username), "user%zu", i);
		strcpy(user.username, username);
		coredb_hash_insert(table, &user, NULL);
		i++;
	}
	ok = assert_true(coredb_hash_bucket_count(table) > COREDB_HASH_INITIAL_BUCKETS,
		"inserting many users triggers at least one resize");
	if (assert_true(coredb_hash_user_count(table) == n,
			"user_count matches number of inserts") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	i = 0;
	while (i < n)
	{
		snprintf(username, sizeof(username), "user%zu", i);
		if (coredb_hash_find_by_username(table, username) == NULL
			|| coredb_hash_find_by_player_id(table, (uint32_t)i + 1) == NULL)
		{
			ok = assert_true(0, "every user findable by both keys after resize");
			break ;
		}
		i++;
	}
	if (i == n)
		assert_true(1, "every user findable by both keys after resize");
	coredb_hash_table_destroy(table);
	return (ok);
}

int	main(void)
{
	if (test_insert_then_find_both_paths() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_find_returns_live_pointer_for_mutation() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_resize_keeps_every_user_findable() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
