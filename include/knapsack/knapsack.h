#ifndef KNAPSACK_KNAPSACK_H
#define KNAPSACK_KNAPSACK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Represents a single item with weight and value. Weight must be positive; value
 *  must be non-negative.
 */
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

typedef enum { // NOLINT(performance-enum-size)
  KNAPSACK_OK = 0,
  KNAPSACK_ERR_NULL_RESULT,
  KNAPSACK_ERR_INVALID_ITEMS,      /* null pointer, zero items, zero/negative weight, or negative value */
  KNAPSACK_ERR_TOO_MANY_ITEMS,     /* exceeds compiled bound (100) */
  KNAPSACK_ERR_INVALID_CAPACITY,   /* negative or exceeds compiled bound (100000) */
  KNAPSACK_ERR_DIMENSION_OVERFLOW, /* would overflow internal buffers */
  KNAPSACK_ERR_INT_OVERFLOW,       /* value accumulation overflow */
  KNAPSACK_ERR_ALLOC               /* allocation failed */
} knapsack_status_t;

/**
 * Solve the 0/1 knapsack problem.
 *
 * @param items Array of items (weights and values). Must be non-NULL and count > 0.
 *        Each weight must be > 0, each value >= 0. Negative values and zero weights
 *        are rejected.
 * @param count Number of items in the array (must be <= 100).
 * @param capacity Maximum weight capacity (must be >= 0 and <= 100000). Very large
 *        capacities that would overflow internal buffers are rejected.
 * @param out_result Output struct written on success. Caller must call
 *        knapsack_result_free to release internal allocations. On failure, out_result
 *        is left zeroed.
 * Note: parameter order (items, count, capacity, out_result) is intentional and
 * kept stable for ABI/API compatibility.
 * Tie-break: among optimal values, the solution with the smallest total weight is
 * returned. Selected indices are reported in ascending order.
 * Complexity: O(count * capacity) time, O(count * capacity) extra space (bounded by
 * count<=100 and capacity<=100000).
 * @return true on success; false if inputs are invalid, allocation fails, or
 *         intermediate sums would overflow int.
 */
bool knapsack_solve(const knapsack_item_t *items, size_t count, int capacity,
                    knapsack_result_t *out_result);

/**
 * Status-returning variant of knapsack_solve. See knapsack_solve for semantics.
 * On success, writes the result to out_result and returns KNAPSACK_OK; otherwise
 * returns a specific error code. On error, out_result is left zeroed.
 */
knapsack_status_t knapsack_solve_status(const knapsack_item_t *items, size_t count, int capacity,
                                        knapsack_result_t *out_result);

/** Release memory held by a knapsack_result_t. Safe to call on empty structs. */
void knapsack_result_free(knapsack_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* KNAPSACK_KNAPSACK_H */