#ifndef COREIPC_H
# define COREIPC_H

# include <errno.h>
# include <pthread.h>
# include <stdatomic.h>
# include <stddef.h>
# include <stdint.h>
# include <sys/stat.h>
# include <sys/types.h>

# define SP_READ	0
# define SP_WRITE	1

/* macOS has no POSIX message-queue kernel support, so this library implements
 * the mq_helpers API in-process on top of the ring buffer (see mq_helpers.c).
 * We define a plain int handle so no platform ifdef is needed anywhere. */
typedef int	mqd_t;

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
