#include "libft.h"

/**
 * @brief Lowercases a string in place.
 *
 * @param str Address of the string to modify.
 */
void	ft_str_to_lower(char **str)
{
	int	i;

	i = 0;
	while ((*str)[i])
	{
		if ((*str)[i] >= 'A' && (*str)[i] <= 'Z')
			(*str)[i] += 32;
		i++;
	}
}
