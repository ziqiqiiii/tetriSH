#include "internal.h"
#include <limits.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>

static int	valid_session(const t_session *sess)
{
	return (sess != NULL && sess->fd >= 0 && sess->established
		&& (sess->role == TETRISSH_ROLE_CLIENT
			|| sess->role == TETRISSH_ROLE_SERVER));
}

static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[9])
{
	tsh_u64_be(seq, aad);
	aad[8] = marker;
}

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

void	session_close(t_session *sess)
{
	if (sess == NULL)
		return;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}
