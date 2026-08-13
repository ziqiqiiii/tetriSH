#include "htttp.h"

static t_htttp_result	validate_message(const t_htttp_message *message);
static int			valid_method(const char *text);
static int			valid_path(const char *text);
static int			valid_reason(const char *text);
static int			valid_header_name(const char *text);
static int			valid_header_value(const char *text);
static unsigned char	ascii_lower(unsigned char c);
static int			ascii_case_equal(const char *left, const char *right);
static t_htttp_result	calculate_size(const t_htttp_message *message,
						size_t *out);
static t_htttp_result	add_size(size_t *total, size_t amount);
static t_htttp_result	add_text_size(size_t *total, const char *text);
static size_t		decimal_digits(size_t value);
static void			write_bytes(unsigned char *wire, size_t wire_len,
						size_t *cursor, const void *data, size_t len);
static void			write_text(unsigned char *wire, size_t wire_len,
						size_t *cursor, const char *text);
static void			write_decimal(unsigned char *wire, size_t wire_len,
						size_t *cursor, size_t value);
static size_t		write_message(unsigned char *wire, size_t wire_len,
						const t_htttp_message *message);

static const char	g_content_length_name[] = "Content-Length";
static const char	g_content_length_prefix[] = "Content-Length: ";

t_htttp_result	htttp_serialize(const t_htttp_message *message,
		unsigned char **out, size_t *out_len)
{
	t_htttp_result	result;
	unsigned char	*wire;
	size_t			wire_len;
	size_t			written;

	if (out != NULL)
		*out = NULL;
	if (out_len != NULL)
		*out_len = 0u;
	if (message == NULL || out == NULL || out_len == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	result = validate_message(message);
	if (result != HTTTP_OK)
		return (result);
	/* AI-assisted: compute and cap every byte before allocation so public
	 * lengths cannot wrap or create a plaintext larger than one secure frame. */
	result = calculate_size(message, &wire_len);
	if (result != HTTTP_OK)
		return (result);
	wire = malloc(wire_len);
	if (wire == NULL)
		return (HTTTP_ERR_NO_MEMORY);
	written = write_message(wire, wire_len, message);
	if (written != wire_len)
	{
		free(wire);
		return (HTTTP_ERR_INVALID_MESSAGE);
	}
	*out = wire;
	*out_len = wire_len;
	return (HTTTP_OK);
}

static t_htttp_result	validate_message(const t_htttp_message *message)
{
	size_t	emitted_header_count;
	size_t	i;
	size_t	j;

	if (message->header_count > HTTTP_MAX_HEADERS
		|| (message->header_count > 0u && message->headers == NULL)
		|| (message->body_len > 0u && message->body == NULL)
		|| (message->body_len == 0u && message->body != NULL))
		return (HTTTP_ERR_INVALID_MESSAGE);
	if (message->type == HTTTP_MESSAGE_REQUEST)
	{
		if (!valid_method(message->method) || !valid_path(message->path)
			|| message->status_code != 0u || message->reason != NULL)
			return (HTTTP_ERR_INVALID_MESSAGE);
	}
	else if (message->type == HTTTP_MESSAGE_RESPONSE)
	{
		if (message->method != NULL || message->path != NULL
			|| message->status_code < 100u || message->status_code > 599u
			|| !valid_reason(message->reason))
			return (HTTTP_ERR_INVALID_MESSAGE);
	}
	else
		return (HTTTP_ERR_INVALID_MESSAGE);
	emitted_header_count = 0u;
	i = 0u;
	while (i < message->header_count)
	{
		if (!valid_header_name(message->headers[i].name)
			|| !valid_header_value(message->headers[i].value))
			return (HTTTP_ERR_INVALID_MESSAGE);
		j = 0u;
		while (j < i)
		{
			if (ascii_case_equal(message->headers[j].name,
					message->headers[i].name))
				return (HTTTP_ERR_INVALID_MESSAGE);
			j++;
		}
		if (!ascii_case_equal(message->headers[i].name,
				g_content_length_name))
			emitted_header_count++;
		i++;
	}
	/* AI-assisted: generated Content-Length consumes one wire-header slot even
	 * though it is absent from or replaces a caller-supplied header. */
	if (message->body_len > 0u)
		emitted_header_count++;
	if (emitted_header_count > HTTTP_MAX_HEADERS)
		return (HTTTP_ERR_TOO_MANY_HEADERS);
	return (HTTTP_OK);
}

static int	valid_method(const char *text)
{
	size_t	i;

	if (text == NULL || text[0] < 'A' || text[0] > 'Z')
		return (0);
	i = 1u;
	while (text[i] != '\0')
	{
		if (!((text[i] >= 'A' && text[i] <= 'Z')
				|| (text[i] >= '0' && text[i] <= '9')
				|| text[i] == '_' || text[i] == '-'))
			return (0);
		i++;
	}
	return (1);
}

static int	valid_path(const char *text)
{
	size_t	i;

	if (text == NULL || text[0] != '/')
		return (0);
	i = 0u;
	while (text[i] != '\0')
	{
		if ((unsigned char)text[i] < 0x21u
			|| (unsigned char)text[i] > 0x7eu)
			return (0);
		i++;
	}
	return (1);
}

static int	valid_reason(const char *text)
{
	size_t	i;

	if (text == NULL || text[0] == '\0')
		return (0);
	i = 0u;
	while (text[i] != '\0')
	{
		if (text[i] != ' ' && ((unsigned char)text[i] < 0x21u
				|| (unsigned char)text[i] > 0x7eu))
			return (0);
		i++;
	}
	return (1);
}

static int	valid_header_name(const char *text)
{
	size_t	i;

	if (text == NULL || text[0] == '\0')
		return (0);
	i = 0u;
	while (text[i] != '\0')
	{
		if (!((text[i] >= 'A' && text[i] <= 'Z')
				|| (text[i] >= 'a' && text[i] <= 'z')
				|| (text[i] >= '0' && text[i] <= '9') || text[i] == '-'))
			return (0);
		i++;
	}
	return (1);
}

static int	valid_header_value(const char *text)
{
	size_t	i;

	if (text == NULL)
		return (0);
	i = 0u;
	while (text[i] != '\0')
	{
		if (text[i] != '\t' && ((unsigned char)text[i] < 0x20u
				|| (unsigned char)text[i] > 0x7eu))
			return (0);
		i++;
	}
	return (1);
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

	i = 0u;
	while (left[i] != '\0' && right[i] != '\0')
	{
		if (ascii_lower((unsigned char)left[i])
			!= ascii_lower((unsigned char)right[i]))
			return (0);
		i++;
	}
	return (left[i] == right[i]);
}

static t_htttp_result	calculate_size(const t_htttp_message *message,
		size_t *out)
{
	t_htttp_result	result;
	size_t			total;
	size_t			i;

	total = 0u;
	if (message->type == HTTTP_MESSAGE_REQUEST)
	{
		result = add_text_size(&total, message->method);
		if (result == HTTTP_OK)
			result = add_size(&total, 1u);
		if (result == HTTTP_OK)
			result = add_text_size(&total, message->path);
		if (result == HTTTP_OK)
			result = add_size(&total, 1u);
		if (result == HTTTP_OK)
			result = add_text_size(&total, HTTTP_VERSION);
		if (result == HTTTP_OK)
			result = add_size(&total, 2u);
	}
	else
	{
		result = add_text_size(&total, HTTTP_VERSION);
		if (result == HTTTP_OK)
			result = add_size(&total, 1u);
		if (result == HTTTP_OK)
			result = add_size(&total, 3u);
		if (result == HTTTP_OK)
			result = add_size(&total, 1u);
		if (result == HTTTP_OK)
			result = add_text_size(&total, message->reason);
		if (result == HTTTP_OK)
			result = add_size(&total, 2u);
	}
	if (result != HTTTP_OK)
		return (result);
	i = 0u;
	while (i < message->header_count)
	{
		if (!ascii_case_equal(message->headers[i].name,
				g_content_length_name))
		{
			result = add_text_size(&total, message->headers[i].name);
			if (result == HTTTP_OK)
				result = add_size(&total, 2u);
			if (result == HTTTP_OK)
				result = add_text_size(&total, message->headers[i].value);
			if (result == HTTTP_OK)
				result = add_size(&total, 2u);
			if (result != HTTTP_OK)
				return (result);
		}
		i++;
	}
	if (message->body_len > 0u)
	{
		result = add_text_size(&total, g_content_length_prefix);
		if (result == HTTTP_OK)
			result = add_size(&total, decimal_digits(message->body_len));
		if (result == HTTTP_OK)
			result = add_size(&total, 2u);
		if (result != HTTTP_OK)
			return (result);
	}
	result = add_size(&total, 2u);
	if (result == HTTTP_OK)
		result = add_size(&total, message->body_len);
	if (result != HTTTP_OK)
		return (result);
	*out = total;
	return (HTTTP_OK);
}

static t_htttp_result	add_size(size_t *total, size_t amount)
{
	if (*total > HTTTP_MAX_MESSAGE_SIZE
		|| amount > HTTTP_MAX_MESSAGE_SIZE - *total)
		return (HTTTP_ERR_TOO_LARGE);
	*total += amount;
	return (HTTTP_OK);
}

static t_htttp_result	add_text_size(size_t *total, const char *text)
{
	return (add_size(total, strlen(text)));
}

static size_t	decimal_digits(size_t value)
{
	size_t	digits;

	digits = 1u;
	while (value >= 10u)
	{
		value /= 10u;
		digits++;
	}
	return (digits);
}

static void	write_bytes(unsigned char *wire, size_t wire_len,
		size_t *cursor, const void *data, size_t len)
{
	if (*cursor > wire_len || len > wire_len - *cursor)
	{
		*cursor = wire_len + 1u;
		return;
	}
	memcpy(wire + *cursor, data, len);
	*cursor += len;
}

static void	write_text(unsigned char *wire, size_t wire_len,
		size_t *cursor, const char *text)
{
	write_bytes(wire, wire_len, cursor, text, strlen(text));
}

static void	write_decimal(unsigned char *wire, size_t wire_len,
		size_t *cursor, size_t value)
{
	size_t	digits;
	size_t	i;

	digits = decimal_digits(value);
	if (*cursor > wire_len || digits > wire_len - *cursor)
	{
		*cursor = wire_len + 1u;
		return;
	}
	i = digits;
	while (i > 0u)
	{
		wire[*cursor + i - 1u] = (unsigned char)('0' + value % 10u);
		value /= 10u;
		i--;
	}
	*cursor += digits;
}

static size_t	write_message(unsigned char *wire, size_t wire_len,
		const t_htttp_message *message)
{
	size_t	cursor;
	size_t	i;

	cursor = 0u;
	if (message->type == HTTTP_MESSAGE_REQUEST)
	{
		write_text(wire, wire_len, &cursor, message->method);
		write_bytes(wire, wire_len, &cursor, " ", 1u);
		write_text(wire, wire_len, &cursor, message->path);
		write_bytes(wire, wire_len, &cursor, " ", 1u);
		write_text(wire, wire_len, &cursor, HTTTP_VERSION);
	}
	else
	{
		write_text(wire, wire_len, &cursor, HTTTP_VERSION);
		write_bytes(wire, wire_len, &cursor, " ", 1u);
		if (cursor > wire_len || 3u > wire_len - cursor)
			return (wire_len + 1u);
		wire[cursor++] = (unsigned char)('0' + message->status_code / 100u);
		wire[cursor++] = (unsigned char)('0'
				+ (message->status_code / 10u) % 10u);
		wire[cursor++] = (unsigned char)('0' + message->status_code % 10u);
		write_bytes(wire, wire_len, &cursor, " ", 1u);
		write_text(wire, wire_len, &cursor, message->reason);
	}
	write_bytes(wire, wire_len, &cursor, "\r\n", 2u);
	i = 0u;
	while (i < message->header_count)
	{
		if (!ascii_case_equal(message->headers[i].name,
				g_content_length_name))
		{
			write_text(wire, wire_len, &cursor, message->headers[i].name);
			write_bytes(wire, wire_len, &cursor, ": ", 2u);
			write_text(wire, wire_len, &cursor, message->headers[i].value);
			write_bytes(wire, wire_len, &cursor, "\r\n", 2u);
		}
		i++;
	}
	if (message->body_len > 0u)
	{
		write_text(wire, wire_len, &cursor, g_content_length_prefix);
		write_decimal(wire, wire_len, &cursor, message->body_len);
		write_bytes(wire, wire_len, &cursor, "\r\n", 2u);
	}
	write_bytes(wire, wire_len, &cursor, "\r\n", 2u);
	if (message->body_len > 0u)
		write_bytes(wire, wire_len, &cursor, message->body, message->body_len);
	return (cursor);
}
