#include "tetrisctl.h"

/*
** Static Variables
**
** The daemons tetrisctl knows how to manage, and the .tetrishrc key each one
** publishes its pidfile under. This much is compiled in on purpose: what
** must not be is *which* of them run and in what order, and that is
** TETRISCTL_DAEMONS's job (docs/adr/0007).
**
** The key names differ because each daemon keeps its own prefix's existing
** habit - tetrisd already had CERT_PATH and KEY_PATH, tetrislogd already had
** SOCK and FILE - and a daemon reading its own settings is the one place
** consistency actually matters.
**
** The fallbacks repeat each daemon's own compiled default, so tetrisctl
** answers the same way its target does when .tetrishrc says nothing. They are
** the one piece of duplication here; test_cfg pins them against the shipped
** start-up file so the two cannot drift apart quietly.
*/
static const struct s_known
{
	const char	*name;
	const char	*pid_key;
	const char	*pid_default;
}	g_known[] = {
	{"tetrislogd", "TETRISLOGD_PID", "tmp/tetrislogd/tetrislogd.pid"},
	{"tetrisd", "TETRISD_PID_PATH", "tmp/tetrisd/tetrisd.pid"},
	{NULL, NULL, NULL}
};

// Static Functions
static int			known_index(const char *key);
static int			name_index(const char *name);
static int			set_str(char *dst, size_t cap, const char *value);
static int			split_assignment(const char *line, char *key,
						size_t key_cap, char *value, size_t value_cap);
static const char	*skip_ws(const char *s);
static void			strip_quotes(char *value);
static int			apply_env(t_ctl *ctl);

/**
 * @brief Fills a roster with what tetrisctl manages when .tetrishrc is silent.
 *
 * @param ctl Roster to fill (ignored when NULL).
 */
void	cfg_defaults(t_ctl *ctl)
{
	if (ctl == NULL)
		return ;
	memset(ctl, 0, sizeof(*ctl));
	snprintf(ctl->order, TC_LINE_MAX, "%s", TC_DEF_DAEMONS);
	snprintf(ctl->rc_path, TC_PATH_MAX, "%s", "./" TC_RC_NAME);
	ctl->stop_ms = TC_STOP_MS;
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
int	cfg_set(t_ctl *ctl, const char *key, const char *value)
{
	int	i;

	if (ctl == NULL || key == NULL || value == NULL)
		return (-1);
	if (strcmp(key, TC_KEY_PREFIX "DAEMONS") == 0)
		return (set_str(ctl->order, TC_LINE_MAX, value));
	i = known_index(key);
	if (i < 0)
		return (-1);
	return (set_str(ctl->paths[i], TC_PATH_MAX, value));
}

/**
 * @brief Applies one .tetrishrc line, ignoring everything that is not ours.
 *
 * The file is a shell start-up script, so most lines are commands or another
 * component's settings: anything unrecognised is skipped rather than refused.
 * tetrisctl is stricter than the daemons in only one place, cfg_resolve,
 * where a roster naming a daemon it cannot manage is an error worth stopping
 * for rather than a line worth skipping.
 *
 * @param ctl Roster to update.
 * @param line One raw line, with or without its newline.
 * @return 0 when applied or deliberately ignored, -1 for a bad value.
 */
int	cfg_parse_line(t_ctl *ctl, const char *line)
{
	char	key[TC_LINE_MAX];
	char	value[TC_LINE_MAX];

	if (ctl == NULL || line == NULL)
		return (-1);
	if (split_assignment(line, key, sizeof(key), value, sizeof(value)) != 0)
		return (0);
	if (strcmp(key, TC_KEY_PREFIX "DAEMONS") != 0 && known_index(key) < 0)
		return (0);
	return (cfg_set(ctl, key, value));
}

/**
 * @brief Turns the raw roster line into the list of daemons to act on.
 *
 * Kept separate from parsing because a start-up file may name the daemons
 * before or after it names their pidfiles, and neither ordering should change
 * the answer. Everything is resolved once, here, when the whole file has been
 * read.
 *
 * @param ctl Roster holding an order line and any pidfile paths read.
 * @return 0 on success, -1 when the roster names a daemon tetrisctl does not
 * manage or lists more than it can hold.
 */
int	cfg_resolve(t_ctl *ctl)
{
	char	work[TC_LINE_MAX];
	char	*token;
	int		i;

	if (ctl == NULL)
		return (-1);
	ctl->count = 0;
	snprintf(work, TC_LINE_MAX, "%s", ctl->order);
	token = strtok(work, " \t");
	while (token != NULL)
	{
		i = name_index(token);
		if (i < 0 || ctl->count >= TC_MAX_DAEMONS)
			return (-1);
		snprintf(ctl->daemons[ctl->count].name, TC_NAME_MAX, "%s", token);
		if (ctl->paths[i][0] != '\0')
			snprintf(ctl->daemons[ctl->count].pid_path, TC_PATH_MAX, "%s",
				ctl->paths[i]);
		else
			snprintf(ctl->daemons[ctl->count].pid_path, TC_PATH_MAX, "%s",
				g_known[i].pid_default);
		ctl->count++;
		token = strtok(NULL, " \t");
	}
	return (0);
}

/**
 * @brief Loads the whole roster: defaults, then the rc file, then env.
 *
 * A missing rc file is not an error - the defaults name both daemons in the
 * right order. The environment is applied last so an exported override beats
 * the file, which is the same precedence both daemons use.
 *
 * @param ctl Roster to fill.
 * @param rc_override Path from argv, or NULL to resolve the usual way.
 * @return 0 on success, -1 when a setting or the roster was invalid.
 */
int	cfg_load(t_ctl *ctl, const char *rc_override)
{
	char	line[TC_LINE_MAX];
	FILE	*f;
	int		rc;

	if (ctl == NULL)
		return (-1);
	cfg_defaults(ctl);
	if (cfg_resolve_rc(rc_override, ctl->rc_path, TC_PATH_MAX) != 0)
		return (-1);
	rc = 0;
	f = fopen(ctl->rc_path, "r");
	if (f != NULL)
	{
		while (fgets(line, sizeof(line), f) != NULL)
		{
			if (cfg_parse_line(ctl, line) != 0)
				rc = -1;
		}
		fclose(f);
	}
	if (apply_env(ctl) != 0)
		rc = -1;
	if (cfg_resolve(ctl) != 0)
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
	return (set_str(out, cap, "./" TC_RC_NAME));
}

/**
 * @brief Looks a resolved daemon up by name.
 *
 * @param ctl Resolved roster.
 * @param name Daemon name to find.
 * @return The daemon, or NULL when the roster does not list it.
 */
const t_daemon	*ctl_find(const t_ctl *ctl, const char *name)
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
	value = getenv(TC_KEY_PREFIX "DAEMONS");
	if (value != NULL && value[0] != '\0'
		&& cfg_set(ctl, TC_KEY_PREFIX "DAEMONS", value) != 0)
		rc = -1;
	i = 0;
	while (g_known[i].name != NULL)
	{
		value = getenv(g_known[i].pid_key);
		if (value != NULL && value[0] != '\0'
			&& cfg_set(ctl, g_known[i].pid_key, value) != 0)
			rc = -1;
		i++;
	}
	return (rc);
}
