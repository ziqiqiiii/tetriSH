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
/*
** Connections the server accepts at once, and seats a Battle Royale room is
** created with. The two are related: one full Battle Royale is 99 players, so
** a ceiling of 64 would have made the mode's own maximum unreachable before
** anything else in the server refused it.
*/
# define TETRISD_DEFAULT_MAX_CLIENTS			128
# define TETRISD_DEFAULT_TICK_MS				12
# define TETRISD_DEFAULT_BATTLE_ROYALE_SLOTS	99
# define TETRISD_DEFAULT_INPUT_BURST			120
# define TETRISD_DEFAULT_INPUT_RATE				60
# define TETRISD_DEFAULT_HANDSHAKE_WORKERS		4
/*
** How many accounts the server keeps for the bots a player fills a room with.
** They are created at boot if absent and named BOT_01 upward, and a player
** cannot sign up as one - DB_RESERVED_PREFIX is refused by db_signup.
**
** The pool is sized rather than fixed at four because a player holds at most
** one connection and so does a bot: four names would be four bots for the
** whole server, and the fifth would displace the first out of a match in
** progress.
**
** Sixty-four because the client's F1 fills a room with up to BOT_FARM_MAX (50)
** of them to measure what a crowded Battle Royale costs, and thirty-two would
** have refused the second half of one such room - a refusal that arrives as a
** bot exiting after its JOIN, which reads as a broken bot rather than as a
** pool that ran out.
*/
# define TETRISD_DEFAULT_BOT_ACCOUNTS			64
/*
** The pool's shared credentials. Exactly DB_SALT_LEN characters of salt,
** because the store keeps a salt as a fixed-width blob rather than a string.
**
** Not a secret, and not pretending to be one - see bot_pool_hash. The bot
** binary computes the same hash from the same two constants, which is what
** lets a bot log in without a route that hands out credentials.
*/
# define TETRISD_BOT_SECRET						"tetrish-bot-"
# define TETRISD_BOT_SALT						"tetrishbotsalttetrishbotsalt0001"
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
# define TETRISD_BOT_ACCOUNTS_MAX				256
# define TETRISD_HANDSHAKE_TIMEOUT_MIN			100
# define TETRISD_HANDSHAKE_TIMEOUT_MAX			60000


# define TETRISD_TOKEN_SCALE					1000

/* buffers */
# define TETRISD_FRAME_MAX_BYTES				HTTTP_MAX_MESSAGE_SIZE
/*
** The widest body this server builds, which is a Battle Royale STATE: 8192 did
** not hold one, and a full arena silently failing to encode would have shown
** up as a client whose rivals stopped moving rather than as an error. 16384
** did not hold one either, once an arena card went from one bit per cell to
** one nibble - and the assertion below is what said so, at build time, which
** is the whole reason it is written that way.
**
** It is the next power of two above BODY_STATE_MAX_BYTES rather than a number
** chosen to look sufficient, and the assertion below is what keeps the two in
** step - libstatusbody derives its own ceiling from the constants that produce
** it, so a field added to an arena card fails the build here instead of
** overrunning this buffer at run time.
*/
# define TETRISD_BODY_MAX_BYTES					32768
# define TETRISD_CONFIG_LINE_MAX				512
_Static_assert(TETRISD_BODY_MAX_BYTES >= BODY_STATE_MAX_BYTES,
	"TETRISD_BODY_MAX_BYTES cannot hold the widest state body libstatusbody "
	"can produce - raise it to the next power of two above "
	"BODY_STATE_MAX_BYTES");
_Static_assert(TETRISD_BODY_MAX_BYTES <= TETRISD_FRAME_MAX_BYTES,
	"a body that cannot fit in a frame would be built and then refused");
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

/*
** Games a single room can run at once, which is every slot the room domain
** allows. A Battle Royale is one room of up to 99 players, so anything less
** would be a ceiling on the mode rather than on the memory.
**
** It does cost tens of megabytes, and that was the argument for 16: a game is
** a whole board, so 99 of them is 190 KB per room and 12.2 MB across the
** lobby's 64. It is paid once, by the single calloc of t_server at boot, and
** never on a stack - so it is one allocation at start-up rather than a cost
** that scales with anything happening. If the footprint ever matters the lever
** is LOBBY_MAX_ROOMS, not this: sixty-four simultaneous rooms is the far more
** speculative of the two numbers.
**
** Sizing rooms for the maximum is also what lets a Battle Royale room be
** created at full capacity regardless of how many people turn up, which is the
** point - an empty seat costs the wire nothing, because every projection walks
** occupied slots only.
*/
# define TD_MAX_GAMES							99

# define TD_MAX_PENDING							8

# define TETRISD_PENTARIS_ROWS					5

# define TETRISD_BOMB_CELLS						12

# define TETRISD_DARK_PIECES					4
# define TETRISD_PALS_PIECES					4

# define TETRISD_POINTS_PER_WALLET_POINT		100

# define TETRISD_MATCH_COUNTDOWN_MS				3000
# define TETRISD_MATCH_SELECT_MS				15000
/*
** How often a Battle Royale's arena is pushed, and how much of it is spent on
** players who are out.
**
** The arena has a clock of its own because it is not the player's board. Their
** own board is pushed the instant it changes and always will be - that is the
** thing they are steering. Ninety-eight thumbnails are glanced at, and pushing
** those at the tick rate would spend the whole connection re-sending boards
** nobody is looking at closely enough to notice the difference.
**
** 300 ms is 3.3 Hz. It is chosen against bandwidth rather than against
** perception: every client is sent every card, so cost grows with the square
** of the room and the cadence is the one lever that divides all of it.
**
** ARENA_DEAD_EVERY is the other half. A board that has topped out never
** changes again, so its card carries a mask only on every fifth push and the
** client keeps the one it holds in between. That is bounded staleness and not
** a delta: a client that misses a push is correct again inside two seconds
** with nothing acknowledged and nothing tracked per connection. It is also
** what makes the cost of a match fall as players are knocked out, which it
** otherwise would not - an eliminated player keeps their card on everyone's
** screen and keeps watching everyone else's.
*/
# define TETRISD_BR_ARENA_MS					300
# define TETRISD_BR_ARENA_DEAD_EVERY			5

/*
** How recently somebody's rows have to have landed on you for them to count
** as attacking you, and how many attackers are remembered at once.
**
** Eight, because nobody has been usefully attacked by more than a handful of
** people inside the window, and because the ring answers one question - who
** has landed rows on me lately - which a TD_MAX_GAMES-wide array of timestamps
** would answer no better at ten times the size.
**
** The window is measured on the room's own match clock rather than on the
** wall, so a match is the same match however long the machine took to run it,
** and a test can drive one without waiting out real seconds.
*/
# define TETRISD_BR_ATTACKER_MS					8000
# define TETRISD_BR_ATTACKER_RING				8


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
	int				bot_accounts;
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
	/*
	** Where the holes in this game's garbage go: an LCG state seeded from the
	** game's own seed, and the column the last row left open.
	**
	** `garbage_seq` was a plain counter and the column was `garbage_seq %
	** BOARD_WIDTH`, which put the holes on 0, 1, 2, 3 in order - a diagonal
	** across the board rather than the scatter it was meant to be. It is a
	** state now and next_garbage_hole draws from it, excluding only the
	** previous column, which is all "a run of rows with the hole in one place
	** would be a wall" ever asked for.
	**
	** Seeded rather than drawn from the C library because a match replayed
	** from one seed has to land the same rows in the same places, and because
	** no randomness enters libtetrisbrain - the column crosses as an argument.
	** `garbage_hole` is -1 until the first row lands, so that draw is uniform
	** over every column instead of over nine of them.
	*/
	uint32_t			garbage_seq;
	int					garbage_hole;
	/*
	** Who sent the rows waiting in the two queues, and who sent the rows that
	** landed at the last lock.
	**
	** They are two fields and not one because a knockout is credited at the
	** landing rather than at the sending: rows queued against a player who
	** then survives them belong to nobody, and rows that arrived while their
	** sender was disconnecting still belong to that sender. The board itself
	** does not use either - it takes rows without asking who sent them, which
	** is what keeps a t_game a board and not a participant in a room - it only
	** carries the id far enough for room.c to file it against the player at
	** the moment the rows land.
	**
	** The last sender wins when two players queue before one lock. That is the
	** definition a knockout uses, "the last player whose garbage landed", and
	** it is the only one that needs no history.
	*/
	t_player_id			garbage_from;
	t_player_id			landed_from;
	/*
	** Lines this game has cleared that have not yet been charged to anybody.
	**
	** room.c takes it after each tick and turns it into garbage against the
	** Target, because who the Target is depends on the room and a game does
	** not know it is in one. It accumulates rather than being overwritten: a
	** tick that ran two clear completions owes both.
	*/
	int					cleared_owed;
	t_item_id			character_id;
	t_pending_ability	pending[TD_MAX_PENDING];
	int					pending_count;
}	t_game;

/*
** One player's state in the match their room is playing, for as long as that
** match lasts.
**
** It exists because a slot index is not a stable identity. room_release
** promotes a successor by *moving* them into the seat the departing owner
** vacated, so a fact filed under a seat number is somebody else's a moment
** later. Three facts were: the fighter a seat had declared, and the result and
** the placing the match wrote there. rehome_successor followed the move for
** the board and for nothing else, so an owner leaving during the
** character-select window stranded the successor's declared fighter at the
** seat they had just left - the room read that seat as undecided, waited out
** the full window instead of dealing early, and gave them whatever their
** account had equipped. Silently, because falling back to the equipped
** character is also what a client with no selector asks for.
**
** So a fact about a match is filed under the player it is about. `player_id`
** is the key and 0 means the record is free. Nothing here is reachable by seat,
** which is what keeps the answer right when the seats move, and it leaves
** rehome_successor holding the one thing that genuinely belongs to a seat: the
** board being played in it.
**
** The record also outlives the board, which is the second reason it is not a
** field on t_game. forfeit_slot copies a game out and resets the slot the
** moment a player disconnects, so anything living on the game goes with them -
** and a placing is exactly the thing a player who quits still owns.
*/
/*
** One player whose garbage landed on somebody, and when it did.
**
** The time is the room's match clock, not the wall clock: it is only ever
** compared against another reading of the same clock, and a match that is
** paused, slow or being driven by a test then behaves the same as one being
** played.
*/
typedef struct s_attacker
{
	t_player_id		player_id;
	uint64_t		when_ms;
}	t_attacker;

typedef struct s_participant
{
	t_player_id		player_id;
	/*
	** The character declared for this match, or 0 for "whatever the account
	** has equipped". It is per match and not per account because a character
	** is picked for a match: EQUIP is an account-wide change, and making one
	** in order to play one game is the wrong scope - it would also mean a
	** purchase made mid-match could change which abilities a player's levels
	** select from.
	**
	** Declared with READY and read once by deal_games, so it is fixed for the
	** length of the match by construction.
	*/
	t_item_id		character;
	/*
	** How the match ended for this player, decided once when it ends and read
	** by the final snapshot they are sent. It cannot be derived from the game:
	** the winner's board is active with a piece on it, which is what every
	** board mid-match looks like.
	*/
	t_body_result	result;
	/*
	** The placing, taken the moment this player is eliminated rather than
	** when the match ends. At the end it is the same number for everybody
	** still out - nineteen losers all placed second - because by then the
	** only fact left is that they lost. At the elimination it is a fact: how
	** many players were still in the match when this one stopped being one.
	**
	** 0 while they are still playing, which is what the wire sends and what
	** lets a card be drawn dead with its placing on it the instant it is
	** decided.
	*/
	int				rank;
	/*
	** Whether this player is still in the match. It is not read off the game:
	** a game is reset when its player disconnects, so the board of somebody
	** who quit in third place says nothing at all a moment later, and the
	** elimination sweep has to be able to tell "already placed" from "placed
	** this tick".
	*/
	bool			alive;
	/*
	** Knockouts credited to this player - the ones whose last landed rows
	** were theirs.
	*/
	int				ko;
	/*
	** Who last buried this player, stamped at the lock that lands the rows
	** and not when they are queued. A player killed by rows that arrived
	** after their sender left the room still credits that sender, which is
	** why this is a player id and never a seat: seats move between players
	** mid-match and ids do not.
	*/
	t_player_id		last_attacker_id;
	/*
	** Who has landed rows on this player lately, oldest overwritten first.
	** It answers the arena's attacking-you flag, and it is per player rather
	** than per board because it has to survive a board being reset.
	*/
	t_attacker		attackers[TETRISD_BR_ATTACKER_RING];
	int				attacker_next;
	/*
	** Which kind of rival this player's garbage goes to. It is per match and
	** per player, declared with TARGET and remembered until the match ends,
	** because it is a preference a person holds rather than a property of a
	** board - and it has to survive the seat moving under them.
	*/
	t_target_mode	target_mode;
}	t_participant;

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
	/*
	** The hold before a match begins, and the second of it last sent. The
	** countdown is pushed when the displayed second changes rather than on
	** every tick: three seconds at the tick rate would be a couple of hundred
	** frames of a number the client can interpolate between four of.
	*/
	int				countdown_ms;
	int				countdown_second;
	/*
	** The character-select window, and the second of it last narrated. It runs
	** before the countdown does and on the same principle: the room owns the
	** clock, so both players see the same number and the match is dealt for
	** them at the same instant. It closes early the moment every seat has
	** named a fighter, which is what makes locking in worth doing.
	*/
	int				select_ms;
	int				select_second;
	/*
	** Everyone playing this match, found by player id rather than by seat -
	** see t_participant for why that distinction is load-bearing rather than
	** stylistic. The array is the room's runtime, so room_blank clears it for
	** the same reason it clears `ticking`; record_and_reset clears it too,
	** because a room outlives the match played in it and the next one must not
	** open with the last one's verdicts and fighters already written down.
	*/
	t_participant	participants[TD_MAX_GAMES];
	/*
	** How many players are still in this match, and how long it has been
	** running.
	**
	** `alive` is the room's own number and is what every placing is taken
	** from, so it is decremented by the size of an elimination group rather
	** than one at a time - see sweep_eliminations. It is also what the head
	** count on every frame is read from, which is why it cannot be recovered
	** by counting live games: a player who has quit still owns their placing
	** but no longer has a board.
	**
	** `match_ms` is milliseconds since the match was dealt, countdown
	** excluded. Only the attacker window reads it, and it is the room's clock
	** rather than the wall's so that the window means the same thing in a test
	** as in a game.
	*/
	int				alive;
	uint64_t		match_ms;
	/*
	** The room's own randomness, and the only randomness in a match that is
	** not a game's own bag.
	**
	** It is the room's rather than a game's because what it draws is a Target,
	** which is a fact about the room; it is one number rather than a call into
	** the C library because a match seeded once has to replay the same way
	** twice, which is what makes a targeting test able to assert a draw at
	** all. No randomness enters libtetrisbrain, which is pure by contract.
	*/
	uint32_t		rng;
	/*
	** The arena's own clock, and the number of arenas this match has pushed.
	**
	** `arena_ms` counts down to the next push. `arena_push` only ever goes up,
	** and its only reader is the every-fifth-push rule that decides whether a
	** dead player's card carries its mask - so it is a phase, not a sequence
	** number, and nothing on the wire depends on its value.
	**
	** `arena_dirty` is what stops the room pushing an arena nobody needs. A
	** push is skipped outright when no card has changed since the last one,
	** which is almost never true mid-match and is true for the whole of a
	** countdown, a long tail of two survivors, and a room between matches.
	** Skipping is safe here in a way that sending only the changed cards would
	** not be: every arena that does go out is still the complete roster.
	*/
	int				arena_ms;
	unsigned		arena_push;
	bool			arena_dirty;
	t_server		*srv;
	int				index;
	/*
	** The feed's own counter, numbering every line this room has sent. It is
	** part of the runtime and so is blanked with it: a room is destroyed when
	** its last player leaves, and the next room handed this index is a
	** different room whose feed starts again at one.
	*/
	uint64_t		chat_seq;
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

/*
** The Gaiden ability catalogue, as tetrisd has to see it.
**
** A player's four abilities come from the character they have equipped, so
** (character, level) is the identity of an ability and the level alone is
** not - level 1 is Fry for Halloween and Cut for Wolf-man. The client sends
** the level; the server reads the character out of the store, because that
** is a fact about the account and not something a request may assert.
**
** `needs_target` is the whole of why Single mode refuses most of them.
** docs/CONTEXT.md: "Single mode has no Target, so offensive abilities are
** unavailable there." An ability that lands on somebody else has nobody to
** land on in a one-player room, so it is refused rather than quietly
** redirected at the player who paid for it.
**
** `hits_every_target` splits the eleven that need somebody in two, and the
** question it asks is "what does this put where". Seven queue something onto
** the Target's board, so a targeting mode that named four rivals puts it onto
** four boards. The other four need a Target to be legal but transform the
** *sender* - Mirror and Pals set an effect on their own game, Vampire drains
** one rival's charge into it, Copy takes one rival's board. Running those
** once per victim would pay a player four times for one activation, so they
** land on the first Target and stop.
*/
typedef struct s_ability_def
{
	t_item_id	character_id;
	int			level;
	const char	*name;
	bool		needs_target;
	bool		hits_every_target;
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
	size_t					body_len;
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
	/*
	** Armed when somebody has left a room, so the loop asks once whether any
	** room is now holding nothing but bots. Armed rather than checked every
	** batch for the same reason sweep is: the answer can only have changed
	** when a seat was released, and asking otherwise walks the registry for
	** nothing.
	*/
	bool			bot_rooms_due;
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
bool				logger_sink_reaching(const t_logger *lg);
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
int				signup_handler(const t_htttp_message *msg, void *context);
int				login_handler(const t_htttp_message *msg, void *context);
int				password_hash(const char *password, const char *salt, char *out, size_t cap);

/* BOT_POOL.C — the accounts a player's bots log in as */
int				bot_pool_open(t_server *srv, int count);
void			bot_pool_name(int index, char *out, size_t cap);
int				bot_pool_hash(const char *name, char *out, size_t cap);
int				salt_generate(char *out, size_t cap);

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

/* HANDLERS_TARGET.C */
int				target_handler(const t_htttp_message *msg, void *context);

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
void			game_reset(t_game *g);
void			game_start(t_game *g, t_player_id pid, uint32_t seed);
bool			game_gravity(t_game *g, int elapsed_ms);
bool			game_move(t_game *g, int dcol);
bool			game_rotate(t_game *g, int dir);
bool			game_drop(t_game *g, bool hard);
bool			game_hold(t_game *g);
bool			game_pause(t_game *g, bool paused);
bool			game_restart(t_game *g);
void			game_snapshot(const t_game *g, t_body_state *out);
void			game_queue_garbage(t_game *g, int lines, t_player_id from);
void			game_queue_ability_garbage(t_game *g, int lines,
					t_player_id from);
int				game_take_fry(t_game *g);
void			game_queue_ability(t_game *g, t_pending_kind kind,
					int argument);
int				game_take_cleared(t_game *g);
t_player_id		game_take_attacker(t_game *g);

/* ABILITY_CTRL.C */
const t_ability_def	*ability_lookup(t_item_id character_id, int level);
bool			ability_is_playable_solo(const t_ability_def *def);
t_ability_verdict	game_ability(t_game *g, t_game *target,
					const t_ability_def *def, int argument);
t_ability_verdict	game_ability_spread(t_game *g, t_game **targets,
					int count, const t_ability_def *def, int argument);
const char		*ability_verdict_reason(t_ability_verdict verdict);

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
bool			server_room_set_ready(t_server_room *server_room, t_client *cli, bool ready, t_item_id character);
bool			server_room_all_ready(const t_server_room *server_room);
bool			server_room_autostart(t_server_room *server_room);
bool			server_room_begin_selection(t_server_room *server_room);
bool			server_room_all_locked(const t_server_room *server_room);
bool			server_room_input(t_server_room *server_room, t_client *cli, t_input_action action, int argument);
bool			server_room_is_solo(const t_server_room *server_room);
bool			server_room_is_arena(const t_server_room *server_room);
bool			server_room_starts_on_ready(const t_server_room *server_room);
int				server_room_target_of(t_server_room *server_room,
					int from_slot);
int				server_room_targets_of(t_server_room *server_room,
					int from_slot, int *out);
int				server_room_target_games(t_server_room *server_room,
					const t_client *cli, t_game **out);
void			server_room_charge_targets(t_server_room *server_room,
					int from, const int *targets, int count);
bool			server_room_set_target(t_server_room *server_room,
					t_client *cli, t_target_mode mode);
t_game			*server_room_target_game(t_server_room *server_room,
					const t_client *cli);
t_game			*server_room_game_of(t_server_room *server_room, const t_client *cli);
void			server_room_mark_dirty(t_server_room *server_room, const t_client *cli);
void			server_room_forfeit(t_server *srv, t_client *cli);
void			server_rooms_evict_abandoned(t_server *srv);
bool			server_room_describe(const t_server_room *server_room, t_server_room_view *out);
size_t			server_rooms_list(t_server *srv, t_body_room_row *rows, size_t cap);
bool			server_room_snapshot(const t_server_room *server_room,
					t_body_room *out);
bool			server_room_is_muted(const t_server_room *server_room, t_player_id pid);
const t_game	*server_room_game_at(const t_server_room *server_room, int slot);

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
void				server_health_assemble(const t_server *srv, t_body_health *out);

/* CONTROL.C */
int					control_open(t_server *srv);
void				control_close(t_server *srv);
void				control_accept_ready(t_server *srv);
void				control_connection_ready(t_control_connection *conn, uint32_t events);

/* CONTROLIO.C */
void				control_flush(t_control_connection *conn);
void				control_hangup(t_control_connection *conn);
void				control_reply(t_control_connection *conn, unsigned int status, const char *body, size_t body_len, size_t omitted);
void				control_queue(t_control_connection *conn, unsigned char *bytes, size_t len);

/* HANDLERS_ADMIN.C */
void				control_handle_frame(t_control_connection *conn, const unsigned char *frame, size_t len);

# endif
