#include "coreipc.h"

#include <string.h>
#include <time.h>

#define MQ_MAX_QUEUES	32
#define MQ_NAME_MAX		64

typedef struct s_mq_entry
{
	char				name[MQ_NAME_MAX];
	t_ring_buffer		rb;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	long				msgsize;
	int					in_use;
	int					refcount;
}	t_mq_entry;

static t_mq_entry		g_queues[MQ_MAX_QUEUES];
static pthread_mutex_t	g_table_lock = PTHREAD_MUTEX_INITIALIZER;

static t_mq_entry	*entry_by_handle(mqd_t q)
{
	int	i;

	i = (int)q;
	if (i < 0 || i >= MQ_MAX_QUEUES || !g_queues[i].in_use)
		return (NULL);
	return (&g_queues[i]);
}

static t_mq_entry	*entry_by_name(const char *name)
{
	int	i;

	i = 0;
	while (i < MQ_MAX_QUEUES)
	{
		if (g_queues[i].in_use && strcmp(g_queues[i].name, name) == 0)
			return (&g_queues[i]);
		i++;
	}
	return (NULL);
}

mqd_t	mqh_open(const char *name, long maxmsg, long msgsize, mode_t mode)
{
	t_mq_entry	*e;
	int			i;

	(void)mode;
	if (name == NULL || name[0] != '/' || maxmsg <= 0 || msgsize <= 0)
	{
		errno = EINVAL;
		return ((mqd_t)-1);
	}
	pthread_mutex_lock(&g_table_lock);
	e = entry_by_name(name);
	if (e != NULL)
	{
		e->refcount++;
		pthread_mutex_unlock(&g_table_lock);
		return ((mqd_t)(e - g_queues));
	}
	i = 0;
	while (i < MQ_MAX_QUEUES && g_queues[i].in_use)
		i++;
	if (i == MQ_MAX_QUEUES)
	{
		pthread_mutex_unlock(&g_table_lock);
		errno = ENOSPC;
		return ((mqd_t)-1);
	}
	e = &g_queues[i];
	memset(e, 0, sizeof(*e));
	strncpy(e->name, name, MQ_NAME_MAX - 1);
	e->name[MQ_NAME_MAX - 1] = '\0';
	e->msgsize = msgsize;
	e->in_use = 1;
	e->refcount = 1;
	if (pthread_mutex_init(&e->mutex, NULL) != 0
		|| pthread_cond_init(&e->cond, NULL) != 0)
	{
		e->in_use = 0;
		pthread_mutex_unlock(&g_table_lock);
		return ((mqd_t)-1);
	}
	if (rb_init(&e->rb, (size_t)msgsize, (size_t)maxmsg) != 0)
	{
		pthread_mutex_destroy(&e->mutex);
		pthread_cond_destroy(&e->cond);
		e->in_use = 0;
		pthread_mutex_unlock(&g_table_lock);
		return ((mqd_t)-1);
	}
	pthread_mutex_unlock(&g_table_lock);
	return ((mqd_t)i);
}

int	mqh_send_nb(mqd_t q, const void *msg, size_t len)
{
	t_mq_entry	*e;
	int			rc;

	e = entry_by_handle(q);
	if (e == NULL)
	{
		errno = EBADF;
		return (-1);
	}
	if (len > (size_t)e->msgsize)
	{
		errno = EMSGSIZE;
		return (-1);
	}
	pthread_mutex_lock(&e->mutex);
	rc = rb_push(&e->rb, msg);
	if (rc == -1)
	{
		pthread_mutex_unlock(&e->mutex);
		errno = EAGAIN;
		return (-1);
	}
	pthread_cond_signal(&e->cond);
	pthread_mutex_unlock(&e->mutex);
	return (0);
}

ssize_t	mqh_recv_nb(mqd_t q, void *buf, size_t buflen)
{
	t_mq_entry	*e;
	int			rc;

	e = entry_by_handle(q);
	if (e == NULL)
	{
		errno = EBADF;
		return (-1);
	}
	if (buflen < (size_t)e->msgsize)
	{
		errno = EMSGSIZE;
		return (-1);
	}
	pthread_mutex_lock(&e->mutex);
	rc = rb_pop(&e->rb, buf);
	if (rc == -1)
	{
		pthread_mutex_unlock(&e->mutex);
		errno = EAGAIN;
		return (-1);
	}
	pthread_mutex_unlock(&e->mutex);
	return ((ssize_t)e->msgsize);
}

ssize_t	mqh_recv_timed(mqd_t q, void *buf, size_t buflen, int timeout_ms)
{
	t_mq_entry		*e;
	struct timespec	deadline;
	int				rc;

	e = entry_by_handle(q);
	if (e == NULL)
	{
		errno = EBADF;
		return (-1);
	}
	if (buflen < (size_t)e->msgsize)
	{
		errno = EMSGSIZE;
		return (-1);
	}
	if (timeout_ms > 0)
	{
		clock_gettime(CLOCK_REALTIME, &deadline);
		deadline.tv_sec += timeout_ms / 1000;
		deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
		if (deadline.tv_nsec >= 1000000000L)
		{
			deadline.tv_sec += 1;
			deadline.tv_nsec -= 1000000000L;
		}
	}
	pthread_mutex_lock(&e->mutex);
	for (;;)
	{
		rc = rb_pop(&e->rb, buf);
		if (rc == 0)
		{
			pthread_mutex_unlock(&e->mutex);
			return ((ssize_t)e->msgsize);
		}
		if (timeout_ms <= 0)
		{
			pthread_mutex_unlock(&e->mutex);
			errno = ETIMEDOUT;
			return (-1);
		}
		rc = pthread_cond_timedwait(&e->cond, &e->mutex, &deadline);
		if (rc == ETIMEDOUT)
		{
			pthread_mutex_unlock(&e->mutex);
			errno = ETIMEDOUT;
			return (-1);
		}
	}
}

int	mqh_close(mqd_t q)
{
	t_mq_entry	*e;

	e = entry_by_handle(q);
	if (e == NULL)
	{
		errno = EBADF;
		return (-1);
	}
	pthread_mutex_lock(&g_table_lock);
	if (e->refcount > 0)
		e->refcount--;
	pthread_mutex_unlock(&g_table_lock);
	return (0);
}

int	mqh_unlink(const char *name)
{
	t_mq_entry	*e;

	if (name == NULL || name[0] != '/')
	{
		errno = EINVAL;
		return (-1);
	}
	pthread_mutex_lock(&g_table_lock);
	e = entry_by_name(name);
	if (e == NULL)
	{
		pthread_mutex_unlock(&g_table_lock);
		errno = ENOENT;
		return (-1);
	}
	pthread_mutex_lock(&e->mutex);
	rb_destroy(&e->rb);
	pthread_mutex_unlock(&e->mutex);
	pthread_mutex_destroy(&e->mutex);
	pthread_cond_destroy(&e->cond);
	e->in_use = 0;
	memset(e->name, 0, sizeof(e->name));
	pthread_mutex_unlock(&g_table_lock);
	return (0);
}
