/* libFuzzer entry point: drives cli_parse_buffer with arbitrary bytes. */
#include "cli_internal.h"
#include "knapsack/knapsack.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int capacity = 0;
  knapsack_item_t *items = NULL;
  size_t count = 0;
  if (cli_parse_buffer((const char *)data, size, &capacity, &items, &count) == 0) {
    knapsack_result_t result;
    if (knapsack_solve_status(items, count, capacity, &result) == KNAPSACK_OK) {
      knapsack_result_free(&result);
    }
    free(items);
  }
  return 0;
}
