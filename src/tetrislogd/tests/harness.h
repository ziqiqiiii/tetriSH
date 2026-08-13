# ifndef HARNESS_H
# define HARNESS_H

# include "tetrislogd.h"

/*
** The shared fixture for the tetrislogd suites. Every test that touches the
** filesystem works inside its own mkdtemp directory and removes it at the
** end, so suites never collide over a socket path, a log file, or the flock
** that guards it.
**
** There is no fake producer here: tests send real t_log_record datagrams down
** a real AF_UNIX socket through libcoreipc, which is exactly what tetrisd's
** shipper thread does. Nothing reaches the daemon by a test-only back door.
*/

# define FIXTURE_IDLE_MS	20

typedef struct s_fixture
{
	char	dir[96];
	char	sock_path[192];
	char	file_path[192];
	char	pid_path[192];
	t_config	cfg;
}	t_fixture;

/* HARNESS.C */
int		fx_make(t_fixture *fx);
void	fx_destroy(t_fixture *fx);
int		fx_producer(const t_fixture *fx);
int		fx_send(int fd, t_log_level level, const char *msg);
int		fx_send_raw(int fd, const void *buf, size_t len);
ssize_t	fx_slurp(const char *path, char *out, size_t cap);
int		fx_contains(const char *path, const char *needle);
int		fx_line_count(const char *path);

# endif
