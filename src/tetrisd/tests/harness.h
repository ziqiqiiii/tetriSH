# ifndef HARNESS_H
# define HARNESS_H

# include "tetrisd.h"

/*
** The headless client the integration suite drives tetrisd with. It speaks
** the real protocol - TCP, the libtetrissh handshake, HTTTP messages, the
** libstatusbody bodies - so every test exercises exactly what tetrisu will
** experience, with no test-only back doors into the server.
**
** t_fixture is the other half: a throwaway server on a kernel-assigned port,
** with its own data directory and its own certificates, both in a mkdtemp
** directory that is removed when the test ends.
*/

# define HC_TIMEOUT_MS	4000

typedef struct s_harness
{
	t_session		sess;
	int				fd;
	t_player_id		player_id;
	bool			authed;
	t_body_state	last_state;
	bool			has_state;
}	t_harness;

typedef struct s_fixture
{
	t_server		*srv;
	t_config		cfg;
	char			dir[96];
	char			ca_path[192];
}	t_fixture;

/* HARNESS.C */
int		fx_start(t_fixture *fx);
void	fx_stop(t_fixture *fx);
int		hc_connect(t_harness *hc, const t_fixture *fx);
void	hc_close(t_harness *hc);
int		hc_request(t_harness *hc, const char *method, const char *path, const char *body, t_htttp_message *out);
int		hc_send(t_harness *hc, const char *method, const char *path, const char *body);
int		hc_recv(t_harness *hc, t_htttp_message *out, int timeout_ms);
int		hc_wait_state(t_harness *hc, t_body_state *out, int timeout_ms);
int		hc_wait_chat(t_harness *hc, t_body_chat *out, int timeout_ms);
int		hc_signup(t_harness *hc, const char *username, const char *password);
int		hc_login(t_harness *hc, const char *username, const char *password);
int		hc_join_new(t_harness *hc, const char *mode, char *room_out, size_t cap);
int		hc_lock_in(t_harness *hc, t_fixture *fx, const char *path);

# endif
