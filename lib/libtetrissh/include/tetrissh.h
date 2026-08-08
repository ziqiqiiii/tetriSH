#ifndef TETRISSH_H
# define TETRISSH_H

# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>

# define TETRISSH_KEY_LEN			32
# define TETRISSH_MAX_PLAINTEXT		65536u

/*
** Wire layout of one encrypted frame: a 12-byte GCM nonce, a 16-byte GCM tag,
** then the ciphertext, which is exactly as long as the plaintext. A caller
** that owns its own socket - the tetrisd reactor - sizes its buffers and
** validates a length prefix against these, so they are public.
*/
# define TETRISSH_GCM_NONCE_LEN		12u
# define TETRISSH_GCM_TAG_LEN		16u
# define TETRISSH_FRAME_OVERHEAD	(TETRISSH_GCM_NONCE_LEN + TETRISSH_GCM_TAG_LEN)
# define TETRISSH_MAX_FRAME			(TETRISSH_FRAME_OVERHEAD + TETRISSH_MAX_PLAINTEXT)

typedef enum e_tetrissh_role
{
	TETRISSH_ROLE_NONE	=	0,
	TETRISSH_ROLE_CLIENT,
	TETRISSH_ROLE_SERVER,
}	t_tetrissh_role;

typedef struct s_session
{
	int				fd;
	t_tetrissh_role	role;
	unsigned char	aes_key[TETRISSH_KEY_LEN];
	uint64_t		send_seq;
	uint64_t		recv_seq;
	int				established;
}	t_session;

/*
** The server's certificate bytes and parsed private key, loaded once at boot
** and shared by every accepted connection. Opaque so callers need no OpenSSL
** header of their own. It is read-only after loading, so any number of
** handshakes may use one instance concurrently.
*/
typedef struct s_tetrissh_credentials	t_tetrissh_credentials;

/* CREDENTIALS.C */
int		session_credentials_load(const char *cert_path, const char *key_path, t_tetrissh_credentials **out);
void	session_credentials_free(t_tetrissh_credentials *credentials);

/* HANDSHAKE.C */
int		session_handshake_server(int fd, t_session *sess, const t_tetrissh_credentials *credentials);
int		session_handshake_client(int fd, t_session *sess, const char *ca_path);

/* FRAME.C */
ssize_t	session_frame_seal(t_session *sess, const void *plain, size_t plain_len, void *frame, size_t frame_cap);
ssize_t	session_frame_open(t_session *sess, const void *frame, size_t frame_len, void *plain, size_t plain_cap);

/* SESSION.C */
ssize_t	session_send(t_session *sess, const void *buf, size_t len);
ssize_t	session_recv(t_session *sess, void *buf, size_t max_len);
void	session_close(t_session *sess);

#endif
