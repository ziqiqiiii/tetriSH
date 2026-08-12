/* ************************************************************************** */
/*                                                                            */
/*   test_bot_proc.c - finding tetrisu-bot, which is not one question         */
/*                                                                            */
/*   Every other bots test sets TETRISU_BOT_BIN, because a test binary does   */
/*   not live where the client does. That is exactly why this file exists:    */
/*   the override was the only path under test, and the path a real player    */
/*   takes - the running executable, and argv[0] behind it - was not tested   */
/*   at all. It was also wrong, and silently, on macOS, which has no /proc    */
/*   and is a supported client here.                                          */
/*                                                                            */
/*   No process is spawned by any of this: resolution answers a question      */
/*   about the filesystem, so the fixtures are empty executable files.        */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu_bot.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Shorter than BOT_PATH_MAX so that a path composed inside one provably fits */
# define TEST_DIR_MAX	256

// Static Functions
static void	test_the_override_is_asked_first(void);
static void	test_a_named_binary_that_cannot_run_is_refused(void);
static void	test_the_running_executable_is_where_it_looks(const char *argv0);
static void	test_argv0_answers_when_the_kernel_does_not(void);
static void	test_a_bare_argv0_is_looked_up_on_path(void);

static int	make_tree(char *dir, size_t cap, bool with_bot);
static void	drop_tree(const char *dir);
static void	touch_exec(const char *dir, const char *name);
static void	check(const char *name, int ok);

static int	g_failures = 0;

int	main(int argc, char **argv)
{
	(void)argc;
	test_the_override_is_asked_first();
	test_a_named_binary_that_cannot_run_is_refused();
	test_the_running_executable_is_where_it_looks(argv[0]);
	test_argv0_answers_when_the_kernel_does_not();
	test_a_bare_argv0_is_looked_up_on_path();
	printf("%s\n", g_failures == 0 ? "all bot_proc tests passed"
		: "bot_proc tests failed");
	return (g_failures != 0);
}

/*
** TETRISU_BOT_BIN beats everything else, which is what lets the integration
** suites run a client out of tests/bin.
*/
static void	test_the_override_is_asked_first(void)
{
	char	dir[TEST_DIR_MAX];
	char	named[BOT_PATH_MAX];
	char	found[BOT_PATH_MAX];

	if (make_tree(dir, sizeof(dir), true) != 0)
	{
		check("the override is asked first", 0);
		return ;
	}
	snprintf(named, sizeof(named), "%s/%s", dir, BOT_BINARY_NAME);
	setenv("TETRISU_BOT_BIN", named, 1);
	check("the override is asked first",
		bot_farm_binary(found, sizeof(found)) == 0
		&& strcmp(found, named) == 0);
	unsetenv("TETRISU_BOT_BIN");
	drop_tree(dir);
}

/*
** A named binary that is not there is refused rather than fallen back from.
** Somebody who set the variable meant that file, and quietly running a
** different one is worse than saying no.
*/
static void	test_a_named_binary_that_cannot_run_is_refused(void)
{
	char	found[BOT_PATH_MAX];

	setenv("TETRISU_BOT_BIN", "/nonexistent/tetrisu-bot", 1);
	check("a named binary that cannot run is refused",
		bot_farm_binary(found, sizeof(found)) != 0);
	unsetenv("TETRISU_BOT_BIN");
}

/*
** The path a player is on: no override, and the bot beside the running
** executable. This test binary is the running executable, so the fixture is
** a copy of the bot's *name* beside it - which is what the client's own
** bin/ looks like.
*/
static void	test_the_running_executable_is_where_it_looks(const char *argv0)
{
	char	found[BOT_PATH_MAX];
	char	self[BOT_PATH_MAX];
	char	*slash;

	unsetenv("TETRISU_BOT_BIN");
	bot_farm_remember_self("");
	snprintf(self, sizeof(self), "%s", argv0);
	slash = strrchr(self, '/');
	if (slash == NULL)
	{
		check("the running executable is where it looks", 0);
		return ;
	}
	*slash = '\0';
	touch_exec(self, BOT_BINARY_NAME);
	check("the running executable is where it looks",
		bot_farm_binary(found, sizeof(found)) == 0
		&& strstr(found, BOT_BINARY_NAME) != NULL);
	snprintf(self + strlen(self), sizeof(self) - strlen(self), "/%s",
		BOT_BINARY_NAME);
	unlink(self);
}

/*
** What macOS needed. With no override and nothing beside the running
** executable, argv[0] is the answer - and on a platform where the kernel has
** no answer at all it is the only one.
*/
static void	test_argv0_answers_when_the_kernel_does_not(void)
{
	char	dir[TEST_DIR_MAX];
	char	invoked[BOT_PATH_MAX];
	char	found[BOT_PATH_MAX];

	unsetenv("TETRISU_BOT_BIN");
	if (make_tree(dir, sizeof(dir), true) != 0)
	{
		check("argv0 answers when the kernel does not", 0);
		return ;
	}
	touch_exec(dir, "tetrisu");
	snprintf(invoked, sizeof(invoked), "%s/tetrisu", dir);
	bot_farm_remember_self(invoked);
	check("argv0 answers when the kernel does not",
		bot_farm_binary(found, sizeof(found)) == 0
		&& strncmp(found, dir, strlen(dir)) == 0);
	bot_farm_remember_self("");
	drop_tree(dir);
}

/*
** A client started by bare name - which is how tetrish starts it, with ./bin
** on PATH - has an argv[0] with no directory in it, so PATH is walked the way
** execvp would walk it.
*/
static void	test_a_bare_argv0_is_looked_up_on_path(void)
{
	char		dir[TEST_DIR_MAX];
	char		found[BOT_PATH_MAX];
	const char	*previous;
	char		kept[BOT_PATH_MAX];

	unsetenv("TETRISU_BOT_BIN");
	if (make_tree(dir, sizeof(dir), true) != 0)
	{
		check("a bare argv0 is looked up on PATH", 0);
		return ;
	}
	bot_farm_remember_self("tetrisu");
	previous = getenv("PATH");
	snprintf(kept, sizeof(kept), "%s", previous == NULL ? "" : previous);
	setenv("PATH", dir, 1);
	check("a bare argv0 is looked up on PATH",
		bot_farm_binary(found, sizeof(found)) == 0
		&& strncmp(found, dir, strlen(dir)) == 0);
	setenv("PATH", kept, 1);
	bot_farm_remember_self("");
	drop_tree(dir);
}

/**
 * @brief Makes a throwaway directory, optionally with a bot binary in it.
 *
 * @param dir Buffer receiving the directory's path.
 * @param cap Size of dir.
 * @param with_bot Whether to put an executable named tetrisu-bot inside.
 * @return 0 on success, -1 otherwise.
 */
static int	make_tree(char *dir, size_t cap, bool with_bot)
{
	snprintf(dir, cap, "/tmp/tetrisu-bot-test-%d", (int)getpid());
	if (mkdir(dir, 0755) != 0 && errno != EEXIST)
		return (-1);
	if (with_bot)
		touch_exec(dir, BOT_BINARY_NAME);
	return (0);
}

/**
 * @brief Removes a directory made by make_tree, and whatever it holds.
 *
 * @param dir The directory.
 */
static void	drop_tree(const char *dir)
{
	char	path[BOT_PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", dir, BOT_BINARY_NAME);
	unlink(path);
	snprintf(path, sizeof(path), "%s/tetrisu", dir);
	unlink(path);
	rmdir(dir);
}

/**
 * @brief Creates an empty executable file.
 *
 * Nothing runs it - resolution only ever asks access(X_OK).
 *
 * @param dir Where to put it.
 * @param name What to call it.
 */
static void	touch_exec(const char *dir, const char *name)
{
	char	path[BOT_PATH_MAX];
	FILE	*file;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	file = fopen(path, "w");
	if (file != NULL)
		fclose(file);
	chmod(path, 0755);
}

/**
 * @brief Prints one check's verdict and counts the failures.
 *
 * @param name What was checked.
 * @param ok Whether it held.
 */
static void	check(const char *name, int ok)
{
	if (ok)
		printf("PASS: %s\n", name);
	else
	{
		printf("FAIL: %s\n", name);
		g_failures++;
	}
}
