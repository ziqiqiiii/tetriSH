#include "internal.h"
#include <openssl/crypto.h>

ssize_t	session_send(t_session *sess, const void *buf, size_t len)
{
	(void)sess;
	(void)buf;
	(void)len;
	return (-1);
}

ssize_t	session_recv(t_session *sess, void *buf, size_t max_len)
{
	(void)sess;
	(void)buf;
	(void)max_len;
	return (-1);
}

void	session_close(t_session *sess)
{
	if (sess == NULL)
		return;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}
