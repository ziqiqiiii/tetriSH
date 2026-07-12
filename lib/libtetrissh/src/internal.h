#ifndef TETRISSH_INTERNAL_H
# define TETRISSH_INTERNAL_H

# include "tetrissh.h"
# include <stddef.h>
# include <stdint.h>

# define TSH_NONCE_LEN 32u
# define TSH_GCM_NONCE_LEN 12u
# define TSH_GCM_TAG_LEN 16u
# define TSH_FRAME_OVERHEAD (TSH_GCM_NONCE_LEN + TSH_GCM_TAG_LEN)
# define TSH_MAX_CERT_LEN 65536u
# define TSH_CLIENT_MARKER 0x43u
# define TSH_SERVER_MARKER 0x53u

typedef enum e_tsh_io_result
{
	TSH_IO_OK = 1,
	TSH_IO_EOF = 0,
	TSH_IO_ERR = -1
} t_tsh_io_result;

t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len);
int					tsh_write_exact(int fd, const void *buf, size_t len);
t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value);
int					tsh_write_u32(int fd, uint32_t value);
void				tsh_u64_be(uint64_t value, unsigned char out[8]);
unsigned char		tsh_send_marker(t_tetrissh_role role);
unsigned char		tsh_recv_marker(t_tetrissh_role role);

#endif
