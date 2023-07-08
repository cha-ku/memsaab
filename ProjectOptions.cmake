include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(memsaab_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
    set(SUPPORTS_UBSAN ON)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    set(SUPPORTS_ASAN ON)
  endif()
endmacro()

macro(memsaab_setup_options)
  option(memsaab_ENABLE_HARDENING "Enable hardening" ON)
  option(memsaab_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    memsaab_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    memsaab_ENABLE_HARDENING
    OFF)

  memsaab_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR memsaab_PACKAGING_MAINTAINER_MODE)
    option(memsaab_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(memsaab_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(memsaab_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(memsaab_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(memsaab_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(memsaab_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(memsaab_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(memsaab_ENABLE_PCH "Enable precompiled headers" OFF)
    option(memsaab_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(memsaab_ENABLE_IPO "Enable IPO/LTO" ON)
    option(memsaab_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(memsaab_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(memsaab_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(memsaab_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(memsaab_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(memsaab_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(memsaab_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(memsaab_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(memsaab_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(memsaab_ENABLE_PCH "Enable precompiled headers" OFF)
    option(memsaab_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      memsaab_ENABLE_IPO
      memsaab_WARNINGS_AS_ERRORS
      memsaab_ENABLE_USER_LINKER
      memsaab_ENABLE_SANITIZER_ADDRESS
      memsaab_ENABLE_SANITIZER_LEAK
      memsaab_ENABLE_SANITIZER_UNDEFINED
      memsaab_ENABLE_SANITIZER_THREAD
      memsaab_ENABLE_SANITIZER_MEMORY
      memsaab_ENABLE_UNITY_BUILD
      memsaab_ENABLE_CLANG_TIDY
      memsaab_ENABLE_CPPCHECK
      memsaab_ENABLE_COVERAGE
      memsaab_ENABLE_PCH
      memsaab_ENABLE_CACHE)
  endif()

  memsaab_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (memsaab_ENABLE_SANITIZER_ADDRESS OR memsaab_ENABLE_SANITIZER_THREAD OR memsaab_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(memsaab_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(memsaab_global_options)
  if(memsaab_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    memsaab_enable_ipo()
  endif()

  memsaab_supports_sanitizers()

  if(memsaab_ENABLE_HARDENING AND memsaab_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR memsaab_ENABLE_SANITIZER_UNDEFINED
       OR memsaab_ENABLE_SANITIZER_ADDRESS
       OR memsaab_ENABLE_SANITIZER_THREAD
       OR memsaab_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${memsaab_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${memsaab_ENABLE_SANITIZER_UNDEFINED}")
    memsaab_enable_hardening(memsaab_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(memsaab_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(memsaab_warnings INTERFACE)
  add_library(memsaab_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  memsaab_set_project_warnings(
    memsaab_warnings
    ${memsaab_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(memsaab_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    configure_linker(memsaab_options)
  endif()

  include(cmake/Sanitizers.cmake)
  memsaab_enable_sanitizers(
    memsaab_options
    ${memsaab_ENABLE_SANITIZER_ADDRESS}
    ${memsaab_ENABLE_SANITIZER_LEAK}
    ${memsaab_ENABLE_SANITIZER_UNDEFINED}
    ${memsaab_ENABLE_SANITIZER_THREAD}
    ${memsaab_ENABLE_SANITIZER_MEMORY})

  set_target_properties(memsaab_options PROPERTIES UNITY_BUILD ${memsaab_ENABLE_UNITY_BUILD})

  if(memsaab_ENABLE_PCH)
    target_precompile_headers(
      memsaab_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(memsaab_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    memsaab_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(memsaab_ENABLE_CLANG_TIDY)
    memsaab_enable_clang_tidy(memsaab_options ${memsaab_WARNINGS_AS_ERRORS})
  endif()

  if(memsaab_ENABLE_CPPCHECK)
    memsaab_enable_cppcheck(${memsaab_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(memsaab_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    memsaab_enable_coverage(memsaab_options)
  endif()

  if(memsaab_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(memsaab_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(memsaab_ENABLE_HARDENING AND NOT memsaab_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR memsaab_ENABLE_SANITIZER_UNDEFINED
       OR memsaab_ENABLE_SANITIZER_ADDRESS
       OR memsaab_ENABLE_SANITIZER_THREAD
       OR memsaab_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    memsaab_enable_hardening(memsaab_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
