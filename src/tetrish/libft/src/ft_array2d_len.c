#include "libft.h"

/**
 * @brief Counts the elements of a NULL-terminated array of strings.
 *
 * @param str Pointer to the first element of a NULL-terminated string array.
 * @return The number of strings in the array.
 */
int	ft_array2d_len(char **str)
{
	int	i;

	i = 0;
	while (str[i])
		i++;
	return (i);
}
