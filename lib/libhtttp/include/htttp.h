#ifndef HTTTP_H
# define HTTTP_H

# include <stddef.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>

# define HTTTP_VERSION						"HTTTP/1.0"
# define HTTTP_MAX_MESSAGE_SIZE				65536u
# define HTTTP_MAX_HEADERS					64u
# define HTTTP_DATE_BUFSIZE					30u
# define HTTTP_CONTENT_TYPE_COMMAND			"application/tetris-command"
# define HTTTP_CONTENT_TYPE_STATE			"application/tetris-state"
# define HTTTP_VALIDATE_AUTHENTICATED_REQUEST	0x01u

typedef enum
{
	HTTTP_MESSAGE_REQUEST = 0,
	HTTTP_MESSAGE_RESPONSE
}	t_htttp_message_type;

typedef enum
{
	HTTTP_OK = 0,
	HTTTP_ERR_INVALID_ARGUMENT,
	HTTTP_ERR_TOO_LARGE,
	HTTTP_ERR_NO_MEMORY,
	HTTTP_ERR_MALFORMED_START_LINE,
	HTTTP_ERR_UNSUPPORTED_VERSION,
	HTTTP_ERR_MALFORMED_HEADER,
	HTTTP_ERR_TOO_MANY_HEADERS,
	HTTTP_ERR_DUPLICATE_HEADER,
	HTTTP_ERR_INVALID_CONTENT_LENGTH,
	HTTTP_ERR_LENGTH_MISMATCH,
	HTTTP_ERR_MISSING_REQUIRED_HEADER,
	HTTTP_ERR_INVALID_MESSAGE,
	HTTTP_ERR_NO_HANDLER
}	t_htttp_result;

typedef struct
{
	char	*name;
	char	*value;
}	t_htttp_header;

typedef struct
{
	t_htttp_message_type	type;
	char					*method;
	char					*path;
	unsigned int			status_code;
	char					*reason;
	t_htttp_header			*headers;
	size_t					header_count;
	unsigned char			*body;
	size_t					body_len;
}	t_htttp_message;

typedef struct
{
	const char	*method;
	int			(*handler)(const t_htttp_message *message, void *context);
}	t_htttp_route;

/* MESSAGE.C */

/**
 * Initializes caller-owned storage to empty request state. NULL is ignored.
 * Calling this on a message that still owns fields would leak those fields;
 * call htttp_message_free() before reuse.
 */
void			htttp_message_init(t_htttp_message *message);

/**
 * Frees every field in an initialized message and restores empty state.
 * NULL and repeated calls after initialization are safe.
 */
void			htttp_message_free(t_htttp_message *message);

/**
 * Deep-copies a request into an initialized, empty message. Arguments must be
 * non-NULL. Failure returns INVALID_ARGUMENT or NO_MEMORY and leaves it empty.
 */
t_htttp_result	htttp_message_make_request(t_htttp_message *message,
					const char *method, const char *path);

/**
 * Deep-copies a response into an initialized, empty message. Reason must be
 * non-NULL. Failure returns INVALID_ARGUMENT or NO_MEMORY and leaves it empty.
 */
t_htttp_result	htttp_message_make_response(t_htttp_message *message,
					unsigned int status_code, const char *reason);

/**
 * Adds or case-insensitively replaces one header with owned copies. Failure
 * leaves existing headers unchanged and reports invalid input, limit, or OOM.
 */
t_htttp_result	htttp_message_set_header(t_htttp_message *message,
					const char *name, const char *value);

/**
 * Returns a borrowed value for a case-insensitive name, or NULL if absent.
 * The pointer remains valid until that header changes or message is freed.
 */
const char		*htttp_message_get_header(const t_htttp_message *message,
					const char *name);

/**
 * Replaces body with an owned byte copy; zero length clears it. Nonzero length
 * requires non-NULL input. Allocation failure preserves the previous body.
 */
t_htttp_result	htttp_message_set_body(t_htttp_message *message,
					const void *body, size_t body_len);

/** Returns borrowed immutable default reason text, or NULL for unknown code. */
const char		*htttp_reason_phrase(unsigned int status_code);

/** Returns borrowed immutable diagnostic text, or NULL for unknown result. */
const char		*htttp_result_string(t_htttp_result result);

/* PARSER.C */

/**
 * Parses one complete frame into an initialized, empty output message. Success
 * transfers owned fields to out; every failure leaves out empty.
 */
t_htttp_result	htttp_parse(const unsigned char *data, size_t data_len,
					t_htttp_message *out);

/* SERIALISER.C */

/**
 * Allocates exact wire bytes. On entry/failure outputs become NULL and zero;
 * on success caller owns *out and releases it with free().
 */
t_htttp_result	htttp_serialize(const t_htttp_message *message,
					unsigned char **out, size_t *out_len);

/* VALIDATION.C */

/**
 * Validates required headers under context flags without mutating message.
 * Returns MISSING_REQUIRED_HEADER for absent or wrong required values.
 */
t_htttp_result	htttp_validate(const t_htttp_message *message,
					unsigned int flags);

/**
 * Formats timestamp as 29-byte RFC 1123 UTC text plus NUL. Output must provide
 * HTTTP_DATE_BUFSIZE bytes. Failure stores an empty string when out is non-NULL.
 */
t_htttp_result	htttp_format_date(time_t timestamp,
					char out[HTTTP_DATE_BUFSIZE]);

/* DISPATCH.C */

/**
 * Invokes one exact request-method match and writes its integer return value.
 * handler_result is set to zero before validation and remains zero on error.
 * The function retains no route, message, or context pointer after return.
 */
t_htttp_result	htttp_dispatch(const t_htttp_message *message,
					const t_htttp_route *routes, size_t route_count,
					void *context, int *handler_result);

#endif
