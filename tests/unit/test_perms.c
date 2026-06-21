/*
 * tests/unit/test_perms.c
 *
 * Unit tests for perms_to_string(), implemented in
 * src/common/perms.c and declared in includes/common.h.
 *
 * Run with:
 *   make unit
 */
#include "unity.h"
#include "minishell.h"
#include "common.h"

int	g_exit_status = 0;

void setUp(void) {}
void tearDown(void) {}

/* --- base type prefix --- */

static void test_no_mode_gives_all_dashes(void)
{
	char buf[16];
	perms_to_string(0, buf);
	TEST_ASSERT_EQUAL_STRING("----------", buf);
}

static void test_directory_prefix(void)
{
	char buf[16];
	perms_to_string(S_IFDIR, buf);
	TEST_ASSERT_EQUAL_STRING("d---------", buf);
}

static void test_char_device_prefix(void)
{
	char buf[16];
	perms_to_string(S_IFCHR, buf);
	TEST_ASSERT_EQUAL_STRING("c---------", buf);
}

static void test_block_device_prefix(void)
{
	char buf[16];
	perms_to_string(S_IFBLK, buf);
	TEST_ASSERT_EQUAL_STRING("b---------", buf);
}

static void test_regular_file_has_dash_prefix(void)
{
	char buf[16];
	perms_to_string(S_IFREG, buf);
	TEST_ASSERT_EQUAL_STRING("----------", buf);
}

/* --- owner permissions --- */

static void test_owner_read_only(void)
{
	char buf[16];
	perms_to_string(S_IRUSR, buf);
	TEST_ASSERT_EQUAL_STRING("-r--------", buf);
}

static void test_owner_write_only(void)
{
	char buf[16];
	perms_to_string(S_IWUSR, buf);
	TEST_ASSERT_EQUAL_STRING("--w-------", buf);
}

static void test_owner_execute_only(void)
{
	char buf[16];
	perms_to_string(S_IXUSR, buf);
	TEST_ASSERT_EQUAL_STRING("---x------", buf);
}

static void test_owner_all(void)
{
	char buf[16];
	perms_to_string(S_IRUSR | S_IWUSR | S_IXUSR, buf);
	TEST_ASSERT_EQUAL_STRING("-rwx------", buf);
}

/* --- group permissions --- */

static void test_group_read_only(void)
{
	char buf[16];
	perms_to_string(S_IRGRP, buf);
	TEST_ASSERT_EQUAL_STRING("----r-----", buf);
}

static void test_group_write_only(void)
{
	char buf[16];
	perms_to_string(S_IWGRP, buf);
	TEST_ASSERT_EQUAL_STRING("-----w----", buf);
}

static void test_group_execute_only(void)
{
	char buf[16];
	perms_to_string(S_IXGRP, buf);
	TEST_ASSERT_EQUAL_STRING("------x---", buf);
}

static void test_group_all(void)
{
	char buf[16];
	perms_to_string(S_IRGRP | S_IWGRP | S_IXGRP, buf);
	TEST_ASSERT_EQUAL_STRING("----rwx---", buf);
}

/* --- other permissions --- */

static void test_other_read_only(void)
{
	char buf[16];
	perms_to_string(S_IROTH, buf);
	TEST_ASSERT_EQUAL_STRING("-------r--", buf);
}

static void test_other_write_only(void)
{
	char buf[16];
	perms_to_string(S_IWOTH, buf);
	TEST_ASSERT_EQUAL_STRING("--------w-", buf);
}

static void test_other_execute_only(void)
{
	char buf[16];
	perms_to_string(S_IXOTH, buf);
	TEST_ASSERT_EQUAL_STRING("---------x", buf);
}

static void test_other_all(void)
{
	char buf[16];
	perms_to_string(S_IROTH | S_IWOTH | S_IXOTH, buf);
	TEST_ASSERT_EQUAL_STRING("-------rwx", buf);
}

/* --- common composite modes --- */

static void test_regular_file_0644(void)
{
	char buf[16];
	perms_to_string(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, buf);
	TEST_ASSERT_EQUAL_STRING("-rw-r--r--", buf);
}

static void test_regular_file_0755(void)
{
	char buf[16];
	mode_t m = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("-rwxr-xr-x", buf);
}

static void test_directory_0755(void)
{
	char buf[16];
	mode_t m = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("drwxr-xr-x", buf);
}

static void test_all_permissions_0777(void)
{
	char buf[16];
	mode_t m = S_IRUSR | S_IWUSR | S_IXUSR
		| S_IRGRP | S_IWGRP | S_IXGRP
		| S_IROTH | S_IWOTH | S_IXOTH;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("-rwxrwxrwx", buf);
}

static void test_directory_0700(void)
{
	char buf[16];
	mode_t m = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("drwx------", buf);
}

static void test_regular_file_0600(void)
{
	char buf[16];
	perms_to_string(S_IRUSR | S_IWUSR, buf);
	TEST_ASSERT_EQUAL_STRING("-rw-------", buf);
}

static void test_regular_file_0444(void)
{
	char buf[16];
	perms_to_string(S_IRUSR | S_IRGRP | S_IROTH, buf);
	TEST_ASSERT_EQUAL_STRING("-r--r--r--", buf);
}

static void test_regular_file_0111(void)
{
	char buf[16];
	perms_to_string(S_IXUSR | S_IXGRP | S_IXOTH, buf);
	TEST_ASSERT_EQUAL_STRING("---x--x--x", buf);
}

static void test_char_device_with_rw(void)
{
	char buf[16];
	mode_t m = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("crw-rw----", buf);
}

static void test_block_device_with_perms(void)
{
	char buf[16];
	mode_t m = S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP;
	perms_to_string(m, buf);
	TEST_ASSERT_EQUAL_STRING("brw-r-----", buf);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_no_mode_gives_all_dashes);
	RUN_TEST(test_directory_prefix);
	RUN_TEST(test_char_device_prefix);
	RUN_TEST(test_block_device_prefix);
	RUN_TEST(test_regular_file_has_dash_prefix);
	RUN_TEST(test_owner_read_only);
	RUN_TEST(test_owner_write_only);
	RUN_TEST(test_owner_execute_only);
	RUN_TEST(test_owner_all);
	RUN_TEST(test_group_read_only);
	RUN_TEST(test_group_write_only);
	RUN_TEST(test_group_execute_only);
	RUN_TEST(test_group_all);
	RUN_TEST(test_other_read_only);
	RUN_TEST(test_other_write_only);
	RUN_TEST(test_other_execute_only);
	RUN_TEST(test_other_all);
	RUN_TEST(test_regular_file_0644);
	RUN_TEST(test_regular_file_0755);
	RUN_TEST(test_directory_0755);
	RUN_TEST(test_all_permissions_0777);
	RUN_TEST(test_directory_0700);
	RUN_TEST(test_regular_file_0600);
	RUN_TEST(test_regular_file_0444);
	RUN_TEST(test_regular_file_0111);
	RUN_TEST(test_char_device_with_rw);
	RUN_TEST(test_block_device_with_perms);
	return UNITY_END();
}
