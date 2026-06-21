/*
**	The ft_atoi() function converts the initial portion of the string pointed to 
**	by str to int representation.
*/

int	ft_atoi(const char *str)
{
	int	res;
	int	sign;

	res = 0;
	sign = 1;
	while ((*str == ' ') || (*str >= 9 && *str <= 13))
		str++;
	if ((*str == '-') || (*str == '+'))
	{
		if (*str++ == '-')
			sign *= -1;
	}
	while (*str >= '0' && *str <= '9')
		res = (res * 10) + (*str++) - '0';
	return (res * sign);
}
