#include "common.h"

/**
 * @brief Render a file mode as a 10-character permission string.
 *
 * Produces an ls-style string such as "drwxr-xr-x": index 0 is the file
 * type (d/c/b or '-'), followed by user, group and other rwx triplets.
 *
 * @param mode File mode bits, e.g. from struct stat's st_mode.
 * @param str Output buffer of at least 11 bytes; filled and NUL-terminated.
 */
void	perms_to_string(mode_t mode, char str[11])
{
	strcpy(str, "----------");
	if (S_ISDIR(mode))
		str[0] = 'd';
	if (S_ISCHR(mode))
		str[0] = 'c';
	if (S_ISBLK(mode))
		str[0] = 'b';
	if (mode & S_IRUSR)
		str[1] = 'r';
	if (mode & S_IWUSR)
		str[2] = 'w';
	if (mode & S_IXUSR)
		str[3] = 'x';
	if (mode & S_IRGRP)
		str[4] = 'r';
	if (mode & S_IWGRP)
		str[5] = 'w';
	if (mode & S_IXGRP)
		str[6] = 'x';
	if (mode & S_IROTH)
		str[7] = 'r';
	if (mode & S_IWOTH)
		str[8] = 'w';
	if (mode & S_IXOTH)
		str[9] = 'x';
}
