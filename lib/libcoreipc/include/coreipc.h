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

# define SELFPIPE_READ	0
# define SELFPIPE_WRITE	1

/*
** Log record: the one wire format shared by the tetrisd ring buffer, the
** shipper's datagrams, and tetrislogd's parser. Fixed-size so it flows
** through ring_* (record_size = sizeof(t_log_record)) and one DGRAM each.
** Host-endian by design - it never leaves the machine.
*/

# define COREIPC_LOG_MAGIC			0x4C4F4752u
# define COREIPC_LOG_VERSION		1
# define COREIPC_LOG_COMPONENT_MAX	16
# define COREIPC_LOG_MSG_MAX		256

typedef enum e_log_level
{
	COREIPC_LOG_DEBUG = 0,
	COREIPC_LOG_INFO,
	COREIPC_LOG_WARNING,
	COREIPC_LOG_ERROR
}	t_log_level;

typedef struct s_log_record
{
	uint32_t	magic;
	uint8_t		version;
	uint8_t		level;
	uint16_t	msg_len;
	uint64_t	timestamp_ms;
	uint32_t	pid;
	char		component[COREIPC_LOG_COMPONENT_MAX];
	char		msg[COREIPC_LOG_MSG_MAX];
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
int			logrecord_make(t_log_record *out, t_log_level level, uint64_t timestamp_ms, uint32_t pid, const char *component, const char *msg);
int			logrecord_validate(const void *buf, size_t len);
const char	*logrecord_level_name(t_log_level level);
int			logrecord_level_parse(const char *name);
int			logrecord_format_line(const t_log_record *rec, char *out, size_t cap);

/* RING_BUFFER.C */
int			ring_init(t_ring_buffer *rb, size_t record_size, size_t capacity);
int			ring_push(t_ring_buffer *rb, const void *record);
int			ring_pop(t_ring_buffer *rb, void *out);
size_t		ring_drain(t_ring_buffer *rb, void *out, size_t max_records);
uint64_t	ring_dropped_count(const t_ring_buffer *rb);
void		ring_destroy(t_ring_buffer *rb);

/* UNIX_DGRAM.C */
int			unixsock_dgram_bind(const char *path, mode_t mode);
int			unixsock_dgram_open(const char *path);
int			unixsock_dgram_send_nonblock(int fd, const void *buf, size_t len);
ssize_t		unixsock_dgram_recv(int fd, void *buf, size_t buflen);

/* UNIX_STREAM.C */
int			unixsock_stream_listen(const char *path, int backlog, mode_t mode);
int			unixsock_stream_accept(int listen_fd);
int			unixsock_stream_connect(const char *path);
int			unixsock_send_all(int fd, const void *buf, size_t len);
int			unixsock_recv_all(int fd, void *buf, size_t len);

/* FD_SIGNAL.C */
int			unixsock_set_nonblock(int fd);
int			unixsock_close_unlink(int fd, const char *path);
int			selfpipe_open(int fds[2]);
void		selfpipe_notify(int write_fd);
int			selfpipe_drain(int read_fd);

/* MSGQUEUE.C */
mqd_t		msgqueue_open(const char *name, long maxmsg, long msgsize, mode_t mode);
int			msgqueue_send_nonblock(mqd_t q, const void *msg, size_t len);
ssize_t		msgqueue_recv_nonblock(mqd_t q, void *buf, size_t buflen);
ssize_t		msgqueue_recv_timed(mqd_t q, void *buf, size_t buflen, int timeout_ms);
int			msgqueue_close(mqd_t q);
int			msgqueue_unlink(const char *name);

#endif
