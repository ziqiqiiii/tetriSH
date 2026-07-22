#include "htttp.h"

t_htttp_result	htttp_dispatch(const t_htttp_message *message,
		const t_htttp_route *routes, size_t route_count,
		void *context, int *handler_result)
{
	size_t	i;

	if (handler_result == NULL)
		return (HTTTP_ERR_INVALID_ARGUMENT);
	*handler_result = 0;
	if (message == NULL || (route_count > 0u && routes == NULL))
		return (HTTTP_ERR_INVALID_ARGUMENT);
	if (message->type != HTTTP_MESSAGE_REQUEST
		|| message->method == NULL || message->method[0] == '\0')
		return (HTTTP_ERR_INVALID_MESSAGE);
	/* AI-assisted: validate the whole caller-owned table before dispatch so a
	 * configuration error cannot be hidden by an earlier matching route. */
	i = 0u;
	while (i < route_count)
	{
		if (routes[i].method == NULL || routes[i].method[0] == '\0'
			|| routes[i].handler == NULL)
			return (HTTTP_ERR_INVALID_ARGUMENT);
		i++;
	}
	i = 0u;
	while (i < route_count)
	{
		if (strcmp(message->method, routes[i].method) == 0)
		{
			*handler_result = routes[i].handler(message, context);
			return (HTTTP_OK);
		}
		i++;
	}
	return (HTTTP_ERR_NO_HANDLER);
}
