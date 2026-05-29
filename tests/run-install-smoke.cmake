cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED SMOKE_MODE)
    message(FATAL_ERROR "SMOKE_MODE is required")
endif()

if(NOT DEFINED NEEDFUL_H_DIR)
    message(FATAL_ERROR "NEEDFUL_H_DIR is required")
endif()

if(NOT DEFINED NEEDFUL_ENHANCED_DIR)
    message(FATAL_ERROR "NEEDFUL_ENHANCED_DIR is required")
endif()

if(NOT DEFINED CXX_COMPILER)
    message(FATAL_ERROR "CXX_COMPILER is required")
endif()

if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "BINARY_DIR is required")
endif()

set(WORK_DIR "${BINARY_DIR}/smoke/${SMOKE_MODE}")
set(INCLUDE_DIR "${WORK_DIR}/include")
set(ENHANCED_INCLUDE_DIR "${INCLUDE_DIR}/needful-enhanced")
set(SOURCE_FILE "${WORK_DIR}/smoke.cpp")

if(MSVC_COMPILER)
    set(EXE_FILE "${WORK_DIR}/smoke.exe")
    set(STD_FLAG "/std:c++14")
    set(INCLUDE_FLAG "/I${INCLUDE_DIR}")
    set(COMPILE_COMMAND
        "${CXX_COMPILER}" "/nologo" "${STD_FLAG}" "${INCLUDE_FLAG}" "${SOURCE_FILE}"
        "/Fe:${EXE_FILE}"
    )
else()
    set(EXE_FILE "${WORK_DIR}/smoke")
    set(STD_FLAG "-std=c++11")
    set(INCLUDE_FLAG "-I${INCLUDE_DIR}")
    set(COMPILE_COMMAND
        "${CXX_COMPILER}" "${STD_FLAG}" "${INCLUDE_FLAG}" "${SOURCE_FILE}"
        "-o" "${EXE_FILE}"
    )
endif()

set(EXPECT_COMPILE_SUCCESS FALSE)
set(EXPECT_RUN_SUCCESS FALSE)
set(EXPECT_ERROR_TEXT "")
set(ENABLE_ENHANCED FALSE)

if(SMOKE_MODE STREQUAL "standalone")
    set(EXPECT_COMPILE_SUCCESS TRUE)
    set(EXPECT_RUN_SUCCESS TRUE)
elseif(SMOKE_MODE STREQUAL "matching")
    set(EXPECT_COMPILE_SUCCESS TRUE)
    set(EXPECT_RUN_SUCCESS TRUE)
    set(ENABLE_ENHANCED TRUE)
elseif(SMOKE_MODE STREQUAL "missing")
    set(ENABLE_ENHANCED TRUE)
    set(EXPECT_ERROR_TEXT "needful-enhanced/cplusplus-needfuls.hpp")
elseif(SMOKE_MODE STREQUAL "mismatched")
    set(ENABLE_ENHANCED TRUE)
    set(EXPECT_ERROR_TEXT "mismatched compatibility versions")
else()
    message(FATAL_ERROR "Unknown SMOKE_MODE: ${SMOKE_MODE}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${INCLUDE_DIR}")

file(COPY "${NEEDFUL_H_DIR}/needful.h" DESTINATION "${INCLUDE_DIR}")

if(NOT SMOKE_MODE STREQUAL "standalone" AND NOT SMOKE_MODE STREQUAL "missing")
    file(MAKE_DIRECTORY "${ENHANCED_INCLUDE_DIR}")

    file(GLOB ENHANCED_HEADER_FILES
        "${NEEDFUL_ENHANCED_DIR}/*.hpp"
    )

    foreach(header_file ${ENHANCED_HEADER_FILES})
        file(COPY "${header_file}" DESTINATION "${ENHANCED_INCLUDE_DIR}")
    endforeach()
endif()

if(SMOKE_MODE STREQUAL "mismatched")
    set(MISMATCH_FILE "${INCLUDE_DIR}/needful-enhanced/cplusplus-needfuls.hpp")
    file(READ "${MISMATCH_FILE}" MISMATCH_TEXT)
    string(REPLACE
        "#define NEEDFUL_ENHANCED_EXPECTED_VERSION_PATCH  0"
        "#define NEEDFUL_ENHANCED_EXPECTED_VERSION_PATCH  1"
        MISMATCH_TEXT
        "${MISMATCH_TEXT}"
    )
    file(WRITE "${MISMATCH_FILE}" "${MISMATCH_TEXT}")
endif()

set(SOURCE_TEXT "")
if(ENABLE_ENHANCED)
    string(APPEND SOURCE_TEXT "#define NEEDFUL_CPP_ENHANCED  1\n")
endif()
string(APPEND SOURCE_TEXT
    "#include \"needful.h\"\n"
    "int main() {\n"
    "    Option(int) value = 42;\n"
    "    return value == 42 ? 0 : 1;\n"
    "}\n"
)
file(WRITE "${SOURCE_FILE}" "${SOURCE_TEXT}")

execute_process(
    COMMAND ${COMPILE_COMMAND}
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE COMPILE_RESULT
    OUTPUT_VARIABLE COMPILE_STDOUT
    ERROR_VARIABLE COMPILE_STDERR
)

set(COMPILE_OUTPUT "${COMPILE_STDOUT}${COMPILE_STDERR}")

if(EXPECT_COMPILE_SUCCESS)
    if(NOT COMPILE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Smoke test '${SMOKE_MODE}' failed to compile:\n${COMPILE_OUTPUT}"
        )
    endif()

    if(EXPECT_RUN_SUCCESS)
        execute_process(
            COMMAND "${EXE_FILE}"
            WORKING_DIRECTORY "${WORK_DIR}"
            RESULT_VARIABLE RUN_RESULT
            OUTPUT_VARIABLE RUN_STDOUT
            ERROR_VARIABLE RUN_STDERR
        )
        if(NOT RUN_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Smoke test '${SMOKE_MODE}' compiled but failed at runtime:\n"
                "stdout:\n${RUN_STDOUT}\n"
                "stderr:\n${RUN_STDERR}"
            )
        endif()
    endif()
else()
    if(COMPILE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Smoke test '${SMOKE_MODE}' compiled successfully when failure was expected"
        )
    endif()

    if(EXPECT_ERROR_TEXT AND COMPILE_OUTPUT MATCHES "${EXPECT_ERROR_TEXT}")
        return()
    elseif(EXPECT_ERROR_TEXT)
        message(FATAL_ERROR
            "Smoke test '${SMOKE_MODE}' failed, but not for the expected reason.\n"
            "Expected to find: ${EXPECT_ERROR_TEXT}\n"
            "Actual output:\n${COMPILE_OUTPUT}"
        )
    endif()
endif()
