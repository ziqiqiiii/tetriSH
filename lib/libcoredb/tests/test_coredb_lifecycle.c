#include "coredb.h"

#include <stdlib.h>
#include <sys/stat.h>
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

static int	test_open_creates_db_file(void)
{
	const char	*path;
	t_coredb	*db;
	struct stat	st;
	int			status;

	path = "/tmp/coredb_lifecycle_test.db";
	(void)unlink(path);
	db = NULL;
	status = coredb_open(&db, path);
	if (assert_true(status == EXIT_SUCCESS, "coredb_open creates file") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(db != NULL, "coredb_open returns file-backed handle") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(stat(path, &st) == 0 && S_ISREG(st.st_mode),
			"coredb_open leaves regular file on disk") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(coredb_close(db) == EXIT_SUCCESS,
			"coredb_close closes file-backed handle") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (assert_true(unlink(path) == 0, "temporary db file removed"));
}

static int	test_open_rejects_missing_parent(void)
{
	t_coredb	*db;

	db = NULL;
	return (assert_true(coredb_open(&db,
				"/tmp/coredb_missing_parent_for_test/db") == EXIT_FAILURE
			&& db == NULL, "coredb_open rejects missing parent"));
}

static int	test_null_arguments_fail(void)
{
	t_coredb	*db;

	db = NULL;
	if (assert_true(coredb_open(NULL, "/tmp/coredb_null_test.db") == EXIT_FAILURE,
			"coredb_open rejects null output pointer") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(coredb_open(&db, NULL) == EXIT_FAILURE && db == NULL,
			"coredb_open rejects null path") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (assert_true(coredb_close(NULL) == EXIT_FAILURE,
			"coredb_close rejects null handle"));
}

int	main(void)
{
	if (test_open_creates_db_file() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_open_rejects_missing_parent() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_null_arguments_fail() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
