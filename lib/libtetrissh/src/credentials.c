#include "internal.h"

// Static Functions
static int	read_file_bytes(const char *path, unsigned char **out,
				uint32_t *len);

/**
 * @brief Loads the server's certificate bytes and private key once.
 *
 * Both files are read and parsed here so the handshake never touches the disk:
 * a server that accepts thousands of connections opens these two paths once at
 * boot. The result is read-only, so every connection may share one instance.
 *
 * @param cert_path Path to the server certificate (PEM).
 * @param key_path Path to the server private key.
 * @param out Out-parameter receiving the loaded credentials, set to NULL on
 *        any failure; the caller frees it with session_credentials_free.
 * @return 0 on success, -1 on a null argument, unreadable file, unparseable
 *         key, or allocation failure.
 */
int	session_credentials_load(const char *cert_path, const char *key_path,
		t_tetrissh_credentials **out)
{
	t_tetrissh_credentials	*credentials;

	if (out == NULL)
		return (-1);
	*out = NULL;
	if (cert_path == NULL || key_path == NULL)
		return (-1);
	credentials = calloc(1, sizeof(*credentials));
	if (credentials == NULL)
		return (-1);
	if (read_file_bytes(cert_path, &credentials->cert_bytes,
			&credentials->cert_len) != 0)
		return (session_credentials_free(credentials), -1);
	credentials->key = load_private_key(key_path);
	if (credentials->key == NULL)
		return (session_credentials_free(credentials), -1);
	credentials->key_size = EVP_PKEY_size(credentials->key);
	if (credentials->key_size <= 0)
		return (session_credentials_free(credentials), -1);
	*out = credentials;
	return (0);
}

/**
 * @brief Releases loaded credentials and the private key inside them.
 *
 * @param credentials The credentials to free; a null pointer is ignored.
 */
void	session_credentials_free(t_tetrissh_credentials *credentials)
{
	if (credentials == NULL)
		return ;
	EVP_PKEY_free(credentials->key);
	free(credentials->cert_bytes);
	free(credentials);
}

/**
 * @brief Reads an entire file into a freshly allocated buffer.
 *
 * Rejects empty files and files larger than SESSIONIO_MAX_CERT_LEN before
 * allocating, so a hostile certificate path cannot force an unbounded
 * allocation. On success *out owns memory the caller must free.
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
	if (size <= 0 || (unsigned long)size > SESSIONIO_MAX_CERT_LEN)
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
