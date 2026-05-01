# Knapsack C Library

C17 library for the exact 0/1 knapsack problem, with a CLI demo and a GoogleTest test suite.

## Limitations

- Items: `1 <= count <= KNAPSACK_MAX_ITEMS` (=100). Each `weight > 0`, each `value >= 0`.
- Capacity: `0 <= W <= KNAPSACK_MAX_CAPACITY` (=100000).
- Numeric domain: `int` weights and values. The solver detects sum-of-values overflow and reports
  `KNAPSACK_ERR_INT_OVERFLOW` rather than wrapping.
- Determinism: tie-break on smallest total weight, then ascending indices.
- Platform: tested on Linux with gcc 13 and clang 18.

## Performance

- Time and memory: `O(n * W)` where `n = count` and `W = capacity`.
- At the maximum `n=100`, `W=100000` the DP performs `1e7` updates; on a modern x86-64 desktop a
  single solve completes in roughly 15–20 ms. See `bench/` for measured numbers.
- Memory layout: two rolling rows of `int` (values) plus two rolling rows of `size_t` (weights for
  tiebreaking) plus a packed bitset of "take" decisions — total `O(n*W)` bits for reconstruction.

## Build

```bash
cmake --preset default
cmake --build --preset default
```

Or without presets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Requires CMake ≥ 3.20 (≥ 3.23 to use presets). Produces:
- `build/libknapsack.a` — library.
- `build/libknapsack_cli.a` — internal CLI helpers.
- `build/knapsack_demo` — CLI executable.

### CMake options

| Option              | Default | Effect                                                        |
| ------------------- | ------- | ------------------------------------------------------------- |
| `BUILD_TESTING`     | ON      | Build and register GoogleTest suites (set by CTest module).   |
| `ENABLE_CLANG_TIDY` | ON      | Use `clang-tidy` if available.                                |
| `ENABLE_DOXYGEN`    | ON      | Define a `docs` target if Doxygen is available.               |
| `ENABLE_SANITIZERS` | OFF     | Build first-party targets with `-fsanitize=address,undefined`.|
| `ENABLE_COVERAGE`   | OFF     | Build first-party targets with `--coverage`.                  |
| `WARNINGS_AS_ERRORS`| OFF     | Promote compiler warnings to errors (used in CI).             |
| `BUILD_BENCHMARKS`  | OFF     | Build `bench/bench_knapsack` using google/benchmark.          |
| `ENABLE_FUZZING`    | OFF     | Build `fuzz/fuzz_parse` (Clang only, libFuzzer + ASan/UBSan). |

### CMake presets

`CMakePresets.json` defines ready-to-use configurations. Each preset writes to its
own build directory (e.g. `build/`, `build-asan/`, `build-fuzz/`).

| Preset     | Build dir       | Purpose                                            | Has tests |
| ---------- | --------------- | -------------------------------------------------- | --------- |
| `default`  | `build/`        | Release + `-Werror`, tests on.                     | yes       |
| `strict`   | `build-strict/` | RelWithDebInfo + `-Werror`.                        | yes       |
| `tidy`     | `build-tidy/`   | Release + `-Werror` + clang-tidy on the build.     | yes       |
| `asan`     | `build-asan/`   | Debug + ASan+UBSan.                                | yes       |
| `coverage` | `build-cov/`    | Debug + `--coverage` (use with `gcovr`).           | yes       |
| `bench`    | `build-bench/`  | Release + benchmarks (`BUILD_TESTING=OFF`).        | no        |
| `fuzz`     | `build-fuzz/`   | Clang + libFuzzer harness (`BUILD_TESTING=OFF`).   | no        |

For test-bearing presets (`default`, `strict`, `tidy`, `asan`, `coverage`):

```bash
cmake --preset <name>          # configure
cmake --build --preset <name>  # build
ctest  --preset <name>         # run tests
```

The `bench` and `fuzz` presets disable `BUILD_TESTING`, so they have no `ctest` step.
Run their binaries directly:

```bash
# Benchmarks
cmake --preset bench && cmake --build --preset bench
./build-bench/bench/bench_knapsack                    # full suite
./build-bench/bench/bench_knapsack --help             # all options

# Fuzzer (Clang only)
cmake --preset fuzz && cmake --build --preset fuzz
./build-fuzz/fuzz/fuzz_parse build-fuzz/fuzz/corpus               # run forever
./build-fuzz/fuzz/fuzz_parse -max_total_time=60 build-fuzz/fuzz/corpus  # 60-second smoke run
```

## Demo

Input file format (two lines):

```
<capacity>
w1:v1 w2:v2 w3:v3
```

Pairs may be separated by spaces, tabs, or commas. Example:

```
10
2:3 3:4 4:5 5:6
```

Run:

```bash
./build/knapsack_demo data/sample.txt
./build/knapsack_demo --json data/sample.txt
./build/knapsack_demo --help
./build/knapsack_demo --version
```

### Exit codes

| Exit | Meaning                                                       |
| ---- | ------------------------------------------------------------- |
| 0    | Success.                                                      |
| 1    | Any failure (bad arguments, missing file, parse error, solve error). |

In text mode, errors are written to `stderr`. In `--json` mode, error responses are written to
`stdout` (so a tool can read a structured response from a single stream).

### JSON schema

Success:

```json
{ "status": "ok", "optimal_value": <int>, "selected_indices": [<size_t>, ...] }
```

Error:

```json
{ "status": "error", "code": "<STATUS_NAME>", "message": "<text>" }
```

`<STATUS_NAME>` is one of `NULL_RESULT`, `INVALID_ITEMS`, `TOO_MANY_ITEMS`,
`INVALID_CAPACITY`, `DIMENSION_OVERFLOW`, `INT_OVERFLOW`, `ALLOC`. Strings in `message` are
JSON-escaped.

## API

```c
#include "knapsack/knapsack.h"

knapsack_item_t items[] = {{2, 3}, {3, 4}, {4, 8}};
knapsack_result_t result;
if (knapsack_solve_status(items, 3, 10, &result) == KNAPSACK_OK) {
    /* use result.optimal_value, result.selected_indices, result.selected_count */
    knapsack_result_free(&result);
}
```

### Custom allocator

`knapsack_solve_status_ex` accepts a `knapsack_allocator_t` so callers can route the solver's
allocations through their own arena, pool, or instrumentation:

```c
knapsack_allocator_t alloc = { my_alloc, my_calloc, my_free, user_data };
knapsack_solve_status_ex(items, n, W, &alloc, &result);
knapsack_result_free_ex(&result, &alloc);
```

Passing `NULL` falls back to `malloc` / `calloc` / `free`.

## Tests

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Suites:
- `unit_knapsack_tests` — solver edge cases, tie-breaks, allocator failures, brute-force property
  test on small instances.
- `unit_knapsack_cli_tests` — direct unit tests for the CLI parser/formatter (capacity, items,
  buffer parsing, JSON quoting).
- `integration_knapsack_demo` — spawns `knapsack_demo` via `posix_spawn` and asserts on its
  stdout/stderr/exit code (text mode, `--help`, `--version`, error paths). For `--json` output it
  parses the response with an embedded minimal JSON parser and asserts on the resulting schema
  (status, code, message, optimal_value, selected_indices).
- `system_knapsack_demo_sample` — runs the binary against `data/sample.txt`.

Sanitizer build:

```bash
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```

Coverage build (uses `gcovr`, which handles gcc 13's intermediate gcov
format correctly; lcov 2.x silently mis-counts on this toolchain):

```bash
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage
gcovr -r . --filter src/ build-cov                       # text summary
gcovr -r . --filter src/ build-cov --html-details cov.html  # HTML report
```

Install `gcovr` once via `pipx install gcovr` or in a throw-away venv
(`python3 -m venv .venv && .venv/bin/pip install gcovr`).

If you must use lcov, invoke `gcov` directly per source file
(`gcov -b -c CMakeFiles/knapsack.dir/src/knapsack.c.gcno` from `build-cov/`)
and aggregate manually — `lcov --capture` on this toolchain produces
nonsensical percentages (>100% on template-heavy headers, 0% functions
on first-party code) due to a known bug in lcov 2.0 with the gcc 13
intermediate format.

## Benchmarks

```bash
cmake --preset bench && cmake --build --preset bench
./build-bench/bench/bench_knapsack
```

The suite exercises four input patterns — `Dense` (most items fit), `Sparse` (few items fit),
`TooHeavy` (every item heavier than `W`), and `ExactFit` (uniform weights dividing `W`) — across
four `(n, W)` size points each, and reports `dp_cells` and `solve_failures` counters in addition
to wall time.

## Fuzzing

Clang-only:

```bash
cmake --preset fuzz && cmake --build --preset fuzz
./build-fuzz/fuzz/fuzz_parse build-fuzz/fuzz/corpus
```

The seed corpus under `fuzz/corpus/` covers nominal and adversarial inputs: zero/oversized
capacity, comma/space/tab separators, mixed whitespace, single-too-heavy item, zero-weight item,
overflowing weight, non-numeric tokens, and a many-item case.

## Tooling

- `cmake --build build --target format`       — apply clang-format in place.
- `cmake --build build --target format-check` — fail if any file would be reformatted.
- clang-tidy is run during the build when `ENABLE_CLANG_TIDY=ON` and `clang-tidy` is on `PATH`.
- `cmake --build build --target docs`         — generate Doxygen API documentation
  (requires reconfiguring with `-DENABLE_DOXYGEN=ON`, since presets disable it by default;
  e.g. `cmake -B build-docs -DENABLE_DOXYGEN=ON && cmake --build build-docs --target docs`).
- Pre-commit hooks (clang-format + cppcheck + whitespace checks):
  ```bash
  pip install pre-commit
  pre-commit install
  pre-commit run --all-files
  ```

## Continuous integration

`.github/workflows/ci.yml` is driven entirely by `CMakePresets.json` and runs:
- a build/test matrix `{gcc-13, clang-18} × {default, strict}` (`Release` and `RelWithDebInfo`,
  both with `-Werror`),
- a clang ASan+UBSan job using the `asan` preset,
- `clang-format --dry-run --Werror` via the `format-check` target,
- `clang-tidy` via the `tidy` preset build,
- `cppcheck --enable=warning,style,performance,portability --error-exitcode=1`,
- coverage with `gcovr` via the `coverage` preset, uploaded as a build artefact,
- a 60-second libFuzzer smoke run via the `fuzz` preset that uploads any crash artefacts on
  failure.
