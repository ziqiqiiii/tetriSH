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
** One reactor thread waits in epoll_wait and owns every established
** connection: it reads the socket, opens the frame, dispatches the request,
** seals the answer and writes it. Beside it sits a bounded pool of handshake
** workers, because session_handshake_server is the one genuinely blocking
** thing tetrisd does; a worker owns only its own client until it hands the
** established session back (docs/adr/0008, step 3).
**
** Gravity is one timerfd in the same epoll set. On expiry the loop reads the
** monotonic clock once, advances every in-game room by that same elapsed, and
** pushes STATE for whatever came back dirty - so a late or coalesced tick
** stays correct rather than slowing the game down.
**
** There is no lock order, because there are no locks over game state:
**
**     tetrisd has exactly one owner of all mutable game state.
**
** The lobby, every room, every game, the client registry and every outbox are
** touched by the reactor and by nothing else. The four-level lock order this
** replaces - lobby_mutex > room->mutex > registry rwlock > outbox mutex - was
** a rule a person had to hold in their head; this is a property of the
** program's shape, and the cheapest way to check an invariant is to make it
** structural (docs/adr/0008, step 5).
**
** Two locks survive, and neither guards game state: the handshake pool's own
** mutex, which hands connections between the reactor and its workers, and
** whatever libmacminidb holds internally. If you ever find yourself wanting a
** third, the thing to question is which thread you have put the work on.
**
** The rule that makes epoll_event.data.ptr safe is the one that replaced the
** registry rwlock: no client is ever freed inside the event loop. client_kill
** unlinks it and parks it on srv->zombies; client_reap, called once after
** every event in a batch has been processed, is the only free() site for a
** client.
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
** Reactor sizing. The length prefix is the 4 bytes libtetrissh writes in front
** of every frame, so a receive buffer that holds the prefix plus the largest
** frame can always make progress on a well-formed stream; a client is read in
** chunks up to that ceiling rather than being given it up front.
*/
# define TETRISD_EPOLL_BATCH					64
# define TETRISD_LENGTH_PREFIX_BYTES			4
# define TETRISD_READ_CHUNK_BYTES				4096
# define TETRISD_RECV_BUFFER_MAX				(TETRISD_LENGTH_PREFIX_BYTES \
													+ TETRISSH_MAX_FRAME)

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
** What an epoll_event.data.ptr points back at. Every watched object begins
** with one of these, so the reactor reads the tag first and only then knows
** which pointer it is holding - which is what lets a client be recovered from
** the kernel without an fd-to-client map to keep in step.
*/
typedef enum e_event_source
{
	EVENT_LISTENER,
	EVENT_WAKE,
	EVENT_TIMER,
	EVENT_CLIENT
}	t_event_source;

typedef struct s_event_tag
{
	t_event_source	source;
}	t_event_tag;

/*
** A growable byte buffer with a cursor. `len` is how many bytes are valid and
** `used` how many of them are finished with - consumed, on the receive side,
** or already written to the socket on the send side. The cursor is what makes
** a short write survivable now that no retry loop is allowed to block.
*/
typedef struct s_bytes
{
	unsigned char	*data;
	size_t			cap;
	size_t			len;
	size_t			used;
}	t_buffer;

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
	int		handshake_workers;
	int		handshake_timeout_ms;
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

/* one serialised message waiting for the reactor to seal and write it */
typedef struct s_outbound_message
{
	unsigned char	*bytes;
	size_t			len;
}	t_outbound_message;

/*
** A bounded FIFO of responses plus a one-slot mailbox holding the latest
** STATE. Responses that overflow the FIFO close the client (it cannot keep
** up); STATE snapshots overwrite instead, so a stalled client loses
** intermediate frames but never holds up the tick that produced them.
**
** That split is the load-bearing idea and has nothing to do with threading,
** which is why it outlived the writer thread, the condition variable and the
** mutex unchanged.
*/
typedef struct s_outbox
{
	t_outbound_message	slots[TETRISD_OUTBOX_CAPACITY];
	size_t				head;
	size_t				count;
	t_outbound_message	state;
	bool				state_pending;
	bool				closed;
	bool				overflowed;
}	t_outbox;

/*
** One player's game: the aggregate libtetrisbrain deliberately does not own.
** The reactor is its only writer, which is now the whole of the rule.
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
** A Room, as tetrisd knows one: the pure t_room the domain library owns, plus
** the runtime beside it - the per-slot games, which of them changed, and
** whether it is playing. `ticking` is the whole of what a ticker thread used
** to be; the server's one timer walks every room and skips the ones that are
** not.
**
** Both halves are room.c's, and only room.c's. The two objects share an index
** and therefore share a lifetime, so exactly one module opens and closes them
** together - a runtime that outlived its room once evicted the next player to
** be handed that index (docs/bugs/room_runtime_outlived_its_room.md). Reaching
** through `->room` from another file is what put those two halves out of step,
** so nothing outside room.c does: the interface below answers every question
** other files had been asking the domain object directly, and lobby_create_room
** and lobby_destroy_room have no other caller.
*/
typedef struct s_server_room
{
	t_room			*room;
	t_game			games[TD_MAX_GAMES];
	bool			dirty[TD_MAX_GAMES];
	bool			ticking;
	t_server		*srv;
	int				index;
}	t_server_room;

/* everything a reader outside room.c may know about a room, in one read */
typedef struct s_server_room_view
{
	const char		*name;
	t_game_mode		mode;
	t_room_status	status;
	int				players;
	int				slot_count;
	bool			ticking;
}	t_server_room_view;

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

/*
** The client's half of a Slot: which room it is sitting in and where. A Slot
** is one fact held in two places, so this is written by room.c and by nothing
** else - server_room_open and server_room_seat bind it, server_room_unbind is
** the only way it is cleared, and server_room_resolve is how everyone else
** asks what it currently means. Reading it is free; a reader that wants to
** act on it resolves it first, because a finished game clears every slot and
** the binding outlives the seat.
**
** `room_index` of -1 is "sitting in no room", which is what a fresh connection
** and a forfeited one both are.
*/
typedef struct s_room_binding
{
	char			room_name[ROOM_NAME_MAX];
	int				room_index;
	int				slot_index;
}	t_room_binding;

struct s_client
{
	/* first member: this is what epoll_event.data.ptr is read back through */
	t_event_tag		tag;
	int				fd;
	int				index;
	t_session		sess;
	t_server		*srv;
	t_outbox		outbox;
	t_buffer			recv;
	t_buffer			send;
	t_client_state	state;
	t_player_id		player_id;
	char			username[DB_MAX_USERNAME];
	t_room_binding	binding;
	/*
	** Reactor bookkeeping. `watched` says the descriptor is in the epoll set,
	** so the socket is established and non-blocking; `writable_armed` tracks
	** EPOLLOUT, which is only asked for after a short write. `dead` marks a
	** client that has been unlinked and is waiting on the zombie list - every
	** later event in the same batch has to skip it rather than touch it.
	*/
	bool			watched;
	bool			writable_armed;
	bool			dead;
	bool			handshake_ok;
	/*
	** The wall-clock moment this connection's handshake stops being worth
	** waiting for, set when a worker picks it up. It is a budget for the whole
	** handshake, not for one read: SO_RCVTIMEO bounds a single recv, and
	** libtetrissh loops until it has the bytes it asked for, so a peer that
	** dribbles one byte per timeout would otherwise hold a worker
	** indefinitely - the very denial of service the pool has to survive.
	*/
	uint64_t		handshake_deadline_ms;
	bool			handshake_expired;
	t_client		*next_zombie;
	/*
	** Input rate bucket. Only ever touched by the reactor, which is why it
	** needs no lock of its own.
	*/
	int				tokens;
	uint64_t		tokens_at_ms;
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

struct s_server
{
	t_config			cfg;
	t_logger		log;
	t_db			*db;
	/*
	** The certificate bytes and parsed private key, read once at boot. They
	** are immutable afterwards, so every handshake worker shares one copy and
	** none of them opens a file. SIGHUP does not reload them - the listening
	** socket they authenticate is already open.
	*/
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
	t_handshake_pool	pool;
	/*
	** Clients unlinked during the current batch, freed by client_reap once the
	** batch is over, and the scratch buffer every frame is decrypted into.
	** Both belong to the reactor thread alone.
	*/
	t_client		*zombies;
	unsigned char	*scratch;
	t_client		**sweep;
	bool			sweep_due;
	pthread_t		loop;
	bool			loop_started;
	atomic_bool		running;
	atomic_bool		stopping;
	/*
	** The live tick period and when the timer last fired. Both belong to the
	** loop alone: SIGHUP retiming is now a timerfd_settime call on the thread
	** that owns the timer, rather than a store ninety-nine tickers read.
	*/
	int				tick_ms;
	struct timespec	last_tick;
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

/* BUFFER.C */
int				buffer_reserve(t_buffer *b, size_t cap);
void			buffer_compact(t_buffer *b);
void			buffer_put_u32(unsigned char *p, uint32_t value);
uint32_t		buffer_get_u32(const unsigned char *p);
void			buffer_free(t_buffer *b);

/* OUTBOX.C */
int				outbox_init(t_outbox *ob);
int				outbox_push(t_outbox *ob, unsigned char *bytes, size_t len);
int				outbox_push_state(t_outbox *ob, unsigned char *bytes, size_t len);
int				outbox_pop(t_outbox *ob, t_outbound_message *out);
bool			outbox_idle(t_outbox *ob);
void			outbox_close(t_outbox *ob);
void			outbox_destroy(t_outbox *ob);

/* REGISTRY.C */
int				registry_init(t_registry *rg, size_t cap);
int				registry_add(t_registry *rg, t_client *cli);
void			registry_remove(t_registry *rg, t_client *cli);
int				registry_enqueue(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len, bool is_state);
void			registry_bind(t_registry *rg, t_client *cli, t_player_id pid, const char *username);
void			registry_mark_state(t_registry *rg, t_client *cli, t_client_state state);
t_client		*registry_find_other(t_registry *rg, t_player_id pid, const t_client *keep);
size_t			registry_snapshot(t_registry *rg, t_client **out, size_t cap);
bool			registry_player_online(t_registry *rg, t_player_id pid);
void			registry_destroy(t_registry *rg);

/* HANDSHAKE_POOL.C */
int				handshake_pool_start(t_handshake_pool *pool, t_server *srv);
int				handshake_pool_submit(t_handshake_pool *pool, t_client *cli);
int				handshake_pool_take(t_handshake_pool *pool, t_client **out);
int				handshake_pool_expire(t_handshake_pool *pool);
void			handshake_pool_stop(t_handshake_pool *pool);
void			handshake_pool_destroy(t_handshake_pool *pool);

/* CLIENT.C */
int				client_spawn(t_server *srv, int fd);
void			client_adopt(t_client *cli);
void			client_kill(t_client *cli);
void			client_reap(t_server *srv);

/* CLIENTIO.C */
void			client_readable(t_client *cli);
void			client_flush(t_client *cli);
void			client_send(t_client *cli, t_htttp_message *msg, bool is_state);

/* REACTOR.C */
void			reactor_run(t_server *srv);
int				reactor_arm_timer(t_server *srv);

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
int				server_rooms_init(t_server *srv, int br_slots);
void			server_rooms_tick(t_server *srv, int elapsed_ms);
t_server_room	*server_room_at(t_server *srv, int index);
t_server_room	*server_room_find(t_server *srv, const char *name);
t_server_room	*server_room_resolve(t_server *srv, const t_client *cli, const char *name);
void			server_room_unbind(t_client *cli);
int				server_room_open(t_server *srv, t_client *cli, t_game_mode mode);
t_join_verdict	server_room_seat(t_server_room *server_room, t_client *cli, int *slot);
t_start_verdict	server_room_start(t_server_room *server_room, t_client *cli);
bool			server_room_input(t_server_room *server_room, t_client *cli, t_input_action action, int argument);
void			server_room_forfeit(t_server *srv, t_client *cli);
bool			server_room_describe(const t_server_room *server_room, t_server_room_view *out);
const t_game	*server_room_game_at(const t_server_room *server_room, int slot);

/* SERVER.C */
int				server_start(const t_config *cfg, t_server **out);
void			server_stop(t_server *srv);
int				server_port(const t_server *srv);
void			server_wait(t_server *srv);
void			server_request_stop(t_server *srv);
void			server_wake(t_server *srv);
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
