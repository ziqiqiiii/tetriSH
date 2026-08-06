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
# define SESSIONIO_GCM_NONCE_LEN		12u
# define SESSIONIO_GCM_TAG_LEN		16u
# define SESSIONIO_FRAME_OVERHEAD		(SESSIONIO_GCM_NONCE_LEN + SESSIONIO_GCM_TAG_LEN)
# define SESSIONIO_MAX_CERT_LEN		65536u
# define SESSIONIO_CLIENT_MARKER		0x43u
# define SESSIONIO_SERVER_MARKER		0x53u

typedef enum e_tsh_io_result
{
	SESSIONIO_IO_OK	=	1,
	SESSIONIO_IO_EOF	=	0,
	SESSIONIO_IO_ERR	=	-1,
}	t_tsh_io_result;

/* IO.C */
t_tsh_io_result	sessionio_read_exact(int fd, void *buf, size_t len);
int				sessionio_write_exact(int fd, const void *buf, size_t len);
t_tsh_io_result	sessionio_read_u32(int fd, uint32_t *value);
int				sessionio_write_u32(int fd, uint32_t value);
void			sessionio_u64_be(uint64_t value, unsigned char out[8]);
unsigned char	sessionio_send_marker(t_tetrissh_role role);
unsigned char	sessionio_recv_marker(t_tetrissh_role role);

#endif
