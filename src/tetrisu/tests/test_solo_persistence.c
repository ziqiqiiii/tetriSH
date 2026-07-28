#include "tetrisu.h"
#include <sys/stat.h>

// Static Functions
static void	test_xdg_state_round_trip_and_corruption(void);
static void	test_home_fallback_and_missing_environment(void);
static void	write_corrupt_state(const char *path);
static void	cleanup_xdg_tree(const char *root);
static void	cleanup_home_tree(const char *root);

/**
 * @brief Runs isolated filesystem tests for the offline Solo best score.
 */
int	main(void)
{
	test_xdg_state_round_trip_and_corruption();
	test_home_fallback_and_missing_environment();
	return (0);
}

/**
 * @brief Verifies versioned XDG storage, replacement, and safe corruption.
 */
static void	test_xdg_state_round_trip_and_corruption(void)
{
	char	root[] = "/tmp/tetrisu-xdg-state-XXXXXX";
	char	path[512];
	struct stat	info;

	assert(mkdtemp(root) != NULL);
	assert(setenv("XDG_STATE_HOME", root, 1) == 0);
	assert(solo_best_load() == 0);
	assert(solo_best_store(UINT64_C(123456789)));
	assert(solo_best_load() == UINT64_C(123456789));
	snprintf(path, sizeof(path), "%s/tetrisu/solo-best-v1", root);
	assert(stat(path, &info) == 0);
	assert((info.st_mode & 0777) == 0600);
	assert(solo_best_store(UINT64_C(987654321)));
	assert(solo_best_load() == UINT64_C(987654321));
	write_corrupt_state(path);
	assert(solo_best_load() == 0);
	cleanup_xdg_tree(root);
	printf("PASS test_xdg_state_round_trip_and_corruption\n");
}

/**
 * @brief Verifies the standard HOME fallback and failure-safe empty profile.
 */
static void	test_home_fallback_and_missing_environment(void)
{
	char	root[] = "/tmp/tetrisu-home-state-XXXXXX";

	assert(mkdtemp(root) != NULL);
	assert(setenv("XDG_STATE_HOME", "relative-path-is-invalid", 1) == 0);
	assert(setenv("HOME", root, 1) == 0);
	assert(solo_best_store(UINT64_C(24680)));
	assert(solo_best_load() == UINT64_C(24680));
	cleanup_home_tree(root);
	assert(unsetenv("XDG_STATE_HOME") == 0);
	assert(unsetenv("HOME") == 0);
	assert(solo_best_load() == 0);
	assert(!solo_best_store(UINT64_C(1)));
	printf("PASS test_home_fallback_and_missing_environment\n");
}

/**
 * @brief Replaces a valid state file with malformed versioned content.
 */
static void	write_corrupt_state(const char *path)
{
	FILE	*file;

	file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs("tetrisu-state-v1\nsolo_best=-1\n", file) >= 0);
	assert(fclose(file) == 0);
}

/**
 * @brief Removes the shallow XDG fixture without recursive shell deletion.
 */
static void	cleanup_xdg_tree(const char *root)
{
	char	path[512];

	snprintf(path, sizeof(path), "%s/tetrisu/solo-best-v1", root);
	assert(unlink(path) == 0);
	snprintf(path, sizeof(path), "%s/tetrisu", root);
	assert(rmdir(path) == 0);
	assert(rmdir(root) == 0);
}

/**
 * @brief Removes the standard HOME fallback fixture from the leaves upward.
 */
static void	cleanup_home_tree(const char *root)
{
	char	path[512];

	snprintf(path, sizeof(path), "%s/.local/state/tetrisu/solo-best-v1", root);
	assert(unlink(path) == 0);
	snprintf(path, sizeof(path), "%s/.local/state/tetrisu", root);
	assert(rmdir(path) == 0);
	snprintf(path, sizeof(path), "%s/.local/state", root);
	assert(rmdir(path) == 0);
	snprintf(path, sizeof(path), "%s/.local", root);
	assert(rmdir(path) == 0);
	assert(rmdir(root) == 0);
}
