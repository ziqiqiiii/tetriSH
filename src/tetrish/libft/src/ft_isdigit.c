/**
 * @brief Checks if a given character is a digit.
 *
 * This function checks whether the provided ASCII character, 
 * represented as an integer, is a digit (0-9).
 *
 * @param c The ASCII value of the character.
 * @return 1 if the character is a digit, 0 otherwise.
 */
int	ft_isdigit(int c)
{
	return (c >= '0' && c <= '9');
}
