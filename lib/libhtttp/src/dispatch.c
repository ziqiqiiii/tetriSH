#include "htttp.h"

t_htttp_result	htttp_dispatch(const t_htttp_message *message,
		const t_htttp_route *routes, size_t route_count,
		void *context, int *handler_result)
{
	t_htttp_result	result;
	size_t			i;

	if (handler_result == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	*handler_result = 0;
	if (message == NULL || (route_count > 0u && routes == NULL))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	/* Validate the whole caller-owned table before any message check so a
	 * configuration error is never masked by a malformed request or hidden
	 * behind an earlier matching route. */
	i = 0u;
	while (i < route_count)
	{
		if (routes[i].method == NULL || routes[i].method[0] == '\0'
			|| routes[i].handler == NULL
			|| (routes[i].validation_flags
				& ~HTTTP_VALIDATE_AUTHENTICATED_REQUEST) != 0u)
			return (HTTTP_ERR_INVALID_ARGUMENT);
		i++;
	}
	if (message->type != HTTTP_MESSAGE_REQUEST)
		return (HTTTP_ERR_INVALID_MESSAGE);
	result = htttp_validate(message, 0u);
	if (result != HTTTP_OK)
		return (result);
	i = 0u;
	while (i < route_count)
	{
		if (strcmp(message->method, routes[i].method) == 0)
		{
			/* Route policy enforces required identity metadata before application
			 * code runs; daemon still verifies it against connection-bound state. */
			result = htttp_validate(message, routes[i].validation_flags);
			if (result != HTTTP_OK)
				return (result);
			*handler_result = routes[i].handler(message, context);
			return (HTTTP_OK);
		}
		i++;
	}
	return (HTTTP_ERR_NO_HANDLER);
}
