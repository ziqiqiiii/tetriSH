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

// Static Functions
static int	spawn_bot(const char *binary, const char *room, t_bot_level level,
				int deadman);
static void	child_exec(const char *binary, const char *room,
				t_bot_level level, int deadman);
static void	child_stdio(void);
static int	reap(pid_t pid, int deadman);

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
 * @brief Where the bot binary is, without writing a path down anywhere.
 *
 * No hard-coded paths is a project rule, so it is resolved from the directory
 * of the running tetrisu - the two are built into the same bin/ and installed
 * together, so wherever one is the other is beside it. TETRISU_BOT_BIN
 * overrides that, for a layout where they are not.
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
	ssize_t		len;
	char		*slash;

	override = getenv("TETRISU_BOT_BIN");
	if (override != NULL && override[0] != '\0')
	{
		snprintf(out, cap, "%s", override);
		return (access(out, X_OK));
	}
	len = readlink("/proc/self/exe", self, sizeof(self) - 1);
	if (len <= 0)
		return (-1);
	self[len] = '\0';
	slash = strrchr(self, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	snprintf(out, cap, "%s/%s", self, BOT_BINARY_NAME);
	return (access(out, X_OK));
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

	if (farm == NULL || room == NULL || farm->count >= BOT_FARM_MAX)
		return (-1);
	if (bot_farm_binary(binary, sizeof(binary)) != 0)
		return (-1);
	if (pipe(pipes) != 0)
		return (-1);
	farm->bots[farm->count].pid = spawn_bot(binary, room, level, pipes[0]);
	close(pipes[0]);
	if (farm->bots[farm->count].pid <= 0)
		return (close(pipes[1]), -1);
	farm->bots[farm->count].deadman = pipes[1];
	farm->bots[farm->count].level = level;
	farm->count++;
	return (0);
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
			int deadman)
{
	pid_t	pid;

	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0)
	{
		child_exec(binary, room, level, deadman);
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
			t_bot_level level, int deadman)
{
	char	fd_text[16];

	child_stdio();
	snprintf(fd_text, sizeof(fd_text), "%d", deadman);
	execl(binary, binary, "--room", room, "--level", bot_level_word(level),
		"--deadman", fd_text, (char *)NULL);
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
 */
static void	child_stdio(void)
{
	const char	*path;
	int			fd;

	path = getenv("TETRISU_BOT_LOG");
	if (path == NULL || path[0] == '\0')
		path = BOT_LOG_DEFAULT;
	fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
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
