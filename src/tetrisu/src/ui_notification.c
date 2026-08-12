#include "tetrisu.h"

static int	clamp_int(int value, int min, int max);
static bool	show_notification(t_ui_notification_stack *stack,
					t_ui_notification_kind kind, const char *title,
					const char *message, int percent, uint64_t now_ms);
static int	find_title(const t_ui_notification_stack *stack,
				const char *title);
static void	move_to_front(t_ui_notification_stack *stack, int index);
static bool	notification_expired(const t_ui_notification *notification,
				uint64_t now_ms);
static int	clamp_delay(uint64_t delay_ms);

/**
 * @brief Clears a reusable notification stack.
 *
 * @param stack Stack to initialize.
 */
void	ui_notification_stack_init(t_ui_notification_stack *stack)
{
	if (stack != NULL)
		memset(stack, 0, sizeof(*stack));
}

/**
 * @brief Adds or refreshes one percentage notification.
 *
 * Repeated events with the same title replace the existing item and return it
 * to the top of the stack. A full stack drops its oldest entry.
 *
 * @param stack Destination stack.
 * @param title Short display title.
 * @param percent Percentage value, clamped to 0..100.
 * @param now_ms Current monotonic timestamp.
 */
bool	ui_notification_show(t_ui_notification_stack *stack,
	const char *title, int percent, uint64_t now_ms)
{
	return (show_notification(stack, UI_NOTIFICATION_VOLUME, title, "",
			percent, now_ms));
}

/**
 * @brief Adds or refreshes the fixed ownership-error notification.
 *
 * Keeping this copy in the model makes bitmap and compatibility renderers
 * present the same Marketplace guidance without duplicating strings.
 */
bool	ui_notification_show_ownership(t_ui_notification_stack *stack,
	uint64_t now_ms)
{
	return (show_notification(stack, UI_NOTIFICATION_OWNERSHIP,
		UI_NOTIFICATION_OWNERSHIP_TITLE, UI_NOTIFICATION_OWNERSHIP_MESSAGE,
		0, now_ms));
}

/**
 * @brief Adds or refreshes one incoming-ability notification.
 *
 * @param stack Destination stack.
 * @param title What landed.
 * @param message What it does, in the player's terms.
 * @param now_ms Current monotonic timestamp.
 * @return true when the stack's content changed.
 */
bool	ui_notification_show_effect(t_ui_notification_stack *stack,
	const char *title, const char *message, uint64_t now_ms)
{
	return (show_notification(stack, UI_NOTIFICATION_EFFECT, title, message,
			0, now_ms));
}

static bool	show_notification(t_ui_notification_stack *stack,
	t_ui_notification_kind kind, const char *title, const char *message,
	int percent, uint64_t now_ms)
{
	int		index;
	int		insert;
	int		clamped_percent;
	const char	*safe_message;

	if (stack == NULL || title == NULL || title[0] == '\0')
		return (false);
	safe_message = message != NULL ? message : "";
	clamped_percent = clamp_int(percent, 0, 100);
	index = find_title(stack, title);
	if (index == 0 && stack->items[0].kind == kind
		&& stack->items[0].percent == clamped_percent
		&& strncmp(stack->items[0].message, safe_message,
			UI_NOTIFICATION_MESSAGE_MAX) == 0)
	{
		stack->items[0].shown_at_ms = now_ms;
		return (false);
	}
	if (index >= 0)
		move_to_front(stack, index);
	else
	{
		insert = stack->count;
		if (insert >= UI_NOTIFICATION_STACK_MAX)
			insert = UI_NOTIFICATION_STACK_MAX - 1;
		while (insert > 0)
		{
			stack->items[insert] = stack->items[insert - 1];
			insert--;
		}
		if (stack->count < UI_NOTIFICATION_STACK_MAX)
			stack->count++;
	}
	memset(&stack->items[0], 0, sizeof(stack->items[0]));
	stack->items[0].kind = kind;
	snprintf(stack->items[0].title, sizeof(stack->items[0].title),
		"%.*s", UI_NOTIFICATION_TITLE_MAX, title);
	snprintf(stack->items[0].message, sizeof(stack->items[0].message),
		"%.*s", UI_NOTIFICATION_MESSAGE_MAX, safe_message);
	stack->items[0].percent = clamped_percent;
	stack->items[0].shown_at_ms = now_ms;
	return (true);
}

/**
 * @brief Removes every notification whose hold and fade have completed.
 *
 * @param stack Stack to update.
 * @param now_ms Current monotonic timestamp.
 * @return true when one or more entries were removed.
 */
bool	ui_notification_update(t_ui_notification_stack *stack,
	uint64_t now_ms)
{
	int		read;
	int		write;
	int		new_count;
	bool	changed;

	if (stack == NULL)
		return (false);
	read = 0;
	write = 0;
	changed = false;
	while (read < stack->count)
	{
		if (!notification_expired(&stack->items[read], now_ms))
		{
			if (write != read)
				stack->items[write] = stack->items[read];
			write++;
		}
		else
			changed = true;
		read++;
	}
	new_count = write;
	while (write < stack->count)
	{
		memset(&stack->items[write], 0, sizeof(stack->items[write]));
		write++;
	}
	if (changed)
		stack->count = new_count;
	return (changed);
}

/**
 * @brief Returns the current fade opacity for one notification.
 *
 * @param notification Notification to inspect.
 * @param now_ms Current monotonic timestamp.
 * @return Opacity from 255 while held to 0 after the fade.
 */
int	ui_notification_opacity(const t_ui_notification *notification,
	uint64_t now_ms)
{
	uint64_t	elapsed;
	uint64_t	fade_elapsed;

	if (notification == NULL || now_ms < notification->shown_at_ms)
		return (255);
	elapsed = now_ms - notification->shown_at_ms;
	if (elapsed <= UI_NOTIFICATION_HOLD_MS)
		return (255);
	fade_elapsed = elapsed - UI_NOTIFICATION_HOLD_MS;
	if (fade_elapsed >= UI_NOTIFICATION_FADE_MS)
		return (0);
	return ((int)(((UI_NOTIFICATION_FADE_MS - fade_elapsed) * 255u)
			/ UI_NOTIFICATION_FADE_MS));
}

/**
 * @brief Returns the next time the stack needs a presentation update.
 *
 * @param stack Stack to inspect.
 * @param now_ms Current monotonic timestamp.
 * @return Delay in milliseconds, or -1 when the stack is empty.
 */
int	ui_notification_next_wake_ms(const t_ui_notification_stack *stack,
	uint64_t now_ms)
{
	uint64_t	elapsed;
	uint64_t	delay;
	int			index;
	int			next;

	if (stack == NULL || stack->count == 0)
		return (-1);
	next = -1;
	index = 0;
	while (index < stack->count)
	{
		if (now_ms < stack->items[index].shown_at_ms)
			delay = UI_NOTIFICATION_HOLD_MS;
		else
		{
			elapsed = now_ms - stack->items[index].shown_at_ms;
			if (elapsed < UI_NOTIFICATION_HOLD_MS)
				delay = UI_NOTIFICATION_HOLD_MS - elapsed;
			else if (elapsed < UI_NOTIFICATION_HOLD_MS
				+ UI_NOTIFICATION_FADE_MS)
			{
				delay = UI_NOTIFICATION_HOLD_MS
					+ UI_NOTIFICATION_FADE_MS - elapsed;
				if (delay > UI_NOTIFICATION_FRAME_MS)
					delay = UI_NOTIFICATION_FRAME_MS;
			}
			else
				delay = 0;
		}
		if (next < 0 || clamp_delay(delay) < next)
			next = clamp_delay(delay);
		index++;
	}
	return (next);
}

/**
 * @brief Returns the delay until the next notification fully expires.
 *
 * Stationary bitmap protocols cannot alpha-blend fade frames. They keep the
 * authored card opaque and sleep until its model lifetime ends, avoiding
 * repeated Sixel plane replacement while preserving the same total duration.
 *
 * @param stack Stack to inspect.
 * @param now_ms Current monotonic timestamp.
 * @return Delay in milliseconds, or -1 when the stack is empty.
 */
int	ui_notification_next_expiry_ms(const t_ui_notification_stack *stack,
	uint64_t now_ms)
{
	uint64_t	delay;
	uint64_t	elapsed;
	int			index;
	int			next;

	if (stack == NULL || stack->count == 0)
		return (-1);
	next = -1;
	index = 0;
	while (index < stack->count)
	{
		if (now_ms < stack->items[index].shown_at_ms)
			delay = stack->items[index].shown_at_ms - now_ms
				+ UI_NOTIFICATION_HOLD_MS + UI_NOTIFICATION_FADE_MS;
		else
		{
			elapsed = now_ms - stack->items[index].shown_at_ms;
			if (elapsed >= UI_NOTIFICATION_HOLD_MS + UI_NOTIFICATION_FADE_MS)
				delay = 0;
			else
				delay = UI_NOTIFICATION_HOLD_MS + UI_NOTIFICATION_FADE_MS
					- elapsed;
		}
		if (next < 0 || clamp_delay(delay) < next)
			next = clamp_delay(delay);
		index++;
	}
	return (next);
}

/**
 * @brief Converts the mixer volume range to a rounded percentage.
 *
 * @param volume Mixer volume.
 * @return Percentage clamped to 0..100.
 */
int	ui_notification_volume_percent(int volume)
{
	volume = clamp_int(volume, 0, AUDIO_MAX_VOLUME);
	return ((volume * 100 + AUDIO_MAX_VOLUME / 2) / AUDIO_MAX_VOLUME);
}

/**
 * @brief Reads the monotonic clock for notification deadlines.
 *
 * @return Milliseconds from the platform monotonic epoch.
 */
uint64_t	ui_notification_now_ms(void)
{
	struct timespec	now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t)now.tv_sec * 1000u
		+ (uint64_t)now.tv_nsec / 1000000u);
}

static int	clamp_int(int value, int min, int max)
{
	if (value < min)
		return (min);
	if (value > max)
		return (max);
	return (value);
}

static int	find_title(const t_ui_notification_stack *stack,
	const char *title)
{
	int	index;

	index = 0;
	while (index < stack->count)
	{
		if (strncmp(stack->items[index].title, title,
				UI_NOTIFICATION_TITLE_MAX) == 0)
			return (index);
		index++;
	}
	return (-1);
}

static void	move_to_front(t_ui_notification_stack *stack, int index)
{
	t_ui_notification	item;

	item = stack->items[index];
	while (index > 0)
	{
		stack->items[index] = stack->items[index - 1];
		index--;
	}
	stack->items[0] = item;
}

static bool	notification_expired(const t_ui_notification *notification,
	uint64_t now_ms)
{
	if (now_ms < notification->shown_at_ms)
		return (false);
	return (now_ms - notification->shown_at_ms
		>= UI_NOTIFICATION_HOLD_MS + UI_NOTIFICATION_FADE_MS);
}

static int	clamp_delay(uint64_t delay_ms)
{
	if (delay_ms > INT32_MAX)
		return (INT32_MAX);
	return ((int)delay_ms);
}
