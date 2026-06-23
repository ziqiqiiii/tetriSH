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

static int	test_user_snapshot_keeps_equipped_theme(void)
{
	t_coredb_user	user;

	user = (t_coredb_user){0};
	user.equipped_theme = 7;
	return (assert_true(user.equipped_theme == 7,
			"user snapshot exposes equipped theme"));
}

int	main(void)
{
	if (test_user_snapshot_keeps_equipped_theme() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
