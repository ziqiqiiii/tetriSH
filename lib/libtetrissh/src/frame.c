#include "internal.h"

// Static Functions
static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[SESSIONIO_AAD_LEN]);
static int	gcm_encrypt(const t_session *sess, const unsigned char *plain, size_t plain_len, unsigned char *frame);
static int	gcm_decrypt(const t_session *sess, const unsigned char *frame, size_t plain_len, unsigned char *plain);

/**
 * @brief Encrypts one plaintext into a caller-owned frame buffer.
 *
 * Writes nonce, tag, and ciphertext into frame and allocates nothing, so the
 * caller keeps both the buffer and the socket. The send sequence number
 * advances on every sealed frame; a frame that is sealed and then not written
 * consumes its number, which is correct because a failed write ends the
 * connection rather than resynchronising it.
 *
 * @param sess The established session; the descriptor is not used.
 * @param plain Plaintext to encrypt; may be null only when plain_len is 0.
 * @param plain_len Plaintext length; must not exceed TETRISSH_MAX_PLAINTEXT.
 * @param frame Destination for the frame bytes.
 * @param frame_cap Capacity of frame; must be at least plain_len plus
 *        TETRISSH_FRAME_OVERHEAD.
 * @return The frame byte count on success, -1 on invalid state, oversized
 *         input, insufficient capacity, or crypto failure.
 */
ssize_t	session_frame_seal(t_session *sess, const void *plain, size_t plain_len,
		void *frame, size_t frame_cap)
{
	if (!session_frame_ready(sess) || frame == NULL || (plain == NULL && plain_len != 0)
		|| plain_len > TETRISSH_MAX_PLAINTEXT
		|| frame_cap < plain_len + TETRISSH_FRAME_OVERHEAD)
		return (-1);
	if (gcm_encrypt(sess, plain, plain_len, frame) != 0)
		return (-1);
	sess->send_seq++;
	return ((ssize_t)(plain_len + TETRISSH_FRAME_OVERHEAD));
}

/**
 * @brief Authenticates one frame and writes its plaintext to the caller.
 *
 * Verifies the GCM tag against the expected sequence and direction before the
 * plaintext is accepted, and allocates nothing. A frame that fails to
 * authenticate leaves no readable plaintext behind: the destination is wiped
 * and the receive sequence number does not advance, so the next frame is still
 * checked against the number this one failed at.
 *
 * @param sess The established session; the descriptor is not used.
 * @param frame The complete frame bytes, length prefix already stripped.
 * @param frame_len The frame length in bytes.
 * @param plain Destination for the decrypted plaintext.
 * @param plain_cap Capacity of plain in bytes.
 * @return The plaintext byte count on success, -1 on invalid state, a
 *         malformed frame, insufficient capacity, or authentication failure.
 */
ssize_t	session_frame_open(t_session *sess, const void *frame, size_t frame_len,
		void *plain, size_t plain_cap)
{
	size_t	plain_len;

	if (!session_frame_ready(sess) || frame == NULL || plain == NULL
		|| frame_len < TETRISSH_FRAME_OVERHEAD || frame_len > TETRISSH_MAX_FRAME)
		return (-1);
	plain_len = frame_len - TETRISSH_FRAME_OVERHEAD;
	if (plain_len > plain_cap)
		return (-1);
	if (gcm_decrypt(sess, frame, plain_len, plain) != 0)
	{
		OPENSSL_cleanse(plain, plain_len);
		return (-1);
	}
	sess->recv_seq++;
	return ((ssize_t)plain_len);
}

/**
 * @brief Reports whether a session can seal or open frames.
 *
 * Deliberately ignores fd: the frame codec touches no descriptor, so a session
 * detached from its socket still holds everything a frame needs.
 *
 * @param sess The session to check; may be null.
 * @return Non-zero if sess is non-null, established, and holds a client or
 *         server role; 0 otherwise.
 */
int	session_frame_ready(const t_session *sess)
{
	return (sess != NULL && sess->established
			&& (sess->role == TETRISSH_ROLE_CLIENT
			|| sess->role == TETRISSH_ROLE_SERVER));
}

/**
 * @brief Builds the 9-byte GCM additional authenticated data.
 *
 * Binds the sequence number and direction marker into the GCM tag so replayed
 * or reflected ciphertext fails before any plaintext is accepted.
 *
 * @param seq The per-direction sequence number.
 * @param marker The direction marker byte.
 * @param aad Destination buffer of exactly SESSIONIO_AAD_LEN bytes.
 */
static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[SESSIONIO_AAD_LEN])
{
	sessionio_u64_be(seq, aad);
	aad[8] = marker;
}

/**
 * @brief Encrypts plaintext in place into an already-sized frame buffer.
 *
 * Fills a fresh random nonce, encrypts under the session key with
 * sequence/direction as AAD, and stores the tag between the two. The frame is
 * wiped on any failure so no partial ciphertext is left for a caller to send.
 *
 * @param sess The established session providing key, sequence, and role.
 * @param plain Plaintext bytes to encrypt.
 * @param plain_len Plaintext length.
 * @param frame Destination of at least plain_len plus TETRISSH_FRAME_OVERHEAD.
 * @return 0 on success, -1 on nonce or crypto failure.
 */
static int	gcm_encrypt(const t_session *sess, const unsigned char *plain, size_t plain_len, unsigned char *frame)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	aad[SESSIONIO_AAD_LEN];
	int				out_len;
	int				final_len;

	if (RAND_bytes(frame, TETRISSH_GCM_NONCE_LEN) != 1)
		return (-1);
	build_aad(sess->send_seq, sessionio_send_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (-1);
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_EncryptInit_ex(ctx, NULL, NULL, sess->aes_key, frame) != 1
		|| EVP_EncryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_EncryptUpdate(ctx, frame + TETRISSH_FRAME_OVERHEAD, &out_len,
			plain, (int)plain_len) != 1
		|| EVP_EncryptFinal_ex(ctx, frame + TETRISSH_FRAME_OVERHEAD + out_len,
			&final_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
			TETRISSH_GCM_TAG_LEN, frame + TETRISSH_GCM_NONCE_LEN) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		OPENSSL_cleanse(frame, TETRISSH_FRAME_OVERHEAD + plain_len);
		return (-1);
	}
	EVP_CIPHER_CTX_free(ctx);
	return (0);
}

/**
 * @brief Decrypts a frame's ciphertext and verifies its tag.
 *
 * Reconstructs the expected sequence/direction AAD and checks the tag last, so
 * a caller that honours the return value never acts on unauthenticated bytes.
 * The destination may hold plaintext-shaped garbage after a failure; wiping it
 * is the caller's job.
 *
 * @param sess The established session providing key, sequence, and role.
 * @param frame The received frame bytes.
 * @param plain_len The plaintext length implied by the frame length.
 * @param plain Destination of at least plain_len bytes.
 * @return 0 on success, -1 on crypto or authentication failure.
 */
static int	gcm_decrypt(const t_session *sess, const unsigned char *frame,
		size_t plain_len, unsigned char *plain)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	aad[SESSIONIO_AAD_LEN];
	int				out_len;
	int				final_ok;

	build_aad(sess->recv_seq, sessionio_recv_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (-1);
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_DecryptInit_ex(ctx, NULL, NULL, sess->aes_key, frame) != 1
		|| EVP_DecryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_DecryptUpdate(ctx, plain, &out_len,
			frame + TETRISSH_FRAME_OVERHEAD, (int)plain_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TETRISSH_GCM_TAG_LEN,
			(void *)(frame + TETRISSH_GCM_NONCE_LEN)) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		return (-1);
	}
	final_ok = EVP_DecryptFinal_ex(ctx, plain + out_len, &out_len);
	EVP_CIPHER_CTX_free(ctx);
	if (final_ok != 1)
		return (-1);
	return (0);
}
