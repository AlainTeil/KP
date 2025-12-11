#include "knapsack/knapsack.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

static bool validate_items(const knapsack_item_t *items, size_t count) {
  if (!items || count == 0U) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (items[i].weight < 0) {
      return false;
    }
  }
  return true;
}

static bool validate_capacity(int capacity) { return capacity >= 0; }

static bool validate_dimensions(size_t count, size_t width, size_t *take_size_out) {
  if (!take_size_out) {
    return false;
  }
  if (width == 0U) {
    return false;
  }
  if (count != 0U && width > SIZE_MAX / count) {
    return false;
  }
  const size_t take_size = count * width;
  if (take_size == 0U) {
    return false;
  }
  *take_size_out = take_size;
  return true;
}

typedef struct {
  size_t width;
  size_t take_size;
  int *prev;
  int *curr;
  unsigned char *take;
} knapsack_workspace_t;

static bool allocate_workspace(knapsack_workspace_t *workspace) {
  if (!workspace) {
    return false;
  }
  workspace->prev = (int *)calloc(workspace->width, sizeof(int));
  workspace->curr = (int *)calloc(workspace->width, sizeof(int));
  workspace->take = (unsigned char *)calloc(workspace->take_size, sizeof(unsigned char));
  if (!workspace->prev || !workspace->curr || !workspace->take) {
    free(workspace->prev);
    free(workspace->curr);
    free(workspace->take);
    workspace->prev = NULL;
    workspace->curr = NULL;
    workspace->take = NULL;
    return false;
  }
  return true;
}

static void free_workspace(knapsack_workspace_t *workspace) {
  if (!workspace) {
    return;
  }
  free(workspace->prev);
  free(workspace->curr);
  free(workspace->take);
  workspace->prev = NULL;
  workspace->curr = NULL;
  workspace->take = NULL;
}

static void reset_result(knapsack_result_t *result) {
  if (!result) {
    return;
  }
  result->optimal_value = 0;
  result->selected_count = 0;
  result->selected_indices = NULL;
}

static void copy_row(size_t width, int *dest, const int *src) {
  for (size_t col = 0; col < width; ++col) {
    dest[col] = src[col];
  }
}

/* Returns false if the addition would overflow an int. */
static bool add_int_no_overflow(int lhs, int rhs, int *out) {
  if (!out) {
    return false;
  }
  const long long sum = (long long)lhs + (long long)rhs;
  if (sum > INT_MAX || sum < INT_MIN) {
    return false;
  }
  *out = (int)sum;
  return true;
}

/*
 * Fills the DP table row by row. Workspace buffers (`prev`, `curr`, `take`) are
 * owned by the caller and reused in-place. Complexity: O(count * width).
 * Returns false if any intermediate sum would overflow an int.
 */
static bool run_dp(const knapsack_item_t *items, size_t count, knapsack_workspace_t *workspace,
                   int **last_row_out) {
  if (!workspace || !last_row_out) {
    return false;
  }
  *last_row_out = NULL;

  int *local_prev = workspace->prev;
  int *local_curr = workspace->curr;
  const size_t width = workspace->width;

  for (size_t i = 0; i < count; ++i) {
    copy_row(width, local_curr, local_prev);
    const int item_weight = items[i].weight;
    const int item_value = items[i].value;
    for (size_t cap = (size_t)item_weight; cap < width; ++cap) {
      int candidate = 0;
      if (!add_int_no_overflow(local_prev[cap - (size_t)item_weight], item_value, &candidate)) {
        return false;
      }
      if (candidate > local_curr[cap]) {
        local_curr[cap] = candidate;
        workspace->take[i * width + cap] = 1U;
      }
    }
    int *tmp = local_prev;
    local_prev = local_curr;
    local_curr = tmp;
  }

  *last_row_out = local_prev;
  return true;
}

/*
 * Reconstructs the chosen items from the take matrix into out_result. Ownership of
 * workspace buffers remains with the caller; out_result allocates selected_indices.
 */
static bool reconstruct_solution(const knapsack_item_t *items, size_t count,
                                 const knapsack_workspace_t *workspace, const int *last_row,
                                 knapsack_result_t *out_result) {
  if (!workspace || !last_row || !out_result) {
    return false;
  }

  out_result->optimal_value = last_row[workspace->width - 1U];

  size_t cap = workspace->width - 1U;
  size_t selected = 0;
  for (size_t i = count; i-- > 0;) {
    const size_t idx = i * workspace->width + cap;
    if (workspace->take[idx]) {
      ++selected;
      cap -= (size_t)items[i].weight;
    }
  }

  if (selected == 0U) {
    return true;
  }

  out_result->selected_indices = (size_t *)malloc(selected * sizeof(size_t));
  if (!out_result->selected_indices) {
    reset_result(out_result);
    return false;
  }

  size_t write = selected;
  cap = workspace->width - 1U;
  for (size_t i = count; i-- > 0;) {
    const size_t idx = i * workspace->width + cap;
    if (workspace->take[idx]) {
      out_result->selected_indices[--write] = i;
      cap -= (size_t)items[i].weight;
    }
  }
  out_result->selected_count = selected;
  return true;
}

void knapsack_result_free(knapsack_result_t *result) {
  if (!result) {
    return;
  }
  free(result->selected_indices);
  result->selected_indices = NULL;
  result->selected_count = 0;
  result->optimal_value = 0;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool knapsack_solve(const knapsack_item_t *items, size_t count, int capacity,
                    knapsack_result_t *out_result) {
  if (!out_result) {
    return false;
  }
  reset_result(out_result);

  if (!validate_items(items, count)) {
    return false;
  }
  if (!validate_capacity(capacity)) {
    return false;
  }
  const size_t width = (size_t)capacity + 1U;
  size_t take_size = 0U;
  if (!validate_dimensions(count, width, &take_size)) {
    return false;
  }

  knapsack_workspace_t workspace = {
      .width = width, .take_size = take_size, .prev = NULL, .curr = NULL, .take = NULL};
  if (!allocate_workspace(&workspace)) {
    return false;
  }

  int *last_row = NULL;
  const bool dp_ok = run_dp(items, count, &workspace, &last_row);
  const bool rebuild_ok = dp_ok && last_row != NULL &&
                          reconstruct_solution(items, count, &workspace, last_row, out_result);

  free_workspace(&workspace);
  return dp_ok && rebuild_ok;
}
// NOLINTEND(bugprone-easily-swappable-parameters)