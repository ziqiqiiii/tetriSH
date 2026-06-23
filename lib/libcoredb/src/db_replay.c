#include "coredb_internal.h"

#include <stdlib.h>
#include <string.h>

static int	coredb_replay_apply_user_create(t_coredb *db,
		const t_coredb_ev_user_create *ev)
{
	t_coredb_user	user;

	if (coredb_hash_find_by_player_id(db->table, ev->player_id) != NULL)
		return (EXIT_FAILURE);
	memset(&user, 0, sizeof(user));
	user.player_id = ev->player_id;
	memcpy(user.username, ev->username, sizeof(user.username));
	memcpy(user.password_salt, ev->password_salt,
		sizeof(user.password_salt));
	memcpy(user.password_hash, ev->password_hash,
		sizeof(user.password_hash));
	user.password_iters = ev->password_iters;
	if (coredb_hash_insert(db->table, &user, NULL) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (ev->player_id + 1 > db->next_player_id)
		db->next_player_id = ev->player_id + 1;
	return (EXIT_SUCCESS);
}

static int	coredb_replay_apply_points_delta(t_coredb *db,
		const t_coredb_ev_points_delta *ev)
{
	t_coredb_user	*user;

	user = coredb_hash_find_by_player_id(db->table, ev->player_id);
	if (user == NULL)
		return (EXIT_FAILURE);
	user->points = (uint32_t)((int64_t)user->points + ev->points_delta);
	return (EXIT_SUCCESS);
}

static int	coredb_replay_apply_grant_item(t_coredb *db,
		const t_coredb_ev_grant_item *ev, int is_theme)
{
	t_coredb_user	*user;

	if (ev->item_id >= 64)
		return (EXIT_FAILURE);
	user = coredb_hash_find_by_player_id(db->table, ev->player_id);
	if (user == NULL)
		return (EXIT_FAILURE);
	if (is_theme)
		user->owned_themes |= (1ULL << ev->item_id);
	else
		user->owned_characters |= (1ULL << ev->item_id);
	return (EXIT_SUCCESS);
}

static int	coredb_replay_apply_equip_theme(t_coredb *db,
		const t_coredb_ev_equip_item *ev)
{
	t_coredb_user	*user;

	user = coredb_hash_find_by_player_id(db->table, ev->player_id);
	if (user == NULL)
		return (EXIT_FAILURE);
	user->equipped_theme = ev->item_id;
	return (EXIT_SUCCESS);
}

static int	coredb_replay_apply_high_score(t_coredb *db,
		const t_coredb_ev_high_score *ev)
{
	t_coredb_user	*user;

	user = coredb_hash_find_by_player_id(db->table, ev->player_id);
	if (user == NULL)
		return (EXIT_FAILURE);
	user->high_score = ev->high_score;
	return (EXIT_SUCCESS);
}

/* Dispatches one decoded event onto db->table. Any event_type this switch
 * does not recognise (e.g. COREDB_EV_PASSWORD_SET, reserved for a future
 * password-reset feature with no payload defined yet) is treated as
 * corrupt/unhandled, same as a bad CRC. */
static int	coredb_replay_apply(t_coredb *db,
		const t_coredb_record_header *hdr,
		const t_coredb_event_payload *payload)
{
	if (hdr->event_type == COREDB_EV_USER_CREATE)
		return (coredb_replay_apply_user_create(db, &payload->user_create));
	if (hdr->event_type == COREDB_EV_POINTS_DELTA)
		return (coredb_replay_apply_points_delta(db,
				&payload->points_delta));
	if (hdr->event_type == COREDB_EV_GRANT_CHARACTER)
		return (coredb_replay_apply_grant_item(db, &payload->grant_item, 0));
	if (hdr->event_type == COREDB_EV_GRANT_THEME)
		return (coredb_replay_apply_grant_item(db, &payload->grant_item, 1));
	if (hdr->event_type == COREDB_EV_EQUIP_THEME)
		return (coredb_replay_apply_equip_theme(db, &payload->equip_item));
	if (hdr->event_type == COREDB_EV_SET_HIGH_SCORE)
		return (coredb_replay_apply_high_score(db, &payload->high_score));
	return (EXIT_FAILURE);
}

t_coredb_replay_status	coredb_replay(t_coredb *db)
{
	t_coredb_record_header	hdr;
	t_coredb_event_payload	payload;
	t_coredb_io_status		io_status;

	if (db == NULL || db->file == NULL || db->table == NULL)
		return (COREDB_REPLAY_CORRUPT_TAIL);
	if (fseek(db->file, 0, SEEK_SET) != 0)
		return (COREDB_REPLAY_CORRUPT_TAIL);
	while (1)
	{
		io_status = coredb_storage_read_record(db->file, &hdr, &payload,
				sizeof(payload));
		if (io_status == COREDB_IO_EOF)
			break ;
		if (io_status != COREDB_IO_OK)
			return (COREDB_REPLAY_CORRUPT_TAIL);
		if (coredb_record_payload_check(&hdr, &payload) != EXIT_SUCCESS)
			return (COREDB_REPLAY_CORRUPT_TAIL);
		if (coredb_replay_apply(db, &hdr, &payload) != EXIT_SUCCESS)
			return (COREDB_REPLAY_CORRUPT_TAIL);
		db->next_seq_no = hdr.seq_no + 1;
	}
	if (fseek(db->file, 0, SEEK_END) != 0)
		return (COREDB_REPLAY_CORRUPT_TAIL);
	return (COREDB_REPLAY_OK);
}
