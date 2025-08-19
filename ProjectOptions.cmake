include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(sail_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(sail_setup_options)
  option(sail_ENABLE_HARDENING "Enable hardening" ON)
  option(sail_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    sail_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    sail_ENABLE_HARDENING
    OFF)

  sail_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR sail_PACKAGING_MAINTAINER_MODE)
    option(sail_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(sail_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(sail_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(sail_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(sail_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(sail_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(sail_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(sail_ENABLE_PCH "Enable precompiled headers" OFF)
    option(sail_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(sail_ENABLE_IPO "Enable IPO/LTO" ON)
    option(sail_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(sail_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(sail_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(sail_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(sail_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(sail_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(sail_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(sail_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(sail_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(sail_ENABLE_PCH "Enable precompiled headers" OFF)
    option(sail_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      sail_ENABLE_IPO
      sail_WARNINGS_AS_ERRORS
      sail_ENABLE_USER_LINKER
      sail_ENABLE_SANITIZER_ADDRESS
      sail_ENABLE_SANITIZER_LEAK
      sail_ENABLE_SANITIZER_UNDEFINED
      sail_ENABLE_SANITIZER_THREAD
      sail_ENABLE_SANITIZER_MEMORY
      sail_ENABLE_UNITY_BUILD
      sail_ENABLE_CLANG_TIDY
      sail_ENABLE_CPPCHECK
      sail_ENABLE_COVERAGE
      sail_ENABLE_PCH
      sail_ENABLE_CACHE)
  endif()

  sail_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (sail_ENABLE_SANITIZER_ADDRESS OR sail_ENABLE_SANITIZER_THREAD OR sail_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(sail_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(sail_global_options)
  if(sail_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    sail_enable_ipo()
  endif()

  sail_supports_sanitizers()

  if(sail_ENABLE_HARDENING AND sail_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR sail_ENABLE_SANITIZER_UNDEFINED
       OR sail_ENABLE_SANITIZER_ADDRESS
       OR sail_ENABLE_SANITIZER_THREAD
       OR sail_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${sail_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${sail_ENABLE_SANITIZER_UNDEFINED}")
    sail_enable_hardening(sail_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(sail_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(sail_warnings INTERFACE)
  add_library(sail_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  sail_set_project_warnings(
    sail_warnings
    ${sail_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(sail_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    sail_configure_linker(sail_options)
  endif()

  include(cmake/Sanitizers.cmake)
  sail_enable_sanitizers(
    sail_options
    ${sail_ENABLE_SANITIZER_ADDRESS}
    ${sail_ENABLE_SANITIZER_LEAK}
    ${sail_ENABLE_SANITIZER_UNDEFINED}
    ${sail_ENABLE_SANITIZER_THREAD}
    ${sail_ENABLE_SANITIZER_MEMORY})

  set_target_properties(sail_options PROPERTIES UNITY_BUILD ${sail_ENABLE_UNITY_BUILD})

  if(sail_ENABLE_PCH)
    target_precompile_headers(
      sail_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(sail_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    sail_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(sail_ENABLE_CLANG_TIDY)
    sail_enable_clang_tidy(sail_options ${sail_WARNINGS_AS_ERRORS})
  endif()

  if(sail_ENABLE_CPPCHECK)
    sail_enable_cppcheck(${sail_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(sail_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    sail_enable_coverage(sail_options)
  endif()

  if(sail_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(sail_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(sail_ENABLE_HARDENING AND NOT sail_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR sail_ENABLE_SANITIZER_UNDEFINED
       OR sail_ENABLE_SANITIZER_ADDRESS
       OR sail_ENABLE_SANITIZER_THREAD
       OR sail_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    sail_enable_hardening(sail_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
