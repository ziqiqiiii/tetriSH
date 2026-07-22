#include "htttp.h"

static int			parser_message_is_empty(const t_htttp_message *message);
static int			read_line(const unsigned char *data, size_t data_len,
						size_t *cursor, const unsigned char **line,
						size_t *line_len);
static t_htttp_result	parse_start_line(const unsigned char *line,
						size_t line_len, t_htttp_message *message);
static t_htttp_result	parse_status_line(const unsigned char *line,
						size_t line_len, t_htttp_message *message);
static t_htttp_result	parse_request_line(const unsigned char *line,
						size_t line_len, t_htttp_message *message);
static t_htttp_result	parse_headers(const unsigned char *data,
						size_t data_len, size_t *cursor,
						t_htttp_message *message);
static t_htttp_result	parse_body(const unsigned char *data, size_t data_len,
						size_t body_start, t_htttp_message *message);
static t_htttp_result	parse_content_length(const char *text, size_t *out);
static t_htttp_result	parse_header_line(const unsigned char *line,
						size_t line_len, t_htttp_message *message);
static int			valid_method(const unsigned char *text, size_t len);
static int			valid_path(const unsigned char *text, size_t len);
static int			valid_reason(const unsigned char *text, size_t len);
static int			valid_header_name(const unsigned char *text, size_t len);
static int			valid_header_value(const unsigned char *text, size_t len);
static t_htttp_result	copy_range(const unsigned char *text, size_t len,
						char **out);

t_htttp_result	htttp_parse(const unsigned char *data, size_t data_len,
		t_htttp_message *out)
{
	t_htttp_message		message;
	t_htttp_result		result;
	const unsigned char	*line;
	size_t				line_len;
	size_t				cursor;

	if (data == NULL || out == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (data_len > HTTTP_MAX_MESSAGE_SIZE)
		return (HTTTP_ERR_TOO_LARGE);
	if (!parser_message_is_empty(out))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (data_len == 0u)
		return (HTTTP_ERR_MALFORMED_START_LINE);
	htttp_message_init(&message);
	cursor = 0u;
	if (!read_line(data, data_len, &cursor, &line, &line_len))
		return (HTTTP_ERR_MALFORMED_START_LINE);
	result = parse_start_line(line, line_len, &message);
	if (result == HTTTP_OK)
		result = parse_headers(data, data_len, &cursor, &message);
	if (result == HTTTP_OK)
		result = parse_body(data, data_len, cursor, &message);
	if (result != HTTTP_OK)
	{
		htttp_message_free(&message);
		return (result);
	}
	*out = message;
	return (HTTTP_OK);
}

static int	parser_message_is_empty(const t_htttp_message *message)
{
	return (message->type == HTTTP_MESSAGE_REQUEST
		&& message->method == NULL && message->path == NULL
		&& message->status_code == 0u && message->reason == NULL
		&& message->headers == NULL && message->header_count == 0u
		&& message->body == NULL && message->body_len == 0u);
}

static int	read_line(const unsigned char *data, size_t data_len,
		size_t *cursor, const unsigned char **line, size_t *line_len)
{
	size_t	i;

	/* AI-assisted: session_recv() does not append NUL, so every scan checks the
	 * frame length before reading the current or following byte. */
	i = *cursor;
	while (i < data_len)
	{
		if (data[i] == '\n')
			return (0);
		if (data[i] == '\r')
		{
			if (i + 1u >= data_len || data[i + 1u] != '\n')
				return (0);
			*line = data + *cursor;
			*line_len = i - *cursor;
			*cursor = i + 2u;
			return (1);
		}
		i++;
	}
	return (0);
}

static t_htttp_result	parse_start_line(const unsigned char *line,
		size_t line_len, t_htttp_message *message)
{
	if (line_len >= 6u && memcmp(line, "HTTTP/", 6u) == 0)
		return (parse_status_line(line, line_len, message));
	return (parse_request_line(line, line_len, message));
}

static t_htttp_result	parse_status_line(const unsigned char *line,
		size_t line_len, t_htttp_message *message)
{
	t_htttp_result	result;
	size_t			version_len;
	size_t			code_start;
	size_t			reason_start;
	unsigned int	status_code;
	char			*reason;

	version_len = 0u;
	while (version_len < line_len && line[version_len] != ' ')
		version_len++;
	if (version_len != strlen(HTTTP_VERSION)
		|| memcmp(line, HTTTP_VERSION, version_len) != 0)
		return (HTTTP_ERR_UNSUPPORTED_VERSION);
	code_start = version_len + 1u;
	if (code_start + 3u >= line_len || line[code_start + 3u] != ' ')
		return (HTTTP_ERR_MALFORMED_START_LINE);
	if (line[code_start] < '0' || line[code_start] > '9'
		|| line[code_start + 1u] < '0' || line[code_start + 1u] > '9'
		|| line[code_start + 2u] < '0' || line[code_start + 2u] > '9')
		return (HTTTP_ERR_MALFORMED_START_LINE);
	status_code = (unsigned int)(line[code_start] - '0') * 100u
		+ (unsigned int)(line[code_start + 1u] - '0') * 10u
		+ (unsigned int)(line[code_start + 2u] - '0');
	if (status_code < 100u || status_code > 599u)
		return (HTTTP_ERR_MALFORMED_START_LINE);
	reason_start = code_start + 4u;
	if (!valid_reason(line + reason_start, line_len - reason_start))
		return (HTTTP_ERR_MALFORMED_START_LINE);
	reason = NULL;
	result = copy_range(line + reason_start, line_len - reason_start, &reason);
	if (result != HTTTP_OK)
		return (result);
	message->type = HTTTP_MESSAGE_RESPONSE;
	message->status_code = status_code;
	message->reason = reason;
	return (HTTTP_OK);
}

static t_htttp_result	parse_request_line(const unsigned char *line,
		size_t line_len, t_htttp_message *message)
{
	t_htttp_result	result;
	size_t			first_space;
	size_t			second_space;
	size_t			version_start;
	size_t			i;
	char			*method;
	char			*path;

	first_space = 0u;
	while (first_space < line_len && line[first_space] != ' ')
		first_space++;
	second_space = first_space + 1u;
	while (second_space < line_len && line[second_space] != ' ')
		second_space++;
	if (first_space == 0u || first_space >= line_len
		|| second_space == first_space + 1u || second_space >= line_len)
		return (HTTTP_ERR_MALFORMED_START_LINE);
	version_start = second_space + 1u;
	if (version_start == line_len)
		return (HTTTP_ERR_MALFORMED_START_LINE);
	i = version_start;
	while (i < line_len)
	{
		if (line[i] == ' ')
			return (HTTTP_ERR_MALFORMED_START_LINE);
		i++;
	}
	if (!valid_method(line, first_space)
		|| !valid_path(line + first_space + 1u,
			second_space - first_space - 1u))
		return (HTTTP_ERR_MALFORMED_START_LINE);
	if (line_len - version_start != strlen(HTTTP_VERSION)
		|| memcmp(line + version_start, HTTTP_VERSION,
			line_len - version_start) != 0)
		return (HTTTP_ERR_UNSUPPORTED_VERSION);
	method = NULL;
	path = NULL;
	result = copy_range(line, first_space, &method);
	if (result != HTTTP_OK)
		return (result);
	result = copy_range(line + first_space + 1u,
			second_space - first_space - 1u, &path);
	if (result != HTTTP_OK)
	{
		free(method);
		return (result);
	}
	message->method = method;
	message->path = path;
	return (HTTTP_OK);
}

static t_htttp_result	parse_headers(const unsigned char *data,
		size_t data_len, size_t *cursor, t_htttp_message *message)
{
	t_htttp_result		result;
	const unsigned char	*line;
	size_t				line_len;

	while (1)
	{
		if (!read_line(data, data_len, cursor, &line, &line_len))
			return (HTTTP_ERR_MALFORMED_HEADER);
		if (line_len == 0u)
			return (HTTTP_OK);
		result = parse_header_line(line, line_len, message);
		if (result != HTTTP_OK)
			return (result);
	}
}

static t_htttp_result	parse_body(const unsigned char *data, size_t data_len,
		size_t body_start, t_htttp_message *message)
{
	const char		*length_text;
	t_htttp_result	result;
	size_t			content_length;
	size_t			remaining;

	remaining = data_len - body_start;
	length_text = htttp_message_get_header(message, "Content-Length");
	if (length_text == NULL)
	{
		if (remaining == 0u)
			return (HTTTP_OK);
		return (HTTTP_ERR_INVALID_CONTENT_LENGTH);
	}
	result = parse_content_length(length_text, &content_length);
	if (result != HTTTP_OK)
		return (result);
	if (content_length != remaining)
		return (HTTTP_ERR_LENGTH_MISMATCH);
	if (content_length == 0u)
		return (HTTTP_OK);
	return (htttp_message_set_body(message, data + body_start, content_length));
}

static t_htttp_result	parse_content_length(const char *text, size_t *out)
{
	size_t	value;
	size_t	i;
	size_t	digit;

	if (text[0] == '\0')
		return (HTTTP_ERR_INVALID_CONTENT_LENGTH);
	value = 0u;
	i = 0u;
	while (text[i] != '\0')
	{
		if (text[i] < '0' || text[i] > '9')
			return (HTTTP_ERR_INVALID_CONTENT_LENGTH);
		digit = (size_t)(text[i] - '0');
		/* Reject before multiply/add so attacker-controlled decimal text cannot
		 * wrap into a smaller allocation or body-length comparison. */
		if (value > (SIZE_MAX - digit) / 10u)
			return (HTTTP_ERR_INVALID_CONTENT_LENGTH);
		value = value * 10u + digit;
		i++;
	}
	*out = value;
	return (HTTTP_OK);
}

static t_htttp_result	parse_header_line(const unsigned char *line,
		size_t line_len, t_htttp_message *message)
{
	t_htttp_result	result;
	size_t			colon;
	size_t			value_start;
	size_t			value_end;
	char			*name;
	char			*value;

	colon = 0u;
	while (colon < line_len && line[colon] != ':')
		colon++;
	if (colon == 0u || colon == line_len || !valid_header_name(line, colon))
		return (HTTTP_ERR_MALFORMED_HEADER);
	value_start = colon + 1u;
	while (value_start < line_len
		&& (line[value_start] == ' ' || line[value_start] == '\t'))
		value_start++;
	value_end = line_len;
	while (value_end > value_start
		&& (line[value_end - 1u] == ' ' || line[value_end - 1u] == '\t'))
		value_end--;
	if (!valid_header_value(line + value_start, value_end - value_start))
		return (HTTTP_ERR_MALFORMED_HEADER);
	name = NULL;
	value = NULL;
	result = copy_range(line, colon, &name);
	if (result != HTTTP_OK)
		return (result);
	result = copy_range(line + value_start, value_end - value_start, &value);
	if (result != HTTTP_OK)
	{
		free(name);
		return (result);
	}
	if (htttp_message_get_header(message, name) != NULL)
		result = HTTTP_ERR_DUPLICATE_HEADER;
	else
		result = htttp_message_set_header(message, name, value);
	free(name);
	free(value);
	return (result);
}

static int	valid_method(const unsigned char *text, size_t len)
{
	size_t	i;

	if (len == 0u || text[0] < 'A' || text[0] > 'Z')
		return (0);
	i = 1u;
	while (i < len)
	{
		if (!((text[i] >= 'A' && text[i] <= 'Z')
				|| (text[i] >= '0' && text[i] <= '9')
				|| text[i] == '_' || text[i] == '-'))
			return (0);
		i++;
	}
	return (1);
}

static int	valid_path(const unsigned char *text, size_t len)
{
	size_t	i;

	if (len == 0u || text[0] != '/')
		return (0);
	i = 0u;
	while (i < len)
	{
		if (text[i] < 0x21u || text[i] > 0x7eu)
			return (0);
		i++;
	}
	return (1);
}

static int	valid_reason(const unsigned char *text, size_t len)
{
	size_t	i;

	if (len == 0u)
		return (0);
	i = 0u;
	while (i < len)
	{
		if (text[i] != ' ' && (text[i] < 0x21u || text[i] > 0x7eu))
			return (0);
		i++;
	}
	return (1);
}

static int	valid_header_name(const unsigned char *text, size_t len)
{
	size_t	i;

	i = 0u;
	while (i < len)
	{
		if (!((text[i] >= 'A' && text[i] <= 'Z')
				|| (text[i] >= 'a' && text[i] <= 'z')
				|| (text[i] >= '0' && text[i] <= '9') || text[i] == '-'))
			return (0);
		i++;
	}
	return (len > 0u);
}

static int	valid_header_value(const unsigned char *text, size_t len)
{
	size_t	i;

	i = 0u;
	while (i < len)
	{
		if (text[i] != '\t' && (text[i] < 0x20u || text[i] > 0x7eu))
			return (0);
		i++;
	}
	return (1);
}

static t_htttp_result	copy_range(const unsigned char *text, size_t len,
		char **out)
{
	char	*copy;

	copy = malloc(len + 1u);
	if (copy == NULL)
		return (HTTTP_ERR_NO_MEMORY);
	memcpy(copy, text, len);
	copy[len] = '\0';
	*out = copy;
	return (HTTTP_OK);
}
