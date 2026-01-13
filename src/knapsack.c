#include "knapsack/knapsack.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static const size_t KNAPSACK_MAX_ITEMS = 100U;
static const int KNAPSACK_MAX_CAPACITY = 100000;

static void reset_result(knapsack_result_t *result) {
  if (!result) {
    return;
  }
  result->optimal_value = 0;
  result->selected_count = 0;
  result->selected_indices = NULL;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static knapsack_status_t validate_inputs(const knapsack_item_t *items, size_t count,
                                         int capacity) {
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

typedef struct {
  size_t width;
  size_t take_size;
  int *prev_value;
  int *curr_value;
  size_t *prev_weight;
  size_t *curr_weight;
  unsigned char *take;
} knapsack_workspace_t;

static bool allocate_workspace(knapsack_workspace_t *workspace) {
  workspace->prev_value = (int *)calloc(workspace->width, sizeof(int));
  workspace->curr_value = (int *)calloc(workspace->width, sizeof(int));
  workspace->prev_weight = (size_t *)calloc(workspace->width, sizeof(size_t));
  workspace->curr_weight = (size_t *)calloc(workspace->width, sizeof(size_t));
  workspace->take = (unsigned char *)calloc(workspace->take_size, sizeof(unsigned char));

  if (!workspace->prev_value || !workspace->curr_value || !workspace->prev_weight ||
      !workspace->curr_weight || !workspace->take) {
    free(workspace->prev_value);
    free(workspace->curr_value);
    free(workspace->prev_weight);
    free(workspace->curr_weight);
    free(workspace->take);
    workspace->prev_value = NULL;
    workspace->curr_value = NULL;
    workspace->prev_weight = NULL;
    workspace->curr_weight = NULL;
    workspace->take = NULL;
    return false;
  }

  return true;
}

static void free_workspace(knapsack_workspace_t *workspace) {
  if (!workspace) {
    return;
  }
  free(workspace->prev_value);
  free(workspace->curr_value);
  free(workspace->prev_weight);
  free(workspace->curr_weight);
  free(workspace->take);
  workspace->prev_value = NULL;
  workspace->curr_value = NULL;
  workspace->prev_weight = NULL;
  workspace->curr_weight = NULL;
  workspace->take = NULL;
}

static void copy_row(size_t width, int *dest_value, const int *src_value, size_t *dest_weight,
                     const size_t *src_weight) {
  for (size_t col = 0; col < width; ++col) {
    dest_value[col] = src_value[col];
    dest_weight[col] = src_weight[col];
  }
}

static bool run_dp(const knapsack_item_t *items, size_t count, knapsack_workspace_t *workspace) {
  const size_t width = workspace->width;

  for (size_t i = 0; i < count; ++i) {
    copy_row(width, workspace->curr_value, workspace->prev_value, workspace->curr_weight,
             workspace->prev_weight);

    const int item_weight = items[i].weight;
    const int item_value = items[i].value;
    for (size_t cap = (size_t)item_weight; cap < width; ++cap) {
      int candidate_val = 0;
      if (!add_int_no_overflow(workspace->prev_value[cap - (size_t)item_weight], item_value,
                               &candidate_val)) {
        return false;
      }
      const size_t candidate_weight =
          workspace->prev_weight[cap - (size_t)item_weight] + (size_t)item_weight;
      const int current_val = workspace->curr_value[cap];
      const size_t current_weight = workspace->curr_weight[cap];

      if (candidate_val > current_val ||
          (candidate_val == current_val && candidate_weight < current_weight)) {
        workspace->curr_value[cap] = candidate_val;
        workspace->curr_weight[cap] = candidate_weight;
        workspace->take[i * width + cap] = 1U;
      }
    }

    int *tmp_value = workspace->prev_value;
    workspace->prev_value = workspace->curr_value;
    workspace->curr_value = tmp_value;

    size_t *tmp_weight = workspace->prev_weight;
    workspace->prev_weight = workspace->curr_weight;
    workspace->curr_weight = tmp_weight;
  }

  return true;
}

static void select_best_cap(const knapsack_workspace_t *workspace, int *best_cap_out) {
  size_t best_cap = 0U;
  int best_val = workspace->prev_value[0];
  size_t best_weight = workspace->prev_weight[0];

  for (size_t cap = 1U; cap < workspace->width; ++cap) {
    const int val = workspace->prev_value[cap];
    const size_t weight_at_cap = workspace->prev_weight[cap];
    if (val > best_val || (val == best_val && weight_at_cap < best_weight)) {
      best_val = val;
      best_weight = weight_at_cap;
      best_cap = cap;
    }
  }
  *best_cap_out = (int)best_cap;
}

static int compare_size_t(const void *lhs, const void *rhs) {
  const size_t lhs_value = *(const size_t *)lhs;
  const size_t rhs_value = *(const size_t *)rhs;
  return (lhs_value > rhs_value) - (lhs_value < rhs_value);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static bool reconstruct_solution(const knapsack_workspace_t *workspace,
                                 const knapsack_item_t *items, size_t count, int best_cap,
                                 knapsack_result_t *out_result) {
  size_t cap = (size_t)best_cap;
  size_t selected = 0U;

  for (size_t i = count; i-- > 0;) {
    const size_t idx = i * workspace->width + cap;
    if (workspace->take[idx]) {
      ++selected;
      cap -= (size_t)items[i].weight;
    }
  }

  if (selected == 0U) {
    out_result->optimal_value = workspace->prev_value[best_cap];
    return true;
  }

  size_t *indices = (size_t *)malloc(selected * sizeof(size_t));
  if (!indices) {
    return false;
  }

  size_t write = selected;
  cap = best_cap;
  for (size_t i = count; i-- > 0;) {
    const size_t idx = i * workspace->width + cap;
    if (workspace->take[idx]) {
      indices[--write] = i;
      cap -= (size_t)items[i].weight;
    }
  }

  qsort(indices, selected, sizeof(size_t), compare_size_t);

  out_result->optimal_value = workspace->prev_value[best_cap];
  out_result->selected_indices = indices;
  out_result->selected_count = selected;
  return true;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

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
  knapsack_status_t status = knapsack_solve_status(items, count, capacity, out_result);
  return status == KNAPSACK_OK;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

knapsack_status_t knapsack_solve_status(const knapsack_item_t *items, size_t count, int capacity,
                                        knapsack_result_t *out_result) {
  if (!out_result) {
    return KNAPSACK_ERR_NULL_RESULT;
  }
  reset_result(out_result);

  const knapsack_status_t input_status = validate_inputs(items, count, capacity);
  if (input_status != KNAPSACK_OK) {
    return input_status;
  }

  const size_t width = (size_t)capacity + 1U;
  if (count != 0U && width > SIZE_MAX / count) {
    return KNAPSACK_ERR_DIMENSION_OVERFLOW;
  }
  const size_t take_size = width * count;
  if (take_size == 0U) {
    return KNAPSACK_ERR_DIMENSION_OVERFLOW;
  }
  knapsack_workspace_t workspace = {.width = width,
                                    .take_size = take_size,
                                    .prev_value = NULL,
                                    .curr_value = NULL,
                                    .prev_weight = NULL,
                                    .curr_weight = NULL,
                                    .take = NULL};

  if (!allocate_workspace(&workspace)) {
    return KNAPSACK_ERR_ALLOC;
  }

  knapsack_status_t status = KNAPSACK_OK;
  const bool dp_ok = run_dp(items, count, &workspace);
  if (!dp_ok) {
    status = KNAPSACK_ERR_INT_OVERFLOW;
  }

  int best_cap = 0;
  if (status == KNAPSACK_OK) {
    select_best_cap(&workspace, &best_cap);
  }

  const bool rebuild_ok = (status == KNAPSACK_OK) &&
                          reconstruct_solution(&workspace, items, count, best_cap, out_result);
  if (!rebuild_ok && status == KNAPSACK_OK) {
    status = KNAPSACK_ERR_ALLOC;
  }

  free_workspace(&workspace);
  return status;
}