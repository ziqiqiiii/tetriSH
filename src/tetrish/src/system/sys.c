#include "system_program.h"

// Static Variables
static char *logo[MAX_LOGO_LINES];
static int  logo_count = 0;
static int  max_logo_width = 0;
static char *info[MAX_LINES];
static int  info_count = 0;

// Static Functions
static void	load_logo(void);
static void userinfo(void);
static void resourceinfo(void);
static void systeminfo(void);
static void render();
static int	utf8_display_width(const char *s);
static void display_line(const char *disp, ...);

/**
 * @brief Entry point for the sys utility.
 *
 * Collects user, resource, and system information into the info buffer, then
 * renders the ASCII logo side-by-side with the info lines. Frees all
 * allocated info strings before returning.
 *
 * @param argc Number of command-line arguments (unused).
 * @param argv Array of command-line arguments (unused).
 * @return 0 on success.
 */
int main(int argc, char **argv) {

        (void)  argc;
        (void)  argv;

        load_logo();
        userinfo();
        resourceinfo();
        systeminfo();
        render();

        return 0;
}

/**
 * @brief Count the display width of a UTF-8 string.
 *
 * Counts only leading bytes (ignoring 0x80-prefixed continuation bytes) so
 * multi-byte characters contribute a single display column.
 *
 * @param s NUL-terminated UTF-8 string.
 * @return Number of display columns the string occupies.
 */
static int utf8_display_width(const char *s)
{
	int w = 0;
	while (*s)
	{
		if (((unsigned char)*s & 0xC0) != 0x80)
			w++;
		s++;
	}
	return w;
}

/**
 * @brief Load the ASCII logo from logo.txt next to the executable.
 *
 * Resolves the executable's directory via /proc/self/exe, reads
 * "<dir>/logo.txt" line by line into the global logo array (stripping CR/LF),
 * and tracks the widest line in max_logo_width for later alignment.
 */
static void load_logo(void)
{
	char	line[MAX_LOGO_WIDTH];
	FILE	*f;
	size_t	line_len;
	int	vis_w;
	char	exe[512];
	char	*slash;
	char	*path;
	ssize_t	exe_len;

	exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (exe_len < 0)
		return;
	exe[exe_len] = '\0';
	slash = strrchr(exe, '/');
	if (slash)
		*slash = '\0';
	path = malloc(strlen(exe) + strlen("/logo.txt") + 1);
	if (!path)
		return;
	snprintf(path, strlen(exe) + strlen("/logo.txt") + 1, "%s/logo.txt", exe);
	f = ft_fopen(path, "r");
	free(path);
	while (logo_count < MAX_LOGO_LINES && fgets(line, sizeof(line), f))
	{
		line_len = strlen(line);
		if (line_len > 0 && line[line_len - 1] == '\n')
			line[--line_len] = '\0';
		if (line_len > 0 && line[line_len - 1] == '\r')
			line[--line_len] = '\0';
		vis_w = utf8_display_width(line);
		if (vis_w > max_logo_width)
			max_logo_width = vis_w;
		logo[logo_count++] = strdup(line);
	}

	fclose(f);
}

/**
 * @brief Formats a string and appends it to the info display buffer.
 *
 * Accepts a printf-style format string and variadic arguments. Allocates
 * INFO_WIDTH bytes for the resulting string and stores the pointer in the
 * global info array. No-ops if the buffer is already full (MAX_LINES reached).
 *
 * @param disp printf-style format string.
 * @param ...  Variadic arguments matching the format string.
 */
static void display_line(const char *disp, ...)
{
        if (info_count >= MAX_LINES)
                return;

        va_list args;
        va_start(args, disp);
        char *buff = malloc(INFO_WIDTH);
        vsnprintf(buff, INFO_WIDTH, disp, args);
        va_end(args);

        info[info_count++] = buff;
}

/**
 * @brief Collects and queues current user information for display.
 *
 * Retrieves the passwd entry for the current UID and appends the username,
 * home directory, and UID:GID to the info buffer via display_line().
 */
static void userinfo(void)
{
        // do not pass this pointer to free!
        struct passwd *pw = getpwuid(getuid());
        struct rusage ru;
        
        getrusage(RUSAGE_SELF, &ru);

        display_line("User:               %s", pw->pw_name);
        display_line("Home:               %s", pw->pw_dir);
        display_line("UID:GID:            %d:%d", pw->pw_uid, pw->pw_gid);
}

/**
 * @brief Collects and queues system resource statistics for display.
 *
 * Uses sysinfo(2) to retrieve uptime, free RAM, buffer RAM, shared RAM, and
 * total RAM, then appends each value (in MiB or minutes) to the info buffer.
 */
static void resourceinfo(void)
{
        struct sysinfo si;

        sysinfo(&si);

        display_line("Uptime:             %ld min", si.uptime / 60);
        display_line("Free Ram:           %ld MiB", si.freeram / 1024 / 1024);
        display_line("Buffer Ram:         %ld MiB", si.bufferram / 1024 / 1024);
        display_line("Shared Ram:         %ld MiB", si.sharedram / 1024 / 1024);
        display_line("Total Ram:          %ld MiB", si.totalram / 1024 / 1024);
}

/**
 * @brief Collects and queues OS and kernel information for display.
 *
 * Parses /etc/os-release for the PRETTY_NAME field and calls uname(2) for
 * the kernel name, release, and machine architecture. Appends both to the
 * info buffer via display_line().
 */
static void systeminfo(void)
{
        FILE            *f;
        char            osversion[256];
        struct utsname  un;
        
        f = ft_fopen("/etc/os-release", "r");

        while (fgets(osversion, sizeof(osversion), f)) {
                if (strncmp(osversion, "PRETTY_NAME=", 12) == 0) {
                        char *start = strchr(osversion, '=') + 1;
                        if (*start == '\"')
                                start++; // goto next address
                        char *end = strchr(start, '\"');
                        if (end)
                                *end = '\0';
                        display_line("OS:                 %s", start);
                        break;
                }
        }
        fclose(f);

        uname(&un);
        display_line("Kernel:             %s  %s  (%s)", un.sysname, un.release, un.machine);
}

/**
 * @brief Render the logo and info buffers side by side, then free them.
 *
 * Prints each row with the logo on the left (padded to max_logo_width) and
 * the corresponding info line on the right, then frees all logo and info
 * strings.
 */
static void render()
{
        int lines = (logo_count > info_count) ? logo_count : info_count;

        for (int i = 0; i < lines; ++i) {
                const char *l = (i < logo_count) ? logo[i] : "";
                const char *r = (i < info_count) ? info[i] : "";
                int pad = max_logo_width - utf8_display_width(l);
                printf("%s%*s  %s\n", l, pad, "", r);
        }

        for (int i = 0; i < logo_count; ++i)
                free(logo[i]);
        for (int i = 0; i < info_count; ++i)
                free(info[i]);
}
