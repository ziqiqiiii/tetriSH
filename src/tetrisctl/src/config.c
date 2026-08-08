#include "tetrisctl.h"

/*
** Static Variables
**
** The daemons this build knows how to manage, and the .tetrishrc key each one
** publishes its pidfile under. This is the only thing compiled in, and it is
** knowledge about the programs rather than about a deployment of them: which
** of them run, in what order, and where their pidfiles actually live are all
** read from the start-up file (docs/adr/0007).
**
** No default path appears here on purpose. Repeating each daemon's own
** compiled default would put the same path in a third place and let the two
** drift apart silently, which is the failure this table exists to avoid.
*/
static const t_known_daemon	g_known[] = {
	{"tetrislogd", "TETRISLOGD_PID_PATH"},
	{"tetrisd", "TETRISD_PID_PATH"},
	{NULL, NULL}
};

// Static Functions
static int			known_index(const char *key);
static int			name_index(const char *name);
static int			add_daemon(t_ctl *ctl, const char *name);
static int			set_str(char *dst, size_t cap, const char *value);
static int			split_assignment(const char *line, char *key, size_t key_cap, char *value, size_t value_cap);
static const char	*skip_ws(const char *s);
static void			strip_quotes(char *value);
static int			apply_env(t_ctl *ctl);

/**
 * @brief Empties a roster, ready for a start-up file to fill it.
 *
 * There is deliberately no default set of daemons and no default pidfile. A
 * roster that was never declared is an error rather than a guess, because the
 * launch order this program exists to take from a file would otherwise be
 * sitting in this one instead.
 *
 * @param ctl Roster to blank (ignored when NULL).
 */
void	config_defaults(t_ctl *ctl)
{
	if (ctl == NULL)
		return ;
	memset(ctl, 0, sizeof(*ctl));
	snprintf(ctl->rc_path, TETRISCTL_FILESYSTEM_PATH_MAX, "%s", "./" TETRISCTL_RC_FILENAME);
	ctl->stop_ms = TETRISCTL_STOP_MS;
}

/**
 * @brief Applies one setting tetrisctl cares about.
 *
 * Two kinds of key land here, and they come from different prefixes on
 * purpose: TETRISCTL_DAEMONS is tetrisctl's own, while the pidfile keys
 * belong to the daemons and are read rather than owned. That is the whole
 * reason this program parses a file it does not otherwise use - both ends
 * have to name the same pidfile, and naming it twice is how they would drift.
 *
 * @param ctl Roster to update.
 * @param key Full setting name.
 * @param value Value text, already unquoted.
 * @return 0 when applied, -1 for a key that is not ours or a bad value.
 */
int	config_set(t_ctl *ctl, const char *key, const char *value)
{
	int	i;

	if (ctl == NULL || key == NULL || value == NULL)
		return (-1);
	if (strcmp(key, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS") == 0)
		return (set_str(ctl->order, TETRISCTL_CONFIG_LINE_MAX, value));
	i = known_index(key);
	if (i < 0)
		return (-1);
	return (set_str(ctl->paths[i], TETRISCTL_FILESYSTEM_PATH_MAX, value));
}

/**
 * @brief Applies one .tetrishrc line, ignoring everything that is not ours.
 *
 * The file is a shell start-up script, so most lines are commands or another
 * component's settings: anything unrecognised is skipped rather than refused.
 * tetrisctl is stricter than the daemons in only one place, config_resolve,
 * where a roster naming a daemon it cannot manage is an error worth stopping
 * for rather than a line worth skipping.
 *
 * @param ctl Roster to update.
 * @param line One raw line, with or without its newline.
 * @return 0 when applied or deliberately ignored, -1 for a bad value.
 */
int	config_parse_line(t_ctl *ctl, const char *line)
{
	char	key[TETRISCTL_CONFIG_LINE_MAX];
	char	value[TETRISCTL_CONFIG_LINE_MAX];

	if (ctl == NULL || line == NULL)
		return (-1);
	if (split_assignment(line, key, sizeof(key), value, sizeof(value)) != 0)
		return (0);
	if (strcmp(key, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS") != 0 && known_index(key) < 0)
		return (0);
	return (config_set(ctl, key, value));
}

/**
 * @brief Turns the raw roster line into the list of daemons to act on.
 *
 * Kept separate from parsing because a start-up file may name the daemons
 * before or after it names their pidfiles, and neither ordering should change
 * the answer. Everything is resolved once, here, when the whole file has been
 * read.
 *
 * Every failure here is loud. An unknown *key* in the start-up file is
 * skipped, because the file is a shell script full of lines that are none of
 * this program's business - but a daemon named with no pidfile to find it by,
 * or a name nothing manages, would produce a stack that is half up under a
 * zero exit code.
 *
 * @param ctl Roster holding an order line and any pidfile paths read.
 * @return 0 on success, -1 after reporting which daemon could not be resolved.
 */
int	config_resolve(t_ctl *ctl)
{
	char	work[TETRISCTL_CONFIG_LINE_MAX];
	char	*token;

	if (ctl == NULL)
		return (-1);
	ctl->count = 0;
	if (ctl->order[0] == '\0')
	{
		snprintf(work, TETRISCTL_CONFIG_LINE_MAX, "is not set in %s", ctl->rc_path);
		daemon_report_error(TETRISCTL_COMPONENT_NAME, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS", work);
		return (-1);
	}
	snprintf(work, TETRISCTL_CONFIG_LINE_MAX, "%s", ctl->order);
	token = strtok(work, " \t");
	while (token != NULL)
	{
		if (add_daemon(ctl, token) != 0)
			return (-1);
		token = strtok(NULL, " \t");
	}
	return (0);
}

/**
 * @brief Loads the whole roster: the rc file, then the environment.
 *
 * A missing or silent rc file *is* an error, unlike in either daemon, and the
 * asymmetry is deliberate: a daemon with no settings has working defaults to
 * fall back on, whereas a lifecycle manager with no roster has nothing to
 * manage and would exit 0 having done nothing. The environment is applied
 * last so an exported override beats the file, which is the same precedence
 * both daemons use.
 *
 * @param ctl Roster to fill.
 * @param rc_override Path from argv, or NULL to resolve the usual way.
 * @return 0 on success, -1 when a setting or the roster was invalid.
 */
int	config_load(t_ctl *ctl, const char *rc_override)
{
	char	line[TETRISCTL_CONFIG_LINE_MAX];
	FILE	*f;
	int		rc;

	if (ctl == NULL)
		return (-1);
	config_defaults(ctl);
	if (config_resolve_rc_path(rc_override, ctl->rc_path, TETRISCTL_FILESYSTEM_PATH_MAX) != 0)
		return (-1);
	rc = 0;
	f = fopen(ctl->rc_path, "r");
	if (f != NULL)
	{
		while (fgets(line, sizeof(line), f) != NULL)
		{
			if (config_parse_line(ctl, line) != 0)
				rc = -1;
		}
		fclose(f);
	}
	if (apply_env(ctl) != 0)
		rc = -1;
	if (config_resolve(ctl) != 0)
		rc = -1;
	return (rc);
}

/**
 * @brief Resolves which start-up file tetrisctl reads its roster from.
 *
 * An explicit command-line path wins, then $TETRISHRC, then the working
 * directory's .tetrishrc - the same order both daemons use, so the file
 * tetrisctl reads the roster from is the file they read their settings from.
 *
 * @param override Path from argv, or NULL/empty when none was given.
 * @param out Buffer receiving the resolved path.
 * @param cap Size of out.
 * @return 0 on success, -1 on invalid arguments or an overlong path.
 */
int	config_resolve_rc_path(const char *override, char *out, size_t cap)
{
	const char	*env;

	if (out == NULL || cap == 0)
		return (-1);
	if (override != NULL && override[0] != '\0')
		return (set_str(out, cap, override));
	env = getenv("TETRISHRC");
	if (env != NULL && env[0] != '\0')
		return (set_str(out, cap, env));
	return (set_str(out, cap, "./" TETRISCTL_RC_FILENAME));
}

/**
 * @brief Looks a resolved daemon up by name.
 *
 * @param ctl Resolved roster.
 * @param name Daemon name to find.
 * @return The daemon, or NULL when the roster does not list it.
 */
const t_managed	*ctl_find_daemon(const t_ctl *ctl, const char *name)
{
	int	i;

	if (ctl == NULL || name == NULL)
		return (NULL);
	i = 0;
	while (i < ctl->count)
	{
		if (strcmp(ctl->daemons[i].name, name) == 0)
			return (&ctl->daemons[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Appends one named daemon to the roster, resolving its pidfile.
 *
 * @param ctl Roster to append to.
 * @param name Daemon name from the order line.
 * @return 0 on success, -1 after reporting why the name could not be used.
 */
static int	add_daemon(t_ctl *ctl, const char *name)
{
	char	reason[TETRISCTL_CONFIG_LINE_MAX];
	int		i;

	i = name_index(name);
	if (i < 0)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, name,
			"is not a daemon this build manages");
		return (-1);
	}
	if (ctl->count >= TETRISCTL_MAX_DAEMONS)
	{
		snprintf(reason, sizeof(reason), "more than %d daemons",
			TETRISCTL_MAX_DAEMONS);
		daemon_report_error(TETRISCTL_COMPONENT_NAME,
			TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS", reason);
		return (-1);
	}
	if (ctl->paths[i][0] == '\0')
	{
		snprintf(reason, sizeof(reason),
			"is not set in %s, so %s has no pidfile", ctl->rc_path, name);
		daemon_report_error(TETRISCTL_COMPONENT_NAME, g_known[i].pid_key,
			reason);
		return (-1);
	}
	snprintf(ctl->daemons[ctl->count].name, TETRISCTL_NAME_MAX, "%s", name);
	snprintf(ctl->daemons[ctl->count].pid_path, TETRISCTL_FILESYSTEM_PATH_MAX, "%s",
		ctl->paths[i]);
	ctl->count++;
	return (0);
}

/**
 * @brief Finds which managed daemon a pidfile key belongs to.
 *
 * @param key Full setting name.
 * @return Index into g_known, or -1 when the key is not a pidfile key.
 */
static int	known_index(const char *key)
{
	int	i;

	i = 0;
	while (g_known[i].name != NULL)
	{
		if (strcmp(key, g_known[i].pid_key) == 0)
			return (i);
		i++;
	}
	return (-1);
}

/**
 * @brief Finds a managed daemon by name.
 *
 * @param name Daemon name from the roster.
 * @return Index into g_known, or -1 when nothing manages that name.
 */
static int	name_index(const char *name)
{
	int	i;

	i = 0;
	while (g_known[i].name != NULL)
	{
		if (strcmp(name, g_known[i].name) == 0)
			return (i);
		i++;
	}
	return (-1);
}

/**
 * @brief Copies a non-empty value into a fixed-size field.
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
static int	split_assignment(const char *line, char *key, size_t key_cap,
	char *value, size_t value_cap)
{
	const char	*eq;
	size_t		len;

	line = skip_ws(line);
	if (*line == '\0' || *line == '#' || *line == '\n')
		return (-1);
	if (strncmp(line, "export", 6) == 0 && (line[6] == ' ' || line[6] == '\t'))
		line = skip_ws(line + 6);
	eq = strchr(line, '=');
	if (eq == NULL || eq == line)
		return (-1);
	len = (size_t)(eq - line);
	if (len >= key_cap)
		return (-1);
	memcpy(key, line, len);
	key[len] = '\0';
	len = strlen(eq + 1);
	while (len > 0 && (eq[len] == '\n' || eq[len] == '\r'))
		len--;
	if (len >= value_cap)
		return (-1);
	memcpy(value, eq + 1, len);
	value[len] = '\0';
	strip_quotes(value);
	return (0);
}

/**
 * @brief Skips leading spaces and tabs.
 *
 * @param s Text to advance through.
 * @return The first character that is not a space or tab.
 */
static const char	*skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/**
 * @brief Removes one matching pair of surrounding quotes, in place.
 *
 * The roster is the one value here that usually carries them, because it
 * holds spaces: TETRISCTL_DAEMONS="tetrislogd tetrisd" is how a shell would
 * be made to keep it as a single word.
 *
 * @param value Value text to unquote.
 */
static void	strip_quotes(char *value)
{
	size_t	len;

	len = strlen(value);
	if (len < 2)
		return ;
	if ((value[0] != '"' && value[0] != '\'') || value[len - 1] != value[0])
		return ;
	memmove(value, value + 1, len - 2);
	value[len - 2] = '\0';
}

/**
 * @brief Overlays any matching environment variables onto the roster.
 *
 * @param ctl Roster to update.
 * @return 0 on success, -1 when an exported value was invalid.
 */
static int	apply_env(t_ctl *ctl)
{
	const char	*value;
	int			rc;
	int			i;

	rc = 0;
	value = getenv(TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS");
	if (value != NULL && value[0] != '\0'
		&& config_set(ctl, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS", value) != 0)
		rc = -1;
	i = 0;
	while (g_known[i].name != NULL)
	{
		value = getenv(g_known[i].pid_key);
		if (value != NULL && value[0] != '\0'
			&& config_set(ctl, g_known[i].pid_key, value) != 0)
			rc = -1;
		i++;
	}
	return (rc);
}
