# Knapsack C Library

Simple C17 library for solving the 0/1 knapsack problem with a demo CLI and GTest/GMock tests.

Constraints and behavior:
- Items: count <= 100; each weight > 0; each value >= 0; negative values and zero weights are rejected.
- Capacity: 0 <= W <= 100000; extremely large capacities that would overflow internal buffers are rejected.
- Optimality: exact DP with tie-break on smallest total weight; selected indices are reported in ascending order.
- Complexity: O(items * capacity) time, O(items * capacity) space (bounded by the limits above).
- API: legacy `knapsack_solve` returns bool; `knapsack_solve_status` returns a specific `knapsack_status_t`
	(e.g., INVALID_ITEMS, INVALID_CAPACITY, DIMENSION_OVERFLOW, INT_OVERFLOW, ALLOC). `knapsack_result_free`
	still frees internal allocations.

Strengths:
- Exact optimal solutions with deterministic tie-break on minimal total weight; stable ascending indices.
- Clear input validation (rejects zero/negative weights, negative values, oversized n/W, and overflows).
- Small, dependency-light C17 core with straightforward CLI and tests; easy to embed.

Weaknesses:
- DP space is O(n * W); with W up to 100000 and n up to 100, memory is bounded but not suited for much larger capacities.
- Runtime scales linearly with W; very large capacities are slow compared to heuristic/approximate schemes.
- Single-threaded; no built-in parallelization or SIMD tuning.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Notes: requires CMake ≥ 3.20; builds `knapsack` (library) and `knapsack_demo` (CLI).

## Demo

Input file format (two lines):

```
<capacity>
w1:v1 w2:v2 w3:v3
```

Weights/values are integers; pairs may be separated by spaces or commas. Example:

```
10
2:3 3:4 4:5 5:6
```

Run:

```bash
./build/knapsack_demo data/sample.txt
```

JSON mode:

```bash
./build/knapsack_demo --json data/sample.txt
```

Success emits `{"status":"ok","optimal_value":...,"selected_indices":[...]}`.
Errors emit JSON with `status:"error"` and `code`, otherwise human-readable text.

CLI enforces the same limits as the library (count<=100, capacity<=100000, weights>0, values>=0).

Malformed inputs: a capacity line with trailing tokens or non-numeric data is rejected with
"Failed to parse capacity"; malformed item tokens (missing colon/overflow) emit
"Failed to parse items".

## Tests

Build with testing enabled (default):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Test types:
- Unit (`unit_knapsack_tests`): validates solver edge cases and optimal reconstruction.
- Integration (`integration_knapsack_demo`): runs demo via `popen` on temp inputs (good and bad).
- System (`system_knapsack_demo_sample`): executes `knapsack_demo` on `data/sample.txt` and checks output.

## Tooling

- `format`: format all sources via CMake: `cmake --build build --target format` (requires `clang-format` in PATH). Re-run the configure step first if `build/` is missing: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.
- clang-tidy: auto-enabled when available; toggle with `-DENABLE_CLANG_TIDY=OFF`. Vendored gtest/gmock targets have clang-tidy disabled to avoid third-party noise.
- Doxygen: `cmake --build build --target docs` (requires `doxygen`).