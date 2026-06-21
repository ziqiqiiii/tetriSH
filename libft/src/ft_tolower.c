/*
**	The ft_tolower() takes in the ASCII value of uppercase alphabetical character
**	and converts it to lowercase. If not uppercase alphabet, it returns c.
*/
int	ft_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return (c + 32);
	return (c);
}
