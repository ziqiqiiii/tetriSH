#include "libft.h"

char	*ft_itoa_base(long n, int base, char *base_c)
{
	t_itoa	pt;

	pt.nb = n;
	pt.a = ft_calloc(2, sizeof(char));
	if (pt.nb == 0)
		*(pt.a) = '0';
	if (pt.nb < 0)
		pt.nb *= -1;
	while (pt.nb > 0)
	{
		pt.b = ft_substr(base_c, (pt.nb % base), 1);
		pt.tmp = pt.a;
		pt.a = ft_strjoin(pt.b, pt.a);
		free(pt.b);
		free(pt.tmp);
		pt.nb /= base;
	}
	if (n < 0)
	{
		pt.tmp = pt.a;
		pt.a = ft_strjoin("-", pt.a);
		free(pt.tmp);
	}
	return (pt.a);
}
