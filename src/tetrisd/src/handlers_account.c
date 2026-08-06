#include "tetrisd.h"

// Static Functions
static int	read_credentials(t_request_context *ctx, char *username, char *password);
static int	signup_status(t_db_result res);
static int	displace_previous(t_request_context *ctx, t_player_id pid);
static void	bind_identity(t_request_context *ctx, const t_player *player);
static void	to_hex(const unsigned char *bytes, size_t len, char *out);

/**
 * @brief SIGNUP /account - registers a new player.
 *
 * The password never reaches the store: tetrisd salts and hashes it here, so
 * what is written (and what a stolen data directory would expose) is a hash.
 *
 * @param msg The request (unused; the body is read through the context).
 * @param context The request context.
 * @return 201 on success, 409 when taken, 400 on a bad body, 500 otherwise.
 */
int	signup_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	char		username[DB_MAX_USERNAME];
	char		password[TETRISD_PASSWORD_MAX];
	char		salt[DB_SALT_LEN + 1];
	char		hash[DB_HASH_LEN + 1];
	t_player_id	id;
	t_db_result	res;

	(void)msg;
	ctx = context;
	if (read_credentials(ctx, username, password) != 0)
		return (400);
	if (salt_generate(salt, sizeof(salt)) != 0
		|| password_hash(password, salt, hash, sizeof(hash)) != 0)
		return (500);
	res = db_signup(ctx->srv->db, username, hash, salt, &id);
	if (res != DB_OK)
		return (signup_status(res));
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "signup %s -> player %llu",
		username, (unsigned long long)id);
	request_body_printf(ctx, "player-id %llu\nusername %s\n",
		(unsigned long long)id, username);
	return (201);
}

/**
 * @brief LOGIN /session - authenticates and binds the connection to a player.
 *
 * Identity is owned by the connection (ADR-0001): there are no tokens, so the
 * player bound here is the only identity this socket can ever act as, and
 * every later request's Player-Id is checked against it.
 *
 * @param msg The request (unused; the body is read through the context).
 * @param context The request context.
 * @return 200 on success, 401 on bad credentials, 400 on a bad body.
 */
int	login_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	char		username[DB_MAX_USERNAME];
	char		password[TETRISD_PASSWORD_MAX];
	char		salt[DB_SALT_LEN + 1];
	char		hash[DB_HASH_LEN + 1];
	t_player	player;

	(void)msg;
	ctx = context;
	if (ctx->cli->state == CLI_AUTHED)
		return (409);
	if (read_credentials(ctx, username, password) != 0)
		return (400);
	memset(salt, 'x', DB_SALT_LEN);
	salt[DB_SALT_LEN] = '\0';
	db_get_salt(ctx->srv->db, username, salt, sizeof(salt) - 1);
	if (password_hash(password, salt, hash, sizeof(hash)) != 0)
		return (500);
	if (db_login(ctx->srv->db, username, hash, &player) != DB_OK)
	{
		logger_emit(&ctx->srv->log, COREIPC_LOG_WARNING, "login refused for %s",
			username);
		return (401);
	}
	if (displace_previous(ctx, player.player_id) != 0)
		return (503);
	bind_identity(ctx, &player);
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "login %s -> player %llu",
		username, (unsigned long long)player.player_id);
	request_body_printf(ctx, "player-id %llu\nusername %s\nscore %lld\nwallet %lld\n",
		(unsigned long long)player.player_id, player.username,
		(long long)player.leaderboard_score, (long long)player.wallet_points);
	return (200);
}

/**
 * @brief Hashes a password with its account's salt.
 *
 * SHA-256 over salt-then-password, written as the 64 hex characters the store
 * expects. The salt is always DB_SALT_LEN bytes and is never NUL-terminated
 * on the store's side, so its length is fixed rather than measured.
 *
 * @param password The plaintext password.
 * @param salt The account's salt, at least DB_SALT_LEN bytes.
 * @param out Buffer receiving DB_HASH_LEN hex characters plus a terminator.
 * @param cap Size of out.
 * @return 0 on success, -1 on invalid arguments.
 */
int	password_hash(const char *password, const char *salt, char *out,
		size_t cap)
{
	unsigned char	digest[SHA256_DIGEST_LENGTH];
	unsigned char	input[DB_SALT_LEN + TETRISD_PASSWORD_MAX];
	size_t			len;

	if (password == NULL || salt == NULL || out == NULL
		|| cap < DB_HASH_LEN + 1)
		return (-1);
	len = strlen(password);
	if (len == 0 || len >= TETRISD_PASSWORD_MAX)
		return (-1);
	memcpy(input, salt, DB_SALT_LEN);
	memcpy(input + DB_SALT_LEN, password, len);
	SHA256(input, DB_SALT_LEN + len, digest);
	to_hex(digest, sizeof(digest), out);
	return (0);
}

/**
 * @brief Draws a fresh random salt as DB_SALT_LEN hex characters.
 *
 * @param out Buffer receiving the salt plus a terminator.
 * @param cap Size of out.
 * @return 0 on success, -1 when the buffer is too small or entropy failed.
 */
int	salt_generate(char *out, size_t cap)
{
	unsigned char	raw[DB_SALT_LEN / 2];

	if (out == NULL || cap < DB_SALT_LEN + 1)
		return (-1);
	if (RAND_bytes(raw, (int)sizeof(raw)) != 1)
		return (-1);
	to_hex(raw, sizeof(raw), out);
	return (0);
}

/**
 * @brief Reads and range-checks the username and password from the body.
 *
 * @param ctx Request context holding the body.
 * @param username Buffer of DB_MAX_USERNAME bytes.
 * @param password Buffer of TETRISD_PASSWORD_MAX bytes.
 * @return 0 when both are present and usable, -1 otherwise.
 */
static int	read_credentials(t_request_context *ctx, char *username, char *password)
{
	if (request_body_field(ctx, "username", username, DB_MAX_USERNAME) == NULL)
		return (-1);
	if (request_body_field(ctx, "password", password, TETRISD_PASSWORD_MAX) == NULL)
		return (-1);
	if (username[0] == '\0' || password[0] == '\0')
		return (-1);
	return (0);
}

/**
 * @brief Maps a store result onto the status a signup should answer with.
 *
 * @param res Result from db_signup.
 * @return 409 when the name is taken, 400 when malformed, 500 otherwise.
 */
static int	signup_status(t_db_result res)
{
	if (res == DB_EXISTS)
		return (409);
	if (res == DB_INVALID)
		return (400);
	return (500);
}

/**
 * @brief Ends any older connection still acting as the player logging in.
 *
 * A player has one connection at a time (ADR-0004), so this login takes the
 * identity back rather than being turned away - otherwise a client that died
 * without closing its socket would lock its own account out for as long as
 * the kernel takes to notice, which is hours.
 *
 * The wait is the load-bearing part: the displaced connection forfeits its
 * room before it unlinks itself, so returning only once it is gone is what
 * stops that forfeit from evicting the room this connection joins next.
 *
 * @param ctx Request context of the connection claiming the player.
 * @param pid Player being claimed.
 * @return 0 when the player is free to bind, -1 when the old connection would
 *         not go away in time.
 */
static int	displace_previous(t_request_context *ctx, t_player_id pid)
{
	if (!registry_displace(&ctx->srv->reg, pid, ctx->cli))
		return (0);
	logger_emit(&ctx->srv->log, COREIPC_LOG_WARNING,
		"player %llu logged in again; closing the previous connection",
		(unsigned long long)pid);
	if (registry_wait_absent(&ctx->srv->reg, pid, ctx->cli,
			TD_DISPLACE_WAIT_MS) != 0)
	{
		logger_emit(&ctx->srv->log, COREIPC_LOG_ERROR,
			"player %llu still bound after %d ms; refusing the login",
			(unsigned long long)pid, TD_DISPLACE_WAIT_MS);
		return (-1);
	}
	return (0);
}

/**
 * @brief Binds an authenticated player to the connection that logged in.
 *
 * Published through the registry so that the write happens under the same
 * lock every other thread reads the binding beneath.
 *
 * @param ctx Request context holding the client and the registry.
 * @param player The authenticated player.
 */
static void	bind_identity(t_request_context *ctx, const t_player *player)
{
	registry_bind(&ctx->srv->reg, ctx->cli, player->player_id, player->username);
}

/**
 * @brief Writes bytes as lowercase hexadecimal text.
 *
 * @param bytes Input bytes.
 * @param len Number of input bytes.
 * @param out Buffer of at least 2 * len + 1 characters.
 */
static void	to_hex(const unsigned char *bytes, size_t len, char *out)
{
	static const char	digits[] = "0123456789abcdef";
	size_t				i;

	i = 0;
	while (i < len)
	{
		out[i * 2] = digits[bytes[i] >> 4];
		out[i * 2 + 1] = digits[bytes[i] & 0x0F];
		i++;
	}
	out[len * 2] = '\0';
}
