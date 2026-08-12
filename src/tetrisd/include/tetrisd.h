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
** established session back (step 3 of the event-driven migration).
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
** structural (step 5 of the event-driven migration).
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
** inspects and stops it through that file. Both the fork and
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
/*
** The widest body this server builds, which is a Battle Royale STATE: 8192 did
** not hold one, and a full arena silently failing to encode would have shown
** up as a client whose rivals stopped moving rather than as an error.
**
** It is the next power of two above BODY_STATE_MAX_BYTES rather than a number
** chosen to look sufficient, and the assertion below is what keeps the two in
** step - libstatusbody derives its own ceiling from the constants that produce
** it, so a field added to an arena card fails the build here instead of
** overrunning this buffer at run time.
*/
# define TETRISD_BODY_MAX_BYTES					16384
# define TETRISD_CONFIG_LINE_MAX				512
_Static_assert(TETRISD_BODY_MAX_BYTES >= BODY_STATE_MAX_BYTES,
	"TETRISD_BODY_MAX_BYTES cannot hold the widest state body libstatusbody "
	"can produce - raise it to the next power of two above "
	"BODY_STATE_MAX_BYTES");
_Static_assert(TETRISD_BODY_MAX_BYTES <= TETRISD_FRAME_MAX_BYTES,
	"a body that cannot fit in a frame would be built and then refused");
# define TETRISD_OUTBOX_CAPACITY				32
/*
** The chat lane is its own ring and its own size. It is small because a
** client that has fallen this far behind wants the newest of the feed, not
** all of it, and because chat must never be able to crowd out a response.
*/
# define TETRISD_CHAT_CAPACITY					16
/*
** Chat's own token bucket, in whole messages. It is deliberately generous:
** its job is to keep a flood off the reactor, not to pace a conversation, and
** a room's feed is already bounded by the drop-oldest ring above. Sized too
** tightly it refuses the sixth line somebody sends in a second, which is
** ordinary use rather than abuse.
**
** A constant rather than a config key because it bounds a person typing, which
** does not vary between deployments the way an input budget does.
*/
# define TETRISD_CHAT_BURST						20
# define TETRISD_CHAT_RATE_PER_SEC				5
/*
** Twice the field a message ends up in, so a line that is merely too long is
** read whole and can be told apart from one carrying a control character. A
** line longer even than this is refused as unsendable, which is honest: at
** that point the server has not read enough of it to say why.
*/
# define TETRISD_CHAT_TEXT_RAW_MAX				(BODY_CHAT_TEXT_MAX * 2)
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

/*
** Abilities that can be waiting on one player's lock at once. Four is the
** whole of a rival's meter spent without a single piece landing, which is the
** worst a Double match can do; past that the oldest is dropped rather than the
** newest refused, because the newest is the one the sender just paid for.
*/
# define TD_MAX_PENDING							8

/*
** Rows Pentaris sends (docs/use_cases.md). It is written here rather than in
** libtetrisbrain because it is a property of one character's level 3, not of
** what a garbage row is.
*/
# define TETRISD_PENTARIS_ROWS					5

/*
** Cells Bomb destroys on the Target's field. Enough to matter and few enough
** that the board is still the one the player was building.
*/
# define TETRISD_BOMB_CELLS						12

/*
** How many of the Target's pieces Dark and Pals last. Four matches Nue and
** Thwack, the two piece-counted effects libtetrisbrain does time itself, so
** "a limited time" means the same length whoever is counting it.
*/
# define TETRISD_DARK_PIECES						4
# define TETRISD_PALS_PIECES						4

/*
** Game points that buy one wallet point (docs/game-economics.md). It is the
** whole of the economy's exchange rate, and it is charged against a player's
** running total rather than each game on its own - see room.c's award_game.
*/
# define TETRISD_POINTS_PER_WALLET_POINT			100

/*
** How long a dealt match is held still before it begins.
**
** Two players have to start on the same tick, and neither client can arrange
** that for itself: each reaches its match screen at a different moment, and
** PAUSE - which is how Solo freezes the board for its own 3-2-1 - is refused
** in a room with anybody else in it precisely because one player stopping
** their own clock is an advantage. So the hold belongs to the room, and the
** client draws its countdown from the number this produces rather than from a
** timer of its own.
**
** Single does not take one: its client already runs a 3-2-1 of its own and
** moving it would change a mode this step is not touching.
*/
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

/*
** How many leaderboard lines one answer carries. The store's skip list is
** ordered by (score, id), so this is a top-N read and not a page: a client
** that wanted the whole table would be asking for a different route.
*/
# define TETRISD_LEADERBOARD_ROWS				10

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
** Three lanes, because three kinds of message fail differently.
**
** A bounded FIFO of responses: a response belongs to a request the client is
** waiting on, so losing one is not an option and overflow closes the client
** instead (it cannot keep up).
**
** A one-slot mailbox holding the latest STATE: a snapshot supersedes the one
** before it, so it overwrites and a stalled client loses intermediate frames
** but never holds up the tick that produced them.
**
** A small ring of chat: best-effort by specification (UC-09 E1), so it drops
** its oldest message and **never closes the client**. It cannot share the
** response FIFO - a room narrating a Battle Royale's knockouts would fill it
** and the next genuine response would kill a connection whose only fault was
** being slow, which is the opposite of what that FIFO's rule is for.
**
** The split is the load-bearing idea and has nothing to do with threading,
** which is why it outlived the writer thread, the condition variable and the
** mutex unchanged.
**
** Three lanes means there is no total order between a response, a snapshot
** and a chat line. Chat is ordered within itself and against nothing else,
** which is why every message carries its own `seq`.
*/
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

/*
** One player's game: the aggregate libtetrisbrain deliberately does not own.
** The reactor is its only writer, which is now the whole of the rule.
*/
/*
** What one queued ability is, waiting on a Target's lock.
**
** `kind` names the transform rather than the ability, because several
** abilities reduce to the same thing done to a board - and because the
** ability that queued it belongs to the sender, whose character the receiver
** has no business knowing.
*/
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
	/*
	** The hold slot, which libtetrisbrain deliberately does not own: it is a
	** rule about a session, not about a board. `hold_used` is what makes hold
	** a swap rather than a shuffle - it is set on every hold and cleared only
	** by a lock, so a piece can be held once and no more.
	*/
	int					hold;
	bool				has_hold;
	bool				hold_used;
	int					lines;
	int					level;
	/*
	** How many snapshots this game has had pushed. It counts STATE frames,
	** not events: docs/tetrisu-local-to-tetrisd.md asks for a monotonically
	** increasing sequence number so a client can ignore a replayed or
	** out-of-order snapshot, and a counter that only moved on interesting
	** events would let two different boards share a number - which is
	** exactly the case that check cannot catch. room.c bumps it once, at the
	** one place a snapshot is taken. The number on the wire is not this one,
	** though: it is the owning connection's state_seq, because the client's
	** staleness check numbers a stream that outlives any one game.
	*/
	uint64_t			seq;
	int					accum_ms;
	/*
	** How long the piece that has landed still belongs to the player.
	**
	** Guideline Extended Placement: a piece does not lock the instant it
	** touches down, it locks half a second later, and moving or rotating it
	** buys that half-second back fifteen times over. It is what makes it
	** possible to slide a piece into a gap under an overhang instead of only
	** dropping it onto one, so it is a rule of the game and belongs to
	** whoever owns the board - here.
	*/
	t_lockdown			lockdown;
	/*
	** `active` is "this game is still being played" and is what decides
	** whether the room is over; `paused` is "it is being played, but not
	** right now". They have to be separate flags: a pause that cleared
	** `active` would read as a finished game and the room would record the
	** score and evict the player who asked for a breather.
	*/
	bool				active;
	bool				paused;
	bool				topped_out;
	bool				recorded;
	t_player_id			player_id;
	/* the seed this game was dealt, so a restart can deal the next one */
	uint32_t			seed;
	t_body_ability		last_ability;
	t_body_clear_label	last_clear;
	/*
	** The completed rows, still on the board, waiting to be taken away.
	**
	** A clear is not instantaneous: for clear_duration_ms the rows are there,
	** no piece has spawned, and the player cannot act. That is a rule and not
	** a flourish, which is why it is the server holding it - a client that
	** animated a clear the server had already finished would be drawing rows
	** that were gone (docs/bugs/the_line_clear_never_reached_the_client.md).
	**
	** clearing_count is 0 whenever no clear is in progress, and it is the
	** whole of the "am I clearing?" question.
	*/
	int					clearing_rows[BODY_CLEARING_MAX];
	int					clearing_count;
	int					clearing_ms;
	/*
	** Garbage rows owed to this player, and not yet on their board.
	**
	** They land at the next piece lock and never on arrival. That is a rule
	** of the game rather than a scheduling convenience: injecting rows under
	** an active piece raises the stack beneath it and can produce a board
	** piece_is_valid would reject, so there is no correct thing to do with
	** the piece already in the air. Waiting for the lock means the rows are
	** always part of the board the *next* piece is validated against, and a
	** spawn that then fails is a top-out, which is the right outcome of being
	** buried.
	**
	** It is also the receiver's warning: the count is on the wire from the
	** moment it is queued, so a player can see what is coming and clear
	** underneath it.
	*/
	int					pending_garbage;
	/*
	** Garbage an ability sent, kept apart from the ordinary kind because Pals
	** treats the two differently: "incoming ordinary garbage lowers the
	** Player's stack instead of raising it; garbage created by abilities is
	** excluded" (docs/use_cases.md). One counter could not tell them apart, so
	** Pals would either absorb Pentaris - which the text forbids - or absorb
	** nothing.
	*/
	int					pending_ability_garbage;
	/*
	** Rows Fry burned off this player's own floor and has not yet passed on.
	** Fry is two halves and only the first was built: the rows go in, and at
	** the next lock they clear *and are sent to the Target*. The burn is
	** consumed by effect_on_piece_lock, so the count is taken before that runs
	** and handed to room.c, which is the only module that knows who the Target
	** is.
	*/
	int					fry_owed;
	/*
	** How many more of this player's pieces Dark and Pals last.
	**
	** libtetrisbrain deliberately leaves both open-ended - its comment says
	** "Dark/Pals/Mirror stay on until effect_clear, the server decides when
	** they end" - so this is the server deciding. Without it "for a limited
	** time" would be forever, and a single Dark would end the game.
	*/
	int					dark_pieces;
	int					pals_pieces;
	/*
	** Which column the next garbage row leaves open. Derived from a counter
	** rather than drawn, so the hole walks instead of stacking - the same
	** fairness trick apply_fry uses, and for the same reason: no randomness
	** enters libtetrisbrain, and a run of rows with the hole in one place
	** would be a wall rather than a handicap.
	*/
	uint32_t			garbage_seq;
	/*
	** Lines this game has cleared that have not yet been charged to anybody.
	**
	** room.c takes it after each tick and turns it into garbage against the
	** Target, because who the Target is depends on the room and a game does
	** not know it is in one. It accumulates rather than being overwritten: a
	** tick that ran two clear completions owes both.
	*/
	int					cleared_owed;
	/*
	** The character this game is being played with, or 0 when the player did
	** not declare one and the account's equipped character stands. It decides
	** which four abilities each level selects from, and it is copied in once
	** at deal time so an EQUIP made mid-match cannot change it.
	*/
	t_item_id			character_id;
	/*
	** Abilities aimed at this player, waiting for their next piece lock.
	**
	** They wait for the same reason garbage does, and the reason is stronger
	** here: a board transform landing under an active piece can leave that
	** piece inside the stack, and a status effect landing mid-piece would
	** take hold of a piece already in the air - so the count of pieces it is
	** meant to last would be short by one before it started. At the lock
	** there is no piece, which is what makes the lock the safe point.
	**
	** Only effects that land on somebody *else* queue. An ability that lands
	** on the player who used it - Mirror, Pals, Copy, Vampire - is applied at
	** once, exactly like the self-affecting four, because there is no second
	** board to be surprised.
	*/
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
	int				rank;
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
*/
typedef struct s_ability_def
{
	t_item_id	character_id;
	int			level;
	const char	*name;
	bool		needs_target;
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
	/*
	** Chat's own bucket, and it is deliberately not the input one. Sharing a
	** budget meant a line of chat cost a piece movement and a busy match could
	** answer 429 to somebody typing, which is two unrelated floods policed by
	** one number. A person types far slower than they press, so this one is
	** sized in whole messages rather than from .tetrishrc.
	*/
	int				chat_tokens;
	uint64_t		chat_tokens_at_ms;
	/*
	** The number stamped on the last STATE frame this connection was sent.
	** The client's staleness check is per connection, so the counter is per
	** connection too: it survives the game and the room that produced the
	** frames, both of which a finished match destroys. Numbering from the
	** game instead reset the stream to zero on every new game, and a client
	** that had not left since the last one dropped the whole of the next as
	** replayed frames.
	*/
	uint64_t		state_seq;
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

/*
** Which catalogue a store request addresses. The two kinds are bought and
** equipped by different store calls but through identical paths, so the path
** parser reports the kind rather than each handler spelling out both routes.
*/
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
int				outbox_push_chat(t_outbox *ob, unsigned char *bytes, size_t len);
int				outbox_pop(t_outbox *ob, t_outbound_message *out);
bool			outbox_idle(t_outbox *ob);
void			outbox_drop_room_pushes(t_outbox *ob);
void			outbox_close(t_outbox *ob);
void			outbox_destroy(t_outbox *ob);

/* REGISTRY.C */
int				registry_init(t_registry *rg, size_t cap);
int				registry_add(t_registry *rg, t_client *cli);
void			registry_remove(t_registry *rg, t_client *cli);
int				registry_enqueue(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len, bool is_state);
int				registry_enqueue_chat(t_registry *rg, t_player_id pid, unsigned char *bytes, size_t len);
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

/* HANDLERS_READY.C */
int				ready_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_CHAT.C */
int				chat_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_LEADERBOARD.C */
int				leaderboard_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_PROFILE.C */
int				profile_handler(const t_htttp_message *msg, void *context);
int				request_profile_body(t_request_context *ctx);

/* HANDLERS_STORE.C */
int				buy_handler(const t_htttp_message *msg, void *context);
int				equip_handler(const t_htttp_message *msg, void *context);
int				store_list_catalogue(t_request_context *ctx);

/* REQUEST_TARGET.C */
int				request_input_target(t_request_context *ctx, t_server_room **out);
const char		*request_room_name(const t_request_context *ctx);
t_player_id		request_player_id(const char *text, const char **end);
int				request_body_token(t_request_context *ctx, char *out, size_t cap);
bool			rate_limit_take_token(t_client *cli);
bool			rate_limit_take_chat_token(t_client *cli);
int				rate_limit_refill_level(int tokens, uint64_t elapsed_ms,
					int cap, int rate);

/* HANDLERS_INPUT.C */
int				move_handler(const t_htttp_message *msg, void *context);
int				rotate_handler(const t_htttp_message *msg, void *context);
int				drop_handler(const t_htttp_message *msg, void *context);
int				hold_handler(const t_htttp_message *msg, void *context);

/* HANDLERS_GAME.C */
int				pause_handler(const t_htttp_message *msg, void *context);
int				restart_handler(const t_htttp_message *msg, void *context);
int				ability_handler(const t_htttp_message *msg, void *context);

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
void			game_queue_garbage(t_game *g, int lines);
void			game_queue_ability_garbage(t_game *g, int lines);
int				game_take_fry(t_game *g);
void			game_queue_ability(t_game *g, t_pending_kind kind,
					int argument);
int				game_take_cleared(t_game *g);

/* ABILITY_CTRL.C */
const t_ability_def	*ability_lookup(t_item_id character_id, int level);
bool			ability_is_playable_solo(const t_ability_def *def);
t_ability_verdict	game_ability(t_game *g, t_game *target,
					const t_ability_def *def, int argument);
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
bool			server_room_starts_on_ready(const t_server_room *server_room);
int				server_room_target_of(t_server_room *server_room,
					int from_slot);
t_game			*server_room_target_game(t_server_room *server_room,
					const t_client *cli);
t_game			*server_room_game_of(t_server_room *server_room, const t_client *cli);
void			server_room_mark_dirty(t_server_room *server_room, const t_client *cli);
void			server_room_forfeit(t_server *srv, t_client *cli);
bool			server_room_describe(const t_server_room *server_room, t_server_room_view *out);
bool			server_room_snapshot(const t_server_room *server_room,
					t_body_room *out);
bool			server_room_is_muted(const t_server_room *server_room, t_player_id pid);
const t_game	*server_room_game_at(const t_server_room *server_room, int slot);

/* NARRATE.C */
bool			room_chat_broadcast(t_server_room *server_room, t_body_chat *chat);
void			room_narrate(t_server_room *server_room, const char *fmt, ...)
					__attribute__((format(printf, 2, 3)));

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
