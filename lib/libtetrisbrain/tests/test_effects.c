#include "tetrisbrain.h"

#include <assert.h>
#include <stdio.h>

static void	lock_pieces(t_effect_state *s, int n)
{
	int	i;

	i = 0;
	while (i < n)
	{
		effect_on_piece_lock(s);
		i++;
	}
}

void	test_effect_init_all_inactive(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	assert(!effect_rotation_blocked(&s));
	assert(!effect_fastdrop_blocked(&s));
	assert(!effect_controls_inverted(&s));
	assert(!effect_thwack_active(&s));
	assert(effect_fry_rows(&s) == 0);
	assert(!s.blackout);
	assert(!s.pals);
	assert(!s.mirror_armed);

	printf("PASS test_effect_init_all_inactive\n");
}

void	test_paralysis_blocks_rotation_for_three_locks(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_PARALYSIS);

	lock_pieces(&s, 2);
	assert(effect_rotation_blocked(&s)); // third piece still paralysed
	effect_on_piece_lock(&s);
	assert(!effect_rotation_blocked(&s)); // expires after exactly 3

	printf("PASS test_paralysis_blocks_rotation_for_three_locks\n");
}

void	test_inversion_inverts_controls_for_three_locks(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_INVERSION);

	lock_pieces(&s, 2);
	assert(effect_controls_inverted(&s));
	effect_on_piece_lock(&s);
	assert(!effect_controls_inverted(&s));

	printf("PASS test_inversion_inverts_controls_for_three_locks\n");
}

void	test_nue_blocks_fastdrop_for_four_locks(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_NUE);

	lock_pieces(&s, 3);
	assert(effect_fastdrop_blocked(&s));
	effect_on_piece_lock(&s);
	assert(!effect_fastdrop_blocked(&s));

	printf("PASS test_nue_blocks_fastdrop_for_four_locks\n");
}

void	test_thwack_active_for_four_locks(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_THWACK);

	lock_pieces(&s, 3);
	assert(effect_thwack_active(&s));
	effect_on_piece_lock(&s);
	assert(!effect_thwack_active(&s));

	printf("PASS test_thwack_active_for_four_locks\n");
}

void	test_fry_rows_pending_until_next_lock(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_FRY);
	assert(effect_fry_rows(&s) == 3); // read before the lock to forward garbage

	effect_on_piece_lock(&s);
	assert(effect_fry_rows(&s) == 0); // one lock consumes the burn

	printf("PASS test_fry_rows_pending_until_next_lock\n");
}

void	test_dark_sets_blackout_until_cleared(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_DARK);
	assert(s.blackout);

	lock_pieces(&s, 10); // locks never end it - the server does
	assert(s.blackout);

	effect_clear(&s, EFFECT_DARK);
	assert(!s.blackout);

	printf("PASS test_dark_sets_blackout_until_cleared\n");
}

void	test_pals_flag_set_and_cleared(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_PALS);
	assert(s.pals);

	lock_pieces(&s, 10); // Pals is timed, not piece-counted
	assert(s.pals);

	effect_clear(&s, EFFECT_PALS);
	assert(!s.pals);

	printf("PASS test_pals_flag_set_and_cleared\n");
}

void	test_mirror_armed_set_and_cleared(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_MIRROR);
	assert(s.mirror_armed);

	effect_clear(&s, EFFECT_MIRROR); // server clears it when the steal fires
	assert(!s.mirror_armed);

	printf("PASS test_mirror_armed_set_and_cleared\n");
}

void	test_lock_with_nothing_active_is_noop(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	lock_pieces(&s, 5); // counters must not go negative

	assert(s.no_rotate_pieces == 0);
	assert(s.inverted_pieces == 0);
	assert(s.no_fastdrop_pieces == 0);
	assert(s.thwack_pieces == 0);
	assert(s.fry_rows == 0);

	printf("PASS test_lock_with_nothing_active_is_noop\n");
}

void	test_effects_run_down_independently(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_PARALYSIS); // 3 pieces
	effect_apply(&s, EFFECT_NUE); // 4 pieces

	lock_pieces(&s, 3);
	assert(!effect_rotation_blocked(&s)); // paralysis is done
	assert(effect_fastdrop_blocked(&s)); // nue has one piece left

	effect_on_piece_lock(&s);
	assert(!effect_fastdrop_blocked(&s));

	printf("PASS test_effects_run_down_independently\n");
}

void	test_reapply_refreshes_counter(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_PARALYSIS);
	lock_pieces(&s, 2); // one piece left

	effect_apply(&s, EFFECT_PARALYSIS); // fresh cast resets to 3
	lock_pieces(&s, 2);
	assert(effect_rotation_blocked(&s));
	effect_on_piece_lock(&s);
	assert(!effect_rotation_blocked(&s));

	printf("PASS test_reapply_refreshes_counter\n");
}

void	test_clear_ends_a_counter_effect_early(void)
{
	t_effect_state	s;

	effect_state_init(&s);
	effect_apply(&s, EFFECT_INVERSION);
	assert(effect_controls_inverted(&s));

	effect_clear(&s, EFFECT_INVERSION); // e.g. game ends mid-effect
	assert(!effect_controls_inverted(&s));

	printf("PASS test_clear_ends_a_counter_effect_early\n");
}

int	main(void)
{
	test_effect_init_all_inactive();

	test_paralysis_blocks_rotation_for_three_locks();
	test_inversion_inverts_controls_for_three_locks();
	test_nue_blocks_fastdrop_for_four_locks();
	test_thwack_active_for_four_locks();
	test_fry_rows_pending_until_next_lock();

	test_dark_sets_blackout_until_cleared();
	test_pals_flag_set_and_cleared();
	test_mirror_armed_set_and_cleared();

	test_lock_with_nothing_active_is_noop();
	test_effects_run_down_independently();
	test_reapply_refreshes_counter();
	test_clear_ends_a_counter_effect_early();

	return (0);
}
