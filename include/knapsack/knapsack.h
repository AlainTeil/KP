#ifndef KNAPSACK_KNAPSACK_H
#define KNAPSACK_KNAPSACK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file knapsack.h
 *  @brief Public API for the exact 0/1 knapsack solver.
 *
 *  Constraints enforced by the solver:
 *  - Item count: 1 .. KNAPSACK_MAX_ITEMS (100)
 *  - Capacity:   0 .. KNAPSACK_MAX_CAPACITY (100000)
 *  - Each weight must be > 0; each value must be >= 0.
 *  - Tie-break: among solutions with the optimal value, the one with the
 *    smallest total weight is returned. Selected indices are reported in
 *    ascending order.
 *  - Complexity: O(count * (capacity+1)) time and memory.
 */

/** Library version. */
#define KNAPSACK_VERSION_MAJOR 0
#define KNAPSACK_VERSION_MINOR 2
#define KNAPSACK_VERSION_PATCH 0

/** Compile-time bounds (also exposed for header consumers that need them). */
#define KNAPSACK_MAX_ITEMS 100U
#define KNAPSACK_MAX_CAPACITY 100000

/** A single item: positive weight, non-negative value. */
typedef struct {
  int weight;
  int value;
} knapsack_item_t;

/** Solver result. selected_indices is allocated by the solver and must be
 *  released by the caller via knapsack_result_free.
 */
typedef struct {
  int optimal_value;
  size_t selected_count;
  size_t *selected_indices;
} knapsack_result_t;

/** Status / error codes returned by the solver.
 *  @note enum is intentionally int-sized for stable ABI and exhaustive
 *        switch handling; do not change the underlying type.
 */
typedef enum { /* NOLINT(performance-enum-size) -- ABI: keep int-sized. */
               KNAPSACK_OK = 0,
               KNAPSACK_ERR_NULL_RESULT,    /**< out_result was NULL. */
               KNAPSACK_ERR_INVALID_ITEMS,  /**< null pointer, zero count, zero/negative weight, or
                                               negative value. */
               KNAPSACK_ERR_TOO_MANY_ITEMS, /**< count exceeds KNAPSACK_MAX_ITEMS. */
               KNAPSACK_ERR_INVALID_CAPACITY,   /**< capacity negative or exceeds
                                                   KNAPSACK_MAX_CAPACITY. */
               KNAPSACK_ERR_DIMENSION_OVERFLOW, /**< would overflow internal buffers. */
               KNAPSACK_ERR_INT_OVERFLOW,       /**< value accumulation overflows int. */
               KNAPSACK_ERR_ALLOC               /**< allocation failed. */
} knapsack_status_t;

/** Pluggable allocator for testing and embedding.
 *
 *  Each callback receives the allocator's user_data as its last parameter.
 *  Either pass NULL (and the standard library is used), or supply a fully
 *  populated structure -- partial allocators are not supported.
 */
typedef struct {
  void *(*alloc_fn)(size_t size, void *user_data);
  void *(*calloc_fn)(size_t nmemb, size_t size, void *user_data);
  void (*free_fn)(void *ptr, void *user_data);
  void *user_data;
} knapsack_allocator_t;

/** Solve the 0/1 knapsack problem.
 *
 *  @param items     Array of @p count items. Must be non-NULL when count > 0.
 *  @param count     Number of items (1 .. KNAPSACK_MAX_ITEMS).
 *  @param capacity  Knapsack capacity (0 .. KNAPSACK_MAX_CAPACITY).
 *  @param out_result Result destination. On success, the caller must release
 *                    it via knapsack_result_free. On failure, it is zeroed.
 *  @return KNAPSACK_OK on success, otherwise a specific error code.
 */
knapsack_status_t knapsack_solve_status(const knapsack_item_t *items, size_t count, int capacity,
                                        knapsack_result_t *out_result);

/** Same as knapsack_solve_status but with an injectable allocator.
 *
 *  @param items     See knapsack_solve_status.
 *  @param count     See knapsack_solve_status.
 *  @param capacity  See knapsack_solve_status.
 *  @param allocator Custom allocator, or NULL to use malloc/calloc/free.
 *                   When non-NULL, all four members must be populated.
 *  @param out_result See knapsack_solve_status. Memory inside the result is
 *                   owned by @p allocator and must be released via
 *                   knapsack_result_free_ex with the same allocator.
 *  @return KNAPSACK_OK on success, otherwise a specific error code.
 */
knapsack_status_t knapsack_solve_status_ex(const knapsack_item_t *items, size_t count, int capacity,
                                           const knapsack_allocator_t *allocator,
                                           knapsack_result_t *out_result);

/** Release memory held by a knapsack_result_t. Safe to call on a zeroed
 *  struct or on a struct populated by knapsack_solve_status_ex with a custom
 *  allocator -- the same allocator that produced the result must be supplied.
 *  Use knapsack_result_free for the default allocator.
 */
void knapsack_result_free(knapsack_result_t *result);

/** Variant of knapsack_result_free that frees with a custom allocator. */
void knapsack_result_free_ex(knapsack_result_t *result, const knapsack_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* KNAPSACK_KNAPSACK_H */
