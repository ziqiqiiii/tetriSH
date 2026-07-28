#include "tetrisbrain.h"

#include <assert.h>
#include <stdio.h>

void	test_charge_init_zeroes_state(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	assert(s.charges == 0);
	assert(s.line_remainder == 0);
	printf("PASS test_charge_init_zeroes_state\n");
}

void	test_two_lines_grant_one_charge(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 2);
	assert(s.charges == 1);
	assert(s.line_remainder == 0);
	printf("PASS test_two_lines_grant_one_charge\n");
}

void	test_single_lines_bank_remainder_across_clears(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 1);
	assert(s.charges == 0);
	assert(s.line_remainder == 1);
	charge_on_clear(&s, 1); // two separate singles = one charge
	assert(s.charges == 1);
	assert(s.line_remainder == 0);
	printf("PASS test_single_lines_bank_remainder_across_clears\n");
}

void	test_tetris_grants_two_charges(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 4);
	assert(s.charges == 2);
	assert(s.line_remainder == 0);
	printf("PASS test_tetris_grants_two_charges\n");
}

void	test_odd_clears_carry_remainder(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 3); // 1 charge, 1 line banked
	assert(s.charges == 1);
	assert(s.line_remainder == 1);
	charge_on_clear(&s, 3); // banked line + 3 = 2 more charges
	assert(s.charges == 3);
	assert(s.line_remainder == 0);
	printf("PASS test_odd_clears_carry_remainder\n");
}

void	test_zero_or_negative_lines_are_noop(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 1);
	charge_on_clear(&s, 0);
	charge_on_clear(&s, -4);
	assert(s.charges == 0);
	assert(s.line_remainder == 1);
	printf("PASS test_zero_or_negative_lines_are_noop\n");
}

void	test_ability_cost_table(void)
{
	assert(ability_cost(1) == 2);
	assert(ability_cost(2) == 4);
	assert(ability_cost(3) == 6);
	assert(ability_cost(4) == 8);
	printf("PASS test_ability_cost_table\n");
}

void	test_ability_cost_invalid_level_is_negative(void)
{
	assert(ability_cost(0) == -1);
	assert(ability_cost(5) == -1);
	assert(ability_cost(-3) == -1);
	printf("PASS test_ability_cost_invalid_level_is_negative\n");
}

void	test_can_afford_exact_boundary(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 6); // 3 charges
	assert(!charge_can_afford(&s, 2)); // costs 4
	charge_on_clear(&s, 2); // 4 charges
	assert(charge_can_afford(&s, 2)); // exactly 4
	printf("PASS test_can_afford_exact_boundary\n");
}

void	test_can_afford_invalid_level_is_false(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 40); // plenty of charge
	assert(!charge_can_afford(&s, 0));
	assert(!charge_can_afford(&s, 5));
	printf("PASS test_can_afford_invalid_level_is_false\n");
}

void	test_deduct_consumes_cost_and_keeps_rest(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 16); // 8 charges
	assert(charge_deduct(&s, 2)); // costs 4
	assert(s.charges == 4);
	assert(charge_deduct(&s, 1)); // costs 2
	assert(s.charges == 2);
	printf("PASS test_deduct_consumes_cost_and_keeps_rest\n");
}

// UC-14 ext 4a: a rejected activation must not consume any charge.
void	test_deduct_insufficient_consumes_nothing(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 7); // 3 charges, 1 banked line
	assert(!charge_deduct(&s, 2)); // costs 4
	assert(s.charges == 3);
	assert(s.line_remainder == 1);
	printf("PASS test_deduct_insufficient_consumes_nothing\n");
}

void	test_deduct_invalid_level_fails(void)
{
	t_charge_state	s;

	charge_state_init(&s);
	charge_on_clear(&s, 40);
	assert(!charge_deduct(&s, 5));
	assert(s.charges == 20);
	printf("PASS test_deduct_invalid_level_fails\n");
}

void	test_transfer_moves_whole_charges_and_zeroes_source(void)
{
	t_charge_state	victim;
	t_charge_state	caster;

	charge_state_init(&victim);
	charge_state_init(&caster);
	charge_on_clear(&victim, 6); // 3 charges
	charge_on_clear(&caster, 2); // 1 charge
	charge_transfer(&victim, &caster);
	assert(victim.charges == 0);
	assert(caster.charges == 4);
	printf("PASS test_transfer_moves_whole_charges_and_zeroes_source\n");
}

void	test_transfer_leaves_line_remainders_in_place(void)
{
	t_charge_state	victim;
	t_charge_state	caster;

	charge_state_init(&victim);
	charge_state_init(&caster);
	charge_on_clear(&victim, 3); // 1 charge, 1 banked line
	charge_on_clear(&caster, 1); // 1 banked line
	charge_transfer(&victim, &caster);
	assert(victim.line_remainder == 1); // partial lines are not "stored" charge
	assert(caster.line_remainder == 1);
	assert(caster.charges == 1);
	printf("PASS test_transfer_leaves_line_remainders_in_place\n");
}

int	main(void)
{
	test_charge_init_zeroes_state();
	test_two_lines_grant_one_charge();
	test_single_lines_bank_remainder_across_clears();
	test_tetris_grants_two_charges();
	test_odd_clears_carry_remainder();
	test_zero_or_negative_lines_are_noop();
	test_ability_cost_table();
	test_ability_cost_invalid_level_is_negative();
	test_can_afford_exact_boundary();
	test_can_afford_invalid_level_is_false();
	test_deduct_consumes_cost_and_keeps_rest();
	test_deduct_insufficient_consumes_nothing();
	test_deduct_invalid_level_fails();
	test_transfer_moves_whole_charges_and_zeroes_source();
	test_transfer_leaves_line_remainders_in_place();
	return (0);
}
