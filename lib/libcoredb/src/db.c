#include "coredb_internal.h"

#include <stdlib.h>

static size_t	coredb_strlen(const char *s)
{
	size_t	len;

	len = 0;
	while (s[len] != '\0')
		len++;
	return (len);
}

static void	coredb_copy_bytes(char *dst, const char *src, size_t len)
{
	size_t	i;

	i = 0;
	while (i < len)
	{
		dst[i] = src[i];
		i++;
	}
}

static char	*coredb_strdup(const char *path)
{
	char	*copy;
	size_t	len;

	len = coredb_strlen(path) + 1;
	copy = malloc(len);
	if (copy == NULL)
		return (NULL);
	coredb_copy_bytes(copy, path, len);
	return (copy);
}

static void	coredb_free_partial(t_coredb *db)
{
	if (db == NULL)
		return ;
	if (db->file != NULL)
		(void)fclose(db->file);
	free(db->path);
	free(db);
}

int	coredb_open(t_coredb **out_db, const char *path)
{
	t_coredb	*db;

	if (out_db == NULL || path == NULL)
		return (EXIT_FAILURE);
	*out_db = NULL;
	db = calloc(1, sizeof(*db));
	if (db == NULL)
		return (EXIT_FAILURE);
	db->path = coredb_strdup(path);
	if (db->path == NULL)
	{
		coredb_free_partial(db);
		return (EXIT_FAILURE);
	}
	db->file = fopen(path, "a+b");
	if (db->file == NULL)
	{
		coredb_free_partial(db);
		return (EXIT_FAILURE);
	}
	if (pthread_mutex_init(&db->mutex, NULL) != 0)
	{
		coredb_free_partial(db);
		return (EXIT_FAILURE);
	}
	db->next_player_id = 1;
	db->next_seq_no = 1;
	*out_db = db;
	return (EXIT_SUCCESS);
}

int	coredb_close(t_coredb *db)
{
	int	status;

	if (db == NULL)
		return (EXIT_FAILURE);
	status = EXIT_SUCCESS;
	if (db->file != NULL && fclose(db->file) != 0)
		status = EXIT_FAILURE;
	if (pthread_mutex_destroy(&db->mutex) != 0)
		status = EXIT_FAILURE;
	free(db->path);
	free(db);
	return (status);
}
