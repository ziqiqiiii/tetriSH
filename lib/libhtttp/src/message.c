#include "htttp.h"

static int			message_is_empty(const t_htttp_message *message);
static t_htttp_result	copy_owned_text(const char *text, char **out);
static unsigned char	ascii_lower(unsigned char c);
static int			ascii_case_equal(const char *left, const char *right);

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

t_htttp_result	htttp_message_make_request(t_htttp_message *message,
		const char *method, const char *path)
{
	t_htttp_result	result;
	char			*method_copy;
	char			*path_copy;

	if (message == NULL || method == NULL || path == NULL
		|| !message_is_empty(message))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	method_copy = NULL;
	path_copy = NULL;
	result = copy_owned_text(method, &method_copy);
	if (result != HTTTP_OK)
		return (result);
	result = copy_owned_text(path, &path_copy);
	if (result != HTTTP_OK)
	{
		free(method_copy);
		return (result);
	}
	message->method = method_copy;
	message->path = path_copy;
	return (HTTTP_OK);
}

t_htttp_result	htttp_message_make_response(t_htttp_message *message,
		unsigned int status_code, const char *reason)
{
	t_htttp_result	result;
	char			*reason_copy;

	if (message == NULL || reason == NULL || !message_is_empty(message))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	reason_copy = NULL;
	result = copy_owned_text(reason, &reason_copy);
	if (result != HTTTP_OK)
		return (result);
	message->type = HTTTP_MESSAGE_RESPONSE;
	message->status_code = status_code;
	message->reason = reason_copy;
	return (HTTTP_OK);
}

t_htttp_result	htttp_message_set_header(t_htttp_message *message,
		const char *name, const char *value)
{
	t_htttp_header	*headers;
	t_htttp_result	result;
	char			*name_copy;
	char			*value_copy;
	size_t			i;

	if (message == NULL || name == NULL || value == NULL
		|| message->header_count > HTTTP_MAX_HEADERS
		|| (message->header_count > 0u && message->headers == NULL))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	i = 0;
	while (i < message->header_count)
	{
		if (message->headers[i].name != NULL
			&& ascii_case_equal(message->headers[i].name, name))
		{
			value_copy = NULL;
			result = copy_owned_text(value, &value_copy);
			if (result != HTTTP_OK)
				return (result);
			free(message->headers[i].value);
			message->headers[i].value = value_copy;
			return (HTTTP_OK);
		}
		i++;
	}
	if (message->header_count == HTTTP_MAX_HEADERS)
		return (HTTTP_ERR_TOO_MANY_HEADERS);
	name_copy = NULL;
	value_copy = NULL;
	/* AI-assisted: own both strings before reallocating so any allocation
	 * failure leaves the existing header array and count unchanged. */
	result = copy_owned_text(name, &name_copy);
	if (result != HTTTP_OK)
		return (result);
	result = copy_owned_text(value, &value_copy);
	if (result != HTTTP_OK)
	{
		free(name_copy);
		return (result);
	}
	headers = realloc(message->headers,
			(message->header_count + 1u) * sizeof(*headers));
	if (headers == NULL)
	{
		free(name_copy);
		free(value_copy);
		return (HTTTP_ERR_NO_MEMORY);
	}
	message->headers = headers;
	message->headers[message->header_count].name = name_copy;
	message->headers[message->header_count].value = value_copy;
	message->header_count++;
	return (HTTTP_OK);
}

const char	*htttp_message_get_header(const t_htttp_message *message,
		const char *name)
{
	size_t	i;

	if (message == NULL || name == NULL
		|| (message->header_count > 0u && message->headers == NULL))
		return (NULL);
	i = 0;
	while (i < message->header_count)
	{
		if (message->headers[i].name != NULL
			&& ascii_case_equal(message->headers[i].name, name))
			return (message->headers[i].value);
		i++;
	}
	return (NULL);
}

t_htttp_result	htttp_message_set_body(t_htttp_message *message,
		const void *body, size_t body_len)
{
	unsigned char	*body_copy;

	if (message == NULL || (body == NULL && body_len > 0u))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (body_len > HTTTP_MAX_MESSAGE_SIZE)
		return (HTTTP_ERR_TOO_LARGE);
	if (body_len == 0u)
	{
		free(message->body);
		message->body = NULL;
		message->body_len = 0u;
		return (HTTTP_OK);
	}
	/* Allocate/copy before freeing old body so replacement is atomic on OOM. */
	body_copy = malloc(body_len);
	if (body_copy == NULL)
		return (HTTTP_ERR_NO_MEMORY);
	memcpy(body_copy, body, body_len);
	free(message->body);
	message->body = body_copy;
	message->body_len = body_len;
	return (HTTTP_OK);
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

static int	message_is_empty(const t_htttp_message *message)
{
	return (message->type == HTTTP_MESSAGE_REQUEST
		&& message->method == NULL && message->path == NULL
		&& message->status_code == 0u && message->reason == NULL
		&& message->headers == NULL && message->header_count == 0u
		&& message->body == NULL && message->body_len == 0u);
}

static t_htttp_result	copy_owned_text(const char *text, char **out)
{
	char	*copy;
	size_t	len;

	len = strlen(text);
	if (len > HTTTP_MAX_MESSAGE_SIZE)
		return (HTTTP_ERR_TOO_LARGE);
	copy = malloc(len + 1u);
	if (copy == NULL)
		return (HTTTP_ERR_NO_MEMORY);
	memcpy(copy, text, len + 1u);
	*out = copy;
	return (HTTTP_OK);
}

static unsigned char	ascii_lower(unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return ((unsigned char)(c + ('a' - 'A')));
	return (c);
}

static int	ascii_case_equal(const char *left, const char *right)
{
	size_t	i;

	i = 0;
	while (left[i] != '\0' && right[i] != '\0')
	{
		if (ascii_lower((unsigned char)left[i])
			!= ascii_lower((unsigned char)right[i]))
			return (0);
		i++;
	}
	return (left[i] == right[i]);
}
