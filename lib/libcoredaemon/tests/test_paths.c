/* ************************************************************************** */
/*                                                                            */
/*   test_paths.c - making the directories a pidfile or error file needs      */
/*                                                                            */
/*   Both daemons are configured with paths under tmp/, which `make reset`    */
/*   deletes wholesale. Creating the parents rather than requiring them is    */
/*   what lets a daemon boot into a directory tree that is not there yet.     */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_mkdir_p_builds_the_whole_chain(void);
static void	test_mkdir_p_accepts_a_directory_that_exists(void);
static void	test_mkdir_parent_ignores_the_leaf(void);
static void	test_mkdir_p_fails_through_a_regular_file(void);
static void	test_mkdir_rejects_bad_arguments(void);

static bool	is_dir(const char *path);

int	main(void)
{
	test_mkdir_p_builds_the_whole_chain();
	test_mkdir_p_accepts_a_directory_that_exists();
	test_mkdir_parent_ignores_the_leaf();
	test_mkdir_p_fails_through_a_regular_file();
	test_mkdir_rejects_bad_arguments();
	return (0);
}

static void	test_mkdir_p_builds_the_whole_chain(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(path, sizeof(path), "%s/a/b/c", dir);
	assert(cd_mkdir_p(path) == 0);
	assert(is_dir(path) == true);
	fx_rmtree(dir);
	printf("PASS test_mkdir_p_builds_the_whole_chain\n");
}

static void	test_mkdir_p_accepts_a_directory_that_exists(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(path, sizeof(path), "%s/a/b", dir);
	assert(cd_mkdir_p(path) == 0);
	assert(cd_mkdir_p(path) == 0);
	fx_rmtree(dir);
	printf("PASS test_mkdir_p_accepts_a_directory_that_exists\n");
}

static void	test_mkdir_parent_ignores_the_leaf(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	char	parent[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(path, sizeof(path), "%s/run/state/x.pid", dir);
	snprintf(parent, sizeof(parent), "%s/run/state", dir);
	assert(cd_mkdir_parent(path) == 0);
	assert(is_dir(parent) == true);
	assert(is_dir(path) == false);
	assert(cd_mkdir_parent("bare.pid") == 0);
	fx_rmtree(dir);
	printf("PASS test_mkdir_parent_ignores_the_leaf\n");
}

static void	test_mkdir_p_fails_through_a_regular_file(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	char	wall[FX_PATH_MAX];
	int		fd;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(wall, sizeof(wall), "%s/wall", dir);
	fd = open(wall, O_WRONLY | O_CREAT | O_TRUNC, CD_FILE_MODE);
	assert(fd >= 0);
	close(fd);
	snprintf(path, sizeof(path), "%s/wall/below", dir);
	assert(cd_mkdir_p(path) == -1);
	fx_rmtree(dir);
	printf("PASS test_mkdir_p_fails_through_a_regular_file\n");
}

static void	test_mkdir_rejects_bad_arguments(void)
{
	assert(cd_mkdir_p(NULL) == -1);
	assert(cd_mkdir_p("") == -1);
	assert(cd_mkdir_parent(NULL) == -1);
	printf("PASS test_mkdir_rejects_bad_arguments\n");
}

static bool	is_dir(const char *path)
{
	struct stat	st;

	if (stat(path, &st) != 0)
		return (false);
	return (S_ISDIR(st.st_mode));
}
