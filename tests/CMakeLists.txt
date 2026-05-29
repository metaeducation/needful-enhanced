# CMakeLists.txt for needful-enhanced tests
#
# Covers both positive tests (compile + run, must exit 0) and negative tests
# (must FAIL to compile -- enforced via CTest's WILL_FAIL property).
#
# By convention, needful.h lives one directory above needful-enhanced/.
# From tests/, that means ../.. (tests/ -> needful-enhanced/ -> parent).
#
# Usage:
#   cd needful-enhanced/tests
#   cmake -B build && cmake --build build && ctest --test-dir build -V
#
# To point at a needful.h that is not in the default location:
#   cmake -B build -DNEEDUL_H_DIR=/path/to/dir/containing/needful.h
#

cmake_minimum_required(VERSION 3.14)
project(needful_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NEEDFUL_H_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    CACHE PATH "Path to directory containing needful.h")

if(NOT EXISTS "${NEEDFUL_H_DIR}/needful.h")
    message(FATAL_ERROR
        "needful.h not found at '${NEEDFUL_H_DIR}/needful.h'.\n"
        "Set -DNEEDUL_H_DIR=<path> to the directory containing needful.h."
    )
endif()

message(STATUS "Using needful.h from: ${NEEDFUL_H_DIR}")


#=[ DOCTEST EXTRACTION ]=======================================================
#
# Extract positive and negative tests embedded in docs/*.md into
# tests/doctest-extracted/{positive,negative}/.  The extractor runs at
# CMake configure time so the generated files are present before compilation.
#
# To skip extraction (e.g. when iterating on test code only), pass:
#   cmake -B build -DNEEDUL_EXTRACT_DOCTESTS=OFF

option(NEEDFUL_EXTRACT_DOCTESTS "Extract tests embedded in docs/*.md" ON)

if(NEEDFUL_EXTRACT_DOCTESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        set(EXTRACTOR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/extract-doctests.py")
        set(DOCS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../docs")
        set(DOCTEST_OUT "${CMAKE_CURRENT_SOURCE_DIR}/doctest-extracted")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" "${EXTRACTOR}"
                    --docs-dir "${DOCS_DIR}"
                    --out-dir  "${DOCTEST_OUT}"
            RESULT_VARIABLE _extract_result
            OUTPUT_VARIABLE _extract_output
            ERROR_VARIABLE  _extract_error
        )
        if(NOT _extract_result EQUAL 0)
            message(WARNING
                "extract-doctests.py failed (exit ${_extract_result}):\n"
                "${_extract_error}"
            )
        else()
            message(STATUS "Doctest extraction:\n${_extract_output}")
        endif()
    else()
        message(STATUS "Python3 not found; skipping doctest extraction")
    endif()
endif()

enable_testing()


#=[ INSTALLATION SMOKE TESTS ]===============================================
#
# These verify the intended consumer layout in a temporary directory:
#
# * standalone header-only use without needful-enhanced/
# * matching needful.h + needful-enhanced/
# * checked build requested but companion directory missing
# * checked build with an intentional version mismatch

set(NEEDFUL_SMOKE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/run-install-smoke.cmake")
set(NEEDFUL_ENHANCED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

function(add_needful_smoke_test test_name smoke_mode)
    add_test(
        NAME "${test_name}"
        COMMAND
            "${CMAKE_COMMAND}"
            -DSMOKE_MODE=${smoke_mode}
            -DNEEDFUL_H_DIR=${NEEDFUL_H_DIR}
            -DNEEDFUL_ENHANCED_DIR=${NEEDFUL_ENHANCED_DIR}
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DMSVC_COMPILER=$<BOOL:${MSVC}>
            -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P "${NEEDFUL_SMOKE_SCRIPT}"
    )
endfunction()

add_needful_smoke_test("smoke-standalone-header-only" "standalone")
add_needful_smoke_test("smoke-enhanced-matching" "matching")
add_needful_smoke_test("smoke-enhanced-missing" "missing")
add_needful_smoke_test("smoke-enhanced-mismatched-version" "mismatched")


#=[ POSITIVE TESTS ]==========================================================
#
# Compiled as normal targets; the resulting binary is run by CTest and must
# exit 0.

set(POSITIVE_TESTS
    test-needful-casts
    test-needful-const
    test-needful-option
    test-needful-need
    test-needful-result
    test-needful-known
    test-needful-comments
    test-needful-contra
    test-needful-nocast
)

foreach(test_name ${POSITIVE_TESTS})
    add_executable(${test_name} positive/${test_name}.cpp)
    target_compile_definitions(${test_name} PRIVATE NEEDFUL_CPP_ENHANCED=1)
    target_include_directories(${test_name} PRIVATE "${NEEDFUL_H_DIR}")
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()


#=[ NEGATIVE TESTS ]===========================================================
#
# Each file in negative/ must FAIL to compile.  We invoke the compiler
# directly as a CTest command and mark the test WILL_FAIL so CTest considers
# a non-zero exit (i.e. compile error) a pass.
#
# MSVC uses /Fe: for the output file; everyone else uses -o.  We write the
# (never-reached) output into the binary dir so parallel runs don't collide.

if(MSVC)
    set(NEEDFUL_CPP_STD_FLAG "/std:c++14")  # MSVC has no /std:c++11; c++14 is minimum explicit
    set(NEEDFUL_INCLUDE_FLAG "/I")
    set(NEEDFUL_DEFINE_FLAG  "/DNEEDUL_CPP_ENHANCEMENTS=1")
    function(needful_output_flag test_name out_var)
        set(${out_var}
            "/Fe:${CMAKE_CURRENT_BINARY_DIR}/neg-${test_name}.exe"
            PARENT_SCOPE)
    endfunction()
else()
    set(NEEDFUL_CPP_STD_FLAG "-std=c++11")
    set(NEEDFUL_INCLUDE_FLAG "-I")
    set(NEEDFUL_DEFINE_FLAG  "-DNEEDUL_CPP_ENHANCEMENTS=1")
    function(needful_output_flag test_name out_var)
        set(${out_var}
            "-o" "${CMAKE_CURRENT_BINARY_DIR}/neg-${test_name}"
            PARENT_SCOPE)
    endfunction()
endif()

set(NEGATIVE_TESTS
    # All negative tests live in docs/*.md as tagged fenced blocks:
    #
    #   <!-- doctest: negative-test -->
    #   ```cpp
    #   ...
    #   ```
    #
    # They are extracted at configure time by extract-doctests.py and
    # registered below via file(GLOB ...).  To add a new negative test,
    # add a tagged block to the appropriate docs page rather than creating
    # a standalone file here.
)

foreach(test_name ${NEGATIVE_TESTS})
    needful_output_flag(${test_name} output_flag)
    add_test(
        NAME "negative-${test_name}"
        COMMAND
            "${CMAKE_CXX_COMPILER}"
            "${NEEDFUL_CPP_STD_FLAG}"
            "${NEEDFUL_DEFINE_FLAG}"
            "${NEEDFUL_INCLUDE_FLAG}${NEEDFUL_H_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/negative/${test_name}.cpp"
            ${output_flag}
    )
    set_tests_properties("negative-${test_name}" PROPERTIES WILL_FAIL TRUE)
endforeach()


#=[ DOCTEST-EXTRACTED POSITIVE TESTS ]========================================
#
# Pick up any positive tests extracted from docs/*.md.  The extraction runs
# at configure time above; the glob is evaluated at configure time too, so
# re-run cmake if docs change.

file(GLOB DOCTEST_POSITIVE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/doctest-extracted/positive/*.cpp"
)

foreach(src ${DOCTEST_POSITIVE_SOURCES})
    get_filename_component(test_name "${src}" NAME_WE)
    set(target_name "doctest-pos-${test_name}")
    add_executable(${target_name} "${src}")
    target_compile_definitions(${target_name} PRIVATE NEEDFUL_CPP_ENHANCED=1)
    target_include_directories(${target_name} PRIVATE "${NEEDFUL_H_DIR}")
    add_test(NAME "${target_name}" COMMAND ${target_name})
endforeach()


#=[ DOCTEST-EXTRACTED NEGATIVE TESTS ]========================================
#
# Negative doctests must fail to compile.  run-negative-tests.py handles the
# MATCH-ERROR-TEXT checking, so we don't duplicate that here -- but we do
# register WILL_FAIL tests in CTest so basic "did it fail?" is tracked.

file(GLOB DOCTEST_NEGATIVE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/doctest-extracted/negative/*.cpp"
)

foreach(src ${DOCTEST_NEGATIVE_SOURCES})
    get_filename_component(test_name "${src}" NAME_WE)
    needful_output_flag("dt-${test_name}" output_flag)
    add_test(
        NAME "negative-doctest-${test_name}"
        COMMAND
            "${CMAKE_CXX_COMPILER}"
            "${NEEDFUL_CPP_STD_FLAG}"
            "${NEEDFUL_DEFINE_FLAG}"
            "${NEEDFUL_INCLUDE_FLAG}${NEEDFUL_H_DIR}"
            "${src}"
            ${output_flag}
    )
    set_tests_properties("negative-doctest-${test_name}" PROPERTIES WILL_FAIL TRUE)
endforeach()
