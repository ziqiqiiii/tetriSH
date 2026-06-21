// tests/test_scoring.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

// ---- score_on_clear ----

void test_score_on_clear_zero_lines_returns_zero(void) {
  assert(score_on_clear(0, 0) == 0);
  assert(score_on_clear(0, 5) == 0);

  printf("PASS test_score_on_clear_zero_lines_returns_zero\n");
}

void test_score_on_clear_base_values_at_level_zero(void) {
  // NES-style base table: single/double/triple/tetris
  assert(score_on_clear(1, 0) == 40);
  assert(score_on_clear(2, 0) == 100);
  assert(score_on_clear(3, 0) == 300);
  assert(score_on_clear(4, 0) == 1200);

  printf("PASS test_score_on_clear_base_values_at_level_zero\n");
}

void test_score_on_clear_scales_with_level(void) {
  // multiplier is (level + 1)
  assert(score_on_clear(1, 1) == 40 * 2);
  assert(score_on_clear(4, 2) == 1200 * 3);

  printf("PASS test_score_on_clear_scales_with_level\n");
}

void test_score_on_clear_clamps_out_of_range_inputs(void) {
  // negative lines_cleared -> no score
  assert(score_on_clear(-1, 0) == 0);

  // lines_cleared above the 4-line max clamps to the tetris value
  assert(score_on_clear(5, 0) == 1200);

  // negative level treated as level 0 (multiplier 1)
  assert(score_on_clear(1, -1) == 40);

  printf("PASS test_score_on_clear_clamps_out_of_range_inputs\n");
}

// ---- level_from_lines ----

void test_level_from_lines_starts_at_zero(void) {
  assert(level_from_lines(0) == 0);
  assert(level_from_lines(9) == 0);

  printf("PASS test_level_from_lines_starts_at_zero\n");
}

void test_level_from_lines_advances_every_ten_lines(void) {
  assert(level_from_lines(10) == 1);
  assert(level_from_lines(19) == 1);
  assert(level_from_lines(25) == 2);

  printf("PASS test_level_from_lines_advances_every_ten_lines\n");
}

void test_level_from_lines_negative_clamps_to_zero(void) {
  assert(level_from_lines(-5) == 0);

  printf("PASS test_level_from_lines_negative_clamps_to_zero\n");
}

// ---- gravity_interval_ms ----

void test_gravity_interval_ms_level_zero(void) {
  assert(gravity_interval_ms(0) == 1000);

  printf("PASS test_gravity_interval_ms_level_zero\n");
}

void test_gravity_interval_ms_decreases_with_level(void) {
  assert(gravity_interval_ms(1) == 950);
  assert(gravity_interval_ms(5) == 750);

  printf("PASS test_gravity_interval_ms_decreases_with_level\n");
}

void test_gravity_interval_ms_floors_at_minimum(void) {
  // 1000 - 18*50 == 100, the floor
  assert(gravity_interval_ms(18) == 100);
  // any higher level clamps to the same floor, never goes to 0 or negative
  assert(gravity_interval_ms(19) == 100);
  assert(gravity_interval_ms(100) == 100);

  printf("PASS test_gravity_interval_ms_floors_at_minimum\n");
}

void test_gravity_interval_ms_negative_level_clamps_to_level_zero(void) {
  assert(gravity_interval_ms(-3) == 1000);

  printf("PASS test_gravity_interval_ms_negative_level_clamps_to_level_zero\n");
}

int main(void) {
  test_score_on_clear_zero_lines_returns_zero();
  test_score_on_clear_base_values_at_level_zero();
  test_score_on_clear_scales_with_level();
  test_score_on_clear_clamps_out_of_range_inputs();

  test_level_from_lines_starts_at_zero();
  test_level_from_lines_advances_every_ten_lines();
  test_level_from_lines_negative_clamps_to_zero();

  test_gravity_interval_ms_level_zero();
  test_gravity_interval_ms_decreases_with_level();
  test_gravity_interval_ms_floors_at_minimum();
  test_gravity_interval_ms_negative_level_clamps_to_level_zero();

  return 0;
}
