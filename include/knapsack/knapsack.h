#ifndef KNAPSACK_KNAPSACK_H
#define KNAPSACK_KNAPSACK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Represents a single item with weight and value. Weight must be non-negative. */
typedef struct {
  int weight;
  int value;
} knapsack_item_t;

/** Result of the knapsack solver. */
typedef struct {
  int optimal_value;
  size_t selected_count;
  size_t *selected_indices; /* Caller must free via knapsack_result_free. */
} knapsack_result_t;

/**
 * Solve the 0/1 knapsack problem.
 *
 * @param items Array of items (weights and values). Must be non-NULL and count > 0.
 * @param count Number of items in the array.
 * @param capacity Maximum weight capacity (must be >= 0). Very large capacities that
 *        would overflow internal buffers are rejected.
 * @param out_result Output struct written on success. Caller must call
 *        knapsack_result_free to release internal allocations. On failure, out_result
 *        is left zeroed.
 * Note: parameter order (items, count, capacity, out_result) is intentional and
 * kept stable for ABI/API compatibility.
 * Complexity: O(count * capacity) time, O(count * capacity) space.
 * @return true on success; false if inputs are invalid, allocation fails, or
 *         intermediate sums would overflow int.
 */
bool knapsack_solve(const knapsack_item_t *items, size_t count, int capacity,
                    knapsack_result_t *out_result);

/** Release memory held by a knapsack_result_t. Safe to call on empty structs. */
void knapsack_result_free(knapsack_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* KNAPSACK_KNAPSACK_H */