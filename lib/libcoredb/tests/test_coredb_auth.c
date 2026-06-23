#include "coredb.h"

#include <stdlib.h>
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

static int	test_open_close_creates_handle(void)
{
	const char	*path;
	t_coredb	*db;
	int			status;

	path = "/tmp/coredb_auth_test.db";
	(void)unlink(path);
	db = NULL;
	status = coredb_open(&db, path);
	if (assert_true(status == EXIT_SUCCESS, "coredb_open returns success") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(db != NULL, "coredb_open stores handle") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(coredb_close(db) == EXIT_SUCCESS,
			"coredb_close returns success") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (assert_true(unlink(path) == 0, "temporary auth db file removed"));
}

int	main(void)
{
	if (test_open_close_creates_handle() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
