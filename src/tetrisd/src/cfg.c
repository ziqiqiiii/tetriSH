#include "tetrisd.h"

// Static Functions
static int			set_str(char *dst, size_t cap, const char *value);
static int			set_int(int *dst, const char *value, int lo, int hi);
static int			set_level(int *dst, const char *value);
static int			split_assignment(const char *line, char *key, size_t key_cap, char *value, size_t value_cap);
static const char	*skip_ws(const char *s);
static void			strip_quotes(char *value);
static int			apply_env(t_cfg *cfg);

/**
 * @brief Fills a configuration with the values tetrisd boots with unset.
 *
 * Every default is a relative path or a plain number, so a fresh clone runs
 * from the repository root without editing anything; .tetrishrc then
 * overrides whatever the deployment needs.
 *
 * @param cfg Configuration to fill (ignored when NULL).
 */
void	cfg_defaults(t_cfg *cfg)
{
	if (cfg == NULL)
		return ;
	memset(cfg, 0, sizeof(*cfg));
	cfg->port = TD_DEF_PORT;
	snprintf(cfg->data_dir, TD_PATH_MAX, "%s", TD_DEF_DATA_DIR);
	snprintf(cfg->config_dir, TD_PATH_MAX, "%s", TD_DEF_CONFIG_DIR);
	snprintf(cfg->cert_path, TD_PATH_MAX, "%s", TD_DEF_CERT);
	snprintf(cfg->key_path, TD_PATH_MAX, "%s", TD_DEF_KEY);
	snprintf(cfg->ca_path, TD_PATH_MAX, "%s", TD_DEF_CA);
	snprintf(cfg->log_ipc, TD_PATH_MAX, "%s", TD_DEF_LOG_IPC);
	snprintf(cfg->pid_path, TD_PATH_MAX, "%s", TD_DEF_PID);
	snprintf(cfg->err_path, TD_PATH_MAX, "%s", TD_DEF_ERR);
	cfg->log_level = CIPC_LOG_INFO;
	cfg->max_clients = TD_DEF_MAX_CLIENTS;
	cfg->tick_ms = TD_DEF_TICK_MS;
	cfg->input_burst = TD_DEF_INPUT_BURST;
	cfg->input_rate = TD_DEF_INPUT_RATE;
	cfg->br_slots = TD_DEF_BR_SLOTS;
}

/**
 * @brief Resolves which start-up file tetrisd reads its settings from.
 *
 * An explicit command-line path wins, then $TETRISHRC, then the working
 * directory's .tetrishrc - the same order the shell itself uses, so both
 * read the same file.
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
	return (set_str(out, cap, "./" TD_RC_NAME));
}

/**
 * @brief Applies one TETRISD_* setting to a configuration.
 *
 * Values are range-checked here rather than at use, so a bad .tetrishrc is
 * refused at boot instead of surfacing as a strange runtime failure. An
 * unknown key is reported so the caller can decide whether to ignore it.
 *
 * @param cfg Configuration to update.
 * @param key Full setting name, including the TETRISD_ prefix.
 * @param value Value text, already unquoted.
 * @return 0 when applied, -1 for an unknown key or an invalid value.
 */
int	cfg_set(t_cfg *cfg, const char *key, const char *value)
{
	if (cfg == NULL || key == NULL || value == NULL)
		return (-1);
	if (strncmp(key, TD_KEY_PREFIX, strlen(TD_KEY_PREFIX)) != 0)
		return (-1);
	key += strlen(TD_KEY_PREFIX);
	if (strcmp(key, "PORT") == 0)
		return (set_int(&cfg->port, value, TD_MIN_PORT, TD_MAX_PORT));
	if (strcmp(key, "DATA_DIR") == 0)
		return (set_str(cfg->data_dir, TD_PATH_MAX, value));
	if (strcmp(key, "CONFIG_DIR") == 0)
		return (set_str(cfg->config_dir, TD_PATH_MAX, value));
	if (strcmp(key, "CERT_PATH") == 0)
		return (set_str(cfg->cert_path, TD_PATH_MAX, value));
	if (strcmp(key, "KEY_PATH") == 0)
		return (set_str(cfg->key_path, TD_PATH_MAX, value));
	if (strcmp(key, "CA_PATH") == 0)
		return (set_str(cfg->ca_path, TD_PATH_MAX, value));
	if (strcmp(key, "LOG_IPC") == 0)
		return (set_str(cfg->log_ipc, TD_PATH_MAX, value));
	if (strcmp(key, "PID_PATH") == 0)
		return (set_str(cfg->pid_path, TD_PATH_MAX, value));
	if (strcmp(key, "ERR_PATH") == 0)
		return (set_str(cfg->err_path, TD_PATH_MAX, value));
	if (strcmp(key, "LOG_LEVEL") == 0)
		return (set_level(&cfg->log_level, value));
	if (strcmp(key, "MAX_CLIENTS") == 0)
		return (set_int(&cfg->max_clients, value, 1, TD_MAX_CLIENT_CAP));
	if (strcmp(key, "TICK_MS") == 0)
		return (set_int(&cfg->tick_ms, value, TD_MIN_TICK_MS, TD_MAX_TICK_MS));
	if (strcmp(key, "INPUT_BURST") == 0)
		return (set_int(&cfg->input_burst, value, TD_MIN_INPUT_LIMIT,
				TD_MAX_INPUT_LIMIT));
	if (strcmp(key, "INPUT_RATE") == 0)
		return (set_int(&cfg->input_rate, value, TD_MIN_INPUT_LIMIT,
				TD_MAX_INPUT_LIMIT));
	if (strcmp(key, "BR_SLOTS") == 0)
		return (set_int(&cfg->br_slots, value, 2, ROOM_MAX_SLOTS));
	return (-1);
}

/**
 * @brief Applies one .tetrishrc line, ignoring everything that is not ours.
 *
 * The file is a shell start-up script: most lines are commands (dspawn, PATH)
 * that mean nothing here, so only `export TETRISD_*=value` and bare
 * `TETRISD_*=value` assignments are read and every other line is skipped.
 *
 * @param cfg Configuration to update.
 * @param line One raw line, with or without its newline.
 * @return 0 when applied or deliberately ignored, -1 for a bad value.
 */
int	cfg_parse_line(t_cfg *cfg, const char *line)
{
	char	key[TD_LINE_MAX];
	char	value[TD_LINE_MAX];

	if (cfg == NULL || line == NULL)
		return (-1);
	if (split_assignment(line, key, sizeof(key), value, sizeof(value)) != 0)
		return (0);
	if (strncmp(key, TD_KEY_PREFIX, strlen(TD_KEY_PREFIX)) != 0)
		return (0);
	return (cfg_set(cfg, key, value));
}

/**
 * @brief Loads the whole configuration: defaults, then the rc file, then env.
 *
 * A missing rc file is not an error - the defaults are a working setup. The
 * environment is applied last so a one-off run (or a test) can override a
 * setting without editing the shared file.
 *
 * @param cfg Configuration to fill.
 * @param override Path from argv, or NULL to resolve the usual way.
 * @return 0 on success, -1 when a setting carried an invalid value.
 */
int	cfg_load(t_cfg *cfg, const char *override)
{
	char	line[TD_LINE_MAX];
	FILE	*f;
	int		rc;

	if (cfg == NULL)
		return (-1);
	cfg_defaults(cfg);
	if (cfg_resolve_rc(override, cfg->rc_path, TD_PATH_MAX) != 0)
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
 * @brief Checks the settings tetrisd cannot start without.
 *
 * The certificate and private key are mandatory: without them the secure
 * session cannot authenticate the server, and booting anyway would mean
 * serving players unauthenticated.
 *
 * @param cfg Configuration to check.
 * @return 0 when usable, -1 when a required file is missing or unreadable.
 */
int	cfg_validate(const t_cfg *cfg)
{
	if (cfg == NULL)
		return (-1);
	if (cfg->cert_path[0] == '\0' || access(cfg->cert_path, R_OK) != 0)
		return (-1);
	if (cfg->key_path[0] == '\0' || access(cfg->key_path, R_OK) != 0)
		return (-1);
	if (cfg->data_dir[0] == '\0' || cfg->config_dir[0] == '\0')
		return (-1);
	return (0);
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
 * @brief Parses a decimal integer setting and range-checks it.
 *
 * @param dst Destination, left untouched on failure.
 * @param value Text to parse; must be a whole decimal number.
 * @param lo Lowest accepted value.
 * @param hi Highest accepted value.
 * @return 0 on success, -1 on trailing junk or an out-of-range value.
 */
static int	set_int(int *dst, const char *value, int lo, int hi)
{
	char	*end;
	long	n;

	if (value[0] == '\0')
		return (-1);
	errno = 0;
	n = strtol(value, &end, 10);
	if (errno != 0 || *end != '\0' || n < (long)lo || n > (long)hi)
		return (-1);
	*dst = (int)n;
	return (0);
}

/**
 * @brief Parses a log level name through the shared libcoreipc mapping.
 *
 * @param dst Destination level, left untouched on failure.
 * @param value Level name (debug, info, warning, error).
 * @return 0 on success, -1 for an unknown name.
 */
static int	set_level(int *dst, const char *value)
{
	int	level;

	level = lr_level_parse(value);
	if (level < 0)
		return (-1);
	*dst = level;
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
 * @brief Splits an rc line into KEY and VALUE, tolerating `export` and quotes.
 *
 * @param line Raw line, newline optional.
 * @param key Buffer receiving the key.
 * @param key_cap Size of key.
 * @param value Buffer receiving the unquoted value.
 * @param value_cap Size of value.
 * @return 0 when the line is an assignment, -1 when it is anything else.
 */
static int	split_assignment(const char *line, char *key, size_t key_cap,
			char *value, size_t value_cap)
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
 * @brief Overlays any TETRISD_* environment variables onto the configuration.
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
		"TETRISD_PORT", "TETRISD_DATA_DIR",
		"TETRISD_CONFIG_DIR", "TETRISD_CERT_PATH", "TETRISD_KEY_PATH",
		"TETRISD_CA_PATH", "TETRISD_LOG_IPC", "TETRISD_LOG_LEVEL",
		"TETRISD_PID_PATH", "TETRISD_ERR_PATH",
		"TETRISD_MAX_CLIENTS", "TETRISD_TICK_MS", "TETRISD_BR_SLOTS", NULL
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
