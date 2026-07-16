#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

void test_normal_clear_table_uses_displayed_level(void) {
  assert(score_on_clear(1, 1) == 100);
  assert(score_on_clear(2, 1) == 300);
  assert(score_on_clear(3, 2) == 1000);
  assert(score_on_clear(4, 3) == 2400);
  assert(score_on_clear(9, 1) == 800);
  assert(score_on_clear(-1, 1) == 0);
  printf("PASS test_normal_clear_table_uses_displayed_level\n");
}

void test_level_advances_every_ten_lines_from_level_one(void) {
  assert(level_from_lines(-1) == 1);
  assert(level_from_lines(0) == 1);
  assert(level_from_lines(9) == 1);
  assert(level_from_lines(10) == 2);
  assert(level_from_lines(29) == 3);
  printf("PASS test_level_advances_every_ten_lines_from_level_one\n");
}

void test_guideline_gravity_reaches_twenty_g(void) {
  assert(gravity_interval_ms(1) == 1000);
  assert(gravity_interval_ms(2) == 793);
  assert(gravity_interval_ms(18) == 1);
  assert(gravity_interval_ms(19) == 0);
  assert(gravity_interval_ms(100) == 0);
  printf("PASS test_guideline_gravity_reaches_twenty_g\n");
}

void test_combo_and_back_to_back_scoring(void) {
  t_score_state state;
  t_score_result first;
  t_score_result second;

  score_state_init(&state);
  first = score_apply_clear(&state, 4, 1, T_SPIN_NONE, false);
  assert(first.action_points == 800 && first.combo_points == 0);
  assert(state.back_to_back && state.combo == 0);
  second = score_apply_clear(&state, 4, 1, T_SPIN_NONE, false);
  assert(second.action_points == 1200);
  assert(second.combo_points == 50);
  assert(state.total == 2050 && state.combo == 1);
  printf("PASS test_combo_and_back_to_back_scoring\n");
}

void test_t_spin_perfect_clear_and_drop_points(void) {
  t_score_state state;
  t_score_result result;

  score_state_init(&state);
  result = score_apply_clear(&state, 2, 2, T_SPIN_FULL, true);
  assert(result.action_points == 2400);
  assert(result.perfect_clear_points == 2400);
  assert(result.total_awarded == 4800);
  assert(score_add_drop(&state, 3, false) == 3);
  assert(score_add_drop(&state, 4, true) == 8);
  assert(state.total == 4811);
  printf("PASS test_t_spin_perfect_clear_and_drop_points\n");
}

void test_no_line_resets_combo_but_preserves_back_to_back(void) {
  t_score_state state;

  score_state_init(&state);
  (void)score_apply_clear(&state, 4, 1, T_SPIN_NONE, false);
  (void)score_apply_clear(&state, 0, 1, T_SPIN_FULL, false);
  assert(state.combo == -1);
  assert(state.back_to_back);
  printf("PASS test_no_line_resets_combo_but_preserves_back_to_back\n");
}

int main(void) {
  test_normal_clear_table_uses_displayed_level();
  test_level_advances_every_ten_lines_from_level_one();
  test_guideline_gravity_reaches_twenty_g();
  test_combo_and_back_to_back_scoring();
  test_t_spin_perfect_clear_and_drop_points();
  test_no_line_resets_combo_but_preserves_back_to_back();
  return 0;
}
