include(FetchContent)
set(BENCHMARK_ENABLE_TESTING   OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL   OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  benchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(benchmark)

if(TARGET benchmark)
  set_target_properties(benchmark PROPERTIES C_CLANG_TIDY "" CXX_CLANG_TIDY "")
  target_compile_options(benchmark PRIVATE -w)
endif()
if(TARGET benchmark_main)
  set_target_properties(benchmark_main PROPERTIES C_CLANG_TIDY "" CXX_CLANG_TIDY "")
  target_compile_options(benchmark_main PRIVATE -w)
endif()

add_executable(bench_knapsack bench_knapsack.cpp)
target_link_libraries(bench_knapsack PRIVATE knapsack benchmark::benchmark benchmark::benchmark_main)
target_compile_features(bench_knapsack PRIVATE cxx_std_17)
