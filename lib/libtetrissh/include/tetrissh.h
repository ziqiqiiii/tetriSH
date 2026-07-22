#ifndef TETRISSH_H
# define TETRISSH_H

# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>

# define TETRISSH_KEY_LEN		32
# define TETRISSH_MAX_PLAINTEXT	65536u

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

/* HANDSHAKE.C */
int		session_handshake_server(int fd, t_session *sess,
			const char *cert_path, const char *key_path);
int		session_handshake_client(int fd, t_session *sess,
			const char *ca_path);

/* SESSION.C */
ssize_t	session_send(t_session *sess, const void *buf, size_t len);
ssize_t	session_recv(t_session *sess, void *buf, size_t max_len);
void	session_close(t_session *sess);

#endif
