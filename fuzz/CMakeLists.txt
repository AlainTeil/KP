add_executable(fuzz_parse fuzz_parse.c)
target_link_libraries(fuzz_parse PRIVATE knapsack_cli)

# libFuzzer flags. Project-wide sanitizers may also be enabled separately.
target_compile_options(fuzz_parse PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
target_link_options(fuzz_parse PRIVATE -fsanitize=fuzzer,address,undefined)

# Make a corpus directory next to the binary.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
