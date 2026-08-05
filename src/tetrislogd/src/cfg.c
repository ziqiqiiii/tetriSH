#include "tetrislogd.h"

// Static Functions
static int			set_str(char *dst, size_t cap, const char *value);
static int			split_assignment(const char *line, char *key, size_t key_cap, char *value, size_t value_cap);
static const char	*skip_ws(const char *s);
static void			strip_quotes(char *value);
static int			apply_env(t_cfg *cfg);

/**
 * @brief Fills a configuration with the values the logger boots with unset.
 *
 * Both defaults are relative paths, so a fresh clone runs from the repository
 * root without editing anything; .tetrishrc then overrides what a deployment
 * needs.
 *
 * @param cfg Configuration to fill (ignored when NULL).
 */
void	cfg_defaults(t_cfg *cfg)
{
	if (cfg == NULL)
		return ;
	memset(cfg, 0, sizeof(*cfg));
	snprintf(cfg->sock_path, TL_PATH_MAX, "%s", TL_DEF_SOCK);
	snprintf(cfg->file_path, TL_PATH_MAX, "%s", TL_DEF_FILE);
	snprintf(cfg->pid_path, TL_PATH_MAX, "%s", TL_DEF_PID);
	snprintf(cfg->err_path, TL_PATH_MAX, "%s", TL_DEF_ERR);
	snprintf(cfg->rc_path, TL_PATH_MAX, "%s", "./" TL_RC_NAME);
}

/**
 * @brief Applies one TETRISLOGD_* setting to a configuration.
 *
 * An unknown key is reported rather than ignored, so a setting documented in
 * .tetrishrc but never wired up here fails the boot instead of silently
 * doing nothing. The prefix check is what keeps two daemons sharing one
 * start-up file from reading each other's settings.
 *
 * @param cfg Configuration to update.
 * @param key Full setting name, including the TETRISLOGD_ prefix.
 * @param value Value text, already unquoted.
 * @return 0 when applied, -1 for an unknown key or an invalid value.
 */
int	cfg_set(t_cfg *cfg, const char *key, const char *value)
{
	if (cfg == NULL || key == NULL || value == NULL)
		return (-1);
	if (strncmp(key, TL_KEY_PREFIX, strlen(TL_KEY_PREFIX)) != 0)
		return (-1);
	key += strlen(TL_KEY_PREFIX);
	if (strcmp(key, "SOCK") == 0)
		return (set_str(cfg->sock_path, TL_PATH_MAX, value));
	if (strcmp(key, "FILE") == 0)
		return (set_str(cfg->file_path, TL_PATH_MAX, value));
	if (strcmp(key, "PID") == 0)
		return (set_str(cfg->pid_path, TL_PATH_MAX, value));
	if (strcmp(key, "ERR") == 0)
		return (set_str(cfg->err_path, TL_PATH_MAX, value));
	return (-1);
}

/**
 * @brief Applies one .tetrishrc line, ignoring everything that is not ours.
 *
 * The file is a shell start-up script: most lines are commands (dspawn, PATH)
 * or another daemon's settings, so only TETRISLOGD_ assignments are read and
 * every other line is skipped rather than refused.
 *
 * @param cfg Configuration to update.
 * @param line One raw line, with or without its newline.
 * @return 0 when applied or deliberately ignored, -1 for a bad value.
 */
int	cfg_parse_line(t_cfg *cfg, const char *line)
{
	char	key[TL_LINE_MAX];
	char	value[TL_LINE_MAX];

	if (cfg == NULL || line == NULL)
		return (-1);
	if (split_assignment(line, key, sizeof(key), value, sizeof(value)) != 0)
		return (0);
	if (strncmp(key, TL_KEY_PREFIX, strlen(TL_KEY_PREFIX)) != 0)
		return (0);
	return (cfg_set(cfg, key, value));
}

/**
 * @brief Loads the whole configuration: defaults, then the rc file, then env.
 *
 * A missing rc file is not an error - the defaults are a working setup. The
 * environment is applied last because dspawn exports these names before
 * exec'ing the daemon, which makes an inherited value the more specific one.
 *
 * @param cfg Configuration to fill.
 * @param rc_override Path from argv, or NULL to resolve the usual way.
 * @return 0 on success, -1 when a setting carried an invalid value.
 */
int	cfg_load(t_cfg *cfg, const char *rc_override)
{
	char	line[TL_LINE_MAX];
	FILE	*f;
	int		rc;

	if (cfg == NULL)
		return (-1);
	cfg_defaults(cfg);
	if (cfg_resolve_rc(rc_override, cfg->rc_path, TL_PATH_MAX) != 0)
		return (-1);
	rc = 0;
	f = fopen(cfg->rc_path, "r");
	if (f != NULL)
	{
		while (fgets(line, sizeof(line), f) != NULL)
		{
			if (cfg_parse_line(cfg, line) != 0)
				rc = -1;
		}
		fclose(f);
	}
	if (apply_env(cfg) != 0)
		rc = -1;
	return (rc);
}

/**
 * @brief Resolves which start-up file the logger reads its settings from.
 *
 * An explicit command-line path wins, then $TETRISHRC, then the working
 * directory's .tetrishrc - the same order tetrisd uses, so both daemons boot
 * from one file.
 *
 * @param override Path from argv, or NULL/empty when none was given.
 * @param out Buffer receiving the resolved path.
 * @param cap Size of out.
 * @return 0 on success, -1 on invalid arguments or an overlong path.
 */
int	cfg_resolve_rc(const char *override, char *out, size_t cap)
{
	const char	*env;

	if (out == NULL || cap == 0)
		return (-1);
	if (override != NULL && override[0] != '\0')
		return (set_str(out, cap, override));
	env = getenv("TETRISHRC");
	if (env != NULL && env[0] != '\0')
		return (set_str(out, cap, env));
	return (set_str(out, cap, "./" TL_RC_NAME));
}

/**
 * @brief Creates a directory and every missing parent above it.
 *
 * The sink and the socket both live under directories a fresh checkout does
 * not have, and `make reset` deletes again.
 *
 * @param path Directory path to create.
 * @return 0 on success, -1 with errno set on failure.
 */
int	cfg_mkdir_p(const char *path)
{
	char	buf[TL_PATH_MAX];
	size_t	i;

	if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof(buf))
		return (-1);
	snprintf(buf, sizeof(buf), "%s", path);
	i = 1;
	while (buf[i] != '\0')
	{
		if (buf[i] == '/')
		{
			buf[i] = '\0';
			if (mkdir(buf, TL_DIR_MODE) != 0 && errno != EEXIST)
				return (-1);
			buf[i] = '/';
		}
		i++;
	}
	if (mkdir(buf, TL_DIR_MODE) != 0 && errno != EEXIST)
		return (-1);
	return (0);
}

/**
 * @brief Creates the directory a file is about to be opened in.
 *
 * Both things the daemon opens - the sink and the socket - live under a
 * directory a fresh checkout does not have, and they ask the same question of
 * their path, so they ask it in one place.
 *
 * @param path Path to a file, not to a directory.
 * @return 0 on success or when there is no parent to make, -1 on failure.
 */
int	cfg_mkdir_parent(const char *path)
{
	char	dir[TL_PATH_MAX];
	char	*slash;

	if (path == NULL || strlen(path) >= sizeof(dir))
		return (-1);
	snprintf(dir, sizeof(dir), "%s", path);
	slash = strrchr(dir, '/');
	if (slash == NULL || slash == dir)
		return (0);
	*slash = '\0';
	return (cfg_mkdir_p(dir));
}

/**
 * @brief Copies a non-empty value into a fixed-size configuration field.
 *
 * @param dst Destination buffer.
 * @param cap Size of dst.
 * @param value Text to copy.
 * @return 0 on success, -1 when empty or too long to store whole.
 */
static int	set_str(char *dst, size_t cap, const char *value)
{
	size_t	len;

	len = strlen(value);
	if (len == 0 || len >= cap)
		return (-1);
	memcpy(dst, value, len + 1);
	return (0);
}

/**
 * @brief Splits an rc line into KEY and VALUE, tolerating `export` and quotes.
 *
 * @param line Raw line, newline optional.
 * @param key Buffer receiving the key.
 * @param key_cap Size of key.
 * @param value Buffer receiving the unquoted value.
 * @param value_cap Size of value.
 * @return 0 when the line is an assignment, -1 when it is anything else.
 */
static int	split_assignment(const char *line, char *key, size_t key_cap, char *value, size_t value_cap)
{
	const char	*eq;
	size_t		len;

	line = skip_ws(line);
	if (*line == '\0' || *line == '#' || *line == '\n')
		return (-1);
	if (strncmp(line, "export ", 7) == 0)
		line = skip_ws(line + 7);
	eq = strchr(line, '=');
	if (eq == NULL || eq == line)
		return (-1);
	len = (size_t)(eq - line);
	if (len >= key_cap)
		return (-1);
	memcpy(key, line, len);
	key[len] = '\0';
	len = 0;
	eq++;
	while (eq[len] != '\0' && eq[len] != '\n' && eq[len] != '\r'
		&& len + 1 < value_cap)
		len++;
	memcpy(value, eq, len);
	value[len] = '\0';
	strip_quotes(value);
	return (0);
}

/**
 * @brief Advances past leading spaces and tabs.
 *
 * @param s Text to skip into.
 * @return Pointer to the first non-blank character.
 */
static const char	*skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/**
 * @brief Removes one matching pair of surrounding quotes and trailing blanks.
 *
 * @param value Value text, modified in place.
 */
static void	strip_quotes(char *value)
{
	size_t	len;

	len = strlen(value);
	while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t'))
		value[--len] = '\0';
	if (len >= 2 && (value[0] == '"' || value[0] == '\'')
		&& value[len - 1] == value[0])
	{
		memmove(value, value + 1, len - 2);
		value[len - 2] = '\0';
	}
}

/**
 * @brief Overlays any TETRISLOGD_* environment variable onto the config.
 *
 * The shell exports these same names when it sources .tetrishrc, so a daemon
 * launched by dspawn sees them either way; the environment wins so a single
 * run can be redirected without touching the shared file.
 *
 * @param cfg Configuration to update.
 * @return 0 on success, -1 when an environment value was invalid.
 */
static int	apply_env(t_cfg *cfg)
{
	static const char	*names[] = {
		TL_KEY_PREFIX "SOCK", TL_KEY_PREFIX "FILE",
		TL_KEY_PREFIX "PID", TL_KEY_PREFIX "ERR", NULL
	};
	const char			*value;
	int					rc;
	size_t				i;

	rc = 0;
	i = 0;
	while (names[i] != NULL)
	{
		value = getenv(names[i]);
		if (value != NULL && value[0] != '\0'
			&& cfg_set(cfg, names[i], value) != 0)
			rc = -1;
		i++;
	}
	return (rc);
}
