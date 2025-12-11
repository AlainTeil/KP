cmake_minimum_required(VERSION 3.20)

project(KnapsackC LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy if available" ON)
option(ENABLE_DOXYGEN "Enable Doxygen documentation target" ON)

# Configure clang-tidy
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

# Silence third-party warnings from gtest/gmock only.
set(GTEST_QUIET_FLAGS "-Wno-everything")

add_library(knapsack
  src/knapsack.c
)
target_include_directories(knapsack PUBLIC include)
target_compile_features(knapsack PUBLIC c_std_17)

add_executable(knapsack_demo src/main.c)
target_link_libraries(knapsack_demo PRIVATE knapsack)

if(BUILD_TESTING)
  include(FetchContent)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)

  # Apply quiet flags to third-party targets only.
  foreach(third_party_target IN ITEMS gtest gmock gtest_main gmock_main)
    if(TARGET ${third_party_target})
      # Disable clang-tidy on third-party targets to avoid noisy diagnostics.
      set_target_properties(${third_party_target} PROPERTIES
        C_CLANG_TIDY ""
        CXX_CLANG_TIDY ""
      )
      target_compile_options(${third_party_target} PRIVATE ${GTEST_QUIET_FLAGS})
    endif()
  endforeach()

  enable_testing()
  add_executable(knapsack_tests tests/knapsack_test.cpp)
  target_link_libraries(knapsack_tests PRIVATE knapsack GTest::gtest GTest::gtest_main GTest::gmock)
  target_include_directories(knapsack_tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gmock_SOURCE_DIR}/include
  )
  target_compile_features(knapsack_tests PRIVATE cxx_std_17)
  add_test(NAME unit_knapsack_tests COMMAND knapsack_tests)

  add_executable(knapsack_integration_tests tests/knapsack_integration_test.cpp)
  target_link_libraries(knapsack_integration_tests PRIVATE knapsack GTest::gtest GTest::gtest_main)
  target_include_directories(knapsack_integration_tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
  )
  target_compile_features(knapsack_integration_tests PRIVATE cxx_std_17)
  target_compile_definitions(knapsack_integration_tests PRIVATE KNAPSACK_DEMO_PATH="$<TARGET_FILE:knapsack_demo>")
  add_dependencies(knapsack_integration_tests knapsack_demo)
  add_test(NAME integration_knapsack_demo COMMAND knapsack_integration_tests)

  add_test(NAME system_knapsack_demo_sample
           COMMAND $<TARGET_FILE:knapsack_demo> ${CMAKE_SOURCE_DIR}/data/sample.txt)
  set_tests_properties(system_knapsack_demo_sample PROPERTIES
    PASS_REGULAR_EXPRESSION "Optimal value: 13")
endif()

# clang-format helper target
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE ALL_CXX_SOURCE_FILES CONFIGURE_DEPENDS
    src/*.c include/**/*.h tests/*.cpp
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format"
  )
endif()

# Doxygen documentation
if(ENABLE_DOXYGEN)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
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