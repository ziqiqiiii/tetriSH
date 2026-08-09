#include "tetrisu.h"

static void	test_login_focus_order(void);
static void	test_sign_up_focus_order(void);
static void	test_utf8_editing_and_masking(void);
static void	test_server_check_state(void);
static void	test_refused_primary_explains_itself(void);
static void	test_validation_and_submission(void);
static void	test_a_username_the_server_would_refuse_is_caught_here(void);
static void	test_secondary_actions(void);

int	main(void)
{
	(void)unsetenv("TETRISU_UI_PREVIEW");
	test_login_focus_order();
	test_sign_up_focus_order();
	test_utf8_editing_and_masking();
	test_server_check_state();
	test_refused_primary_explains_itself();
	test_validation_and_submission();
	test_a_username_the_server_would_refuse_is_caught_here();
	test_secondary_actions();
	return (0);
}

static void	test_login_focus_order(void)
{
	t_auth_form	form;

	auth_form_init(&form, AUTH_FORM_LOGIN);
	assert(form.focus == AUTH_FOCUS_USERNAME);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_PASSWORD);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_DOMAIN);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_PRIMARY);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_SECONDARY);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_OFFLINE);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_USERNAME);
	auth_form_focus_previous(&form);
	assert(form.focus == AUTH_FOCUS_OFFLINE);
	printf("PASS test_login_focus_order\n");
}

static void	test_sign_up_focus_order(void)
{
	t_auth_form	form;

	auth_form_init(&form, AUTH_FORM_SIGN_UP);
	auth_form_focus_next(&form);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_CONFIRM);
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_DOMAIN);
	auth_form_focus_previous(&form);
	assert(form.focus == AUTH_FOCUS_CONFIRM);
	printf("PASS test_sign_up_focus_order\n");
}

static void	test_utf8_editing_and_masking(void)
{
	t_auth_form	form;
	char		masked[AUTH_FIELD_MAX];

	auth_form_init(&form, AUTH_FORM_LOGIN);
	form.focus = AUTH_FOCUS_PASSWORD;
	assert(auth_form_handle_key(&form, 'A') == AUTH_ACTION_NONE);
	assert(auth_form_handle_key(&form, 0x754cu) == AUTH_ACTION_NONE);
	assert(strcmp(form.password, "A界") == 0);
	assert(auth_form_mask_password(form.password, masked, sizeof(masked)));
	assert(strcmp(masked, "**") == 0);
	(void)auth_form_handle_key(&form, NCKEY_BACKSPACE);
	assert(strcmp(form.password, "A") == 0);
	(void)auth_form_handle_key(&form, NCKEY_DEL);
	assert(form.password[0] == '\0');
	assert(auth_form_handle_key(&form, NCKEY_DEL) == AUTH_ACTION_NONE);
	assert(form.password[0] == '\0');
	printf("PASS test_utf8_editing_and_masking\n");
}

static void	test_server_check_state(void)
{
	t_auth_form	form;

	auth_form_init(&form, AUTH_FORM_LOGIN);
	assert(form.server_state == AUTH_SERVER_UNVERIFIED);
	assert(!auth_form_online_enabled(&form));
	form.focus = AUTH_FOCUS_DOMAIN;
	snprintf(form.domain, sizeof(form.domain), "play.example.com");
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_CHECK_SERVER);
	assert(form.server_state == AUTH_SERVER_CHECKING);
	assert(form.feedback == AUTH_FEEDBACK_LOADING);
	auth_form_finish_server_check(&form, false);
	assert(form.server_state == AUTH_SERVER_OFFLINE);
	assert(strstr(form.status, "OFFLINE") != NULL);
	form.focus = AUTH_FOCUS_PRIMARY;
	assert(auth_form_handle_key(&form, NCKEY_ENTER) == AUTH_ACTION_NONE);
	assert(strstr(form.status, "OFFLINE") != NULL);
	form.focus = AUTH_FOCUS_DOMAIN;
	(void)auth_form_handle_key(&form, 'x');
	assert(form.server_state == AUTH_SERVER_UNVERIFIED);
	assert(auth_form_begin_server_check(&form));
	auth_form_finish_server_check(&form, true);
	assert(auth_form_online_enabled(&form));
	assert(strstr(form.status, "READY") != NULL);
	printf("PASS test_server_check_state\n");
}

/*
 * A refused LOGIN used to repaint the status it already showed, so pressing
 * Enter on a screen that cannot sign in looked like a hung client. Each
 * refusal must name its own blocker.
 */
static void	test_refused_primary_explains_itself(void)
{
	t_auth_form	form;

	auth_form_init(&form, AUTH_FORM_LOGIN);
	snprintf(form.username, sizeof(form.username), "PixelPlayer");
	snprintf(form.password, sizeof(form.password), "cute-pass");
	form.focus = AUTH_FOCUS_PRIMARY;
	assert(auth_form_handle_key(&form, NCKEY_ENTER) == AUTH_ACTION_NONE);
	assert(form.feedback == AUTH_FEEDBACK_ERROR);
	assert(strstr(form.status, "CHECK SERVER") != NULL);
	form.focus = AUTH_FOCUS_DOMAIN;
	snprintf(form.domain, sizeof(form.domain), "play.example.com");
	assert(auth_form_begin_server_check(&form));
	auth_form_finish_server_check(&form, false);
	form.focus = AUTH_FOCUS_PRIMARY;
	assert(auth_form_handle_key(&form, NCKEY_ENTER) == AUTH_ACTION_NONE);
	assert(form.feedback == AUTH_FEEDBACK_ERROR);
	assert(strstr(form.status, "PLAY OFFLINE") != NULL);
	form.focus = AUTH_FOCUS_OFFLINE;
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_PLAY_OFFLINE);
	printf("PASS test_refused_primary_explains_itself\n");
}

static void	test_validation_and_submission(void)
{
	t_app_data_provider		provider;
	t_app_auth_view_model	view;
	t_auth_form				form;

	auth_form_init(&form, AUTH_FORM_SIGN_UP);
	snprintf(form.username, sizeof(form.username), "PixelPlayer");
	snprintf(form.password, sizeof(form.password), "cute-pass");
	snprintf(form.confirm, sizeof(form.confirm), "different");
	snprintf(form.domain, sizeof(form.domain), "play.example.com");
	assert(auth_form_begin_server_check(&form));
	auth_form_finish_server_check(&form, true);
	assert(!auth_form_validate(&form));
	assert(form.feedback == AUTH_FEEDBACK_ERROR);
	assert(strstr(form.status, "DO NOT MATCH") != NULL);
	snprintf(form.confirm, sizeof(form.confirm), "%s", form.password);
	snprintf(form.domain, sizeof(form.domain), "play　example.com");
	assert(!auth_form_validate(&form));
	assert(strstr(form.status, "WITHOUT SPACES") != NULL);
	snprintf(form.domain, sizeof(form.domain), "play.example.com");
	assert(auth_form_validate(&form));
	assert(form.feedback == AUTH_FEEDBACK_LOADING);
	app_fixture_provider_init(&provider);
	memset(&view, 0, sizeof(view));
	assert(auth_form_submit(&form, &provider, &view) == APP_PROVIDER_OK);
	assert(form.feedback == AUTH_FEEDBACK_SUCCESS);
	assert(view.signed_in);
	printf("PASS test_validation_and_submission\n");
}

/*
** The server refuses a name it could not put back on the wire, so the form
** refuses it first. This is about where the player finds out: submitted, the
** same name comes back as a 400 with the field already behind them.
**
** The multi-byte space is checked because the composer accepts it as text -
** it is the same character the domain field already had to refuse.
*/
static void	test_a_username_the_server_would_refuse_is_caught_here(void)
{
	t_auth_form	form;
	char		toolong[NET_USER_MAX + 8];

	auth_form_init(&form, AUTH_FORM_LOGIN);
	snprintf(form.password, sizeof(form.password), "cute-pass");
	snprintf(form.domain, sizeof(form.domain), "play.example.com");
	assert(auth_form_begin_server_check(&form));
	auth_form_finish_server_check(&form, true);
	snprintf(form.username, sizeof(form.username), "amber lee");
	assert(!auth_form_validate(&form));
	assert(strstr(form.status, "WITHOUT SPACES") != NULL);
	snprintf(form.username, sizeof(form.username), "amber　lee");
	assert(!auth_form_validate(&form));
	memset(toolong, 'x', sizeof(toolong) - 1);
	toolong[sizeof(toolong) - 1] = '\0';
	snprintf(form.username, sizeof(form.username), "%s", toolong);
	assert(!auth_form_validate(&form));
	// Punctuation is still a name; only what the wire cannot carry is out.
	snprintf(form.username, sizeof(form.username), "DarK-Sanjan");
	assert(auth_form_validate(&form));
	printf("PASS test_a_username_the_server_would_refuse_is_caught_here\n");
}

static void	test_secondary_actions(void)
{
	t_auth_form	form;

	auth_form_init(&form, AUTH_FORM_LOGIN);
	form.focus = AUTH_FOCUS_SECONDARY;
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_OPEN_SIGN_UP);
	form.focus = AUTH_FOCUS_OFFLINE;
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_PLAY_OFFLINE);
	auth_form_set_mode(&form, AUTH_FORM_SIGN_UP);
	form.focus = AUTH_FOCUS_SECONDARY;
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_OPEN_LOGIN);
	assert(auth_form_handle_key(&form, NCKEY_ESC)
		== AUTH_ACTION_OPEN_LOGIN);
	printf("PASS test_secondary_actions\n");
}
