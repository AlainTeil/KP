cmake_minimum_required(VERSION 3.20)

project(KnapsackC
  VERSION 0.2.0
  DESCRIPTION "Exact 0/1 knapsack solver in C with CLI demo and tests"
  LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(KnapsackWarnings)
include(GNUInstallDirs)

# --- Options -----------------------------------------------------------------

option(ENABLE_CLANG_TIDY  "Enable clang-tidy if available"            ON)
option(ENABLE_DOXYGEN     "Enable Doxygen documentation target"       ON)
option(ENABLE_SANITIZERS  "Build first-party targets with ASan+UBSan" OFF)
option(ENABLE_COVERAGE    "Build first-party targets with --coverage" OFF)
option(WARNINGS_AS_ERRORS "Promote warnings to errors (CI use)"       OFF)
option(BUILD_BENCHMARKS   "Build google/benchmark micro-benchmarks"   OFF)
option(ENABLE_FUZZING     "Build libFuzzer harnesses (Clang only)"    OFF)

include(CTest)  # Defines BUILD_TESTING and calls enable_testing().

# --- Tooling -----------------------------------------------------------------

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_C_CLANG_TIDY   "${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

# --- Library: knapsack -------------------------------------------------------

add_library(knapsack
  src/knapsack.c
)
add_library(Knapsack::knapsack ALIAS knapsack)
target_include_directories(knapsack
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(knapsack PUBLIC c_std_17)
set_target_properties(knapsack PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
knapsack_target_hardening(knapsack)

# --- Static library: knapsack_cli (parser + formatter) -----------------------

add_library(knapsack_cli STATIC
  src/cli/parse.c
  src/cli/format.c
)
target_include_directories(knapsack_cli
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
)
target_link_libraries(knapsack_cli PUBLIC knapsack)
target_compile_features(knapsack_cli PUBLIC c_std_17)
knapsack_target_hardening(knapsack_cli)

# --- Executable: knapsack_demo ----------------------------------------------

add_executable(knapsack_demo src/main.c)
target_link_libraries(knapsack_demo PRIVATE knapsack_cli)
knapsack_target_hardening(knapsack_demo)

# --- Tests -------------------------------------------------------------------

if(BUILD_TESTING)
  include(FetchContent)
  set(BUILD_GMOCK   ON  CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)

  # Silence and untidy third-party targets.
  foreach(_t IN ITEMS gtest gmock gtest_main gmock_main)
    if(TARGET ${_t})
      set_target_properties(${_t} PROPERTIES
        C_CLANG_TIDY ""
        CXX_CLANG_TIDY ""
      )
      target_compile_options(${_t} PRIVATE -Wno-everything)
    endif()
  endforeach()

  # Library unit tests.
  add_executable(knapsack_tests tests/knapsack_test.cpp)
  target_link_libraries(knapsack_tests PRIVATE knapsack GTest::gtest GTest::gtest_main GTest::gmock)
  target_include_directories(knapsack_tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gmock_SOURCE_DIR}/include
  )
  target_compile_features(knapsack_tests PRIVATE cxx_std_17)
  knapsack_target_hardening(knapsack_tests)
  add_test(NAME unit_knapsack_tests COMMAND knapsack_tests)

  # CLI parser/formatter unit tests.
  add_executable(knapsack_cli_tests tests/knapsack_cli_test.cpp)
  target_link_libraries(knapsack_cli_tests PRIVATE knapsack_cli GTest::gtest GTest::gtest_main)
  target_include_directories(knapsack_cli_tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
  )
  target_compile_features(knapsack_cli_tests PRIVATE cxx_std_17)
  knapsack_target_hardening(knapsack_cli_tests)
  add_test(NAME unit_knapsack_cli_tests COMMAND knapsack_cli_tests)

  # End-to-end / integration tests.
  add_executable(knapsack_integration_tests tests/knapsack_integration_test.cpp)
  target_link_libraries(knapsack_integration_tests PRIVATE GTest::gtest GTest::gtest_main)
  target_include_directories(knapsack_integration_tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
  )
  target_compile_features(knapsack_integration_tests PRIVATE cxx_std_17)
  target_compile_definitions(knapsack_integration_tests PRIVATE
    KNAPSACK_DEMO_PATH="$<TARGET_FILE:knapsack_demo>"
  )
  add_dependencies(knapsack_integration_tests knapsack_demo)
  knapsack_target_hardening(knapsack_integration_tests)
  add_test(NAME integration_knapsack_demo COMMAND knapsack_integration_tests)

  add_test(NAME system_knapsack_demo_sample
           COMMAND $<TARGET_FILE:knapsack_demo> ${CMAKE_SOURCE_DIR}/data/sample.txt)
  set_tests_properties(system_knapsack_demo_sample PROPERTIES
    PASS_REGULAR_EXPRESSION "Optimal value: 13")
endif()

# --- Benchmarks --------------------------------------------------------------

if(BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

# --- Fuzzing -----------------------------------------------------------------

if(ENABLE_FUZZING)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "ENABLE_FUZZING requires the Clang C compiler.")
  endif()
  add_subdirectory(fuzz)
endif()

# --- Install / package config ------------------------------------------------

install(TARGETS knapsack
  EXPORT KnapsackTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/knapsack
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT KnapsackTargets
  FILE KnapsackTargets.cmake
  NAMESPACE Knapsack::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Knapsack
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/KnapsackConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/KnapsackConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Knapsack
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/KnapsackConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/KnapsackConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/KnapsackConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Knapsack
)

# --- clang-format target -----------------------------------------------------

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE ALL_FORMAT_SOURCES CONFIGURE_DEPENDS
    src/*.c src/**/*.c src/**/*.h include/**/*.h tests/*.cpp bench/*.cpp fuzz/*.c
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format"
  )
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ALL_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Verifying clang-format compliance"
  )
endif()

# --- Doxygen -----------------------------------------------------------------

if(ENABLE_DOXYGEN)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/build)
    add_custom_target(docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )
  endif()
endif()
