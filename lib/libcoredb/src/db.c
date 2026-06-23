#include "coredb.h"

#include <stdlib.h>

struct s_coredb
{
	uint32_t placeholder;
};

int	coredb_open(t_coredb **out_db, const char *path)
{
	if (out_db == NULL || path == NULL)
		return (EXIT_FAILURE);
	*out_db = calloc(1, sizeof(**out_db));
	if (*out_db == NULL)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}

int	coredb_close(t_coredb *db)
{
	if (db == NULL)
		return (EXIT_FAILURE);
	free(db);
	return (EXIT_SUCCESS);
}
