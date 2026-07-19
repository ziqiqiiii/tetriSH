#include "libft.h"

/**
 * @brief Appends src to dst string, ensuring null-termination.
 *
 * Concatenates src to dst string, but not more than dstsize - 1 characters,
 * making sure the result is null-terminated. If there's no room in dst for
 * src, dst will not be changed. 
 *
 * @param dst Destination string.
 * @param src Source string.
 * @param dstsize Size of the destination buffer.
 * @return The total length of the string if there was no size limit. 
 */
size_t	ft_strlcat(char *dst, const char *src, size_t dstsize)
{
	size_t	slen;
	size_t	dlen;

	slen = ft_strlen(src);
	dlen = ft_strlen(dst);
	if (dstsize < (dlen + 1))
		return (slen + dstsize);
	dstsize -= dlen + 1;
	dst += dlen;
	if (dstsize >= slen)
		dstsize = slen;
	ft_memcpy(dst, src, dstsize);
	dst[dstsize] = '\0';
	return (slen + dlen);
}
