#include "internal.h"

// Static Functions
static int	valid_session(const t_session *sess);
static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[9]);
static int	gcm_encrypt(t_session *sess, const unsigned char *plain,
				size_t plain_len, unsigned char **frame, uint32_t *frame_len);
static int	gcm_decrypt(t_session *sess, const unsigned char *frame,
				uint32_t frame_len, unsigned char **plain, size_t *plain_len);

/**
 * @brief Encrypts one plaintext message and writes it as a single frame.
 *
 * Builds an AES-256-GCM frame over the plaintext and writes its length prefix
 * followed by the frame bytes. The send sequence number is incremented only on
 * a fully written frame, keeping both endpoints' counters in step.
 *
 * @param sess The established session; must be valid and established.
 * @param buf Plaintext to send; may be null only when len is 0.
 * @param len Plaintext length; must not exceed TETRISSH_MAX_PLAINTEXT.
 * @return The plaintext byte count on success, -1 on invalid state, oversized
 *         input, crypto failure, or socket failure.
 */
ssize_t	session_send(t_session *sess, const void *buf, size_t len)
{
	unsigned char	*frame;
	uint32_t		frame_len;

	if (!valid_session(sess) || (buf == NULL && len != 0)
		|| len > TETRISSH_MAX_PLAINTEXT)
		return (-1);
	if (gcm_encrypt(sess, buf, len, &frame, &frame_len) != 0)
		return (-1);
	if (tsh_write_u32(sess->fd, frame_len) != 0
		|| tsh_write_exact(sess->fd, frame, frame_len) != 0)
	{
		OPENSSL_cleanse(frame, frame_len);
		free(frame);
		return (-1);
	}
	OPENSSL_cleanse(frame, frame_len);
	free(frame);
	sess->send_seq++;
	return ((ssize_t)len);
}

/**
 * @brief Reads and authenticates one frame, returning its plaintext.
 *
 * Reads the length prefix and frame body, decrypts and verifies the GCM tag
 * against the expected sequence and direction, and copies the plaintext out.
 * The receive sequence number is incremented only on a fully accepted frame.
 *
 * @param sess The established session; must be valid and established.
 * @param buf Destination for the decrypted plaintext.
 * @param max_len Capacity of buf in bytes.
 * @return The plaintext byte count on success, 0 on clean EOF before the next
 *         frame prefix, -1 on invalid state, malformed frame, authentication
 *         failure, insufficient capacity, or socket failure.
 */
ssize_t	session_recv(t_session *sess, void *buf, size_t max_len)
{
	uint32_t		frame_len;
	unsigned char	*frame;
	unsigned char	*plain;
	size_t			plain_len;
	t_tsh_io_result	res;

	if (!valid_session(sess) || buf == NULL)
		return (-1);
	res = tsh_read_u32(sess->fd, &frame_len);
	if (res == TSH_IO_EOF)
		return (0);
	if (res != TSH_IO_OK || frame_len < TSH_FRAME_OVERHEAD
		|| frame_len > TSH_FRAME_OVERHEAD + TETRISSH_MAX_PLAINTEXT)
		return (-1);
	frame = malloc(frame_len);
	if (frame == NULL)
		return (-1);
	plain = NULL;
	if (tsh_read_exact(sess->fd, frame, frame_len) != TSH_IO_OK
		|| gcm_decrypt(sess, frame, frame_len, &plain, &plain_len) != 0)
	{
		OPENSSL_cleanse(frame, frame_len);
		free(frame);
		return (-1);
	}
	OPENSSL_cleanse(frame, frame_len);
	free(frame);
	if (plain_len > max_len)
	{
		OPENSSL_cleanse(plain, plain_len);
		free(plain);
		return (-1);
	}
	memcpy(buf, plain, plain_len);
	OPENSSL_cleanse(plain, plain_len);
	free(plain);
	sess->recv_seq++;
	return ((ssize_t)plain_len);
}

/**
 * @brief Wipes cryptographic state and marks the session unusable.
 *
 * Cleanses the AES key, clears the sequence counters and establishment flag,
 * and sets fd to -1. It does not call close(2); the caller owns the descriptor.
 *
 * @param sess The session to reset; a null pointer is ignored.
 */
void	session_close(t_session *sess)
{
	if (sess == NULL)
		return ;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}

/**
 * @brief Reports whether a session is ready for frame I/O.
 *
 * @param sess The session to check; may be null.
 * @return Non-zero if sess is non-null, established, has a valid descriptor,
 *         and holds a client or server role; 0 otherwise.
 */
static int	valid_session(const t_session *sess)
{
	return (sess != NULL && sess->fd >= 0 && sess->established
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
 * @param aad Destination buffer of exactly 9 bytes.
 */
static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[9])
{
	tsh_u64_be(seq, aad);
	aad[8] = marker;
}

/**
 * @brief Encrypts plaintext into a freshly allocated GCM frame.
 *
 * Allocates the frame, fills a random nonce, and encrypts under the session
 * key with sequence/direction as AAD. On success *frame owns cleansed-on-free
 * memory the caller must free.
 *
 * @param sess The established session providing key, sequence, and role.
 * @param plain Plaintext bytes to encrypt.
 * @param plain_len Plaintext length.
 * @param frame Out-parameter receiving the allocated frame buffer.
 * @param frame_len Out-parameter receiving the frame length.
 * @return 0 on success, -1 on oversized input, allocation, or crypto failure.
 */
static int	gcm_encrypt(t_session *sess, const unsigned char *plain,
		size_t plain_len, unsigned char **frame, uint32_t *frame_len)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	*token;
	unsigned char	aad[9];
	int				out_len;
	int				final_len;

	if (plain_len > TETRISSH_MAX_PLAINTEXT || plain_len > UINT32_MAX)
		return (-1);
	*frame_len = (uint32_t)(TSH_FRAME_OVERHEAD + plain_len);
	token = malloc(*frame_len);
	if (token == NULL)
		return (-1);
	if (RAND_bytes(token, TSH_GCM_NONCE_LEN) != 1)
		return (free(token), -1);
	build_aad(sess->send_seq, tsh_send_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (free(token), -1);
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_EncryptInit_ex(ctx, NULL, NULL, sess->aes_key, token) != 1
		|| EVP_EncryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_EncryptUpdate(ctx, token + TSH_FRAME_OVERHEAD, &out_len,
			plain, (int)plain_len) != 1
		|| EVP_EncryptFinal_ex(ctx, token + TSH_FRAME_OVERHEAD + out_len,
			&final_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
			TSH_GCM_TAG_LEN, token + TSH_GCM_NONCE_LEN) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		OPENSSL_cleanse(token, *frame_len);
		free(token);
		return (-1);
	}
	EVP_CIPHER_CTX_free(ctx);
	*frame = token;
	return (0);
}

/**
 * @brief Decrypts and authenticates a received GCM frame.
 *
 * Reconstructs the expected sequence/direction AAD, verifies the GCM tag, and
 * on success returns freshly allocated plaintext the caller must free. A failed
 * tag or malformed frame yields no plaintext.
 *
 * @param sess The established session providing key, sequence, and role.
 * @param frame The received frame bytes.
 * @param frame_len The frame length in bytes.
 * @param plain Out-parameter receiving the allocated plaintext buffer.
 * @param plain_len Out-parameter receiving the plaintext length.
 * @return 0 on success, -1 on malformed frame, allocation, or authentication
 *         failure.
 */
static int	gcm_decrypt(t_session *sess, const unsigned char *frame,
		uint32_t frame_len, unsigned char **plain, size_t *plain_len)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	*out;
	unsigned char	aad[9];
	int				out_len;
	int				final_ok;

	if (frame_len < TSH_FRAME_OVERHEAD)
		return (-1);
	*plain_len = (size_t)frame_len - TSH_FRAME_OVERHEAD;
	if (*plain_len > TETRISSH_MAX_PLAINTEXT)
		return (-1);
	out = malloc(*plain_len == 0 ? 1 : *plain_len);
	if (out == NULL)
		return (-1);
	build_aad(sess->recv_seq, tsh_recv_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (free(out), -1);
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_DecryptInit_ex(ctx, NULL, NULL, sess->aes_key, frame) != 1
		|| EVP_DecryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_DecryptUpdate(ctx, out, &out_len,
			frame + TSH_FRAME_OVERHEAD, (int)*plain_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
			TSH_GCM_TAG_LEN, (void *)(frame + TSH_GCM_NONCE_LEN)) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		OPENSSL_cleanse(out, *plain_len);
		free(out);
		return (-1);
	}
	final_ok = EVP_DecryptFinal_ex(ctx, out + out_len, &out_len);
	EVP_CIPHER_CTX_free(ctx);
	if (final_ok != 1)
		return (OPENSSL_cleanse(out, *plain_len), free(out), -1);
	*plain = out;
	return (0);
}
