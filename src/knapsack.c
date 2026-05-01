/* Knapsack 0/1 DP solver.
 *
 * Memory layout:
 *   prev_value[width], curr_value[width]   -- two int rows for value rolling.
 *   prev_weight[width], curr_weight[width] -- two size_t rows for weight tiebreaking.
 *   take_bits[ceil(count*width / 64)]      -- packed bitset of "take" decisions
 *                                              for reconstruction.
 *
 * All allocations go through a knapsack_allocator_t; the public default API
 * passes NULL and the implementation falls back to malloc/calloc/free.
 */

#include "knapsack/knapsack.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Allocator plumbing                                                         */
/* ------------------------------------------------------------------------- */

static void *default_alloc(size_t size, void *user_data) {
  (void)user_data;
  return malloc(size);
}
static void *default_calloc(size_t nmemb, size_t size, void *user_data) {
  (void)user_data;
  return calloc(nmemb, size);
}
static void default_free(void *ptr, void *user_data) {
  (void)user_data;
  free(ptr);
}

static const knapsack_allocator_t k_default_allocator = {
    default_alloc,
    default_calloc,
    default_free,
    NULL,
};

static const knapsack_allocator_t *resolve_allocator(const knapsack_allocator_t *user) {
  return user ? user : &k_default_allocator;
}

/* ------------------------------------------------------------------------- */
/* Validation                                                                 */
/* ------------------------------------------------------------------------- */

/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- public API order. */
static knapsack_status_t validate_inputs(const knapsack_item_t *items, size_t count, int capacity) {
  if (!items || count == 0U) {
    return KNAPSACK_ERR_INVALID_ITEMS;
  }
  if (count > KNAPSACK_MAX_ITEMS) {
    return KNAPSACK_ERR_TOO_MANY_ITEMS;
  }
  if (capacity < 0 || capacity > KNAPSACK_MAX_CAPACITY) {
    return KNAPSACK_ERR_INVALID_CAPACITY;
  }
  for (size_t i = 0; i < count; ++i) {
    if (items[i].weight <= 0 || items[i].value < 0) {
      return KNAPSACK_ERR_INVALID_ITEMS;
    }
  }
  return KNAPSACK_OK;
}

static bool add_int_no_overflow(int lhs, int rhs, int *out) {
  const long long sum = (long long)lhs + (long long)rhs;
  if (sum > INT_MAX || sum < INT_MIN) {
    return false;
  }
  *out = (int)sum;
  return true;
}

/* ------------------------------------------------------------------------- */
/* Bitset helpers                                                             */
/* ------------------------------------------------------------------------- */

#define KNAPSACK_BITSET_WORD_BITS 64U

static size_t bitset_words(size_t bit_count) {
  return (bit_count + KNAPSACK_BITSET_WORD_BITS - 1U) / KNAPSACK_BITSET_WORD_BITS;
}

static void bitset_set(uint64_t *bits, size_t idx) {
  bits[idx / KNAPSACK_BITSET_WORD_BITS] |= (uint64_t)1 << (idx % KNAPSACK_BITSET_WORD_BITS);
}

static bool bitset_test(const uint64_t *bits, size_t idx) {
  return (bits[idx / KNAPSACK_BITSET_WORD_BITS] >> (idx % KNAPSACK_BITSET_WORD_BITS)) & 1U;
}

/* ------------------------------------------------------------------------- */
/* Workspace                                                                  */
/* ------------------------------------------------------------------------- */

typedef struct {
  size_t width;          /* capacity + 1 */
  size_t take_bit_count; /* count * width */
  int *prev_value;
  int *curr_value;
  size_t *prev_weight;
  size_t *curr_weight;
  uint64_t *take_bits;
} workspace_t;

static void free_workspace(workspace_t *ws, const knapsack_allocator_t *alloc) {
  if (!ws) {
    return;
  }
  alloc->free_fn(ws->prev_value, alloc->user_data);
  alloc->free_fn(ws->curr_value, alloc->user_data);
  alloc->free_fn(ws->prev_weight, alloc->user_data);
  alloc->free_fn(ws->curr_weight, alloc->user_data);
  alloc->free_fn(ws->take_bits, alloc->user_data);
  ws->prev_value = NULL;
  ws->curr_value = NULL;
  ws->prev_weight = NULL;
  ws->curr_weight = NULL;
  ws->take_bits = NULL;
}

static bool allocate_workspace(workspace_t *ws, const knapsack_allocator_t *alloc) {
  ws->prev_value = alloc->calloc_fn(ws->width, sizeof(int), alloc->user_data);
  ws->curr_value = alloc->calloc_fn(ws->width, sizeof(int), alloc->user_data);
  ws->prev_weight = alloc->calloc_fn(ws->width, sizeof(size_t), alloc->user_data);
  ws->curr_weight = alloc->calloc_fn(ws->width, sizeof(size_t), alloc->user_data);
  const size_t words = bitset_words(ws->take_bit_count);
  ws->take_bits = alloc->calloc_fn(words, sizeof(uint64_t), alloc->user_data);
  if (!ws->prev_value || !ws->curr_value || !ws->prev_weight || !ws->curr_weight ||
      !ws->take_bits) {
    free_workspace(ws, alloc);
    return false;
  }
  return true;
}

/* ------------------------------------------------------------------------- */
/* DP                                                                         */
/* ------------------------------------------------------------------------- */

static void copy_row(size_t width, int *dest_value, const int *src_value, size_t *dest_weight,
                     const size_t *src_weight) {
  memcpy(dest_value, src_value, width * sizeof(int));
  memcpy(dest_weight, src_weight, width * sizeof(size_t));
}

static bool run_dp(const knapsack_item_t *items, size_t count, workspace_t *ws) {
  const size_t width = ws->width;

  for (size_t i = 0; i < count; ++i) {
    copy_row(width, ws->curr_value, ws->prev_value, ws->curr_weight, ws->prev_weight);

    const size_t item_weight = (size_t)items[i].weight;
    const int item_value = items[i].value;

    for (size_t cap = item_weight; cap < width; ++cap) {
      int candidate_val = 0;
      if (!add_int_no_overflow(ws->prev_value[cap - item_weight], item_value, &candidate_val)) {
        return false;
      }
      const size_t candidate_weight = ws->prev_weight[cap - item_weight] + item_weight;
      const int current_val = ws->curr_value[cap];
      const size_t current_weight = ws->curr_weight[cap];

      if (candidate_val > current_val ||
          (candidate_val == current_val && candidate_weight < current_weight)) {
        ws->curr_value[cap] = candidate_val;
        ws->curr_weight[cap] = candidate_weight;
        bitset_set(ws->take_bits, i * width + cap);
      }
    }

    int *tmp_v = ws->prev_value;
    ws->prev_value = ws->curr_value;
    ws->curr_value = tmp_v;
    size_t *tmp_w = ws->prev_weight;
    ws->prev_weight = ws->curr_weight;
    ws->curr_weight = tmp_w;
  }
  return true;
}

static size_t select_best_cap(const workspace_t *ws) {
  size_t best_cap = 0U;
  int best_val = ws->prev_value[0];
  size_t best_weight = ws->prev_weight[0];
  for (size_t cap = 1U; cap < ws->width; ++cap) {
    const int val = ws->prev_value[cap];
    const size_t weight_at_cap = ws->prev_weight[cap];
    if (val > best_val || (val == best_val && weight_at_cap < best_weight)) {
      best_val = val;
      best_weight = weight_at_cap;
      best_cap = cap;
    }
  }
  return best_cap;
}

static int compare_size_t(const void *lhs, const void *rhs) {
  const size_t l = *(const size_t *)lhs;
  const size_t r = *(const size_t *)rhs;
  return (l > r) - (l < r);
}

/* NOLINTBEGIN(bugprone-easily-swappable-parameters) -- internal helper, params are documented. */
static knapsack_status_t reconstruct_solution(const workspace_t *ws, const knapsack_item_t *items,
                                              size_t count, size_t best_cap,
                                              const knapsack_allocator_t *alloc,
                                              knapsack_result_t *out_result) {
  /* First pass: count selections. */
  size_t cap = best_cap;
  size_t selected = 0U;
  for (size_t i = count; i-- > 0;) {
    if (bitset_test(ws->take_bits, i * ws->width + cap)) {
      ++selected;
      cap -= (size_t)items[i].weight;
    }
  }

  out_result->optimal_value = ws->prev_value[best_cap];
  if (selected == 0U) {
    return KNAPSACK_OK;
  }

  size_t *indices = alloc->alloc_fn(selected * sizeof(size_t), alloc->user_data);
  if (!indices) {
    return KNAPSACK_ERR_ALLOC;
  }

  /* Second pass: fill (back-to-front). */
  size_t write = selected;
  cap = best_cap;
  for (size_t i = count; i-- > 0;) {
    if (bitset_test(ws->take_bits, i * ws->width + cap)) {
      indices[--write] = i;
      cap -= (size_t)items[i].weight;
    }
  }

  qsort(indices, selected, sizeof(size_t), compare_size_t);
  out_result->selected_indices = indices;
  out_result->selected_count = selected;
  return KNAPSACK_OK;
}
/* NOLINTEND(bugprone-easily-swappable-parameters) */

/* ------------------------------------------------------------------------- */
/* Public API                                                                 */
/* ------------------------------------------------------------------------- */

void knapsack_result_free(knapsack_result_t *result) { knapsack_result_free_ex(result, NULL); }

void knapsack_result_free_ex(knapsack_result_t *result, const knapsack_allocator_t *allocator) {
  if (!result) {
    return;
  }
  const knapsack_allocator_t *alloc = resolve_allocator(allocator);
  alloc->free_fn(result->selected_indices, alloc->user_data);
  result->selected_indices = NULL;
  result->selected_count = 0;
  result->optimal_value = 0;
}

knapsack_status_t knapsack_solve_status(const knapsack_item_t *items, size_t count, int capacity,
                                        knapsack_result_t *out_result) {
  return knapsack_solve_status_ex(items, count, capacity, NULL, out_result);
}

knapsack_status_t knapsack_solve_status_ex(const knapsack_item_t *items, size_t count, int capacity,
                                           const knapsack_allocator_t *allocator,
                                           knapsack_result_t *out_result) {
  if (!out_result) {
    return KNAPSACK_ERR_NULL_RESULT;
  }
  *out_result = (knapsack_result_t){0};

  const knapsack_status_t input_status = validate_inputs(items, count, capacity);
  if (input_status != KNAPSACK_OK) {
    return input_status;
  }

  const knapsack_allocator_t *alloc = resolve_allocator(allocator);

  const size_t width = (size_t)capacity + 1U;
  if (count != 0U && width > SIZE_MAX / count) {
    return KNAPSACK_ERR_DIMENSION_OVERFLOW;
  }
  const size_t take_bit_count = width * count;
  if (take_bit_count == 0U) {
    return KNAPSACK_ERR_DIMENSION_OVERFLOW;
  }

  workspace_t ws = {
      .width = width,
      .take_bit_count = take_bit_count,
      .prev_value = NULL,
      .curr_value = NULL,
      .prev_weight = NULL,
      .curr_weight = NULL,
      .take_bits = NULL,
  };

  if (!allocate_workspace(&ws, alloc)) {
    return KNAPSACK_ERR_ALLOC;
  }

  knapsack_status_t status = KNAPSACK_OK;
  if (!run_dp(items, count, &ws)) {
    status = KNAPSACK_ERR_INT_OVERFLOW;
  }

  if (status == KNAPSACK_OK) {
    const size_t best_cap = select_best_cap(&ws);
    status = reconstruct_solution(&ws, items, count, best_cap, alloc, out_result);
  }

  free_workspace(&ws, alloc);
  return status;
}
