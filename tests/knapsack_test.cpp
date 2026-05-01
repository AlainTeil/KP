#include "knapsack/knapsack.h"

#include <climits>
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <random>
#include <vector>

using ::testing::ElementsAre;

namespace {

// Helper: solve and ASSERT success.
void SolveOk(const std::vector<knapsack_item_t> &items, int capacity, knapsack_result_t *result) {
  ASSERT_EQ(knapsack_solve_status(items.data(), items.size(), capacity, result), KNAPSACK_OK);
}

// NOLINTBEGIN(readability-magic-numbers)

TEST(KnapsackSolverTest, ComputesOptimalValueAndSelection) {
  std::vector<knapsack_item_t> items = {{2, 3}, {3, 4}, {4, 8}, {5, 8}, {9, 10}};
  knapsack_result_t result;
  SolveOk(items, 20, &result);

  EXPECT_EQ(result.optimal_value, 29);
  ASSERT_EQ(result.selected_count, 4U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(0U, 2U, 3U, 4U));
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, RejectsNegativeCapacity) {
  knapsack_item_t item = {1, 1};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(&item, 1U, -1, &result), KNAPSACK_ERR_INVALID_CAPACITY);
}

TEST(KnapsackSolverTest, RejectsNegativeWeight) {
  knapsack_item_t items[] = {{-1, 5}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items, 1U, 10, &result), KNAPSACK_ERR_INVALID_ITEMS);
}

TEST(KnapsackSolverTest, RejectsZeroWeight) {
  knapsack_item_t items[] = {{0, 5}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items, 1U, 10, &result), KNAPSACK_ERR_INVALID_ITEMS);
}

TEST(KnapsackSolverTest, RejectsNegativeValue) {
  knapsack_item_t items[] = {{1, -5}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items, 1U, 10, &result), KNAPSACK_ERR_INVALID_ITEMS);
}

TEST(KnapsackSolverTest, HandlesZeroCapacity) {
  std::vector<knapsack_item_t> items = {{1, 5}, {2, 10}};
  knapsack_result_t result;
  SolveOk(items, 0, &result);
  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, NullResultPointerReturnsStatus) {
  knapsack_item_t item = {1, 1};
  EXPECT_EQ(knapsack_solve_status(&item, 1U, 1, nullptr), KNAPSACK_ERR_NULL_RESULT);
}

TEST(KnapsackSolverTest, RejectsNullItems) {
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(nullptr, 1U, 1, &result), KNAPSACK_ERR_INVALID_ITEMS);
}

TEST(KnapsackSolverTest, RejectsZeroItems) {
  knapsack_item_t dummy = {1, 1};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(&dummy, 0U, 10, &result), KNAPSACK_ERR_INVALID_ITEMS);
}

TEST(KnapsackSolverTest, SingleItemExactFit) {
  std::vector<knapsack_item_t> items = {{5, 9}};
  knapsack_result_t result;
  SolveOk(items, 5, &result);
  EXPECT_EQ(result.optimal_value, 9);
  ASSERT_EQ(result.selected_count, 1U);
  EXPECT_EQ(result.selected_indices[0], 0U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, HandlesAllItemsTooHeavy) {
  std::vector<knapsack_item_t> items = {{5, 10}, {6, 20}};
  knapsack_result_t result;
  SolveOk(items, 2, &result);
  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, AllZeroValuesChoosesNothing) {
  std::vector<knapsack_item_t> items = {{1, 0}, {2, 0}};
  knapsack_result_t result;
  SolveOk(items, 3, &result);
  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, HandlesLargeCapacityWithSmallItemSet) {
  std::vector<knapsack_item_t> items = {{1, 10}, {2, 15}};
  knapsack_result_t result;
  SolveOk(items, 100000, &result);
  EXPECT_EQ(result.optimal_value, 25);
  ASSERT_EQ(result.selected_count, 2U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(0U, 1U));
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, RejectsCapacityAboveLimit) {
  knapsack_item_t items[] = {{1, 1}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items, 1U, 100001, &result), KNAPSACK_ERR_INVALID_CAPACITY);
}

TEST(KnapsackSolverTest, RejectsItemCountAboveLimit) {
  std::vector<knapsack_item_t> items(101, {1, 1});
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items.data(), items.size(), 10, &result),
            KNAPSACK_ERR_TOO_MANY_ITEMS);
}

TEST(KnapsackSolverTest, AcceptsMaximumLimits) {
  std::vector<knapsack_item_t> items(100, {1, 1});
  knapsack_result_t result;
  SolveOk(items, 100000, &result);
  EXPECT_EQ(result.optimal_value, 100);
  ASSERT_EQ(result.selected_count, items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    EXPECT_EQ(result.selected_indices[i], i);
  }
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, RejectsValueOverflow) {
  knapsack_item_t items[] = {{1, INT_MAX}, {1, 1}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status(items, 2U, 2, &result), KNAPSACK_ERR_INT_OVERFLOW);
}

TEST(KnapsackSolverTest, PicksBestCombinationNotGreedy) {
  std::vector<knapsack_item_t> items = {{4, 6}, {5, 9}, {6, 12}, {3, 5}};
  knapsack_result_t result;
  SolveOk(items, 9, &result);
  EXPECT_EQ(result.optimal_value, 17);
  ASSERT_EQ(result.selected_count, 2U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(2U, 3U));
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, TieBreaksBySmallerTotalWeight) {
  std::vector<knapsack_item_t> items = {{3, 10}, {4, 10}};
  knapsack_result_t result;
  SolveOk(items, 4, &result);
  EXPECT_EQ(result.optimal_value, 10);
  ASSERT_EQ(result.selected_count, 1U);
  EXPECT_EQ(result.selected_indices[0], 0U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, TieBreakPrefersSmallestTotalWeightAcrossCombos) {
  // Two ways to reach value 10: pick {0,1} weight 5, or pick {2} weight 4.
  // Solver should choose the smaller-weight selection (item 2 alone).
  std::vector<knapsack_item_t> items = {{2, 5}, {3, 5}, {4, 10}};
  knapsack_result_t result;
  SolveOk(items, 5, &result);
  EXPECT_EQ(result.optimal_value, 10);
  ASSERT_EQ(result.selected_count, 1U);
  EXPECT_EQ(result.selected_indices[0], 2U);
  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, SelectionIndicesAreAscending) {
  std::vector<knapsack_item_t> items = {{2, 2}, {1, 2}, {3, 3}};
  knapsack_result_t result;
  SolveOk(items, 3, &result);
  EXPECT_EQ(result.optimal_value, 4);
  ASSERT_EQ(result.selected_count, 2U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(0U, 1U));
  knapsack_result_free(&result);
}

// --- Allocator-failure tests via knapsack_solve_status_ex --------------------

namespace {
struct CountingAllocator {
  int alloc_calls;
  int calloc_calls;
  int free_calls;
  int alloc_fail_after;  // -1 = never
  int calloc_fail_after; // -1 = never
};

void *CountAlloc(size_t size, void *ud) {
  auto *a = static_cast<CountingAllocator *>(ud);
  if (a->alloc_fail_after == 0) {
    return nullptr;
  }
  if (a->alloc_fail_after > 0) {
    --a->alloc_fail_after;
  }
  ++a->alloc_calls;
  return std::malloc(size);
}
void *CountCalloc(size_t n, size_t s, void *ud) {
  auto *a = static_cast<CountingAllocator *>(ud);
  if (a->calloc_fail_after == 0) {
    return nullptr;
  }
  if (a->calloc_fail_after > 0) {
    --a->calloc_fail_after;
  }
  ++a->calloc_calls;
  return std::calloc(n, s);
}
void CountFree(void *p, void *ud) {
  auto *a = static_cast<CountingAllocator *>(ud);
  ++a->free_calls;
  std::free(p);
}
} // namespace

TEST(KnapsackAllocatorTest, CustomAllocatorIsUsed) {
  CountingAllocator data{0, 0, 0, -1, -1};
  knapsack_allocator_t alloc = {CountAlloc, CountCalloc, CountFree, &data};
  std::vector<knapsack_item_t> items = {{2, 3}, {3, 4}, {4, 8}};
  knapsack_result_t result;
  ASSERT_EQ(knapsack_solve_status_ex(items.data(), items.size(), 10, &alloc, &result), KNAPSACK_OK);
  EXPECT_GT(data.calloc_calls, 0);
  knapsack_result_free_ex(&result, &alloc);
  EXPECT_GT(data.free_calls, 0);
}

TEST(KnapsackAllocatorTest, FirstCallocFailureReturnsAllocError) {
  CountingAllocator data{0, 0, 0, -1, 0};
  knapsack_allocator_t alloc = {CountAlloc, CountCalloc, CountFree, &data};
  std::vector<knapsack_item_t> items = {{1, 1}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status_ex(items.data(), items.size(), 5, &alloc, &result),
            KNAPSACK_ERR_ALLOC);
}

TEST(KnapsackAllocatorTest, ReconstructionAllocFailureReturnsAllocError) {
  // Solve succeeds through DP but the indices array allocation fails.
  CountingAllocator data{0, 0, 0, 0, -1};
  knapsack_allocator_t alloc = {CountAlloc, CountCalloc, CountFree, &data};
  std::vector<knapsack_item_t> items = {{1, 1}};
  knapsack_result_t result;
  EXPECT_EQ(knapsack_solve_status_ex(items.data(), items.size(), 5, &alloc, &result),
            KNAPSACK_ERR_ALLOC);
}

// --- Property test -----------------------------------------------------------

namespace {
int BruteForceOptimal(const std::vector<knapsack_item_t> &items, int capacity) {
  const size_t count = items.size();
  int best = 0;
  const size_t total = 1ULL << count;
  for (size_t mask = 0; mask < total; ++mask) {
    int total_weight = 0;
    int total_value = 0;
    for (size_t i = 0; i < count; ++i) {
      if ((mask & (1ULL << i)) != 0U) {
        total_weight += items[i].weight;
        total_value += items[i].value;
      }
    }
    if (total_weight <= capacity && total_value > best) {
      best = total_value;
    }
  }
  return best;
}
} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(KnapsackSolverTest, FuzzMatchesBruteForceForSmallInstances) {
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> count_dist(1, 6);
  std::uniform_int_distribution<int> weight_dist(1, 5);
  std::uniform_int_distribution<int> value_dist(0, 10);
  std::uniform_int_distribution<int> capacity_dist(0, 12);

  for (int trial = 0; trial < 200; ++trial) {
    const int count = count_dist(rng);
    std::vector<knapsack_item_t> items;
    items.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      items.push_back({weight_dist(rng), value_dist(rng)});
    }
    const int capacity = capacity_dist(rng);

    const int expected = BruteForceOptimal(items, capacity);

    knapsack_result_t result;
    ASSERT_EQ(knapsack_solve_status(items.data(), items.size(), capacity, &result), KNAPSACK_OK);
    EXPECT_EQ(result.optimal_value, expected);

    int observed_weight = 0;
    for (size_t idx = 0; idx < result.selected_count; ++idx) {
      ASSERT_LT(result.selected_indices[idx], items.size());
      observed_weight += items[result.selected_indices[idx]].weight;
    }
    EXPECT_LE(observed_weight, capacity);

    // Ascending indices invariant.
    for (size_t i = 1; i < result.selected_count; ++i) {
      EXPECT_LT(result.selected_indices[i - 1], result.selected_indices[i]);
    }
    knapsack_result_free(&result);
  }
}

TEST(KnapsackResultFree, NullResultIsNoOp) {
  // Both null result and null allocator must be safe.
  knapsack_result_free(nullptr);
  knapsack_result_free_ex(nullptr, nullptr);
}

// NOLINTEND(readability-magic-numbers)

} // namespace
