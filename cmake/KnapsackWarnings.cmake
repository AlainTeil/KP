# Apply the project's strict warning set to a target. When WARNINGS_AS_ERRORS
# is ON (typically only in CI), warnings become errors.
function(knapsack_apply_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "knapsack_apply_warnings: '${target}' is not a target")
  endif()

  set(_c_warnings
    -Wall -Wextra -Wpedantic
    -Wconversion -Wshadow -Wstrict-prototypes
    -Wcast-align -Wcast-qual -Wnull-dereference
    -Wdouble-promotion -Wformat=2 -Wundef -Wfloat-equal
    -Wmissing-prototypes -Wmissing-declarations -Wwrite-strings
  )
  set(_cxx_warnings
    -Wall -Wextra -Wpedantic
    -Wconversion -Wshadow
    -Wcast-align -Wcast-qual -Wnull-dereference
    -Wdouble-promotion -Wformat=2 -Wundef
    -Wwrite-strings
  )

  if(WARNINGS_AS_ERRORS)
    list(APPEND _c_warnings -Werror)
    list(APPEND _cxx_warnings -Werror)
  endif()

  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${_c_warnings}>
    $<$<COMPILE_LANGUAGE:CXX>:${_cxx_warnings}>
  )
endfunction()

# Apply sanitizer flags (compile + link) when ENABLE_SANITIZERS is ON.
function(knapsack_apply_sanitizers target)
  if(NOT ENABLE_SANITIZERS)
    return()
  endif()
  set(_san_flags -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
  target_compile_options(${target} PRIVATE ${_san_flags})
  target_link_options(${target} PRIVATE ${_san_flags})
endfunction()

# Apply coverage flags when ENABLE_COVERAGE is ON.
function(knapsack_apply_coverage target)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()
  target_compile_options(${target} PRIVATE --coverage -O0 -g)
  target_link_options(${target} PRIVATE --coverage)
endfunction()

# Convenience: warnings + sanitizers + coverage in one call.
function(knapsack_target_hardening target)
  knapsack_apply_warnings(${target})
  knapsack_apply_sanitizers(${target})
  knapsack_apply_coverage(${target})
endfunction()
