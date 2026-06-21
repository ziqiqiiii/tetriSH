#include "common.h"

/**
 * @brief Opens a file stream, exiting on failure.
 *
 * Calls fopen(3) with the given mode. Terminates the process with an error
 * message if the stream cannot be opened.
 *
 * @param file Path to the file to open.
 * @param mode fopen mode string (e.g. "r", "w", "a").
 * @return The open FILE stream, or exits on failure.
 */
FILE	*ft_fopen(const char *file, const char *mode)
{
	FILE	*stream;

	stream = fopen(file, mode);
	if (!stream)
	{
		printf("fopen: %d\n", getpid());
		ft_putstr_fd("Error: Failed to open file\n", 2);
		exit(EXIT_FAILURE);
	}
	return (stream);
}
