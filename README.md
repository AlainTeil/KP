# Knapsack C Library

Simple C17 library for solving the 0/1 knapsack problem with a demo CLI and GTest/GMock tests.

Complexity: `knapsack_solve` uses the standard DP table, O(items * capacity) time and space; extremely large capacities that would overflow internal buffers are rejected.

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