/* ************************************************************************** */
/*                                                                            */
/*   bot_proc.c — the parent's side of a bot: spawn it, hold it, let it go     */
/*                                                                            */
/*   The only module that forks, and the only one that knows a bot is a       */
/*   process rather than a seat. Everything a bot does once it is running is  */
/*   in bot_main.c, over its own socket.                                      */
/*                                                                            */
/*   Kicking needs no protocol because of what is here: a bot is this         */
/*   client's own child, so `K` is a signal, its socket closes, and the       */
/*   server releases the seat through the disconnect path every dropped       */
/*   connection already takes. There is no KICK method, no authorisation      */
/*   question, and no way to kick a person - a client can only signal the     */
/*   processes it started.                                                    */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu_bot.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

# ifdef __APPLE__
#  include <mach-o/dyld.h>
# endif

// Static Functions
static int	note(const char *what, const char *detail);
static int	log_open(void);
static int	usable(const char *path);
static int	beside(const char *anchor, char *out, size_t cap);
static int	self_path(char *out, size_t cap);
static int	from_path_env(char *out, size_t cap);
static int	spawn_bot(const char *binary, const char *room, t_bot_level level,
				int deadman, int report);
static int	open_report(int *report);
static void	child_exec(const char *binary, const char *room,
				t_bot_level level, int deadman, int report);
static int	server_args(char **args, char *port_text);
static void	child_stdio(void);
static int	reap(pid_t pid, int deadman);

/*
** How this process was invoked, for the last of the three ways the bot binary
** is looked for. Static because argv is main's and this is asked for from a
** key handler four screens away from it, and because there is exactly one
** running program to remember.
*/
static char	g_invoked_as[BOT_PATH_MAX];

/*
** The server this client actually reached, which is not the same question as
** what its environment says.
**
** A player types the address into SERVER ID on the sign-in screen, and that
** value lives in the client's own t_net_config and nowhere else. A forked
** child inherits the environment, so it inherited the *default* - 127.0.0.1 -
** and looked for a server on the player's own laptop. Four bots would spawn,
** four would fail to connect, and the room stayed empty while the screen said
** BOT ADDED.
*/
static char	g_server_host[BOT_HOST_MAX];
static char	g_server_ca[BOT_PATH_MAX];
static int	g_server_port;

/**
 * @brief Start an empty farm.
 *
 * @param farm The farm to initialise.
 */
void	bot_farm_init(t_bot_farm *farm)
{
	if (farm == NULL)
		return ;
	memset(farm, 0, sizeof(*farm));
}

/**
 * @brief Remembers argv[0], which is the fallback the kernel cannot supply.
 *
 * Called from main before anything else. It is only ever read when the two
 * better answers have failed, so a client that never calls this loses nothing
 * on the platforms where self_path works.
 *
 * @param argv0 The path this process was invoked as, or NULL.
 */
void	bot_farm_remember_self(const char *argv0)
{
	if (argv0 == NULL)
		return ;
	snprintf(g_invoked_as, sizeof(g_invoked_as), "%s", argv0);
}

/**
 * @brief Remembers which server this client reached, for the bots to be told.
 *
 * Called from net_connect rather than from the screens, so that it cannot be
 * forgotten: every path that opens a session goes through there, including
 * the reconnect the provider does, and a bot spawned after a reconnect to a
 * different address would otherwise be sent to the old one.
 *
 * @param host The host the session was opened to.
 * @param port Its port.
 * @param ca_path The certificate authority it was verified against.
 */
void	bot_farm_remember_server(const char *host, int port,
		const char *ca_path)
{
	if (host != NULL)
		snprintf(g_server_host, sizeof(g_server_host), "%s", host);
	if (ca_path != NULL)
		snprintf(g_server_ca, sizeof(g_server_ca), "%s", ca_path);
	g_server_port = port;
}

/**
 * @brief Where the bot binary is, without writing a path down anywhere.
 *
 * No hard-coded paths is a project rule, so it is resolved from the directory
 * of the running tetrisu - the two are built into the same bin/ and installed
 * together, so wherever one is the other is beside it. TETRISU_BOT_BIN
 * overrides that, for a layout where they are not.
 *
 * Three answers rather than one, because the first two can each be absent:
 *
 *   1. TETRISU_BOT_BIN, for a layout where the two are not siblings.
 *   2. The running executable, asked of the kernel - which is /proc/self/exe
 *      on Linux and _NSGetExecutablePath on macOS. Asking only the first is
 *      what made every B press on a Mac answer "no bot could be started":
 *      Darwin has no /proc at all, and macOS is a supported client here - it
 *      is the platform that cannot run the *server*.
 *   3. argv[0], resolved as the shell resolved it: as a path when it carries a
 *      slash, and through PATH when it does not, which is how the client is
 *      launched from tetrish (.tetrishrc puts ./bin on PATH).
 *
 * A path that cannot be resolved is refused rather than guessed at, and the
 * refusal reaches the player the same way an empty pool does: no bot is added
 * and they are told why.
 *
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 * @return 0 when a runnable binary was found, -1 otherwise.
 */
int	bot_farm_binary(char *out, size_t cap)
{
	const char	*override;
	char		self[BOT_PATH_MAX];

	if (out == NULL || cap == 0)
		return (-1);
	out[0] = '\0';
	override = getenv("TETRISU_BOT_BIN");
	if (override != NULL && override[0] != '\0')
	{
		snprintf(out, cap, "%s", override);
		if (usable(out) == 0)
			return (0);
		return (note("TETRISU_BOT_BIN names nothing runnable", out));
	}
	if (self_path(self, sizeof(self)) != 0)
		snprintf(self, sizeof(self), "%s", "(the kernel gave no answer)");
	else if (beside(self, out, cap) == 0)
		return (0);
	if (strchr(g_invoked_as, '/') != NULL
		&& beside(g_invoked_as, out, cap) == 0)
		return (0);
	if (from_path_env(out, cap) == 0)
		return (0);
	note("no tetrisu-bot beside the running executable", self);
	return (note("nor beside argv[0], nor on PATH; argv[0] was",
			g_invoked_as));
}

/**
 * @brief Writes one line into the bot log, from the parent.
 *
 * The child's own failures reach that file because child_stdio points its
 * stdio at it. A failure to *start* the child never got that far and was
 * reported as five words on a status line, which is not enough to tell a
 * missing build from a wrong layout on a machine one cannot open a shell on.
 *
 * Never stdout: this parent's stdout is the screen notcurses is drawing.
 *
 * @param what The failure.
 * @param detail The path or value it concerns.
 * @return -1 always, so callers can return it directly.
 */
static int	note(const char *what, const char *detail)
{
	char	line[BOT_PATH_MAX * 2];
	int		fd;
	int		len;

	fd = log_open();
	if (fd < 0)
		return (-1);
	len = snprintf(line, sizeof(line), "tetrisu: %s: %s\n", what,
			detail == NULL || detail[0] == '\0' ? "(unset)" : detail);
	if (len > 0)
		(void)!write(fd, line, (size_t)len);
	close(fd);
	return (-1);
}

/**
 * @brief Opens the bot log for appending, wherever it can be opened.
 *
 * TETRISU_BOT_LOG, then a path relative to the working directory, then the
 * temporary directory. The last is not a nicety: the default is relative, so
 * a client launched from anywhere but the repository - which is every macOS
 * checkout that has never run a daemon and so has no tmp/ - would otherwise
 * have nowhere to write and nothing to say.
 *
 * @return An open descriptor, or -1.
 */
static int	log_open(void)
{
	char		fallback[BOT_PATH_MAX];
	const char	*path;
	int			fd;

	path = getenv("TETRISU_BOT_LOG");
	if (path == NULL || path[0] == '\0')
		path = BOT_LOG_DEFAULT;
	fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd >= 0)
		return (fd);
	path = getenv("TMPDIR");
	if (path == NULL || path[0] == '\0')
		path = BOT_LOG_TEMP_DIR;
	snprintf(fallback, sizeof(fallback), "%s/%s", path, BOT_LOG_NAME);
	return (open(fallback, O_WRONLY | O_CREAT | O_APPEND, 0644));
}

/**
 * @brief Whether this path names something this process may execute.
 *
 * @param path The candidate.
 * @return 0 when it is runnable, -1 otherwise.
 */
static int	usable(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return (-1);
	if (access(path, X_OK) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Looks for the bot in the directory holding some other executable.
 *
 * @param anchor A path to an executable, whose last component is dropped.
 * @param out Buffer receiving the candidate path.
 * @param cap Size of out.
 * @return 0 when the candidate is runnable, -1 otherwise.
 */
static int	beside(const char *anchor, char *out, size_t cap)
{
	char	dir[BOT_PATH_MAX];
	char	*slash;

	snprintf(dir, sizeof(dir), "%s", anchor);
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	snprintf(out, cap, "%s/%s", dir, BOT_BINARY_NAME);
	return (usable(out));
}

/**
 * @brief Asks the kernel which file this process is running.
 *
 * The one platform-specific call in tetrisu, and it is here rather than behind
 * a header because it is one line on each side of the #ifdef.
 *
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 * @return 0 on success, -1 when the kernel has no answer.
 */
static int	self_path(char *out, size_t cap)
{
# ifdef __APPLE__
	uint32_t	size;

	size = (uint32_t)cap;
	if (_NSGetExecutablePath(out, &size) != 0)
		return (-1);
	return (0);
# else
	ssize_t	len;

	len = readlink("/proc/self/exe", out, cap - 1);
	if (len <= 0)
		return (-1);
	out[len] = '\0';
	return (0);
# endif
}

/**
 * @brief Walks PATH for the bot binary, the way execvp would.
 *
 * The last resort, and the one that answers a client started by bare name.
 *
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 * @return 0 when one was found, -1 otherwise.
 */
static int	from_path_env(char *out, size_t cap)
{
	const char	*entry;
	const char	*end;

	entry = getenv("PATH");
	while (entry != NULL && *entry != '\0')
	{
		end = strchr(entry, ':');
		if (end == NULL)
			end = entry + strlen(entry);
		if (end != entry)
		{
			snprintf(out, cap, "%.*s/%s", (int)(end - entry), entry,
				BOT_BINARY_NAME);
			if (usable(out) == 0)
				return (0);
		}
		entry = end;
		if (*entry == ':')
			entry++;
	}
	out[0] = '\0';
	return (-1);
}

/**
 * @brief Add one bot to a room, as a child of this process.
 *
 * @param farm The farm to add to.
 * @param room The room the bot should join.
 * @param level The difficulty it plays at.
 * @return 0 on success, -1 when the farm is full or the spawn failed.
 */
int	bot_farm_add(t_bot_farm *farm, const char *room, t_bot_level level)
{
	char	binary[BOT_PATH_MAX];
	int		pipes[2];
	int		report[2];

	if (farm == NULL || room == NULL || farm->count >= BOT_FARM_MAX)
		return (-1);
	if (bot_farm_binary(binary, sizeof(binary)) != 0)
		return (-1);
	if (pipe(pipes) != 0)
		return (note("no pipe for the deadman", strerror(errno)));
	/*
	 * The write end must not survive an exec, and the reason is the whole of
	 * why the deadman works at all. A child inherits every descriptor its
	 * parent holds: without this it keeps the write end of its own pipe, so
	 * the read end it is polling can never reach EOF, and it also inherits
	 * the write ends of every bot spawned before it, so none of theirs can
	 * either. Orphaned bots then play on forever, holding a BOT_ account each.
	 * The read end is deliberately left inheritable - it is passed to the
	 * child by number, across the exec, as --deadman.
	 */
	if (fcntl(pipes[1], F_SETFD, FD_CLOEXEC) != 0)
	{
		close(pipes[0]);
		close(pipes[1]);
		return (note("no close-on-exec for the deadman", strerror(errno)));
	}
	if (open_report(report) != 0)
	{
		close(pipes[0]);
		close(pipes[1]);
		return (-1);
	}
	farm->bots[farm->count].pid = spawn_bot(binary, room, level, pipes[0],
			report[1]);
	close(pipes[0]);
	close(report[1]);
	if (farm->bots[farm->count].pid <= 0)
	{
		close(pipes[1]);
		close(report[0]);
		return (note("could not fork a bot", strerror(errno)));
	}
	farm->bots[farm->count].deadman = pipes[1];
	farm->bots[farm->count].report = report[0];
	farm->bots[farm->count].username[0] = '\0';
	farm->bots[farm->count].level = level;
	farm->count++;
	return (0);
}

/**
 * @brief Opens the pipe a child reports its claimed account back on.
 *
 * The ends go opposite ways to the deadman's, because the traffic does: the
 * child writes and the parent reads. So it is the parent's *read* end that
 * must not survive an exec - a later bot inheriting an earlier one's would
 * hold it open, and a farm that never sees EOF is a farm that cannot tell a
 * bot which never logged in from one still trying. The child's write end is
 * deliberately inheritable and crosses by number, as --report.
 *
 * The read end is non-blocking because it is polled from the roster's refresh,
 * which must not stop for a bot that is still handshaking.
 *
 * @param report Receives {read end, write end}.
 * @return 0 on success, -1 with a note written.
 */
static int	open_report(int *report)
{
	if (pipe(report) != 0)
		return (note("no pipe for the bot report", strerror(errno)));
	if (fcntl(report[0], F_SETFD, FD_CLOEXEC) != 0
		|| fcntl(report[0], F_SETFL, O_NONBLOCK) != 0)
	{
		close(report[0]);
		close(report[1]);
		return (note("could not set up the bot report", strerror(errno)));
	}
	return (0);
}

/**
 * @brief Reads whatever accounts the children have claimed since last asked.
 *
 * One short read per bot, non-blocking, from the roster's own refresh. A bot
 * that has not logged in yet simply has nothing to say and keeps an empty
 * name, which every caller has to handle anyway - the child is forked long
 * before it has an account, and it may never get one.
 *
 * @param farm The farm whose children are asked.
 */
void	bot_farm_collect_names(t_bot_farm *farm)
{
	char	buffer[BOT_NAME_MAX];
	ssize_t	got;
	int		index;

	if (farm == NULL)
		return ;
	index = 0;
	while (index < farm->count)
	{
		if (farm->bots[index].report >= 0
			&& farm->bots[index].username[0] == '\0')
		{
			got = read(farm->bots[index].report, buffer, sizeof(buffer) - 1);
			if (got > 0)
			{
				buffer[got] = '\0';
				buffer[strcspn(buffer, "\n")] = '\0';
				snprintf(farm->bots[index].username,
					sizeof(farm->bots[index].username), "%s", buffer);
			}
		}
		index++;
	}
}

/**
 * @brief Whether this farm started the bot sitting under a given name.
 *
 * The roster names everybody; only some of them are this client's children,
 * and only a child can be kicked - kicking is a signal to a process, not a
 * request to the server.
 *
 * @param farm The farm.
 * @param username The account to look for.
 * @return true when one of these bots claimed it.
 */
bool	bot_farm_holds(const t_bot_farm *farm, const char *username)
{
	int	index;

	if (farm == NULL || username == NULL || username[0] == '\0')
		return (false);
	index = 0;
	while (index < farm->count)
	{
		if (strcmp(farm->bots[index].username, username) == 0)
			return (true);
		index++;
	}
	return (false);
}

/**
 * @brief Let the bot sitting under one account go.
 *
 * @param farm The farm.
 * @param username The account to release.
 * @return 0 when one was released, -1 when this farm has no such bot.
 */
int	bot_farm_drop_named(t_bot_farm *farm, const char *username)
{
	int	index;

	if (farm == NULL || username == NULL || username[0] == '\0')
		return (-1);
	index = 0;
	while (index < farm->count)
	{
		if (strcmp(farm->bots[index].username, username) == 0)
		{
			reap(farm->bots[index].pid, farm->bots[index].deadman);
			close(farm->bots[index].report);
			farm->bots[index] = farm->bots[farm->count - 1];
			memset(&farm->bots[farm->count - 1], 0, sizeof(*farm->bots));
			farm->count--;
			return (0);
		}
		index++;
	}
	return (-1);
}

/**
 * @brief Fork and exec one bot, handing it the read end of the deadman pipe.
 *
 * @param binary The bot binary.
 * @param room The room to join.
 * @param level The difficulty.
 * @param deadman Read end of the pipe this parent holds the other end of.
 * @return The child's pid, or -1 on failure.
 */
static int	spawn_bot(const char *binary, const char *room, t_bot_level level,
			int deadman, int report)
{
	pid_t	pid;

	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0)
	{
		child_exec(binary, room, level, deadman, report);
		_exit(127);
	}
	return ((int)pid);
}

/**
 * @brief The child half of a spawn: fix its stdio, then become the bot.
 *
 * @param binary The bot binary.
 * @param room The room to join.
 * @param level The difficulty.
 * @param deadman Read end of the deadman pipe.
 */
static void	child_exec(const char *binary, const char *room,
			t_bot_level level, int deadman, int report)
{
	char	*args[BOT_ARGV_MAX];
	char	fd_text[16];
	char	report_text[16];
	char	port_text[16];
	int		count;

	child_stdio();
	snprintf(fd_text, sizeof(fd_text), "%d", deadman);
	snprintf(report_text, sizeof(report_text), "%d", report);
	snprintf(port_text, sizeof(port_text), "%d", g_server_port);
	count = 0;
	args[count++] = (char *)binary;
	args[count++] = (char *)"--room";
	args[count++] = (char *)room;
	args[count++] = (char *)"--level";
	args[count++] = (char *)bot_level_word(level);
	args[count++] = (char *)"--deadman";
	args[count++] = fd_text;
	args[count++] = (char *)"--report";
	args[count++] = report_text;
	count += server_args(args + count, port_text);
	args[count] = NULL;
	execv(binary, args);
}

/**
 * @brief Appends the server coordinates, when this client has any to give.
 *
 * Absent only before the first connection, which is before any room exists to
 * add a bot to - so in practice the child is always told, and the omission is
 * a shape the code allows rather than one it reaches.
 *
 * @param args Where to write, which must have room for four.
 * @param port_text The port, already rendered.
 * @return How many arguments were written.
 */
static int	server_args(char **args, char *port_text)
{
	int	count;

	count = 0;
	if (g_server_host[0] != '\0')
	{
		args[count++] = (char *)"--host";
		args[count++] = g_server_host;
		args[count++] = (char *)"--port";
		args[count++] = port_text;
	}
	if (g_server_ca[0] != '\0')
	{
		args[count++] = (char *)"--ca";
		args[count++] = g_server_ca;
	}
	return (count);
}

/**
 * @brief Point the child's stdout and stderr somewhere that is not the board.
 *
 * A forked child inherits its parent's descriptors, and this parent's
 * terminal is the one notcurses is drawing on. The child will print: the
 * frozen common.c reports the certificate on every handshake, which is why
 * net_client.c already mutes both across its own. Muting inside the child
 * would be too late for anything the loader says first, so it happens here,
 * between fork and exec.
 *
 * A log file rather than /dev/null, because a bot that fails to join is
 * otherwise silent in the one way that matters - the parent only ever learns
 * that a child exited, never why, and cannot report the reason for it.
 * /dev/null is the fallback when the log cannot be opened, never the first
 * choice.
 *
 * The default is relative, so a client launched from anywhere but the
 * repository writes it into a directory that may not exist. That is where the
 * temporary directory comes in: it is not a nicety, it is the difference
 * between a failing bot that says why and one that vanishes silently.
 */
static void	child_stdio(void)
{
	int	fd;

	fd = log_open();
	if (fd < 0)
		fd = open("/dev/null", O_WRONLY);
	if (fd < 0)
		return ;
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	if (fd > STDERR_FILENO)
		close(fd);
}

/**
 * @brief Let the most recently added bot go.
 *
 * @param farm The farm.
 * @return 0 when one was released, -1 when there were none.
 */
int	bot_farm_drop(t_bot_farm *farm)
{
	if (farm == NULL || farm->count == 0)
		return (-1);
	farm->count--;
	reap(farm->bots[farm->count].pid, farm->bots[farm->count].deadman);
	close(farm->bots[farm->count].report);
	memset(&farm->bots[farm->count], 0, sizeof(farm->bots[farm->count]));
	return (0);
}

/**
 * @brief Let every bot go, and wait for each of them.
 *
 * Called on the way out of a room and again on the way out of the client, so
 * that a player who quits does not leave seats occupied by processes nobody
 * can reach any more.
 *
 * @param farm The farm.
 */
void	bot_farm_clear(t_bot_farm *farm)
{
	if (farm == NULL)
		return ;
	while (farm->count > 0)
		bot_farm_drop(farm);
}

/**
 * @brief Stop one bot and collect it.
 *
 * Closing the pipe is the polite half and the signal is the certain one. A
 * bot between polls has not noticed the EOF yet, and a parent that only closed
 * the pipe would wait on a child that is still mid-request; a bot that has
 * already gone makes the signal a no-op. Neither alone is enough.
 *
 * @param pid The child to stop.
 * @param deadman The write end of its pipe, closed here.
 * @return 0 once it has been collected.
 */
static int	reap(pid_t pid, int deadman)
{
	int	status;

	if (deadman >= 0)
		close(deadman);
	if (pid <= 0)
		return (0);
	kill(pid, SIGTERM);
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	return (0);
}

/**
 * @brief Collect any bot that has exited on its own, so no zombie is kept.
 *
 * A bot whose session died leaves the room by itself, and nothing else in the
 * client would ever wait on it. Called from the waiting room's loop, where a
 * seat disappearing is something the player can see anyway.
 *
 * @param farm The farm.
 * @return How many were collected.
 */
int	bot_farm_reap_exited(t_bot_farm *farm)
{
	int	index;
	int	collected;

	if (farm == NULL)
		return (0);
	collected = 0;
	index = 0;
	while (index < farm->count)
	{
		if (waitpid(farm->bots[index].pid, NULL, WNOHANG) > 0)
		{
			close(farm->bots[index].deadman);
			close(farm->bots[index].report);
			farm->bots[index] = farm->bots[farm->count - 1];
			memset(&farm->bots[farm->count - 1], 0, sizeof(*farm->bots));
			farm->count--;
			collected++;
			continue ;
		}
		index++;
	}
	return (collected);
}
