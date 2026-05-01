/* Micro-benchmarks for the knapsack solver.
 *
 * Each fixture varies item count and capacity; specialized fixtures cover
 * scenarios where the DP behaves quite differently:
 *
 *   - dense:        weights small relative to W (most items typically fit)
 *   - sparse:       weights close to W (few items fit; many DP rejects)
 *   - too_heavy:    every item heavier than W (DP rejects everything)
 *   - exact_fit:    weights divide W cleanly (forces full reconstruction)
 *
 * The solver is run inside the timed loop; allocation/free are part of the
 * measured cost (which is realistic for one-shot use).
 */

#include "knapsack/knapsack.h"

#include <benchmark/benchmark.h>
#include <random>
#include <vector>

namespace {

enum class Pattern { Dense, Sparse, TooHeavy, ExactFit };

std::vector<knapsack_item_t> MakeItems(size_t count, int capacity, Pattern pattern, unsigned seed) {
  std::mt19937 rng(seed);
  std::vector<knapsack_item_t> items;
  items.reserve(count);

  switch (pattern) {
  case Pattern::Dense: {
    const int max_w = std::max(1, capacity / 50);
    std::uniform_int_distribution<int> w(1, max_w);
    std::uniform_int_distribution<int> v(1, 1000);
    for (size_t i = 0; i < count; ++i) {
      items.push_back({w(rng), v(rng)});
    }
    break;
  }
  case Pattern::Sparse: {
    const int low = std::max(1, capacity * 3 / 4);
    const int high = std::max(low, capacity);
    std::uniform_int_distribution<int> w(low, high);
    std::uniform_int_distribution<int> v(1, 1000);
    for (size_t i = 0; i < count; ++i) {
      items.push_back({w(rng), v(rng)});
    }
    break;
  }
  case Pattern::TooHeavy: {
    std::uniform_int_distribution<int> v(1, 1000);
    for (size_t i = 0; i < count; ++i) {
      items.push_back({capacity + 1, v(rng)});
    }
    break;
  }
  case Pattern::ExactFit: {
    const int per_item = std::max(1, capacity / static_cast<int>(count));
    std::uniform_int_distribution<int> v(1, 1000);
    for (size_t i = 0; i < count; ++i) {
      items.push_back({per_item, v(rng)});
    }
    break;
  }
  }
  return items;
}

void RunSolveLoop(benchmark::State &state, Pattern pattern) {
  const auto count = static_cast<size_t>(state.range(0));
  const int capacity = static_cast<int>(state.range(1));
  const auto items = MakeItems(count, capacity, pattern, 1234U);

  size_t solve_failures = 0;
  for (auto _ : state) {
    knapsack_result_t result;
    const knapsack_status_t status =
        knapsack_solve_status(items.data(), items.size(), capacity, &result);
    benchmark::DoNotOptimize(result.optimal_value);
    if (status == KNAPSACK_OK) {
      knapsack_result_free(&result);
    } else {
      ++solve_failures;
    }
  }
  state.counters["solve_failures"] = static_cast<double>(solve_failures);
  state.counters["dp_cells"] =
      benchmark::Counter(static_cast<double>(count) * (static_cast<double>(capacity) + 1.0),
                         benchmark::Counter::kAvgIterations);
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(count));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(count) *
                          static_cast<int64_t>(sizeof(knapsack_item_t)));
}

void BM_Dense(benchmark::State &state) { RunSolveLoop(state, Pattern::Dense); }
void BM_Sparse(benchmark::State &state) { RunSolveLoop(state, Pattern::Sparse); }
void BM_TooHeavy(benchmark::State &state) { RunSolveLoop(state, Pattern::TooHeavy); }
void BM_ExactFit(benchmark::State &state) { RunSolveLoop(state, Pattern::ExactFit); }

} // namespace

#define KNAPSACK_BENCH_ARGS()                                                                      \
  Args({10, 100})->Args({50, 1000})->Args({100, 10000})->Args({100, 100000})

BENCHMARK(BM_Dense)->KNAPSACK_BENCH_ARGS();
BENCHMARK(BM_Sparse)->KNAPSACK_BENCH_ARGS();
BENCHMARK(BM_TooHeavy)->KNAPSACK_BENCH_ARGS();
BENCHMARK(BM_ExactFit)->KNAPSACK_BENCH_ARGS();

BENCHMARK_MAIN();
