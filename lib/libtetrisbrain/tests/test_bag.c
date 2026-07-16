#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

static void assert_one_of_each(t_piece_bag *bag) {
  bool seen[BRAIN_BAG_SIZE] = {false};

  for (int i = 0; i < BRAIN_BAG_SIZE; i++) {
    t_piece_type type = piece_bag_next(bag);
    assert(type >= PIECE_I && type <= PIECE_L);
    assert(!seen[type]);
    seen[type] = true;
  }
}

void test_each_bag_contains_all_seven_types(void) {
  t_piece_bag bag;

  piece_bag_init(&bag, 12345u);
  assert_one_of_each(&bag);
  assert_one_of_each(&bag);
  printf("PASS test_each_bag_contains_all_seven_types\n");
}

void test_same_seed_repeats_sequence(void) {
  t_piece_bag first;
  t_piece_bag second;

  piece_bag_init(&first, 9876u);
  piece_bag_init(&second, 9876u);
  for (int i = 0; i < 28; i++)
    assert(piece_bag_next(&first) == piece_bag_next(&second));
  printf("PASS test_same_seed_repeats_sequence\n");
}

int main(void) {
  test_each_bag_contains_all_seven_types();
  test_same_seed_repeats_sequence();
  return 0;
}
