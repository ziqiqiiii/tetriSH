/*
**	The ft_tolower() takes in the ASCII value of lowercase alphabetical character
**	and converts it to upper case. If not lowercase alphabet, it returns c.
*/
int	ft_toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		return (c - 32);
	return (c);
}
