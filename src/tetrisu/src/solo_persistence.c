#include "tetrisu.h"

#define SOLO_STATE_PATH_MAX	4096
#define SOLO_STATE_HEADER	"tetrisu-state-v1\n"
#define SOLO_STATE_FILE		"solo-best-v1"

// Static Functions
static bool	state_directory(char *path, size_t size);
static bool	state_file_path(char *path, size_t size);
static bool	ensure_directory_tree(const char *path);
static bool	write_all(int fd, const char *data, size_t length);
static bool	parse_score_line(const char *line, uint64_t *score);

/**
 * @brief Loads the version-one offline Solo best score.
 *
 * Missing, truncated, corrupt, or inaccessible state is treated as an empty
 *   profile so storage can never prevent the game from starting.
 *
 * @return Persisted best score, or zero when no valid state is available.
 */
uint64_t	solo_best_load(void)
{
	char		path[SOLO_STATE_PATH_MAX];
	char		header[64];
	char		score_line[96];
	uint64_t	score;
	FILE		*file;

	if (!state_file_path(path, sizeof(path)))
		return (0);
	file = fopen(path, "r");
	if (file == NULL)
		return (0);
	score = 0;
	if (fgets(header, sizeof(header), file) == NULL
		|| strcmp(header, SOLO_STATE_HEADER) != 0
		|| fgets(score_line, sizeof(score_line), file) == NULL
		|| !parse_score_line(score_line, &score))
		score = 0;
	(void)fclose(file);
	return (score);
}

/**
 * @brief Atomically replaces the version-one offline Solo best score.
 *
 * The temporary file is created beside the destination, flushed, and renamed
 *   over it. Callers intentionally ignore failure so play always continues.
 *
 * @param score Completed top-out score to persist.
 * @return true when the durable file was replaced, otherwise false.
 */
bool	solo_best_store(uint64_t score)
{
	char	directory[SOLO_STATE_PATH_MAX];
	char	path[SOLO_STATE_PATH_MAX];
	char	temporary[SOLO_STATE_PATH_MAX];
	char	content[128];
	int		fd;
	int		length;
	bool	ok;

	if (!state_directory(directory, sizeof(directory))
		|| !state_file_path(path, sizeof(path))
		|| !ensure_directory_tree(directory))
		return (false);
	if (snprintf(temporary, sizeof(temporary), "%s/.solo-best.XXXXXX",
			directory) >= (int)sizeof(temporary))
		return (false);
	fd = mkstemp(temporary);
	if (fd < 0)
		return (false);
	length = snprintf(content, sizeof(content), "%ssolo_best=%" PRIu64 "\n",
			SOLO_STATE_HEADER, score);
	ok = length > 0 && length < (int)sizeof(content)
		&& write_all(fd, content, (size_t)length)
		&& fsync(fd) == 0;
	if (close(fd) != 0)
		ok = false;
	if (ok && rename(temporary, path) != 0)
		ok = false;
	if (!ok)
		(void)unlink(temporary);
	return (ok);
}

/**
 * @brief Resolves the XDG state directory with the standard HOME fallback.
 */
static bool	state_directory(char *path, size_t size)
{
	const char	*base;
	const char	*home;
	int			length;

	base = getenv("XDG_STATE_HOME");
	if (base != NULL && base[0] == '/')
		length = snprintf(path, size, "%s/tetrisu", base);
	else
	{
		home = getenv("HOME");
		if (home == NULL || home[0] == '\0')
			return (false);
		length = snprintf(path, size, "%s/.local/state/tetrisu", home);
	}
	return (length > 0 && (size_t)length < size);
}

/**
 * @brief Resolves the full versioned score-file path.
 */
static bool	state_file_path(char *path, size_t size)
{
	char	directory[SOLO_STATE_PATH_MAX];
	int		length;

	if (!state_directory(directory, sizeof(directory)))
		return (false);
	length = snprintf(path, size, "%s/%s", directory, SOLO_STATE_FILE);
	return (length > 0 && (size_t)length < size);
}

/**
 * @brief Creates every missing directory component with private permissions.
 */
static bool	ensure_directory_tree(const char *path)
{
	char	copy[SOLO_STATE_PATH_MAX];
	char	*cursor;

	if (strlen(path) >= sizeof(copy))
		return (false);
	strcpy(copy, path);
	cursor = copy + 1;
	while (*cursor != '\0')
	{
		if (*cursor == '/')
		{
			*cursor = '\0';
			if (mkdir(copy, 0700) < 0 && errno != EEXIST)
				return (false);
			*cursor = '/';
		}
		cursor++;
	}
	return (mkdir(copy, 0700) == 0 || errno == EEXIST);
}

/**
 * @brief Writes a complete buffer while handling interrupted system calls.
 */
static bool	write_all(int fd, const char *data, size_t length)
{
	ssize_t	written;

	while (length > 0)
	{
		written = write(fd, data, length);
		if (written < 0 && errno == EINTR)
			continue ;
		if (written <= 0)
			return (false);
		data += written;
		length -= (size_t)written;
	}
	return (true);
}

/**
 * @brief Strictly parses the sole score record in a version-one state file.
 */
static bool	parse_score_line(const char *line, uint64_t *score)
{
	const char			*digits;
	char				*end;
	unsigned long long	value;

	if (strncmp(line, "solo_best=", 10) != 0)
		return (false);
	digits = line + 10;
	if (!isdigit((unsigned char)*digits))
		return (false);
	errno = 0;
	value = strtoull(digits, &end, 10);
	if (errno != 0 || end == digits)
		return (false);
	if (*end == '\n')
		end++;
	if (*end != '\0')
		return (false);
	*score = (uint64_t)value;
	return (true);
}
