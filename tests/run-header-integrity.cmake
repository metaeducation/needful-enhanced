# run-header-integrity.cmake
#
# Verifies that needful.h is intact text before anything tries to compile it.
#
# Usage (standalone):
#   cmake -DNEEDFUL_H_DIR=/path/to/dir -P run-header-integrity.cmake
#
# WHY THIS EXISTS
#
# needful.h is authored in one place and hand-copied to the repository that
# publishes it.  A copy across a VirtualBox shared folder silently produced a
# file of exactly the right length whose tail was NUL padding: the destination
# size was set from the new file, but the bytes written were the old content,
# and the remainder was zero-filled.
#
#     old content: 41,255 bytes  +  NUL padding: 1,352  =  42,607  (correct!)
#
# Because the length matched, nothing downstream noticed.  git committed it,
# GitHub served it, and CI turned it into several hundred lines of
# "warning: null character ignored [-Wnull-character]" from a compiler that
# was otherwise happy to keep going.  A truncation like this can also silently
# delete a trailing #endif and leave the file compiling but wrong.
#
# A NUL byte is never valid in a C header, so this is a cheap and unambiguous
# canary for "the transport mangled the file."  It is written in CMake rather
# than Python so it runs everywhere CTest does, with no interpreter needed.

if(NOT DEFINED NEEDFUL_H_DIR)
    message(FATAL_ERROR "run-header-integrity.cmake requires -DNEEDFUL_H_DIR=<dir>")
endif()

set(HEADER "${NEEDFUL_H_DIR}/needful.h")

if(NOT EXISTS "${HEADER}")
    message(FATAL_ERROR "needful.h not found at '${HEADER}'")
endif()

file(SIZE "${HEADER}" HEADER_SIZE)
file(READ "${HEADER}" HEADER_HEX HEX)

# Space out the hex stream one byte per group, so a search for a NUL cannot
# match across a byte boundary.  (Searching the raw hex for "00" reports a
# false positive on any 0x?0 followed by 0x0? -- which includes the extremely
# common "space then newline", 0x20 0x0a -> "200a".)
#
# This is deliberately string work rather than list work: turning 42 KB into a
# 42,000-element CMake list is slow, and any diagnostic that interpolates such
# a list prints the entire file.
#
string(REGEX REPLACE "(..)" " \\1" SPACED_HEX "${HEADER_HEX}")
string(FIND "${SPACED_HEX}" " 00" NUL_AT)

if(NOT NUL_AT EQUAL -1)
    math(EXPR NUL_INDEX "${NUL_AT} / 3")  # 3 chars per byte: space + 2 digits

    # Measure the run, so the report can say whether this is tail padding or
    # an isolated speck.
    string(SUBSTRING "${SPACED_HEX}" ${NUL_AT} -1 TAIL_HEX)
    string(REGEX MATCH "^( 00)+" NUL_RUN "${TAIL_HEX}")
    string(LENGTH "${NUL_RUN}" NUL_RUN_CHARS)
    math(EXPR RUN_LENGTH "${NUL_RUN_CHARS} / 3")

    math(EXPR RUN_END "${NUL_INDEX} + ${RUN_LENGTH}")
    if(RUN_END EQUAL HEADER_SIZE)
        set(SHAPE "The run reaches end-of-file, so this is NUL padding: the destination size was set correctly but the tail was never written.")
    else()
        set(SHAPE "The run is in the middle of the file.")
    endif()

    message(FATAL_ERROR
        "needful.h contains ${RUN_LENGTH} NUL byte(s) starting at offset "
        "${NUL_INDEX} (file is ${HEADER_SIZE} bytes).\n"
        "  ${SHAPE}\n"
        "  A NUL byte is never valid in a C header.  This is almost always a "
        "damaged copy rather than a damaged source -- check the file where it "
        "was authored, re-copy it, and confirm before committing with:\n"
        "      tr -d '\\0' < needful.h | cmp -s - needful.h && echo CLEAN\n"
        "  Note the byte count will not tell you: the known failure produced a "
        "file of exactly the right length.\n"
        "  Path checked: ${HEADER}"
    )
endif()

# A header whose last line lacks a newline is legal C99 but is also what a
# truncated copy looks like, so it is worth a word.
#
string(LENGTH "${HEADER_HEX}" HEX_LENGTH)
math(EXPR LAST_BYTE_AT "${HEX_LENGTH} - 2")
string(SUBSTRING "${HEADER_HEX}" ${LAST_BYTE_AT} 2 LAST_BYTE)
if(NOT LAST_BYTE STREQUAL "0a")
    message(WARNING
        "needful.h does not end with a newline (last byte is 0x${LAST_BYTE}). "
        "Harmless on its own, but it is also what a truncated copy looks like."
    )
endif()

message(STATUS "needful.h integrity OK (${HEADER_SIZE} bytes, no NUL bytes)")
