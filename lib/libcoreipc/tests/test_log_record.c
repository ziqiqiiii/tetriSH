#include "coreipc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// The log record is the one wire format shared by tetrisd's ring buffer,
// the shipper's datagrams, and tetrislogd's parser - so these tests pin
// down the byte layout, the validation gate, and the disk-line rendering.

void	test_make_fills_fields_and_terminates(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 1700000000123ULL, 42,
			"tetrisd", "hello") == 0);
	assert(rec.magic == CIPC_LOG_MAGIC);
	assert(rec.version == CIPC_LOG_VERSION);
	assert(rec.level == CIPC_LOG_INFO);
	assert(rec.timestamp_ms == 1700000000123ULL);
	assert(rec.pid == 42);
	assert(strcmp(rec.component, "tetrisd") == 0);
	assert(strcmp(rec.msg, "hello") == 0);
	assert(rec.msg_len == 5);
	printf("PASS test_make_fills_fields_and_terminates\n");
}

void	test_make_truncates_long_component_and_msg(void)
{
	t_log_record	rec;
	char			long_msg[CIPC_LOG_MSG_MAX + 64];

	memset(long_msg, 'x', sizeof(long_msg) - 1);
	long_msg[sizeof(long_msg) - 1] = '\0';
	assert(lr_make(&rec, CIPC_LOG_DEBUG, 0, 1,
			"a-component-name-way-too-long", long_msg) == 0);
	assert(rec.component[CIPC_LOG_COMPONENT_MAX - 1] == '\0');
	assert(strlen(rec.component) == CIPC_LOG_COMPONENT_MAX - 1);
	assert(rec.msg_len == CIPC_LOG_MSG_MAX - 1);
	assert(rec.msg[CIPC_LOG_MSG_MAX - 1] == '\0');
	printf("PASS test_make_truncates_long_component_and_msg\n");
}

void	test_make_rejects_null_and_bad_level(void)
{
	t_log_record	rec;

	assert(lr_make(NULL, CIPC_LOG_INFO, 0, 1, "c", "m") == -1);
	assert(errno == EINVAL);
	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "c", NULL) == -1);
	assert(errno == EINVAL);
	assert(lr_make(&rec, (t_log_level)99, 0, 1, "c", "m") == -1);
	assert(errno == EINVAL);
	printf("PASS test_make_rejects_null_and_bad_level\n");
}

void	test_make_null_component_means_empty(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, NULL, "m") == 0);
	assert(rec.component[0] == '\0');
	printf("PASS test_make_null_component_means_empty\n");
}

void	test_identical_inputs_are_byte_identical(void)
{
	t_log_record	a;
	t_log_record	b;

	// padding must be zeroed, or dgrams leak stale stack bytes
	memset(&a, 0xAA, sizeof(a));
	memset(&b, 0x55, sizeof(b));
	assert(lr_make(&a, CIPC_LOG_ERROR, 7, 9, "ctl", "boom") == 0);
	assert(lr_make(&b, CIPC_LOG_ERROR, 7, 9, "ctl", "boom") == 0);
	assert(memcmp(&a, &b, sizeof(a)) == 0);
	printf("PASS test_identical_inputs_are_byte_identical\n");
}

void	test_validate_accepts_made_record(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_WARNING, 5, 2, "logd", "warn") == 0);
	assert(lr_validate(&rec, sizeof(rec)) == 0);
	printf("PASS test_validate_accepts_made_record\n");
}

void	test_validate_rejects_wrong_length(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "d", "m") == 0);
	assert(lr_validate(&rec, sizeof(rec) - 1) == -1);
	assert(errno == EBADMSG);
	assert(lr_validate(NULL, sizeof(rec)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_validate_rejects_wrong_length\n");
}

void	test_validate_rejects_bad_magic_and_version(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "d", "m") == 0);
	rec.magic ^= 1;
	assert(lr_validate(&rec, sizeof(rec)) == -1);
	assert(errno == EBADMSG);
	rec.magic = CIPC_LOG_MAGIC;
	rec.version = CIPC_LOG_VERSION + 1;
	assert(lr_validate(&rec, sizeof(rec)) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_validate_rejects_bad_magic_and_version\n");
}

void	test_validate_rejects_bad_msg_framing(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "d", "msg") == 0);
	rec.msg_len = CIPC_LOG_MSG_MAX;
	assert(lr_validate(&rec, sizeof(rec)) == -1);
	assert(errno == EBADMSG);
	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "d", "msg") == 0);
	rec.msg[rec.msg_len] = 'x';
	assert(lr_validate(&rec, sizeof(rec)) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_validate_rejects_bad_msg_framing\n");
}

void	test_validate_rejects_unterminated_component(void)
{
	t_log_record	rec;

	assert(lr_make(&rec, CIPC_LOG_INFO, 0, 1, "d", "m") == 0);
	memset(rec.component, 'c', CIPC_LOG_COMPONENT_MAX);
	assert(lr_validate(&rec, sizeof(rec)) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_validate_rejects_unterminated_component\n");
}

void	test_level_names_and_parse_round_trip(void)
{
	assert(strcmp(lr_level_name(CIPC_LOG_DEBUG), "DEBUG") == 0);
	assert(strcmp(lr_level_name(CIPC_LOG_ERROR), "ERROR") == 0);
	assert(strcmp(lr_level_name((t_log_level)99), "UNKNOWN") == 0);
	assert(lr_level_parse("debug") == CIPC_LOG_DEBUG);
	assert(lr_level_parse("INFO") == CIPC_LOG_INFO);
	assert(lr_level_parse("Warning") == CIPC_LOG_WARNING);
	assert(lr_level_parse("error") == CIPC_LOG_ERROR);
	assert(lr_level_parse("verbose") == -1);
	assert(errno == EINVAL);
	assert(lr_level_parse(NULL) == -1);
	printf("PASS test_level_names_and_parse_round_trip\n");
}

void	test_format_line_renders_expected_layout(void)
{
	t_log_record	rec;
	char			line[512];
	int				n;

	assert(lr_make(&rec, CIPC_LOG_INFO, 1700000000123ULL, 42,
			"tetrisd", "room R-01 created") == 0);
	n = lr_format_line(&rec, line, sizeof(line));
	assert(n > 0 && n == (int)strlen(line));
	assert(strcmp(line,
			"1700000000123 INFO    tetrisd[42]: room R-01 created\n") == 0);
	printf("PASS test_format_line_renders_expected_layout\n");
}

void	test_format_line_rejects_small_buffer(void)
{
	t_log_record	rec;
	char			line[8];

	assert(lr_make(&rec, CIPC_LOG_INFO, 1, 2, "tetrisd", "a msg") == 0);
	assert(lr_format_line(&rec, line, sizeof(line)) == -1);
	assert(errno == ERANGE);
	assert(lr_format_line(NULL, line, sizeof(line)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_format_line_rejects_small_buffer\n");
}

int	main(void)
{
	test_make_fills_fields_and_terminates();
	test_make_truncates_long_component_and_msg();
	test_make_rejects_null_and_bad_level();
	test_make_null_component_means_empty();
	test_identical_inputs_are_byte_identical();
	test_validate_accepts_made_record();
	test_validate_rejects_wrong_length();
	test_validate_rejects_bad_magic_and_version();
	test_validate_rejects_bad_msg_framing();
	test_validate_rejects_unterminated_component();
	test_level_names_and_parse_round_trip();
	test_format_line_renders_expected_layout();
	test_format_line_rejects_small_buffer();
	return (0);
}
