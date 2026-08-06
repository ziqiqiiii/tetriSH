# ifndef TETRISD_H
# define TETRISD_H

# include <ctype.h>
# include <errno.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <poll.h>
# include <pthread.h>
# include <signal.h>
# include <stdarg.h>
# include <stdatomic.h>
# include <stdbool.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <time.h>
# include <unistd.h>

# include <openssl/rand.h>
# include <openssl/sha.h>

# include "coredaemon.h"
# include "coreipc.h"
# include "htttp.h"
# include "macminidb.h"
# include "statusbody.h"
# include "tetrisbrain.h"
# include "tetrisroom.h"
# include "tetrissh.h"

/*
** tetrisd - the concurrent, server-authoritative game server.
**
** Threads: the main loop polls the listener plus a self-pipe; every client
** owns a reader thread (blocks in session_recv, parses, dispatches) and a
** writer thread (the sole session_send caller for that client); every
** in-game room owns a ticker thread driving gravity.
**
** Lock order is strictly descending:
**     lobby_mutex > room->mutex > registry rwlock > outbox mutex
** No db_*, session_*, or IPC send happens under any lock - the outbox push
** is the sole exception, and it never blocks.
**
** It detaches itself and publishes a locked pidfile; tetrisctl starts,
** inspects and stops it through that file (docs/adr/0007). Both the fork and
** the claim live in main.c alone - server_start must stay the seam the tests
** drive in-process, and a start function that forked would take every suite
** with it.
*/

# define TETRISD_FILESYSTEM_PATH_MAX					1024
# define TETRISD_COMPONENT_NAME					"tetrisd"

/* config defaults - every one of them overridable from .tetrishrc */
# define TETRISD_DEFAULT_PORT					4242
# define TETRISD_DEFAULT_DATA_DIR				"tmp/tetrisd"
# define TETRISD_DEFAULT_CONFIG_DIR				"lib/libmacminidb/config"
# define TETRISD_DEFAULT_CERT_PATH				"certs/server.crt"
# define TETRISD_DEFAULT_KEY_PATH				"certs/server.key"
# define TETRISD_DEFAULT_CA_PATH				"certs/ca.crt"
# define TETRISD_DEFAULT_LOG_IPC_PATH			"tmp/tetrisd/tetrislogd.sock"
# define TETRISD_DEFAULT_PID_PATH				"tmp/tetrisd/tetrisd.pid"
# define TETRISD_DEFAULT_ERR_PATH				"tmp/tetrisd/tetrisd.err"
# define TETRISD_DEFAULT_MAX_CLIENTS			64
# define TETRISD_DEFAULT_TICK_MS				12
# define TETRISD_DEFAULT_BATTLE_ROYALE_SLOTS	4
# define TETRISD_DEFAULT_INPUT_BURST			120
# define TETRISD_DEFAULT_INPUT_RATE				60
# define TETRISD_RC_FILENAME					".tetrishrc"
# define TETRISD_CONFIG_KEY_PREFIX				"TETRISD_"

# define TETRISD_PORT_MIN						0
# define TETRISD_PORT_MAX						65535
# define TETRISD_TICK_MS_MIN					1
# define TETRISD_TICK_MS_MAX					1000
# define TETRISD_MAX_CLIENTS_LIMIT				4096
# define TETRISD_INPUT_LIMIT_MIN				1
# define TETRISD_INPUT_LIMIT_MAX				10000

/*
** The token bucket is counted in thousandths of a token, so a refill rate in
** whole tokens per second turns into an exact integer per millisecond and no
** floating point is needed on the input path.
*/
# define TETRISD_TOKEN_SCALE					1000

/* buffers */
# define TETRISD_FRAME_MAX_BYTES				HTTTP_MAX_MESSAGE_SIZE
# define TETRISD_BODY_MAX_BYTES					8192
# define TETRISD_CONFIG_LINE_MAX				512
# define TETRISD_OUTBOX_CAPACITY				32
# define TETRISD_LOG_RING_CAPACITY				1024
# define TETRISD_LOG_DRAIN_MAX					64
# define TETRISD_LOG_SHIPPER_WAIT_MS			20
# define TETRISD_PASSWORD_MAX					128

/*
** How long a LOGIN waits for the connection it displaced to finish tearing
** itself down. The displaced socket has already been shut down, so its reader
** thread wakes at once; this is only a guard against waiting forever if it
** somehow does not.
*/
# define TD_DISPLACE_WAIT_MS					3000

/*
** Games a single room can run at once. The room domain allows 99 slots, but
** a game is a whole board: sizing every room for the maximum would cost tens
** of megabytes for rooms that hold one or four players.
*/
# define TD_MAX_GAMES							16

/* content types and the routes M1 serves */
# define TETRISD_ROUTE_ACCOUNT					"/account"
# define TETRISD_ROUTE_SESSION					"/session"
# define TETRISD_ROUTE_ROOMS					"/rooms"
# define TETRISD_ROUTE_ROOM_PREFIX				"/room/"

typedef struct s_server	t_server;
typedef struct s_client	t_client;

/*
** Every setting tetrisd reads out of .tetrishrc, plus the rc path it was
** read from (SIGHUP re-reads the same file).
*/
typedef struct s_config
{
	int		port;
	char	data_dir[TETRISD_FILESYSTEM_PATH_MAX];
	char	config_dir[TETRISD_FILESYSTEM_PATH_MAX];
	char	cert_path[TETRISD_FILESYSTEM_PATH_MAX];
	char	key_path[TETRISD_FILESYSTEM_PATH_MAX];
	char	ca_path[TETRISD_FILESYSTEM_PATH_MAX];
	char	log_ipc[TETRISD_FILESYSTEM_PATH_MAX];
	char	pid_path[TETRISD_FILESYSTEM_PATH_MAX];
	char	err_path[TETRISD_FILESYSTEM_PATH_MAX];
	char	rc_path[TETRISD_FILESYSTEM_PATH_MAX];
	int		log_level;
	int		max_clients;
	int		tick_ms;
	int		br_slots;
	int		input_burst;
	int		input_rate;
}	t_config;

/*
** Log path. Producers push into the ring and never block; the shipper thread
** drains it and datagrams each record to tetrislogd, falling back to stderr
** while the logger is unreachable. Drops are counted, never waited on.
*/
typedef struct s_logger
{
	t_ring_buffer	ring;
	int				sock_fd;
	int				wake[2];
	atomic_int		level;
	char			ipc_path[TETRISD_FILESYSTEM_PATH_MAX];
	pthread_t		shipper;
	bool			shipper_started;
	atomic_bool		running;
	atomic_bool		fallback;
}	t_logger;

/* one serialised message waiting for its client's writer thread */
typedef struct s_outbound_message
{
	unsigned char	*bytes;
	size_t			len;
}	t_outbound_message;

/*
** A bounded FIFO of responses plus a one-slot mailbox holding the latest
** STATE. Responses that overflow the FIFO close the client (it cannot keep
** up); STATE snapshots overwrite instead, so a stalled client loses
** intermediate frames but never stalls a room's ticker.
*/
typedef struct s_outbox
{
	t_outbound_message	slots[TETRISD_OUTBOX_CAPACITY];
	size_t				head;
	size_t				count;
	t_outbound_message	state;
	bool				state_pending;
	bool				closed;
	atomic_bool			overflowed;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
}	t_outbox;

/*
** One player's game: the aggregate libtetrisbrain deliberately does not own.
** tetrisd is its only writer, always under the owning room's mutex.
*/
typedef struct s_game
{
	t_board				board;
	t_piece				piece;
	t_piece_bag			bag;
	t_score_state		score;
	t_charge_state		charge;
	t_effect_state		effects;
	int					next[BODY_NEXT_COUNT];
	int					lines;
	int					level;
	uint64_t			seq;
	int					accum_ms;
	bool				active;
	bool				topped_out;
	bool				recorded;
	t_player_id			player_id;
	t_body_clear_label	last_clear;
}	t_game;

/*
** A room's mutable runtime beside the pure t_room domain object: the mutex
** that guards both, the per-slot games, and the ticker driving them.
*/
typedef struct s_server_room
{
	pthread_mutex_t	mutex;
	t_room			*room;
	t_game			games[TD_MAX_GAMES];
	bool			dirty[TD_MAX_GAMES];
	struct timespec	last_tick;
	pthread_t		ticker;
	bool			ticker_started;
	atomic_bool		running;
	t_server		*srv;
	int				index;
}	t_server_room;

/* what one input request asks the game to do */
typedef enum e_input_action
{
	INPUT_MOVE,
	INPUT_ROTATE,
	INPUT_DROP
}	t_input_action;

/* connection state machine - identity is owned by the connection (ADR-0001) */
typedef enum e_client_state
{
	CLI_HANDSHAKE,
	CLI_ANONYMOUS,
	CLI_AUTHED,
	CLI_CLOSING
}	t_client_state;

struct s_client
{
	int				fd;
	int				index;
	t_session		sess;
	t_server		*srv;
	t_outbox		outbox;
	pthread_t		reader;
	pthread_t		writer;
	bool			writer_started;
	t_client_state	state;
	t_player_id		player_id;
	char			username[DB_MAX_USERNAME];
	char			room_name[ROOM_NAME_MAX];
	int				room_index;
	int				slot_index;
	/*
	** Input rate bucket. Only ever touched by this connection's own reader
	** thread, which is why it needs no lock of its own.
	*/
	int				tokens;
	uint64_t		tokens_at_ms;
};

/*
** The client registry is the lifetime guard: an enqueuer holds the read lock
** across its outbox push, so a client can never be freed under it; teardown
** takes the write lock, unlinks, and only then shuts the socket down.
*/
typedef struct s_registry
{
	t_client			**slots;
	size_t				cap;
	size_t				count;
	pthread_rwlock_t	lock;
	pthread_mutex_t		empty_mutex;
	pthread_cond_t		empty_cond;
}	t_registry;

struct s_server
{
	t_config			cfg;
	t_logger		log;
	t_db			*db;
	t_registry		reg;
	t_lobby			lobby;
	pthread_mutex_t	lobby_mutex;
	t_server_room		rooms[LOBBY_MAX_ROOMS];
	int				listen_fd;
	int				port;
	int				wake[2];
	pthread_t		loop;
	bool			loop_started;
	atomic_bool		running;
	atomic_bool		stopping;
	/*
	** The live tick period, kept beside the cfg record it was loaded from:
	** SIGHUP rewrites it on the main loop while every room ticker is reading
	** it, so this is the one setting that has to be atomic.
	*/
	atomic_int		tick_ms;
	uint64_t		started_ms;
};

/* one request in flight: what a handler answers with */
typedef struct s_request_context
{
	t_client				*cli;
	t_server				*srv;
	const t_htttp_message	*msg;
	char					body[TETRISD_BODY_MAX_BYTES];
	size_t					body_len;
	const char				*content_type;
}	t_request_context;

/* CONFIG.C */
void			config_defaults(t_config *cfg);
int				config_resolve_rc_path(const char *override, char *out, size_t cap);
int				config_set(t_config *cfg, const char *key, const char *value);
int				config_parse_line(t_config *cfg, const char *line);
int				config_load(t_config *cfg, const char *override);
int				config_validate(const t_config *cfg);

/* LOGGER.C */
void			logger_blank(t_logger *lg);
int				logger_init(t_logger *lg, const t_config *cfg);
void			logger_emit(t_logger *lg, t_log_level level, const char *fmt, ...);
uint64_t		logger_dropped_count(const t_logger *lg);
void			logger_shutdown(t_logger *lg);

/* LISTENER.C */
int				listener_open(int port, int *out_port);
int				listener_accept(int listen_fd);

/* CLOCK.C */
uint64_t		clock_now_ms(void);
int				clock_elapsed_ms(struct timespec *last);

/* OUTBOX.C */
int				outbox_init(t_outbox *ob);
int				outbox_push(t_outbox *ob, unsigned char *bytes, size_t len);
int				outbox_push_state(t_outbox *ob, unsigned char *bytes, size_t len);
int				outbox_pop(t_outbox *ob, t_outbound_message *out);
void			outbox_close(t_outbox *ob);
void			outbox_destroy(t_outbox *ob);

/* REGISTRY.C */
int				registry_init(t_registry *rg, size_t cap);
int				registry_add(t_registry *rg, t_client *cli);
void			registry_remove(t_registry *rg, t_client *cli);
int				registry_enqueue(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len, bool is_state);
void			registry_bind(t_registry *rg, t_client *cli, t_player_id pid, const char *username);
void			registry_mark_state(t_registry *rg, t_client *cli, t_client_state state);
bool			registry_displace(t_registry *rg, t_player_id pid, const t_client *keep);
int				registry_wait_absent(t_registry *rg, t_player_id pid, const t_client *keep, int timeout_ms);
void			registry_shutdown_all(t_registry *rg);
void			registry_wait_empty(t_registry *rg);
bool			registry_player_online(t_registry *rg, t_player_id pid);
void			registry_destroy(t_registry *rg);

/* CLIENT.C */
int				client_spawn(t_server *srv, int fd);
void			client_send(t_client *cli, t_htttp_message *msg, bool is_state);

/* DISPATCH.C */
void			client_handle_frame(t_client *cli, const unsigned char *frame, size_t len);
void			request_reply(t_client *cli, unsigned int status, const char *body, size_t body_len);
const char		*request_body_field(const t_request_context *ctx, const char *key, char *out, size_t cap);
void			request_body_printf(t_request_context *ctx, const char *fmt, ...);
int				request_refuse(t_request_context *ctx, const char *reason);

/* HANDLERS_ACCOUNT.C */
int				signup_handler(const t_htttp_message *msg, void *context);
int				login_handler(const t_htttp_message *msg, void *context);
int				password_hash(const char *password, const char *salt, char *out, size_t cap);
int				salt_generate(char *out, size_t cap);

/* HANDLERS_LOBBY.C */
int				list_handler(const t_htttp_message *msg, void *context);
int				join_handler(const t_htttp_message *msg, void *context);
int				leave_handler(const t_htttp_message *msg, void *context);
int				start_handler(const t_htttp_message *msg, void *context);
bool			request_is_authorised(t_request_context *ctx);

/* HANDLERS_INPUT.C */
bool			rate_limit_take_token(t_client *cli);
int				move_handler(const t_htttp_message *msg, void *context);
int				rotate_handler(const t_htttp_message *msg, void *context);
int				drop_handler(const t_htttp_message *msg, void *context);

/* GAME.C */
void			game_reset(t_game *g);
void			game_start(t_game *g, t_player_id pid, uint32_t seed);
bool			game_gravity(t_game *g, int elapsed_ms);
bool			game_move(t_game *g, int dcol);
bool			game_rotate(t_game *g, int dir);
bool			game_drop(t_game *g, bool hard);
void			game_snapshot(const t_game *g, t_body_state *out);

/* ROOM.C */
void			server_room_init_all(t_server *srv);
t_server_room		*server_room_at(t_server *srv, int index);
t_server_room		*server_room_find(t_server *srv, const char *name);
bool			server_room_probe(void *ctx, t_player_id pid);
bool			server_room_seated(t_server *srv, t_client *cli);
int				server_room_begin(t_server_room *rt, t_server *srv);
void			server_room_stop(t_server_room *rt);
void			server_room_forfeit(t_server *srv, t_client *cli);
void			server_room_push_state(t_server_room *rt, const char *room_name, t_player_id pid, const t_body_state *snap);

/* SERVER.C */
int				server_start(const t_config *cfg, t_server **out);
void			server_stop(t_server *srv);
int				server_port(const t_server *srv);
void			server_wait(t_server *srv);
void			server_request_stop(t_server *srv);
void			server_reload(t_server *srv);

/* SIGNALS.C */
void			signals_install(t_server *srv);
bool			signals_take_stop(void);
void			signals_restore(void);
bool			signals_take_reload(void);
bool			signals_take_state_dump(void);

/* DUMP.C */
void			server_state_dump(t_server *srv);

# endif
