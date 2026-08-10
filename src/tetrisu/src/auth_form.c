#include "tetrisu.h"

static char			*focused_buffer(t_auth_form *form);
static bool			append_codepoint(char *text, size_t capacity,
						uint32_t codepoint);
static void			remove_codepoint(char *text);
static size_t		codepoint_count(const char *text);
static bool			domain_is_valid(const char *domain);
static bool			username_is_valid(const char *username);
static bool			next_codepoint(const unsigned char **cursor,
						uint32_t *codepoint);
static bool			codepoint_is_space(uint32_t codepoint);
static t_auth_action	activate_focus(t_auth_form *form);
static void			reset_server_state(t_auth_form *form);
static void			restore_server_status(t_auth_form *form);
static void			set_status(t_auth_form *form, t_auth_feedback feedback,
						const char *message);

/**
 * @brief Initializes one login or sign-up form.
 */
void	auth_form_init(t_auth_form *form, t_auth_form_mode mode)
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
void	auth_form_set_mode(t_auth_form *form, t_auth_form_mode mode)
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
void	auth_form_focus_next(t_auth_form *form)
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
	{
		if (form->mode == AUTH_FORM_LOGIN && app_ui_preview_enabled())
			form->focus = AUTH_FOCUS_PREVIEW;
		else
			form->focus = AUTH_FOCUS_OFFLINE;
	}
	else if (form->focus == AUTH_FOCUS_PREVIEW)
		form->focus = AUTH_FOCUS_OFFLINE;
	else
		form->focus = AUTH_FOCUS_USERNAME;
}

/**
 * @brief Moves backwards through the accessible visual focus order.
 */
void	auth_form_focus_previous(t_auth_form *form)
{
	if (form == NULL)
		return ;
	if (form->focus == AUTH_FOCUS_USERNAME)
		form->focus = AUTH_FOCUS_OFFLINE;
	else if (form->focus == AUTH_FOCUS_OFFLINE)
	{
		if (form->mode == AUTH_FORM_LOGIN && app_ui_preview_enabled())
			form->focus = AUTH_FOCUS_PREVIEW;
		else
			form->focus = AUTH_FOCUS_SECONDARY;
	}
	else if (form->focus == AUTH_FOCUS_PREVIEW)
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
t_auth_action	auth_form_handle_key(t_auth_form *form, uint32_t key)
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
	else if ((key == 'p' || key == 'P') && form->mode == AUTH_FORM_LOGIN
		&& app_ui_preview_enabled() && form->focus >= AUTH_FOCUS_PRIMARY)
		return (AUTH_ACTION_PREVIEW_LOGIN);
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
bool	auth_form_begin_server_check(t_auth_form *form)
{
	if (form == NULL)
		return (false);
	if (!domain_is_valid(form->domain))
	{
		set_status(form, AUTH_FEEDBACK_ERROR,
			"DOMAIN WITHOUT SPACES");
		return (false);
	}
	form->server_state = AUTH_SERVER_CHECKING;
	set_status(form, AUTH_FEEDBACK_LOADING, "CHECKING SERVER...");
	return (true);
}

/**
 * @brief Completes the server availability check.
 */
void	auth_form_finish_server_check(t_auth_form *form, bool online)
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
			"OFFLINE - USE PLAY OFFLINE");
	}
}

/**
 * @brief Reports whether server-backed primary actions may be selected.
 */
bool	auth_form_online_enabled(const t_auth_form *form)
{
	return (form != NULL && form->server_state == AUTH_SERVER_ONLINE);
}

/**
 * @brief Validates the active form and exposes a concise reader-facing error.
 */
bool	auth_form_validate(t_auth_form *form)
{
	if (form == NULL)
		return (false);
	if (!auth_form_online_enabled(form))
	{
		/*
		 * Repeating the unchanged server status here would leave the screen
		 * identical to the one that refused the key, which reads as a frozen
		 * client. Name the blocker and the door that is actually open instead.
		 */
		if (form->server_state == AUTH_SERVER_OFFLINE)
			set_status(form, AUTH_FEEDBACK_ERROR,
				"OFFLINE - USE PLAY OFFLINE");
		else
			set_status(form, AUTH_FEEDBACK_ERROR, "CHECK SERVER ID FIRST");
		return (false);
	}
	if (form->username[0] == '\0')
		set_status(form, AUTH_FEEDBACK_ERROR, "USERNAME IS REQUIRED");
	else if (!username_is_valid(form->username))
		set_status(form, AUTH_FEEDBACK_ERROR, "USERNAME WITHOUT SPACES");
	else if (codepoint_count(form->password) < AUTH_PASSWORD_MIN)
		set_status(form, AUTH_FEEDBACK_ERROR,
			"PASSWORD NEEDS 4+ CHARS");
	else if (form->mode == AUTH_FORM_SIGN_UP
		&& strcmp(form->password, form->confirm) != 0)
		set_status(form, AUTH_FEEDBACK_ERROR, "PASSWORDS DO NOT MATCH");
	else if (!domain_is_valid(form->domain))
		set_status(form, AUTH_FEEDBACK_ERROR,
			"DOMAIN WITHOUT SPACES");
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
t_app_provider_result	auth_form_submit(t_auth_form *form,
	const t_app_data_provider *provider, t_app_auth_view_model *view)
{
	t_app_provider_result	result;

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
			? "ACCOUNT CREATED - SIGN IN" : "WELCOME TO TETRISU!");
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

static char	*focused_buffer(t_auth_form *form)
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

/**
 * @brief Checks a username against what tetrisd will store and send.
 *
 * Printable ASCII with no space, which is the server's rule (db_username_valid)
 * kept in step by hand: the client cannot link that archive, so this is a copy
 * rather than a call, and the server is still the one that decides.
 *
 * Refusing here is about where the player finds out. A name with a space in it
 * is a 400 they have to interpret after the form has already been submitted;
 * this way the field they must fix is still in front of them, and it is the
 * same shape of check the domain field already gets.
 *
 * Multi-byte input is refused with everything else outside the range - a name
 * is written into bodies that are plain bytes, and the composer already drops
 * what it cannot put on the wire.
 *
 * The length is measured against the wire's field, not the form's: the box
 * holds AUTH_FIELD_MAX bytes and the account holds NET_USER_MAX, so a name
 * between the two would type in cleanly and be refused by the server.
 *
 * @param username Candidate name from the form.
 * @return true when tetrisd would accept the name.
 */
static bool	username_is_valid(const char *username)
{
	size_t	index;

	if (username == NULL || username[0] == '\0')
		return (false);
	index = 0;
	while (username[index] != '\0')
	{
		if ((unsigned char)username[index] <= 0x20
			|| (unsigned char)username[index] >= 0x7f)
			return (false);
		index++;
	}
	return (index < NET_USER_MAX);
}

static bool	domain_is_valid(const char *domain)
{
	const unsigned char	*cursor;
	uint32_t			codepoint;
	bool				saw_colon;

	if (domain == NULL || domain[0] == '\0')
		return (false);
	cursor = (const unsigned char *)domain;
	saw_colon = false;
	while (*cursor != '\0')
	{
		if (!next_codepoint(&cursor, &codepoint)
			|| codepoint_is_space(codepoint))
			return (false);
		if (codepoint == ':')
		{
			if (saw_colon)
				return (false);
			saw_colon = true;
		}
		else if (saw_colon && (codepoint < '0' || codepoint > '9'))
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

static t_auth_action	activate_focus(t_auth_form *form)
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
	if (form->focus == AUTH_FOCUS_PREVIEW
		&& form->mode == AUTH_FORM_LOGIN && app_ui_preview_enabled())
		return (AUTH_ACTION_PREVIEW_LOGIN);
	if (form->focus == AUTH_FOCUS_OFFLINE)
		return (AUTH_ACTION_PLAY_OFFLINE);
	return (AUTH_ACTION_NONE);
}

static void	reset_server_state(t_auth_form *form)
{
	form->server_state = AUTH_SERVER_UNVERIFIED;
	set_status(form, AUTH_FEEDBACK_IDLE, "ENTER SERVER ID TO CHECK");
}

static void	restore_server_status(t_auth_form *form)
{
	if (form->server_state == AUTH_SERVER_CHECKING)
		set_status(form, AUTH_FEEDBACK_LOADING, "CHECKING SERVER...");
	else if (form->server_state == AUTH_SERVER_ONLINE)
		set_status(form, AUTH_FEEDBACK_SUCCESS, "SERVER ONLINE - READY");
	else if (form->server_state == AUTH_SERVER_OFFLINE)
		set_status(form, AUTH_FEEDBACK_ERROR,
			"OFFLINE - USE PLAY OFFLINE");
	else
		set_status(form, AUTH_FEEDBACK_IDLE, "ENTER SERVER ID TO CHECK");
}

static void	set_status(t_auth_form *form, t_auth_feedback feedback,
	const char *message)
{
	form->feedback = feedback;
	snprintf(form->status, sizeof(form->status), "%s", message);
}
