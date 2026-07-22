#include "htttp.h"

static int	message_shape_valid(const t_htttp_message *message);
static int	valid_method(const char *text);
static int	valid_path(const char *text);
static int	valid_reason(const char *text);
static int	valid_header_name(const char *text);
static int	valid_header_value(const char *text);
static unsigned char	ascii_lower(unsigned char c);
static int	ascii_case_equal(const char *left, const char *right);
static t_htttp_result	validate_response(const t_htttp_message *message);
static t_htttp_result	validate_request(const t_htttp_message *message,
						unsigned int flags);
static int	nonempty_non_ows(const char *value);
static int	valid_rfc1123_date(const char *value);
static int	token_in_table(const char *value, const char *const *table,
				size_t count);
static int	two_digits(const char *value);

static const char	*const g_weekdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char	*const g_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

t_htttp_result	htttp_validate(const t_htttp_message *message,
		unsigned int flags)
{
	if (message == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if ((flags & ~HTTTP_VALIDATE_AUTHENTICATED_REQUEST) != 0u)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (!message_shape_valid(message))
		return (HTTTP_ERR_INVALID_MESSAGE);
	if (message->type == HTTTP_MESSAGE_RESPONSE)
	{
		if ((flags & HTTTP_VALIDATE_AUTHENTICATED_REQUEST) != 0u)
			return (HTTTP_ERR_INVALID_ARGUMENT);
		return (validate_response(message));
	}
	return (validate_request(message, flags));
}

t_htttp_result	htttp_format_date(time_t timestamp,
		char out[HTTTP_DATE_BUFSIZE])
{
	struct tm	utc;
	int		year;
	int		written;

	if (out == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	out[0] = '\0';
	if (gmtime_r(&timestamp, &utc) == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (utc.tm_wday < 0 || utc.tm_wday > 6 || utc.tm_mon < 0
		|| utc.tm_mon > 11 || utc.tm_year < 0 || utc.tm_year > 8099)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	year = utc.tm_year + 1900;
	/* AI-assisted: fixed English tables avoid locale-dependent %a/%b output,
	 * while gmtime_r() avoids shared static calendar storage across threads. */
	written = snprintf(out, HTTTP_DATE_BUFSIZE,
			"%s, %02d %s %04d %02d:%02d:%02d GMT",
			g_weekdays[utc.tm_wday], utc.tm_mday, g_months[utc.tm_mon], year,
			utc.tm_hour, utc.tm_min, utc.tm_sec);
	if (written != (int)(HTTTP_DATE_BUFSIZE - 1u))
	{
		out[0] = '\0';
		return (HTTTP_ERR_INVALID_ARGUMENT);
	}
	return (HTTTP_OK);
}

static int	message_shape_valid(const t_htttp_message *message)
{
	size_t	i;
	size_t	j;

	if (message->header_count > HTTTP_MAX_HEADERS
		|| (message->header_count > 0u && message->headers == NULL)
		|| (message->body_len > 0u && message->body == NULL)
		|| (message->body_len == 0u && message->body != NULL))
		return (0);
	i = 0u;
	while (i < message->header_count)
	{
		if (!valid_header_name(message->headers[i].name)
			|| !valid_header_value(message->headers[i].value))
			return (0);
		j = 0u;
		while (j < i)
		{
			if (ascii_case_equal(message->headers[j].name,
					message->headers[i].name))
				return (0);
			j++;
		}
		i++;
	}
	if (message->type == HTTTP_MESSAGE_REQUEST)
		return (valid_method(message->method) && valid_path(message->path)
			&& message->status_code == 0u && message->reason == NULL);
	if (message->type == HTTTP_MESSAGE_RESPONSE)
		return (message->method == NULL && message->path == NULL
			&& message->status_code >= 100u && message->status_code <= 599u
			&& valid_reason(message->reason));
	return (0);
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

static t_htttp_result	validate_response(const t_htttp_message *message)
{
	const char	*date;

	date = htttp_message_get_header(message, "Date");
	if (date == NULL || !valid_rfc1123_date(date))
		return (HTTTP_ERR_MISSING_REQUIRED_HEADER);
	return (HTTTP_OK);
}

static t_htttp_result	validate_request(const t_htttp_message *message,
		unsigned int flags)
{
	const char	*content_type;
	const char	*expected_type;
	const char	*player_id;

	if (message->body_len > 0u)
	{
		content_type = htttp_message_get_header(message, "Content-Type");
		expected_type = HTTTP_CONTENT_TYPE_COMMAND;
		if (strcmp(message->method, "STATE") == 0)
			expected_type = HTTTP_CONTENT_TYPE_STATE;
		if (content_type == NULL || strcmp(content_type, expected_type) != 0)
			return (HTTTP_ERR_MISSING_REQUIRED_HEADER);
	}
	if ((flags & HTTTP_VALIDATE_AUTHENTICATED_REQUEST) != 0u)
	{
		player_id = htttp_message_get_header(message, "Player-Id");
		if (player_id == NULL || !nonempty_non_ows(player_id))
			return (HTTTP_ERR_MISSING_REQUIRED_HEADER);
	}
	return (HTTTP_OK);
}

static int	nonempty_non_ows(const char *value)
{
	size_t	i;

	i = 0u;
	while (value[i] != '\0')
	{
		if (value[i] != ' ' && value[i] != '\t')
			return (1);
		i++;
	}
	return (0);
}

static int	valid_rfc1123_date(const char *value)
{
	int	day;
	int	hour;
	int	minute;
	int	second;
	int	year;
	size_t	i;

	if (strlen(value) != HTTTP_DATE_BUFSIZE - 1u
		|| !token_in_table(value, g_weekdays, 7u)
		|| value[3] != ',' || value[4] != ' '
		|| value[7] != ' ' || !token_in_table(value + 8u, g_months, 12u)
		|| value[11] != ' ' || value[16] != ' ' || value[19] != ':'
		|| value[22] != ':' || value[25] != ' '
		|| memcmp(value + 26u, "GMT", 3u) != 0)
		return (0);
	day = two_digits(value + 5u);
	hour = two_digits(value + 17u);
	minute = two_digits(value + 20u);
	second = two_digits(value + 23u);
	if (day < 1 || day > 31 || hour < 0 || hour > 23
		|| minute < 0 || minute > 59 || second < 0 || second > 60)
		return (0);
	year = 0;
	i = 12u;
	while (i < 16u)
	{
		if (value[i] < '0' || value[i] > '9')
			return (0);
		year = year * 10 + (value[i] - '0');
		i++;
	}
	return (year >= 1900);
}

static int	token_in_table(const char *value, const char *const *table,
		size_t count)
{
	size_t	i;

	i = 0u;
	while (i < count)
	{
		if (memcmp(value, table[i], 3u) == 0)
			return (1);
		i++;
	}
	return (0);
}

static int	two_digits(const char *value)
{
	if (value[0] < '0' || value[0] > '9'
		|| value[1] < '0' || value[1] > '9')
		return (-1);
	return ((value[0] - '0') * 10 + (value[1] - '0'));
}
