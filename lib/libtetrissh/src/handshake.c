#include "internal.h"

// Static Functions
static int	read_file_bytes(const char *path, unsigned char **out,
				uint32_t *len);
static void	init_session(t_session *sess, int fd, t_tetrissh_role role);

/**
 * @brief Runs the server side of the secure-session handshake.
 *
 * Reads the client nonce, sends the certificate and an RSA-PSS signature over
 * that nonce, then receives and unwraps the RSA-OAEP session key. RSA blob
 * lengths must match the configured key size so a peer cannot force unbounded
 * reads. Any failure runs one cleanup path that frees state, wipes secrets,
 * and resets the session so partial handshakes fail closed.
 *
 * @param fd The connected socket descriptor.
 * @param sess Session to populate; wiped and reset on failure.
 * @param cert_path Path to the server certificate (PEM).
 * @param key_path Path to the server private key.
 * @return 0 on success, -1 on failure.
 */
int	session_handshake_server(int fd, t_session *sess,
		const char *cert_path, const char *key_path)
{
	unsigned char	client_nonce[TSH_NONCE_LEN];
	unsigned char	*cert_bytes;
	unsigned char	*sig;
	unsigned char	*wrapped;
	unsigned char	*key_plain;
	uint32_t		cert_len;
	uint32_t		sig_len_u32;
	uint32_t		wrapped_len;
	size_t			sig_len;
	size_t			key_len;
	EVP_PKEY		*priv;
	int				ok;
	int				key_size;

	cert_bytes = NULL;
	sig = NULL;
	wrapped = NULL;
	key_plain = NULL;
	priv = NULL;
	wrapped_len = 0;
	key_len = 0;
	ok = -1;
	key_size = 0;
	if (sess == NULL)
		return (-1);
	init_session(sess, fd, TETRISSH_ROLE_SERVER);
	if (fd < 0 || cert_path == NULL || key_path == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, client_nonce, sizeof(client_nonce)) != TSH_IO_OK)
		goto cleanup;
	if (read_file_bytes(cert_path, &cert_bytes, &cert_len) != 0)
		goto cleanup;
	priv = load_private_key(key_path);
	if (priv == NULL)
		goto cleanup;
	key_size = EVP_PKEY_size(priv);
	if (key_size <= 0)
		goto cleanup;
	sig = sign_message_pss(priv, client_nonce, sizeof(client_nonce), &sig_len);
	if (sig == NULL || sig_len > UINT32_MAX)
		goto cleanup;
	sig_len_u32 = (uint32_t)sig_len;
	if (tsh_write_u32(fd, cert_len) != 0
		|| tsh_write_exact(fd, cert_bytes, cert_len) != 0
		|| tsh_write_u32(fd, sig_len_u32) != 0
		|| tsh_write_exact(fd, sig, sig_len_u32) != 0)
		goto cleanup;
	if (tsh_read_u32(fd, &wrapped_len) != TSH_IO_OK
		|| wrapped_len != (uint32_t)key_size)
		goto cleanup;
	wrapped = malloc(wrapped_len);
	if (wrapped == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, wrapped, wrapped_len) != TSH_IO_OK)
		goto cleanup;
	key_plain = rsa_decrypt_block(priv, wrapped, wrapped_len, &key_len, 1);
	if (key_plain == NULL || key_len != TETRISSH_KEY_LEN)
		goto cleanup;
	memcpy(sess->aes_key, key_plain, TETRISSH_KEY_LEN);
	sess->established = 1;
	ok = 0;

	cleanup:
		EVP_PKEY_free(priv);
		free(cert_bytes);
		free(sig);
		if (wrapped != NULL)
			OPENSSL_cleanse(wrapped, wrapped_len);
		free(wrapped);
		if (key_plain != NULL)
			OPENSSL_cleanse(key_plain, key_len);
		free(key_plain);
		OPENSSL_cleanse(client_nonce, sizeof(client_nonce));
		if (ok != 0)
			session_close(sess);
		return (ok);
}

/**
 * @brief Runs the client side of the secure-session handshake.
 *
 * Sends a fresh nonce, receives and verifies the server certificate chain and
 * its RSA-PSS signature over that nonce, then wraps a fresh AES key with
 * RSA-OAEP and sends it. Signature and wrapped-key lengths must equal the
 * certified key operation size. Any failure runs one cleanup path that frees
 * state, wipes secrets, and resets the session.
 *
 * @param fd The connected socket descriptor.
 * @param sess Session to populate; wiped and reset on failure.
 * @param ca_path Path to the CA used to verify the server certificate.
 * @return 0 on success, -1 on failure.
 */
int	session_handshake_client(int fd, t_session *sess, const char *ca_path)
{
	unsigned char	client_nonce[TSH_NONCE_LEN];
	unsigned char	*cert_bytes;
	unsigned char	*sig;
	unsigned char	*wrapped;
	uint32_t		cert_len;
	uint32_t		sig_len;
	size_t			wrapped_len;
	X509			*cert;
	EVP_PKEY		*pub;
	int				ok;
	int				key_size;

	cert_bytes = NULL;
	sig = NULL;
	wrapped = NULL;
	cert = NULL;
	pub = NULL;
	wrapped_len = 0;
	ok = -1;
	key_size = 0;
	if (sess == NULL)
		return (-1);
	init_session(sess, fd, TETRISSH_ROLE_CLIENT);
	if (fd < 0 || ca_path == NULL)
		goto cleanup;
	if (RAND_bytes(client_nonce, sizeof(client_nonce)) != 1)
		goto cleanup;
	if (tsh_write_exact(fd, client_nonce, sizeof(client_nonce)) != 0)
		goto cleanup;
	if (tsh_read_u32(fd, &cert_len) != TSH_IO_OK || cert_len == 0
		|| cert_len > TSH_MAX_CERT_LEN)
		goto cleanup;
	cert_bytes = malloc(cert_len);
	if (cert_bytes == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, cert_bytes, cert_len) != TSH_IO_OK)
		goto cleanup;
	cert = load_cert_bytes(cert_bytes, (int)cert_len);
	if (cert == NULL || verify_server_cert(cert, ca_path) != 1)
		goto cleanup;
	pub = X509_get_pubkey(cert);
	if (pub == NULL)
		goto cleanup;
	key_size = EVP_PKEY_size(pub);
	if (key_size <= 0)
		goto cleanup;
	if (tsh_read_u32(fd, &sig_len) != TSH_IO_OK
		|| sig_len != (uint32_t)key_size)
		goto cleanup;
	sig = malloc(sig_len);
	if (sig == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, sig, sig_len) != TSH_IO_OK)
		goto cleanup;
	if (verify_message_pss(cert, sig, sig_len,
			client_nonce, sizeof(client_nonce)) != 1)
		goto cleanup;
	if (RAND_bytes(sess->aes_key, TETRISSH_KEY_LEN) != 1)
		goto cleanup;
	wrapped = rsa_encrypt_block(pub, sess->aes_key, TETRISSH_KEY_LEN,
			&wrapped_len, 1);
	if (wrapped == NULL || wrapped_len > UINT32_MAX)
		goto cleanup;
	if (tsh_write_u32(fd, (uint32_t)wrapped_len) != 0
		|| tsh_write_exact(fd, wrapped, wrapped_len) != 0)
		goto cleanup;
	sess->established = 1;
	ok = 0;
	
	cleanup:
		EVP_PKEY_free(pub);
		X509_free(cert);
		free(cert_bytes);
		free(sig);
		if (wrapped != NULL)
			OPENSSL_cleanse(wrapped, wrapped_len);
		free(wrapped);
		OPENSSL_cleanse(client_nonce, sizeof(client_nonce));
		if (ok != 0)
			session_close(sess);
		return (ok);
}

/**
 * @brief Reads an entire file into a freshly allocated buffer.
 *
 * Rejects empty files and files larger than TSH_MAX_CERT_LEN before allocating,
 * so a hostile certificate path cannot force an unbounded allocation. On
 * success *out owns memory the caller must free.
 *
 * @param path Path to the file to read.
 * @param out Out-parameter receiving the allocated buffer.
 * @param len Out-parameter receiving the byte count.
 * @return 0 on success, -1 on a null argument, open, size, allocation, or read
 *         failure.
 */
static int	read_file_bytes(const char *path, unsigned char **out,
		uint32_t *len)
{
	FILE	*fp;
	long	size;

	if (path == NULL || out == NULL || len == NULL)
		return (-1);
	fp = fopen(path, "rb");
	if (fp == NULL)
		return (-1);
	if (fseek(fp, 0, SEEK_END) != 0)
		return (fclose(fp), -1);
	size = ftell(fp);
	if (size <= 0 || (unsigned long)size > TSH_MAX_CERT_LEN)
		return (fclose(fp), -1);
	rewind(fp);
	*out = malloc((size_t)size);
	if (*out == NULL)
		return (fclose(fp), -1);
	if (fread(*out, 1, (size_t)size, fp) != (size_t)size)
	{
		free(*out);
		*out = NULL;
		return (fclose(fp), -1);
	}
	*len = (uint32_t)size;
	return (fclose(fp), 0);
}

/**
 * @brief Zeroes a session and stamps its descriptor and role.
 *
 * @param sess The session to initialise.
 * @param fd The connected socket descriptor.
 * @param role The local endpoint role.
 */
static void	init_session(t_session *sess, int fd, t_tetrissh_role role)
{
	memset(sess, 0, sizeof(*sess));
	sess->fd = fd;
	sess->role = role;
}
