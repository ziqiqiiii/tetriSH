# ifndef TETRISU_NET_H
# define TETRISU_NET_H

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>

# include "htttp.h"
# include "statusbody.h"
# include "tetrissh.h"

/*
** tetrisu's half of the session: the socket, the HTTTP requests that go out
** on it, and the STATE snapshots that come back.
**
** This header pulls in no notcurses, on purpose. Everything here is what the
** client says to tetrisd and what it hears back, which is testable against a
** real server with no terminal in the room - and a rule about who owns the
** board should not be reachable only through a renderer.
**
** The authority boundary is docs/tetrisu-local-to-tetrisd.md: tetrisd owns
** every board mutation, and the client serialises actions and renders the
** snapshots it is given. Nothing here ever writes a board.
*/

# define NET_HOST_MAX			256
# define NET_PATH_MAX			1024
# define NET_ROOM_MAX			16
# define NET_USER_MAX			32
# define NET_REASON_MAX			64
# define NET_DEFAULT_HOST		"127.0.0.1"
# define NET_DEFAULT_PORT		4242
# define NET_DEFAULT_CA_PATH	"certs/ca.crt"
# define NET_CONNECT_TIMEOUT_MS	5000
# define NET_REPLY_TIMEOUT_MS	4000
# define TETRISU_PASSWORD_MAX	128

/* the routes tetrisd serves, spelled once (src/tetrisd/README.md) */
# define TETRISU_ROUTE_ACCOUNT	"/account"
# define TETRISU_ROUTE_SESSION	"/session"
# define TETRISU_ROUTE_ROOMS	"/rooms"
# define TETRISU_ROUTE_ROOM		"/room/"
# define TETRISU_ROUTE_LEADERBOARD	"/leaderboard"

/* where tetrisd is and how to prove it is tetrisd; all of it from .tetrishrc */
typedef struct s_net_config
{
	char	host[NET_HOST_MAX];
	int		port;
	char	ca_path[NET_PATH_MAX];
}	t_net_config;

/*
** How far a connection has got. A client is only allowed to send gameplay
** requests from NET_IN_GAME, and the one thing that moves it there is the
** server accepting a START - never the client deciding it has started.
*/
typedef enum e_net_state
{
	NET_OFFLINE,
	NET_CONNECTED,
	NET_AUTHED,
	NET_IN_ROOM,
	NET_IN_GAME
}	t_net_state;

/*
** One server answer: its status, the `reason` word a refusal carries, and the
** body it came with. The body is kept whole rather than parsed into fields
** here, because different routes answer with different keys and a struct per
** route would be a schema this side does not own.
*/
/*
** Big enough for the longest body a route answers with, which is the
** leaderboard: ten lines of `<rank> <username> <score>`, and a username may
** be 31 characters. At 512 a table of long names came back cut in half and
** decoded as a short one, which reads as a real answer.
*/
# define NET_BODY_MAX	1024

typedef struct s_net_result
{
	int		status;
	char	reason[NET_REASON_MAX];
	char	body[NET_BODY_MAX];
}	t_net_result;

typedef struct s_net_client
{
	t_session		sess;
	int				fd;
	t_net_state		state;
	uint64_t		player_id;
	char			username[NET_USER_MAX];
	char			room[NET_ROOM_MAX];
	char			play_path[NET_PATH_MAX];
	/*
	** The most recent snapshot and whether one has ever arrived. STATE is
	** latest-wins on this side too: a client that fell behind wants where the
	** board is now, not the frames it missed getting there.
	*/
	t_body_state	state_snapshot;
	bool			has_state;
	uint64_t		last_seq;
	/*
	** The sequence number the view model was last built from, which is not
	** the same as the one that arrived. Snapshots are filed by whoever reads
	** the socket, and net_request reads it too - a STATE that crosses a reply
	** is filed there, not by net_pump. A loop that asked net_pump "did
	** anything arrive?" therefore missed every snapshot that crossed an
	** input, which during play is most of them.
	*/
	uint64_t		applied_seq;
	char			error[NET_REASON_MAX];
}	t_net_client;

typedef struct s_app_net_session
{
	t_net_config	cfg;
	t_net_client	net;
	bool		connected;
	char		username[NET_USER_MAX];
	int64_t		score;
	int64_t		wallet;
}	t_app_net_session;

/* NET_CLIENT.C */
void	net_config_load(t_net_config *cfg);
int		net_connect(t_net_client *net, const t_net_config *cfg);
void	net_disconnect(t_net_client *net);
int		net_fd(const t_net_client *net);
int		net_request(t_net_client *net, const char *method, const char *path,
			const char *body, t_net_result *out);
int		net_pump(t_net_client *net);
const char	*net_result_field(const t_net_result *result, const char *key,
				char *out, size_t cap);

/* NET_SESSION.C */
int		net_signup(t_net_client *net, const char *username,
			const char *password, t_net_result *out);
int		net_login(t_net_client *net, const char *username,
			const char *password, t_net_result *out);

# endif
