#include "internal.h"

int	session_handshake_server(int fd, t_session *sess,
		const char *cert_path, const char *key_path)
{
	(void)fd;
	(void)sess;
	(void)cert_path;
	(void)key_path;
	return (-1);
}

int	session_handshake_client(int fd, t_session *sess, const char *ca_path)
{
	(void)fd;
	(void)sess;
	(void)ca_path;
	return (-1);
}
