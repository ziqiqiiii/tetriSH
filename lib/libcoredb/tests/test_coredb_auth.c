#include "coredb.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	write_result(const char *prefix, const char *name)
{
	(void)write(STDOUT_FILENO, prefix, strlen(prefix));
	(void)write(STDOUT_FILENO, name, strlen(name));
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

static int	test_open_close_creates_handle(void)
{
	t_coredb	*db;
	int			status;

	db = NULL;
	status = coredb_open(&db, ":memory:");
	if (assert_true(status == EXIT_SUCCESS, "coredb_open returns success") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(db != NULL, "coredb_open stores handle") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (assert_true(coredb_close(db) == EXIT_SUCCESS,
			"coredb_close returns success"));
}

int	main(void)
{
	if (test_open_close_creates_handle() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
