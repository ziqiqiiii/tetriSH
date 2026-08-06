#ifndef COREIPC_H
# define COREIPC_H

# include <errno.h>
# include <mqueue.h>
# include <pthread.h>
# include <stdatomic.h>
# include <stddef.h>
# include <stdint.h>
# include <sys/stat.h>
# include <sys/types.h>

# define SP_READ	0
# define SP_WRITE	1

/*
** Log record: the one wire format shared by the tetrisd ring buffer, the
** shipper's datagrams, and tetrislogd's parser. Fixed-size so it flows
** through rb_* (record_size = sizeof(t_log_record)) and one DGRAM each.
** Host-endian by design - it never leaves the machine.
*/

# define CIPC_LOG_MAGIC			0x4C4F4752u
# define CIPC_LOG_VERSION		1
# define CIPC_LOG_COMPONENT_MAX	16
# define CIPC_LOG_MSG_MAX		256

typedef enum e_log_level
{
	CIPC_LOG_DEBUG = 0,
	CIPC_LOG_INFO,
	CIPC_LOG_WARNING,
	CIPC_LOG_ERROR
}	t_log_level;

typedef struct s_log_record
{
	uint32_t	magic;
	uint8_t		version;
	uint8_t		level;
	uint16_t	msg_len;
	uint64_t	timestamp_ms;
	uint32_t	pid;
	char		component[CIPC_LOG_COMPONENT_MAX];
	char		msg[CIPC_LOG_MSG_MAX];
}	t_log_record;

typedef struct s_ring_buffer
{
	unsigned char			*slots;
	size_t					record_size;
	size_t					capacity;
	size_t					head;
	size_t					tail;
	pthread_mutex_t			mutex;
	atomic_uint_fast64_t	drops;
}	t_ring_buffer;

/* LOG_RECORD.C */
int			lr_make(t_log_record *out, t_log_level level, uint64_t timestamp_ms, uint32_t pid, const char *component, const char *msg);
int			lr_validate(const void *buf, size_t len);
const char	*lr_level_name(t_log_level level);
int			lr_level_parse(const char *name);
int			lr_format_line(const t_log_record *rec, char *out, size_t cap);

/* RING_BUFFER.C */
int			rb_init(t_ring_buffer *rb, size_t record_size, size_t capacity);
int			rb_push(t_ring_buffer *rb, const void *record);
int			rb_pop(t_ring_buffer *rb, void *out);
size_t		rb_drain(t_ring_buffer *rb, void *out, size_t max_records);
uint64_t	rb_drops(const t_ring_buffer *rb);
void		rb_destroy(t_ring_buffer *rb);

/* UNIX_DGRAM.C */
int			us_dgram_bind(const char *path, mode_t mode);
int			us_dgram_open(const char *path);
int			us_dgram_send_nb(int fd, const void *buf, size_t len);
ssize_t		us_dgram_recv(int fd, void *buf, size_t buflen);

/* UNIX_STREAM.C */
int			us_stream_listen(const char *path, int backlog, mode_t mode);
int			us_stream_accept(int listen_fd);
int			us_stream_connect(const char *path);
int			us_send_all(int fd, const void *buf, size_t len);
int			us_recv_all(int fd, void *buf, size_t len);

/* FD_SIGNAL.C */
int			us_set_nonblock(int fd);
int			us_close_unlink(int fd, const char *path);
int			sp_pipe(int fds[2]);
void		sp_notify(int write_fd);
int			sp_drain(int read_fd);

/* MQ_HELPERS.C */
mqd_t		mqh_open(const char *name, long maxmsg, long msgsize, mode_t mode);
int			mqh_send_nb(mqd_t q, const void *msg, size_t len);
ssize_t		mqh_recv_nb(mqd_t q, void *buf, size_t buflen);
ssize_t		mqh_recv_timed(mqd_t q, void *buf, size_t buflen, int timeout_ms);
int			mqh_close(mqd_t q);
int			mqh_unlink(const char *name);

#endif
