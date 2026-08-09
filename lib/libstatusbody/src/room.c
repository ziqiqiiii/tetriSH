#include "body_util.h"

// Static Variables
static const char	*g_modes[] = {
	"single", "double", "battle-royale"
};
static const char	*g_statuses[] = {
	"waiting", "ready", "in-game", "finished"
};

// Static Functions
static bool	room_valid(const t_body_room *room);
static int	encode_members(const t_body_room *room, char *out, size_t cap,
				size_t *offset);
static int	decode_header(t_body_cursor *cursor, t_body_room *room);
static int	decode_members(t_body_cursor *cursor, t_body_room *room);
static int	read_value(t_body_cursor *cursor, const char *key, char *value,
				size_t cap);
static int	decode_member(const char *line, t_body_room_member *member,
				int previous_slot, int slot_count);

/**
 * @brief Serialises one authoritative waiting-room snapshot.
 *
 * @param in Room and occupied seats to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length, or -1 with errno set to EINVAL or ERANGE.
 */
int	body_room_encode(const t_body_room *in, char *out, size_t cap)
{
	size_t	offset;

	if (!in || !out || !room_valid(in))
		return (body_fail(EINVAL));
	offset = 0;
	if (body_append(out, cap, &offset, "room %s\n", in->name) != 0
		|| body_append(out, cap, &offset, "mode %s\n", g_modes[in->mode]) != 0
		|| body_append(out, cap, &offset, "status %s\n",
			g_statuses[in->status]) != 0
		|| body_append(out, cap, &offset, "required %d\n",
			in->min_to_start) != 0
		|| body_append(out, cap, &offset, "capacity %d\n", in->slot_count) != 0
		|| body_append(out, cap, &offset, "members %zu\n",
			in->member_count) != 0
		|| encode_members(in, out, cap, &offset) != 0)
		return (body_fail(ERANGE));
	return ((int)offset);
}

/**
 * @brief Parses one detailed waiting-room body.
 *
 * @param buf Received body bytes, which need not be NUL-terminated.
 * @param len Number of body bytes.
 * @param out Receives the fully overwritten room snapshot.
 * @return 0, or -1 with errno set to EINVAL or EBADMSG.
 */
int	body_room_decode(const char *buf, size_t len, t_body_room *out)
{
	t_body_cursor	cursor;

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	cursor.p = buf;
	cursor.end = buf + len;
	if (decode_header(&cursor, out) != 0
		|| decode_members(&cursor, out) != 0
		|| !body_at_end(&cursor) || !room_valid(out))
		return (body_fail(EBADMSG));
	return (0);
}

/**
 * @brief Checks the complete room snapshot before it reaches the wire.
 *
 * @param room Room snapshot to validate.
 * @return true when every field and member is representable.
 */
static bool	room_valid(const t_body_room *room)
{
	size_t	index;
	int		previous;

	if (room->name[0] == '\0' || strchr(room->name, ' ') != NULL
		|| room->mode < BODY_MODE_SINGLE || room->mode > BODY_MODE_BATTLE_ROYALE
		|| room->status < BODY_ROOM_WAITING || room->status > BODY_ROOM_FINISHED
		|| room->slot_count < 1 || room->slot_count > BODY_ROOM_MEMBERS_MAX
		|| room->min_to_start < 1 || room->min_to_start > room->slot_count
		|| room->member_count > (size_t)room->slot_count)
		return (false);
	index = 0;
	previous = 0;
	while (index < room->member_count)
	{
		if (room->members[index].slot <= previous
			|| room->members[index].slot > room->slot_count
			|| room->members[index].player_id == 0
			|| room->members[index].username[0] == '\0'
			|| strchr(room->members[index].username, ' ') != NULL)
			return (false);
		previous = room->members[index].slot;
		index++;
	}
	return (true);
}

/**
 * @brief Appends every occupied seat in stable slot order.
 *
 * @param room Room carrying the member rows.
 * @param out Body output buffer.
 * @param cap Size of out.
 * @param offset In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_members(const t_body_room *room, char *out, size_t cap,
	size_t *offset)
{
	const t_body_room_member	*member;
	size_t					index;

	index = 0;
	while (index < room->member_count)
	{
		member = &room->members[index];
		if (body_append(out, cap, offset, "slot %d %" PRIu64 " %s %s %s\n",
				member->slot, member->player_id,
				member->owner ? "owner" : "player",
				member->ready ? "ready" : "waiting",
				member->username) != 0)
			return (-1);
		index++;
	}
	return (0);
}

/**
 * @brief Reads the fixed room header in schema order.
 *
 * @param cursor Bounded body cursor.
 * @param room Room being decoded.
 * @return 0 on success, -1 on a malformed header.
 */
static int	decode_header(t_body_cursor *cursor, t_body_room *room)
{
	char	value[BODY_LINE_MAX];
	int		index;
	int		members;

	if (read_value(cursor, "room", room->name, sizeof(room->name)) != 0
		|| read_value(cursor, "mode", value, sizeof(value)) != 0)
		return (-1);
	index = body_word_index(value, g_modes, 3);
	if (index < 0)
		return (-1);
	room->mode = (t_body_mode)index;
	if (read_value(cursor, "status", value, sizeof(value)) != 0)
		return (-1);
	index = body_word_index(value, g_statuses, 4);
	if (index < 0)
		return (-1);
	room->status = (t_body_room_status)index;
	if (read_value(cursor, "required", value, sizeof(value)) != 0
		|| body_parse_int(value, &room->min_to_start, 1,
			BODY_ROOM_MEMBERS_MAX) != 0
		|| read_value(cursor, "capacity", value, sizeof(value)) != 0
		|| body_parse_int(value, &room->slot_count, 1,
			BODY_ROOM_MEMBERS_MAX) != 0
		|| read_value(cursor, "members", value, sizeof(value)) != 0
		|| body_parse_int(value, &members, 0, BODY_ROOM_MEMBERS_MAX) != 0)
		return (-1);
	room->member_count = (size_t)members;
	return (0);
}

/**
 * @brief Reads the declared occupied-seat rows.
 *
 * @param cursor Bounded body cursor.
 * @param room Room being decoded.
 * @return 0 on success, -1 on a malformed or out-of-order row.
 */
static int	decode_members(t_body_cursor *cursor, t_body_room *room)
{
	char	line[BODY_LINE_MAX];
	size_t	index;
	int		previous;

	index = 0;
	previous = 0;
	while (index < room->member_count)
	{
		if (body_take_line(cursor, line, sizeof(line)) != 0
			|| decode_member(line, &room->members[index], previous,
				room->slot_count) != 0)
			return (-1);
		previous = room->members[index].slot;
		index++;
	}
	return (0);
}

/**
 * @brief Reads one named value line.
 *
 * @param cursor Bounded body cursor.
 * @param key Expected key.
 * @param value Destination for the value.
 * @param cap Size of value.
 * @return 0 on success, -1 on a missing or malformed line.
 */
static int	read_value(t_body_cursor *cursor, const char *key, char *value,
	size_t cap)
{
	char	line[BODY_LINE_MAX];
	size_t	key_len;
	size_t	value_len;

	if (body_take_line(cursor, line, sizeof(line)) != 0)
		return (-1);
	key_len = strlen(key);
	if (strncmp(line, key, key_len) != 0 || line[key_len] != ' ')
		return (-1);
	value_len = strlen(line + key_len + 1);
	if (value_len == 0 || value_len >= cap)
		return (-1);
	memcpy(value, line + key_len + 1, value_len + 1);
	return (0);
}

/**
 * @brief Parses one occupied-seat row.
 *
 * @param line NUL-terminated row without its newline.
 * @param member Destination member.
 * @param previous_slot Previous row's slot, enforcing stable order.
 * @param slot_count Room capacity.
 * @return 0 on success, -1 on malformed or out-of-range input.
 */
static int	decode_member(const char *line, t_body_room_member *member,
	int previous_slot, int slot_count)
{
	unsigned long long	player_id;
	char				role[16];
	char				status[16];
	int					used;

	memset(member, 0, sizeof(*member));
	used = 0;
	if (sscanf(line, "slot %d %llu %15s %15s %31s%n", &member->slot,
			&player_id, role, status, member->username, &used) != 5
		|| line[used] != '\0' || member->slot <= previous_slot
		|| member->slot > slot_count || player_id == 0
		|| (strcmp(role, "owner") != 0 && strcmp(role, "player") != 0)
		|| (strcmp(status, "ready") != 0
			&& strcmp(status, "waiting") != 0))
		return (-1);
	member->player_id = (uint64_t)player_id;
	member->owner = strcmp(role, "owner") == 0;
	member->ready = strcmp(status, "ready") == 0;
	return (0);
}
