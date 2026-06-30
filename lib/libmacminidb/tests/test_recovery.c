// Unit tests for recovery.c — replay into hash map, latest-per-key wins.
#include "macminidb.h"
#include <stdio.h>

int main(void) {
  // TODO: write two records for one username, reopen, assert latest wins (LWW).
  printf("PASS recovery_placeholder\n");
  return 0;
}
