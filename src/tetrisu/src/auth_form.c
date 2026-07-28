#include "tetrisu.h"

static char			*focused_buffer(auth_form_t *form);
static bool			append_codepoint(char *text, size_t capacity,
						uint32_t codepoint);
static void			remove_codepoint(char *text);
static size_t		codepoint_count(const char *text);
static bool			domain_is_valid(const char *domain);
static bool			next_codepoint(const unsigned char **cursor,
						uint32_t *codepoint);
static bool			codepoint_is_space(uint32_t codepoint);
static auth_action_t	activate_focus(auth_form_t *form);
static void			reset_server_state(auth_form_t *form);
static void			restore_server_status(auth_form_t *form);
static void			set_status(auth_form_t *form, auth_feedback_t feedback,
						const char *message);

/**
 * @brief Initializes one login or sign-up form.
 */
void	auth_form_init(auth_form_t *form, auth_form_mode_t mode)
{
	if (form == NULL)
		return ;
	memset(form, 0, sizeof(*form));
	form->mode = mode;
	form->focus = AUTH_FOCUS_USERNAME;
	reset_server_state(form);
}

/**
 * @brief Changes form mode while preserving reusable username/domain values.
 */
void	auth_form_set_mode(auth_form_t *form, auth_form_mode_t mode)
{
	if (form == NULL)
		return ;
	form->mode = mode;
	form->focus = AUTH_FOCUS_USERNAME;
	form->password[0] = '\0';
	form->confirm[0] = '\0';
	restore_server_status(form);
}

/**
 * @brief Advances through the accessible visual focus order.
 */
void	auth_form_focus_next(auth_form_t *form)
{
	if (form == NULL)
		return ;
	if (form->focus == AUTH_FOCUS_USERNAME)
		form->focus = AUTH_FOCUS_PASSWORD;
	else if (form->focus == AUTH_FOCUS_PASSWORD
		&& form->mode == AUTH_FORM_SIGN_UP)
		form->focus = AUTH_FOCUS_CONFIRM;
	else if (form->focus == AUTH_FOCUS_PASSWORD
		|| form->focus == AUTH_FOCUS_CONFIRM)
		form->focus = AUTH_FOCUS_DOMAIN;
	else if (form->focus == AUTH_FOCUS_DOMAIN)
		form->focus = AUTH_FOCUS_PRIMARY;
	else if (form->focus == AUTH_FOCUS_PRIMARY)
		form->focus = AUTH_FOCUS_SECONDARY;
	else if (form->focus == AUTH_FOCUS_SECONDARY)
		form->focus = AUTH_FOCUS_OFFLINE;
	else
		form->focus = AUTH_FOCUS_USERNAME;
}

/**
 * @brief Moves backwards through the accessible visual focus order.
 */
void	auth_form_focus_previous(auth_form_t *form)
{
	if (form == NULL)
		return ;
	if (form->focus == AUTH_FOCUS_USERNAME)
		form->focus = AUTH_FOCUS_OFFLINE;
	else if (form->focus == AUTH_FOCUS_OFFLINE)
		form->focus = AUTH_FOCUS_SECONDARY;
	else if (form->focus == AUTH_FOCUS_SECONDARY)
		form->focus = AUTH_FOCUS_PRIMARY;
	else if (form->focus == AUTH_FOCUS_PRIMARY)
		form->focus = AUTH_FOCUS_DOMAIN;
	else if (form->focus == AUTH_FOCUS_DOMAIN
		&& form->mode == AUTH_FORM_SIGN_UP)
		form->focus = AUTH_FOCUS_CONFIRM;
	else if (form->focus == AUTH_FOCUS_DOMAIN)
		form->focus = AUTH_FOCUS_PASSWORD;
	else if (form->focus == AUTH_FOCUS_CONFIRM)
		form->focus = AUTH_FOCUS_PASSWORD;
	else
		form->focus = AUTH_FOCUS_USERNAME;
}

/**
 * @brief Applies one keyboard event and returns any requested screen action.
 */
auth_action_t	auth_form_handle_key(auth_form_t *form, uint32_t key)
{
	char	*buffer;

	if (form == NULL || form->feedback == AUTH_FEEDBACK_LOADING)
		return (AUTH_ACTION_NONE);
	if (key == 3u)
		return (AUTH_ACTION_QUIT);
	if (key == NCKEY_ESC)
	{
		if (form->mode == AUTH_FORM_SIGN_UP)
			return (AUTH_ACTION_OPEN_LOGIN);
		return (AUTH_ACTION_QUIT);
	}
	if (key == NCKEY_UP)
		auth_form_focus_previous(form);
	else if (key == NCKEY_DOWN || key == NCKEY_TAB)
		auth_form_focus_next(form);
	else if (key == NCKEY_ENTER || key == '\n' || key == '\r')
		return (activate_focus(form));
	else
	{
		buffer = focused_buffer(form);
		if (buffer == NULL)
			return (AUTH_ACTION_NONE);
		if (key == NCKEY_BACKSPACE || key == NCKEY_DEL
			|| key == 127u || key == 8u)
			remove_codepoint(buffer);
		else if (key >= 32u && key <= 0x10ffffu)
			(void)append_codepoint(buffer, AUTH_FIELD_MAX, key);
		else
			return (AUTH_ACTION_NONE);
		if (form->focus == AUTH_FOCUS_DOMAIN)
			reset_server_state(form);
		else
			restore_server_status(form);
	}
	return (AUTH_ACTION_NONE);
}

/**
 * @brief Starts a server availability check for the entered domain.
 */
bool	auth_form_begin_server_check(auth_form_t *form)
{
	if (form == NULL)
		return (false);
	if (!domain_is_valid(form->domain))
	{
		set_status(form, AUTH_FEEDBACK_ERROR,
			"ENTER A DOMAIN WITHOUT SPACES");
		return (false);
	}
	form->server_state = AUTH_SERVER_CHECKING;
	set_status(form, AUTH_FEEDBACK_LOADING, "CHECKING SERVER...");
	return (true);
}

/**
 * @brief Completes the server availability check.
 */
void	auth_form_finish_server_check(auth_form_t *form, bool online)
{
	if (form == NULL || form->server_state != AUTH_SERVER_CHECKING)
		return ;
	if (online)
	{
		form->server_state = AUTH_SERVER_ONLINE;
		set_status(form, AUTH_FEEDBACK_SUCCESS, "SERVER ONLINE - READY");
	}
	else
	{
		form->server_state = AUTH_SERVER_OFFLINE;
		set_status(form, AUTH_FEEDBACK_ERROR,
			"SERVER OFFLINE - PLAY OFFLINE");
	}
}

/**
 * @brief Reports whether server-backed primary actions may be selected.
 */
bool	auth_form_online_enabled(const auth_form_t *form)
{
	return (form != NULL && form->server_state == AUTH_SERVER_ONLINE);
}

/**
 * @brief Validates the active form and exposes a concise reader-facing error.
 */
bool	auth_form_validate(auth_form_t *form)
{
	if (form == NULL)
		return (false);
	if (!auth_form_online_enabled(form))
	{
		restore_server_status(form);
		return (false);
	}
	if (form->username[0] == '\0')
		set_status(form, AUTH_FEEDBACK_ERROR, "USERNAME IS REQUIRED");
	else if (codepoint_count(form->password) < AUTH_PASSWORD_MIN)
		set_status(form, AUTH_FEEDBACK_ERROR,
			"PASSWORD NEEDS AT LEAST 4 CHARACTERS");
	else if (form->mode == AUTH_FORM_SIGN_UP
		&& strcmp(form->password, form->confirm) != 0)
		set_status(form, AUTH_FEEDBACK_ERROR, "PASSWORDS DO NOT MATCH");
	else if (!domain_is_valid(form->domain))
		set_status(form, AUTH_FEEDBACK_ERROR,
			"ENTER A DOMAIN WITHOUT SPACES");
	else
	{
		set_status(form, AUTH_FEEDBACK_LOADING,
			form->mode == AUTH_FORM_SIGN_UP
			? "CREATING ACCOUNT..." : "SIGNING IN...");
		return (true);
	}
	return (false);
}

/**
 * @brief Resolves validated credentials through the selected provider.
 */
app_provider_result_t	auth_form_submit(auth_form_t *form,
	const app_data_provider_t *provider, app_auth_view_model_t *view)
{
	app_provider_result_t	result;

	if (form == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	if (!auth_form_online_enabled(form))
	{
		restore_server_status(form);
		return (APP_PROVIDER_UNAVAILABLE);
	}
	if (form->feedback != AUTH_FEEDBACK_LOADING && !auth_form_validate(form))
		return (APP_PROVIDER_INVALID);
	if (provider == NULL)
		result = APP_PROVIDER_UNAVAILABLE;
	else if (form->mode == AUTH_FORM_SIGN_UP && provider->sign_up != NULL)
		result = provider->sign_up(provider->userdata, form->username,
				form->password, form->domain, view);
	else if (form->mode == AUTH_FORM_LOGIN && provider->login != NULL)
		result = provider->login(provider->userdata, form->username,
				form->password, form->domain, view);
	else
		result = APP_PROVIDER_UNAVAILABLE;
	if (result == APP_PROVIDER_OK)
		set_status(form, AUTH_FEEDBACK_SUCCESS,
			form->mode == AUTH_FORM_SIGN_UP
			? "ACCOUNT CREATED - PLEASE SIGN IN" : "WELCOME TO TETRISU!");
	else if (result == APP_PROVIDER_UNAVAILABLE)
		set_status(form, AUTH_FEEDBACK_ERROR, "SERVER IS UNAVAILABLE");
	else if (result == APP_PROVIDER_INVALID)
		set_status(form, AUTH_FEEDBACK_ERROR, "CHECK YOUR ACCOUNT DETAILS");
	else
		set_status(form, AUTH_FEEDBACK_ERROR, "COULD NOT CONNECT");
	return (result);
}

/**
 * @brief Produces an ASCII mask with one glyph per UTF-8 codepoint.
 */
bool	auth_form_mask_password(const char *password, char *masked, size_t size)
{
	size_t	count;

	if (password == NULL || masked == NULL || size == 0)
		return (false);
	count = codepoint_count(password);
	if (count >= size)
		count = size - 1;
	memset(masked, '*', count);
	masked[count] = '\0';
	return (true);
}

static char	*focused_buffer(auth_form_t *form)
{
	if (form->focus == AUTH_FOCUS_USERNAME)
		return (form->username);
	if (form->focus == AUTH_FOCUS_PASSWORD)
		return (form->password);
	if (form->focus == AUTH_FOCUS_CONFIRM
		&& form->mode == AUTH_FORM_SIGN_UP)
		return (form->confirm);
	if (form->focus == AUTH_FOCUS_DOMAIN)
		return (form->domain);
	return (NULL);
}

static bool	append_codepoint(char *text, size_t capacity, uint32_t codepoint)
{
	unsigned char	bytes[4];
	size_t			length;
	size_t			current;

	if (codepoint >= 0xd800u && codepoint <= 0xdfffu)
		return (false);
	if (codepoint <= 0x7fu)
		length = 1;
	else if (codepoint <= 0x7ffu)
		length = 2;
	else if (codepoint <= 0xffffu)
		length = 3;
	else if (codepoint <= 0x10ffffu)
		length = 4;
	else
		return (false);
	current = strlen(text);
	if (current + length >= capacity)
		return (false);
	if (length == 1)
		bytes[0] = (unsigned char)codepoint;
	else if (length == 2)
	{
		bytes[0] = (unsigned char)(0xc0u | (codepoint >> 6));
		bytes[1] = (unsigned char)(0x80u | (codepoint & 0x3fu));
	}
	else if (length == 3)
	{
		bytes[0] = (unsigned char)(0xe0u | (codepoint >> 12));
		bytes[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
		bytes[2] = (unsigned char)(0x80u | (codepoint & 0x3fu));
	}
	else
	{
		bytes[0] = (unsigned char)(0xf0u | (codepoint >> 18));
		bytes[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3fu));
		bytes[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
		bytes[3] = (unsigned char)(0x80u | (codepoint & 0x3fu));
	}
	memcpy(text + current, bytes, length);
	text[current + length] = '\0';
	return (true);
}

static void	remove_codepoint(char *text)
{
	size_t	length;

	length = strlen(text);
	if (length == 0)
		return ;
	length--;
	while (length > 0 && (((unsigned char)text[length] & 0xc0u) == 0x80u))
		length--;
	text[length] = '\0';
}

static size_t	codepoint_count(const char *text)
{
	size_t	count;

	count = 0;
	while (*text != '\0')
	{
		if (((unsigned char)*text & 0xc0u) != 0x80u)
			count++;
		text++;
	}
	return (count);
}

static bool	domain_is_valid(const char *domain)
{
	const unsigned char	*cursor;
	uint32_t			codepoint;

	if (domain == NULL || domain[0] == '\0')
		return (false);
	cursor = (const unsigned char *)domain;
	while (*cursor != '\0')
	{
		if (!next_codepoint(&cursor, &codepoint)
			|| codepoint_is_space(codepoint))
			return (false);
	}
	return (true);
}

static bool	next_codepoint(const unsigned char **cursor, uint32_t *codepoint)
{
	const unsigned char	*text;
	size_t				length;
	size_t				index;

	text = *cursor;
	if (text[0] < 0x80u)
	{
		*codepoint = text[0];
		length = 1;
	}
	else if ((text[0] & 0xe0u) == 0xc0u)
	{
		*codepoint = text[0] & 0x1fu;
		length = 2;
	}
	else if ((text[0] & 0xf0u) == 0xe0u)
	{
		*codepoint = text[0] & 0x0fu;
		length = 3;
	}
	else if ((text[0] & 0xf8u) == 0xf0u)
	{
		*codepoint = text[0] & 0x07u;
		length = 4;
	}
	else
		return (false);
	index = 1;
	while (index < length)
	{
		if (text[index] == '\0' || (text[index] & 0xc0u) != 0x80u)
			return (false);
		*codepoint = (*codepoint << 6) | (text[index] & 0x3fu);
		index++;
	}
	*cursor += length;
	return (*codepoint <= 0x10ffffu
		&& !(*codepoint >= 0xd800u && *codepoint <= 0xdfffu));
}

static bool	codepoint_is_space(uint32_t codepoint)
{
	if (codepoint <= 0x7fu)
		return (isspace((unsigned char)codepoint) != 0);
	return (codepoint == 0x00a0u || codepoint == 0x1680u
		|| (codepoint >= 0x2000u && codepoint <= 0x200au)
		|| codepoint == 0x2028u || codepoint == 0x2029u
		|| codepoint == 0x202fu || codepoint == 0x205fu
		|| codepoint == 0x3000u);
}

static auth_action_t	activate_focus(auth_form_t *form)
{
	if (form->focus == AUTH_FOCUS_DOMAIN)
	{
		if (auth_form_begin_server_check(form))
			return (AUTH_ACTION_CHECK_SERVER);
		return (AUTH_ACTION_NONE);
	}
	if (form->focus == AUTH_FOCUS_USERNAME
		|| form->focus == AUTH_FOCUS_PASSWORD
		|| form->focus == AUTH_FOCUS_CONFIRM)
	{
		auth_form_focus_next(form);
		return (AUTH_ACTION_NONE);
	}
	if (form->focus == AUTH_FOCUS_PRIMARY)
	{
		if (!auth_form_validate(form))
			return (AUTH_ACTION_NONE);
		if (form->mode == AUTH_FORM_SIGN_UP)
			return (AUTH_ACTION_SUBMIT_SIGN_UP);
		return (AUTH_ACTION_SUBMIT_LOGIN);
	}
	if (form->focus == AUTH_FOCUS_SECONDARY)
	{
		if (form->mode == AUTH_FORM_SIGN_UP)
			return (AUTH_ACTION_OPEN_LOGIN);
		return (AUTH_ACTION_OPEN_SIGN_UP);
	}
	if (form->focus == AUTH_FOCUS_OFFLINE)
		return (AUTH_ACTION_PLAY_OFFLINE);
	return (AUTH_ACTION_NONE);
}

static void	reset_server_state(auth_form_t *form)
{
	form->server_state = AUTH_SERVER_UNVERIFIED;
	set_status(form, AUTH_FEEDBACK_IDLE, "ENTER SERVER ID TO CHECK");
}

static void	restore_server_status(auth_form_t *form)
{
	if (form->server_state == AUTH_SERVER_CHECKING)
		set_status(form, AUTH_FEEDBACK_LOADING, "CHECKING SERVER...");
	else if (form->server_state == AUTH_SERVER_ONLINE)
		set_status(form, AUTH_FEEDBACK_SUCCESS, "SERVER ONLINE - READY");
	else if (form->server_state == AUTH_SERVER_OFFLINE)
		set_status(form, AUTH_FEEDBACK_ERROR,
			"SERVER OFFLINE - PLAY OFFLINE");
	else
		set_status(form, AUTH_FEEDBACK_IDLE, "ENTER SERVER ID TO CHECK");
}

static void	set_status(auth_form_t *form, auth_feedback_t feedback,
	const char *message)
{
	form->feedback = feedback;
	snprintf(form->status, sizeof(form->status), "%s", message);
}
