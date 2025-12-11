#include "knapsack/knapsack.h"

#include <climits>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <random>
#include <vector>

using ::testing::ElementsAre;

// NOLINTBEGIN(readability-magic-numbers)

TEST(KnapsackSolverTest, ComputesOptimalValueAndSelection) {
  knapsack_item_t items[] = {{2, 3}, {3, 4}, {4, 8}, {5, 8}, {9, 10}};
  const size_t count = sizeof(items) / sizeof(items[0]);
  const int capacity = 20;

  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, count, capacity, &result));

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
  EXPECT_FALSE(knapsack_solve(&item, 1U, -1, &result));
}

TEST(KnapsackSolverTest, RejectsNegativeWeight) {
  knapsack_item_t items[] = {{-1, 5}};
  knapsack_result_t result;
  EXPECT_FALSE(knapsack_solve(items, 1U, 10, &result));
}

TEST(KnapsackSolverTest, HandlesZeroCapacity) {
  knapsack_item_t items[] = {{1, 5}, {2, 10}};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, 2U, 0, &result));

  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, RejectsNullResultPointer) {
  knapsack_item_t item = {1, 1};
  EXPECT_FALSE(knapsack_solve(&item, 1U, 1, NULL));
}

TEST(KnapsackSolverTest, RejectsNullItems) {
  knapsack_result_t result;
  EXPECT_FALSE(knapsack_solve(NULL, 1U, 1, &result));
}

TEST(KnapsackSolverTest, RejectsZeroItems) {
  knapsack_item_t dummy = {1, 1};
  knapsack_result_t result;
  EXPECT_FALSE(knapsack_solve(&dummy, 0U, 10, &result));
}

TEST(KnapsackSolverTest, SingleItemExactFit) {
  knapsack_item_t item = {5, 9};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(&item, 1U, 5, &result));

  EXPECT_EQ(result.optimal_value, 9);
  ASSERT_EQ(result.selected_count, 1U);
  EXPECT_EQ(result.selected_indices[0], 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, HandlesAllItemsTooHeavy) {
  knapsack_item_t items[] = {{5, 10}, {6, 20}};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, 2U, 2, &result));

  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, AllZeroValuesChoosesNothing) {
  knapsack_item_t items[] = {{1, 0}, {2, 0}};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, 2U, 3, &result));

  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, HandlesLargeCapacityWithSmallItemSet) {
  knapsack_item_t items[] = {{1, 10}, {2, 15}};
  const size_t count = sizeof(items) / sizeof(items[0]);
  const int capacity = 100000;

  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, count, capacity, &result));

  EXPECT_EQ(result.optimal_value, 25);
  ASSERT_EQ(result.selected_count, 2U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(0U, 1U));

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, RejectsCapacityOverflow) {
  if (SIZE_MAX > UINT32_MAX) {
    GTEST_SKIP() << "Overflow guard only triggers on 32-bit size_t";
  }

  knapsack_item_t items[] = {{1, 1}, {1, 1}};
  const size_t count = sizeof(items) / sizeof(items[0]);
  const int capacity = INT_MAX; // width ~2^31; width * count overflows size_t on 32-bit

  knapsack_result_t result;
  EXPECT_FALSE(knapsack_solve(items, count, capacity, &result));
}

TEST(KnapsackSolverTest, RejectsValueOverflow) {
  knapsack_item_t items[] = {{1, INT_MAX}, {1, 1}}; // combining both would overflow int
  knapsack_result_t result;

  EXPECT_FALSE(knapsack_solve(items, 2U, 2, &result));
}

TEST(KnapsackSolverTest, HandlesNegativeValuesWithoutSelecting) {
  knapsack_item_t items[] = {{1, INT_MIN}, {1, -1}};
  knapsack_result_t result;

  ASSERT_TRUE(knapsack_solve(items, 2U, 2, &result));
  EXPECT_EQ(result.optimal_value, 0);
  EXPECT_EQ(result.selected_count, 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, ZeroWeightPositiveValueSelectedAtZeroCapacity) {
  knapsack_item_t items[] = {{0, 7}};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, 1U, 0, &result));

  EXPECT_EQ(result.optimal_value, 7);
  ASSERT_EQ(result.selected_count, 1U);
  EXPECT_EQ(result.selected_indices[0], 0U);

  knapsack_result_free(&result);
}

TEST(KnapsackSolverTest, PicksBestCombinationNotGreedy) {
  knapsack_item_t items[] = {{4, 6}, {5, 9}, {6, 12}, {3, 5}};
  knapsack_result_t result;
  ASSERT_TRUE(knapsack_solve(items, 4U, 9, &result));

  EXPECT_EQ(result.optimal_value, 17);
  ASSERT_EQ(result.selected_count, 2U);
  EXPECT_THAT(
      std::vector<size_t>(result.selected_indices, result.selected_indices + result.selected_count),
      ElementsAre(2U, 3U));

  knapsack_result_free(&result);
}

static int brute_force_optimal(const std::vector<knapsack_item_t> &items, int capacity) {
  const size_t count = items.size();
  int best = INT_MIN;
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(KnapsackSolverTest, FuzzMatchesBruteForceForSmallInstances) {
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> count_dist(1, 6);
  std::uniform_int_distribution<int> weight_dist(0, 5);
  std::uniform_int_distribution<int> value_dist(-5, 10);
  std::uniform_int_distribution<int> capacity_dist(0, 12);

  for (int trial = 0; trial < 200; ++trial) {
    const int count = count_dist(rng);
    std::vector<knapsack_item_t> items;
    items.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      items.push_back({weight_dist(rng), value_dist(rng)});
    }
    const int capacity = capacity_dist(rng);

    const int expected = brute_force_optimal(items, capacity);

    knapsack_result_t result;
    ASSERT_TRUE(knapsack_solve(items.data(), items.size(), capacity, &result));
    EXPECT_EQ(result.optimal_value, expected);

    int observed_weight = 0;
    for (size_t idx = 0; idx < result.selected_count; ++idx) {
      ASSERT_LT(result.selected_indices[idx], items.size());
      observed_weight += items[result.selected_indices[idx]].weight;
    }
    EXPECT_LE(observed_weight, capacity);

    knapsack_result_free(&result);
  }
}
// NOLINTEND(readability-magic-numbers)
