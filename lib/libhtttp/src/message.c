#include "htttp.h"

void	htttp_message_init(t_htttp_message *message)
{
	if (message == NULL)
		return;
	memset(message, 0, sizeof(*message));
}

void	htttp_message_free(t_htttp_message *message)
{
	size_t	i;

	if (message == NULL)
		return;
	/* AI-assisted: one idempotent cleanup path keeps partially built parser and
	 * builder outputs leak-free without duplicating ownership logic. */
	i = 0;
	while (i < message->header_count)
	{
		free(message->headers[i].name);
		free(message->headers[i].value);
		i++;
	}
	free(message->headers);
	free(message->method);
	free(message->path);
	free(message->reason);
	free(message->body);
	htttp_message_init(message);
}

const char	*htttp_reason_phrase(unsigned int status_code)
{
	if (status_code == 200u)
		return ("OK");
	if (status_code == 201u)
		return ("Created");
	if (status_code == 400u)
		return ("Bad Request");
	if (status_code == 401u)
		return ("Unauthorized");
	if (status_code == 403u)
		return ("Forbidden");
	if (status_code == 404u)
		return ("Not Found");
	if (status_code == 409u)
		return ("Conflict");
	if (status_code == 413u)
		return ("Payload Too Large");
	if (status_code == 429u)
		return ("Too Many Requests");
	if (status_code == 500u)
		return ("Internal Server Error");
	return (NULL);
}

const char	*htttp_result_string(t_htttp_result result)
{
	if (result == HTTTP_OK)
		return ("ok");
	if (result == HTTTP_ERR_INVALID_ARGUMENT)
		return ("invalid argument");
	if (result == HTTTP_ERR_TOO_LARGE)
		return ("message too large");
	if (result == HTTTP_ERR_NO_MEMORY)
		return ("out of memory");
	if (result == HTTTP_ERR_MALFORMED_START_LINE)
		return ("malformed start line");
	if (result == HTTTP_ERR_UNSUPPORTED_VERSION)
		return ("unsupported version");
	if (result == HTTTP_ERR_MALFORMED_HEADER)
		return ("malformed header");
	if (result == HTTTP_ERR_TOO_MANY_HEADERS)
		return ("too many headers");
	if (result == HTTTP_ERR_DUPLICATE_HEADER)
		return ("duplicate header");
	if (result == HTTTP_ERR_INVALID_CONTENT_LENGTH)
		return ("invalid content length");
	if (result == HTTTP_ERR_LENGTH_MISMATCH)
		return ("body length mismatch");
	if (result == HTTTP_ERR_MISSING_REQUIRED_HEADER)
		return ("missing required header");
	if (result == HTTTP_ERR_INVALID_MESSAGE)
		return ("invalid message");
	if (result == HTTTP_ERR_NO_HANDLER)
		return ("no handler");
	return (NULL);
}
