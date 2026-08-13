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
# include <sys/epoll.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/timerfd.h>
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
** tetrisd - the event-driven, server-authoritative game server.
**
** One reactor thread in epoll_wait owns every established connection and all
** mutable game state: lobby, rooms, games, client registry, outboxes. Beside
** it, a bounded pool of workers runs session_handshake_server, the one
** genuinely blocking call, and hands the established session back.
**
** Gravity is one timerfd in the same epoll set: read the monotonic clock
** once, advance every in-game room by that elapsed, push STATE for whatever
** came back dirty - so a late or coalesced tick stays correct.
**
** Three rules follow, and each is structural rather than remembered:
**
** - No lock guards game state. The two survivors - the handshake pool's mutex
**   and libmacminidb's internal one - guard none of it; wanting a third means
**   the work is on the wrong thread.
** - No client is freed inside the event loop. client_kill parks it on
**   srv->zombies and client_reap, run after each event batch, is the only
**   free() site - which is what keeps epoll_event.data.ptr valid.
** - The double-fork and the pidfile flock live in main.c alone, so
**   server_start stays the seam the suites drive in-process.
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
# define TETRISD_DEFAULT_CONTROL_PATH			"tmp/tetrisd/tetrisd.ctl"
# define TETRISD_DEFAULT_PID_PATH				"tmp/tetrisd/tetrisd.pid"
# define TETRISD_DEFAULT_ERR_PATH				"tmp/tetrisd/tetrisd.err"
# define TETRISD_DEFAULT_MAX_CLIENTS			64
# define TETRISD_DEFAULT_TICK_MS				12
# define TETRISD_DEFAULT_BATTLE_ROYALE_SLOTS	4
# define TETRISD_DEFAULT_INPUT_BURST			120
# define TETRISD_DEFAULT_INPUT_RATE				60
# define TETRISD_DEFAULT_HANDSHAKE_WORKERS		4
# define TETRISD_DEFAULT_HANDSHAKE_TIMEOUT_MS	5000
# define TETRISD_RC_FILENAME					".tetrishrc"
# define TETRISD_CONFIG_KEY_PREFIX				"TETRISD_"

# define TETRISD_PORT_MIN						0
# define TETRISD_PORT_MAX						65535
# define TETRISD_TICK_MS_MIN					1
# define TETRISD_TICK_MS_MAX					1000
# define TETRISD_MAX_CLIENTS_LIMIT				4096
# define TETRISD_INPUT_LIMIT_MIN				1
# define TETRISD_INPUT_LIMIT_MAX				10000
# define TETRISD_HANDSHAKE_WORKERS_MAX			64
# define TETRISD_HANDSHAKE_TIMEOUT_MIN			100
# define TETRISD_HANDSHAKE_TIMEOUT_MAX			60000


# define TETRISD_TOKEN_SCALE					1000

/* buffers */
# define TETRISD_FRAME_MAX_BYTES				HTTTP_MAX_MESSAGE_SIZE
# define TETRISD_BODY_MAX_BYTES					8192
# define TETRISD_CONFIG_LINE_MAX				512
# define TETRISD_OUTBOX_CAPACITY				32
# define TETRISD_ACCESS_WORD_MAX				16

# define TETRISD_CHAT_CAPACITY					16

# define TETRISD_CHAT_BURST						20
# define TETRISD_CHAT_RATE_PER_SEC				5

# define TETRISD_CHAT_TEXT_RAW_MAX				(BODY_CHAT_TEXT_MAX * 2)
# define TETRISD_LOG_RING_CAPACITY				1024
# define TETRISD_LOG_DRAIN_MAX					64
# define TETRISD_LOG_SHIPPER_WAIT_MS			20
# define TETRISD_PASSWORD_MAX					128

# define TETRISD_EPOLL_BATCH					64
# define TETRISD_LENGTH_PREFIX_BYTES			4
# define TETRISD_READ_CHUNK_BYTES				4096

/*
** The Control channel: the local, Administrator-only way in, separate from the
** port players connect to. Reachability is the credential - the socket is
** 0600 in a directory only the server's user writes - so nothing arriving on
** it names a Player and no session is established over it.
**
** Four connections is a ceiling rather than a budget: tetrisctl opens one,
** sends one request and exits, so the only way to reach four is several admins
** at once or one that has stopped reading.
**
** Bodies are capped rather than grown. PLAYERS on a server at the 4096-client
** limit would not fit any fixed buffer, so the listing stops at the cap and
** says how many it left out - a truncated answer an operator can see is
** truncated beats an allocation that scales with load on the reactor thread.
*/
# define TETRISD_CONTROL_MAX_CONNECTIONS		4
# define TETRISD_CONTROL_SOCKET_MODE			0600
# define TETRISD_CONTROL_BACKLOG				4
# define TETRISD_CONTROL_BODY_MAX				32768
# define TETRISD_CONTROL_FRAME_MAX				HTTTP_MAX_MESSAGE_SIZE
# define TETRISD_CONTROL_ROUTE					"/admin"
# define TETRISD_CONTROL_ROUTE_PLAYER			"/admin/player/"
# define TETRISD_RECV_BUFFER_MAX				(TETRISD_LENGTH_PREFIX_BYTES + TETRISSH_MAX_FRAME)

# define TD_MAX_GAMES							16

# define TD_MAX_PENDING							8

# define TETRISD_PENTARIS_ROWS					5

# define TETRISD_BOMB_CELLS						12

# define TETRISD_DARK_PIECES					4
# define TETRISD_PALS_PIECES					4

# define TETRISD_POINTS_PER_WALLET_POINT		100

# define TETRISD_MATCH_COUNTDOWN_MS				3000
# define TETRISD_MATCH_SELECT_MS				15000

/* content types and the routes M1 serves */
# define TETRISD_ROUTE_ACCOUNT					"/account"
# define TETRISD_ROUTE_SESSION					"/session"
# define TETRISD_ROUTE_ROOMS					"/rooms"
# define TETRISD_ROUTE_ROOM_PREFIX				"/room/"
# define TETRISD_ROUTE_LEADERBOARD				"/leaderboard"
# define TETRISD_ROUTE_STORE					"/store"
# define TETRISD_ROUTE_STORE_PREFIX				"/store/"
# define TETRISD_ROUTE_PLAYER_PREFIX			"/player/"
# define TETRISD_SEGMENT_CHARACTER				"character/"
# define TETRISD_SEGMENT_THEME					"theme/"

# define TETRISD_LEADERBOARD_ROWS				10

typedef struct s_server	t_server;
typedef struct s_client	t_client;

typedef enum e_event_source
{
	EVENT_LISTENER,
	EVENT_WAKE,
	EVENT_TIMER,
	EVENT_CONTROL_LISTENER,
	EVENT_CONTROL,
	EVENT_CLIENT
}	t_event_source;

typedef struct s_event_tag
{
	t_event_source	source;
}	t_event_tag;

typedef struct s_bytes
{
	unsigned char	*data;
	size_t			cap;
	size_t			len;
	size_t			used;
}	t_buffer;

typedef struct s_config
{
	int				port;
	char			data_dir[TETRISD_FILESYSTEM_PATH_MAX];
	char			config_dir[TETRISD_FILESYSTEM_PATH_MAX];
	char			cert_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			key_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			ca_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			log_ipc[TETRISD_FILESYSTEM_PATH_MAX];
	char			control_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			pid_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			err_path[TETRISD_FILESYSTEM_PATH_MAX];
	char			rc_path[TETRISD_FILESYSTEM_PATH_MAX];
	int				log_level;
	int				max_clients;
	int				tick_ms;
	int				br_slots;
	int				input_burst;
	int				input_rate;
	int				handshake_workers;
	int				handshake_timeout_ms;
}	t_config;

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

typedef struct s_outbound_message
{
	unsigned char	*bytes;
	size_t			len;
}	t_outbound_message;

typedef struct s_outbox
{
	t_outbound_message	slots[TETRISD_OUTBOX_CAPACITY];
	size_t				head;
	size_t				count;
	t_outbound_message	state;
	bool				state_pending;
	t_outbound_message	chat[TETRISD_CHAT_CAPACITY];
	size_t				chat_head;
	size_t				chat_count;
	uint64_t			chat_dropped;
	bool				closed;
	bool				overflowed;
}	t_outbox;

typedef enum e_pending_kind
{
	PENDING_NONE = 0,
	PENDING_EFFECT,
	PENDING_BOMB,
	PENDING_SIRTET
}	t_pending_kind;

typedef struct s_pending_ability
{
	t_pending_kind	kind;
	int				argument;
}	t_pending_ability;

typedef struct s_game
{
	t_board				board;
	t_piece				piece;
	t_piece_bag			bag;
	t_score_state		score;
	t_charge_state		charge;
	t_effect_state		effects;
	int					next[BODY_NEXT_COUNT];
	int					hold;
	bool				has_hold;
	bool				hold_used;
	int					lines;
	int					level;
	uint64_t			seq;
	int					accum_ms;
	t_lockdown			lockdown;
	bool				active;
	bool				paused;
	bool				topped_out;
	bool				recorded;
	t_player_id			player_id;
	uint32_t			seed;
	t_body_ability		last_ability;
	t_body_clear_label	last_clear;
	int					clearing_rows[BODY_CLEARING_MAX];
	int					clearing_count;
	int					clearing_ms;
	int					pending_garbage;
	int					pending_ability_garbage;
	int					fry_owed;
	int					dark_pieces;
	int					pals_pieces;
	uint32_t			garbage_seq;
	int					cleared_owed;
	t_item_id			character_id;
	t_pending_ability	pending[TD_MAX_PENDING];
	int					pending_count;
}	t_game;

typedef struct s_server_room
{
	t_room				*room;
	t_game				games[TD_MAX_GAMES];
	bool				dirty[TD_MAX_GAMES];
	bool				ticking;
	int					countdown_ms;
	int					countdown_second;
	int					select_ms;
	int					select_second;
	t_body_result		result[TD_MAX_GAMES];
	int					rank[TD_MAX_GAMES];
	t_item_id			character[TD_MAX_GAMES];
	t_server			*srv;
	int					index;
	uint64_t			chat_seq;
}	t_server_room;

/* everything a reader outside room.c may know about a room, in one read */
typedef struct s_server_room_view
{
	const char			*name;
	t_game_mode			mode;
	t_room_status		status;
	int					players;
	int					slot_count;
	bool				ticking;
}	t_server_room_view;

/* what one input request asks the game to do */
typedef enum e_input_action
{
	INPUT_MOVE,
	INPUT_ROTATE,
	INPUT_DROP,
	INPUT_HOLD,
	INPUT_PAUSE,
	INPUT_RESTART,
	INPUT_ABILITY
}	t_input_action;

typedef struct s_ability_def
{
	t_item_id			character_id;
	int					level;
	const char			*name;
	bool				needs_target;
}	t_ability_def;

/* how an ABILITY request was answered */
typedef enum e_ability_verdict
{
	ABILITY_ACTIVATED,
	ABILITY_NO_CHARGE,
	ABILITY_NO_TARGET,
	ABILITY_BLOCKED,
	ABILITY_UNAVAILABLE,
	ABILITY_INVALID
}	t_ability_verdict;

/* connection state machine - identity is owned by the connection */
typedef enum e_client_state
{
	CLI_HANDSHAKE,
	CLI_ANONYMOUS,
	CLI_AUTHED,
	CLI_CLOSING
}	t_client_state;

typedef struct s_room_binding
{
	char				room_name[ROOM_NAME_MAX];
	int					room_index;
	int					slot_index;
}	t_room_binding;

struct s_client
{
	t_event_tag			tag;
	int					fd;
	int					index;
	unsigned int		conn_id;
	t_session			sess;
	t_server			*srv;
	t_outbox			outbox;
	t_buffer			recv;
	t_buffer			send;
	t_client_state		state;
	t_player_id			player_id;
	char				username[DB_MAX_USERNAME];
	t_room_binding		binding;
	bool				watched;
	bool				writable_armed;
	bool				dead;
	bool				handshake_ok;
	uint64_t			handshake_deadline_ms;
	bool				handshake_expired;
	t_client			*next_zombie;
	int					tokens;
	uint64_t			tokens_at_ms;
	int					chat_tokens;
	uint64_t			chat_tokens_at_ms;
	uint64_t			state_seq;
};

/*
** Every live connection, addressable by player id. It was the client-lifetime
** guard when several threads could reach a client; now it is a directory, and
** the zombie list is what guards lifetime.
*/
typedef struct s_registry
{
	t_client			**slots;
	size_t				cap;
	size_t				count;
}	t_registry;

/*
** The bounded pool that runs session_handshake_server, the one blocking call
** left in tetrisd. A queued client is waiting for a worker, a seated one is
** being handshaken, and a finished one waits on `done` for the reactor to
** adopt or kill it. Every client in any of the three is already registered,
** so the queues can never outgrow the client limit.
*/
typedef struct s_handshake_pool	t_handshake_pool;

typedef struct s_handshake_seat
{
	t_handshake_pool	*pool;
	int					index;
}	t_handshake_seat;

struct s_handshake_pool
{
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	pthread_t			workers[TETRISD_HANDSHAKE_WORKERS_MAX];
	t_handshake_seat	seats[TETRISD_HANDSHAKE_WORKERS_MAX];
	t_client			*busy[TETRISD_HANDSHAKE_WORKERS_MAX];
	int					worker_count;
	t_client			**queue;
	t_client			**done;
	size_t				cap;
	size_t				queue_head;
	size_t				queue_count;
	size_t				done_head;
	size_t				done_count;
	bool				ready;
	bool				running;
	t_server			*srv;
};

/*
** One Administrator's connection. The tag is first because the reactor reads
** epoll_event.data.ptr as a t_event_tag before it knows what kind of object it
** has, exactly as it does for a client.
**
** Unlike a client this carries no session: the bytes on the wire are plaintext
** HTTTP behind the same four-byte length prefix, because there is no peer to
** authenticate that filesystem permissions have not already authenticated.
*/
typedef struct s_control_connection
{
	t_event_tag		tag;
	t_server		*srv;
	int				fd;
	bool			open;
	bool			writable_armed;
	t_buffer		recv;
	t_buffer		send;
}	t_control_connection;

/*
** The listener and its connections. `stop_requested` is how SHUTDOWN answers
** before it acts: the reply has to reach the Administrator who asked for it,
** so the loop is stopped only once that reply has actually been written.
*/
typedef struct s_control
{
	t_event_tag				tag;
	int						listen_fd;
	char					path[TETRISD_FILESYSTEM_PATH_MAX];
	t_control_connection	slots[TETRISD_CONTROL_MAX_CONNECTIONS];
	char					body[TETRISD_CONTROL_BODY_MAX];
	bool					stop_requested;
}	t_control;

struct s_server
{
	t_config			cfg;
	t_logger		log;
	t_db			*db;
	t_tetrissh_credentials	*credentials;
	t_registry		reg;
	t_lobby			lobby;
	t_server_room		rooms[LOBBY_MAX_ROOMS];
	int				listen_fd;
	int				port;
	int				epoll_fd;
	int				timer_fd;
	int				wake[2];
	t_event_tag		listener_tag;
	t_event_tag		wake_tag;
	t_event_tag		timer_tag;
	t_control		control;
	t_handshake_pool	pool;
	t_client		*zombies;
	unsigned char	*scratch;
	t_client		**sweep;
	bool			sweep_due;
	pthread_t		loop;
	bool			loop_started;
	atomic_bool		running;
	atomic_bool		stopping;
	int				tick_ms;
	struct timespec	last_tick;
	uint64_t		started_ms;
	unsigned int	next_conn_id;
};

typedef enum e_item_kind
{
	ITEM_CHARACTER,
	ITEM_THEME
}	t_item_kind;

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

/*
** One Control channel request in flight. It mirrors t_request_context, minus
** the client: an Administrator is not a Player and has no connection state to
** answer against. The body is borrowed from t_control rather than held inline,
** so a 32 KiB answer never lands on the reactor's stack.
*/
typedef struct s_control_context
{
	t_server				*srv;
	t_control_connection	*conn;
	const t_htttp_message	*msg;
	char					*body;
	size_t					body_len;
}	t_control_context;

/* CONFIG.C */
void				config_defaults(t_config *cfg);
int					config_resolve_rc_path(const char *override, char *out, size_t cap);
int					config_set(t_config *cfg, const char *key, const char *value);
int					config_parse_line(t_config *cfg, const char *line);
int					config_load(t_config *cfg, const char *override);
int					config_validate(const t_config *cfg);

/* LOGGER.C */
void				logger_blank(t_logger *lg);
int					logger_init(t_logger *lg, const t_config *cfg);
void				logger_emit(t_logger *lg, t_log_level level, const char *fmt, ...);
uint64_t			logger_dropped_count(const t_logger *lg);
void				logger_shutdown(t_logger *lg);

/* LISTENER.C */
int					listener_open(int port, int *out_port);
int					listener_accept(int listen_fd);

/* CLOCK.C */
uint64_t			clock_now_ms(void);
int					clock_elapsed_ms(struct timespec *last);

/* BUFFER.C */
int					buffer_reserve(t_buffer *b, size_t cap);
void				buffer_compact(t_buffer *b);
void				buffer_put_u32(unsigned char *p, uint32_t value);
uint32_t			buffer_get_u32(const unsigned char *p);
void				buffer_free(t_buffer *b);

/* OUTBOX.C */
int					outbox_init(t_outbox *ob);
int					outbox_push(t_outbox *ob, unsigned char *bytes, size_t len);
int					outbox_push_state(t_outbox *ob, unsigned char *bytes, size_t len);
int					outbox_push_chat(t_outbox *ob, unsigned char *bytes, size_t len);
int					outbox_pop(t_outbox *ob, t_outbound_message *out);
bool				outbox_idle(t_outbox *ob);
void				outbox_drop_room_pushes(t_outbox *ob);
void				outbox_close(t_outbox *ob);
void				outbox_destroy(t_outbox *ob);

/* REGISTRY.C */
int					registry_init(t_registry *rg, size_t cap);
int					registry_add(t_registry *rg, t_client *cli);
void				registry_remove(t_registry *rg, t_client *cli);
int					registry_enqueue(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len, bool is_state);
int					registry_enqueue_chat(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len);
void				registry_bind(t_registry *rg, t_client *cli, t_player_id pid, const char *username);
void				registry_mark_state(t_registry *rg, t_client *cli, t_client_state state);
t_client			*registry_find_other(t_registry *rg, t_player_id pid, const t_client *keep);
size_t				registry_snapshot(t_registry *rg, t_client **out, size_t cap);
bool				registry_player_online(t_registry *rg, t_player_id pid);
void				registry_destroy(t_registry *rg);

/* HANDSHAKE_POOL.C */
int					handshake_pool_start(t_handshake_pool *pool, t_server *srv);
int					handshake_pool_submit(t_handshake_pool *pool, t_client *cli);
int					handshake_pool_take(t_handshake_pool *pool, t_client **out);
int					handshake_pool_expire(t_handshake_pool *pool);
void				handshake_pool_stop(t_handshake_pool *pool);
void				handshake_pool_destroy(t_handshake_pool *pool);

/* CLIENT.C */
int					client_spawn(t_server *srv, int fd);
void				client_adopt(t_client *cli);
void				client_kill(t_client *cli);
void				client_reap(t_server *srv);

/* CLIENTIO.C */
void				client_readable(t_client *cli);
void				client_flush(t_client *cli);
void				client_send(t_client *cli, t_htttp_message *msg, bool is_state);

/* REACTOR.C */
void				reactor_run(t_server *srv);
int					reactor_arm_timer(t_server *srv);

/* DISPATCH.C */
void				client_handle_frame(t_client *cli, const unsigned char *frame, size_t len);
void				request_reply(t_client *cli, unsigned int status, const char *body, size_t body_len);
const char			*request_body_field(const t_request_context *ctx, const char *key, char *out, size_t cap);
void				request_body_printf(t_request_context *ctx, const char *fmt, ...);
int					request_refuse(t_request_context *ctx, const char *reason);

/* HANDLERS_ACCOUNT.C */
int					signup_handler(const t_htttp_message *msg, void *context);
int					login_handler(const t_htttp_message *msg, void *context);
int					password_hash(const char *password, const char *salt, char *out, size_t cap);
int					salt_generate(char *out, size_t cap);

/* HANDLERS_LOBBY.C */
int					list_handler(const t_htttp_message *msg, void *context);
int					join_handler(const t_htttp_message *msg, void *context);
int					leave_handler(const t_htttp_message *msg, void *context);
int					start_handler(const t_htttp_message *msg, void *context);
bool				request_is_authorised(t_request_context *ctx);

/* HANDLERS_READY.C */
int					ready_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_CHAT.C */
int					chat_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_LEADERBOARD.C */
int					leaderboard_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_PROFILE.C */
int					profile_handler(const t_htttp_message *msg, void *context);
int					request_profile_body(t_request_context *ctx);

/* HANDLERS_STORE.C */
int					buy_handler(const t_htttp_message *msg, void *context);
int					equip_handler(const t_htttp_message *msg, void *context);
int					store_list_catalogue(t_request_context *ctx);

/* REQUEST_TARGET.C */
int					request_input_target(t_request_context *ctx, t_server_room **out);
const char			*request_room_name(const t_request_context *ctx);
t_player_id			request_player_id(const char *text, const char **end);
int					request_body_token(t_request_context *ctx, char *out, size_t cap);
bool				rate_limit_take_token(t_client *cli);
bool				rate_limit_take_chat_token(t_client *cli);
int					rate_limit_refill_level(int tokens, uint64_t elapsed_ms, int cap, int rate);

/* HANDLERS_INPUT.C */
int					move_handler(const t_htttp_message *msg, void *context);
int					rotate_handler(const t_htttp_message *msg, void *context);
int					drop_handler(const t_htttp_message *msg, void *context);
int					hold_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_GAME.C */
int					pause_handler(const t_htttp_message *msg, void *context);
int					restart_handler(const t_htttp_message *msg, void *context);
int					ability_handler(const t_htttp_message *msg, void *context);

/* GAME.C */
void				game_reset(t_game *g);
void				game_start(t_game *g, t_player_id pid, uint32_t seed);
bool				game_gravity(t_game *g, int elapsed_ms);
bool				game_move(t_game *g, int dcol);
bool				game_rotate(t_game *g, int dir);
bool				game_drop(t_game *g, bool hard);
bool				game_hold(t_game *g);
bool				game_pause(t_game *g, bool paused);
bool				game_restart(t_game *g);
void				game_snapshot(const t_game *g, t_body_state *out);
void				game_queue_garbage(t_game *g, int lines);
void				game_queue_ability_garbage(t_game *g, int lines);
int					game_take_fry(t_game *g);
void				game_queue_ability(t_game *g, t_pending_kind kind, int argument);
int					game_take_cleared(t_game *g);

/* ABILITY_CTRL.C */
const t_ability_def	*ability_lookup(t_item_id character_id, int level);
bool				ability_is_playable_solo(const t_ability_def *def);
t_ability_verdict	game_ability(t_game *g, t_game *target, const t_ability_def *def, int argument);
const char			*ability_verdict_reason(t_ability_verdict verdict);

/* ROOM.C */
int					server_rooms_init(t_server *srv, int br_slots);
void				server_rooms_tick(t_server *srv, int elapsed_ms);
t_server_room		*server_room_at(t_server *srv, int index);
t_server_room		*server_room_find(t_server *srv, const char *name);
t_server_room		*server_room_resolve(t_server *srv, const t_client *cli, const char *name);
void				server_room_unbind(t_client *cli);
int					server_room_open(t_server *srv, t_client *cli, t_game_mode mode);
t_join_verdict		server_room_seat(t_server_room *server_room, t_client *cli, int *slot);
t_start_verdict		server_room_start(t_server_room *server_room, t_client *cli);
bool				server_room_set_ready(t_server_room *server_room, t_client *cli, bool ready, t_item_id character);
bool				server_room_all_ready(const t_server_room *server_room);
bool				server_room_autostart(t_server_room *server_room);
bool				server_room_begin_selection(t_server_room *server_room);
bool				server_room_all_locked(const t_server_room *server_room);
bool				server_room_input(t_server_room *server_room, t_client *cli, t_input_action action, int argument);
bool				server_room_is_solo(const t_server_room *server_room);
int					server_room_target_of(t_server_room *server_room, int from_slot);
t_game				*server_room_target_game(t_server_room *server_room, const t_client *cli);
t_game				*server_room_game_of(t_server_room *server_room, const t_client *cli);
void				server_room_mark_dirty(t_server_room *server_room, const t_client *cli);
void				server_room_forfeit(t_server *srv, t_client *cli);
bool				server_room_describe(const t_server_room *server_room, t_server_room_view *out);
bool				server_room_snapshot(const t_server_room *server_room, t_body_room *out);
bool				server_room_is_muted(const t_server_room *server_room, t_player_id pid);
const t_game		*server_room_game_at(const t_server_room *server_room, int slot);

/* NARRATE.C */
bool				room_chat_broadcast(t_server_room *server_room, t_body_chat *chat);
void				room_narrate(t_server_room *server_room, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* SERVER.C */
int					server_start(const t_config *cfg, t_server **out);
void				server_stop(t_server *srv);
int					server_port(const t_server *srv);
void				server_wait(t_server *srv);
void				server_request_stop(t_server *srv);
void				server_wake(t_server *srv);
void				server_reload(t_server *srv);

/* SIGNALS.C */
void				signals_install(t_server *srv);
bool				signals_take_stop(void);
void				signals_restore(void);
bool				signals_take_reload(void);
bool				signals_take_state_dump(void);

/* DUMP.C */
void				server_state_dump(t_server *srv);

# endif
