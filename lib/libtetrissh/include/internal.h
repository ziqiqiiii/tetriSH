#ifndef TETRISSH_INTERNAL_H
# define TETRISSH_INTERNAL_H

# include "tetrissh.h"
# include "libs/common.h"

# include <errno.h>
# include <limits.h>
# include <stddef.h>
# include <stdint.h>
# include <sys/socket.h>
# include <openssl/crypto.h>
# include <openssl/evp.h>
# include <openssl/rand.h>
# include <openssl/x509.h>

# define SESSIONIO_NONCE_LEN			32u
# define SESSIONIO_AAD_LEN				9u
# define SESSIONIO_MAX_CERT_LEN			65536u
# define SESSIONIO_CLIENT_MARKER		0x43u
# define SESSIONIO_SERVER_MARKER		0x53u

typedef enum e_sessionio_result
{
	SESSIONIO_OK	=	1,
	SESSIONIO_EOF	=	0,
	SESSIONIO_ERR	=	-1,
}	t_sessionio_result;

/*
** key_size is EVP_PKEY_size(key), cached because the server compares the
** client's claimed wrapped-key length against it before allocating.
*/
struct s_tetrissh_credentials
{
	unsigned char	*cert_bytes;
	uint32_t		cert_len;
	EVP_PKEY		*key;
	int				key_size;
};

/* IO.C */
t_sessionio_result	sessionio_read_exact(int fd, void *buf, size_t len);
int					sessionio_write_exact(int fd, const void *buf, size_t len);
t_sessionio_result	sessionio_read_u32(int fd, uint32_t *value);
int					sessionio_write_u32(int fd, uint32_t value);
void				sessionio_u64_be(uint64_t value, unsigned char out[8]);
unsigned char		sessionio_send_marker(t_tetrissh_role role);
unsigned char		sessionio_recv_marker(t_tetrissh_role role);

/* FRAME.C */
int					session_frame_ready(const t_session *sess);

#endif
