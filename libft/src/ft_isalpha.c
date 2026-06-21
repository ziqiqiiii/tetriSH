/*
**	The ft_isalpha() function tests for an alphabet.
**
**	The ft_isalpha() function returns zero if the character tests false and 
**	returns non-zero if the character tests true.
*/
int	ft_isalpha(int c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}
