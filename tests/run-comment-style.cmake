# run-comment-style.cmake
#
# needful.h is C89-punctuation-clean on purpose: `//` appears only *inside*
# block comments (in prose, URLs, and example code), never as the comment
# introducer for the header's own code.  Actual comments use slash-star.
#
# This is an aesthetic commitment to the "old school C" register the file is
# written in, and it is exactly the kind of rule that is easy to break by
# accident -- a `// like this` slipped into a macro definition looks fine and
# compiles fine everywhere Needful is actually built.  Nothing would notice.
#
# So: notice.
#
# Usage:  cmake -DNEEDFUL_H_DIR=<dir> -P run-comment-style.cmake

cmake_minimum_required(VERSION 3.14)  # for if()/while() boolean policies

if(NOT DEFINED NEEDFUL_H_DIR)
    message(FATAL_ERROR "NEEDFUL_H_DIR must be set")
endif()

set(header "${NEEDFUL_H_DIR}/needful.h")
if(NOT EXISTS "${header}")
    message(FATAL_ERROR "needful.h not found at '${header}'")
endif()

# Reading a file as lines in CMake silently loses lines (backslash-continued
# #defines weld onto the next line), so see lint-read-lines.cmake.
#
include("${CMAKE_CURRENT_LIST_DIR}/lint-read-lines.cmake")
read_lines("${header}" lines)
list(LENGTH lines n_lines)

# Independent count, so a future regression in the reading above cannot make
# this lint quietly inspect a fraction of the file and still report success.
file(READ "${header}" raw)
string(REGEX MATCHALL "\n" newlines "${raw}")
list(LENGTH newlines n_newlines)
math(EXPR diff "${n_lines}-${n_newlines}")
if(diff GREATER 1 OR diff LESS -1)
    message(FATAL_ERROR
        "line-count mismatch reading needful.h: read_lines() produced "
        "${n_lines} lines but the file has ${n_newlines} newlines.  The "
        "scanner would be inspecting only part of the header."
    )
endif()

set(in_block FALSE)
set(lineno 0)
set(violations "")

set(idx 0)
while(idx LESS n_lines)
    list(GET lines ${idx} line)
    decode_line(line)
    math(EXPR idx "${idx}+1")
    math(EXPR lineno "${idx}")
    set(rest "${line}")

    while(TRUE)
        string(LENGTH "${rest}" rest_len)
        if(rest_len EQUAL 0)
            break()
        endif()

        if(in_block)
            string(FIND "${rest}" "*/" p)
            if(p LESS 0)
                set(rest "")          # comment continues to next line
                break()
            endif()
            math(EXPR after "${p}+2")
            if(after GREATER_EQUAL rest_len)
                set(rest "")
            else()
                string(SUBSTRING "${rest}" ${after} -1 rest)
            endif()
            set(in_block FALSE)
        else()
            string(FIND "${rest}" "/*" pb)
            string(FIND "${rest}" "//" pl)
            string(FIND "${rest}" "\"" pq)

            set(best -1)
            set(kind "")
            foreach(pair "${pb};blk" "${pl};line" "${pq};str")
                list(GET pair 0 pos)
                list(GET pair 1 k)
                if(NOT pos LESS 0)
                    if(best LESS 0 OR pos LESS best)
                        set(best ${pos})
                        set(kind "${k}")
                    endif()
                endif()
            endforeach()

            if(best LESS 0)
                set(rest "")
                break()
            endif()

            if(kind STREQUAL "line")
                string(STRIP "${line}" trimmed)
                string(APPEND violations "  line ${lineno}: ${trimmed}\n")
                set(rest "")
                break()
            elseif(kind STREQUAL "blk")
                math(EXPR after "${best}+2")
                if(after GREATER_EQUAL rest_len)
                    set(rest "")
                else()
                    string(SUBSTRING "${rest}" ${after} -1 rest)
                endif()
                set(in_block TRUE)
            else()   # string literal: skip past its closing quote
                math(EXPR after "${best}+1")
                if(after GREATER_EQUAL rest_len)
                    set(rest "")
                    break()
                endif()
                string(SUBSTRING "${rest}" ${after} -1 tail)
                string(FIND "${tail}" "\"" q2)
                if(q2 LESS 0)
                    set(rest "")
                    break()
                endif()
                math(EXPR after2 "${q2}+1")
                string(LENGTH "${tail}" tail_len)
                if(after2 GREATER_EQUAL tail_len)
                    set(rest "")
                else()
                    string(SUBSTRING "${tail}" ${after2} -1 rest)
                endif()
            endif()
        endif()
    endwhile()
endwhile()

if(violations)
    message(FATAL_ERROR  # violations is a string, not a list (lines hold `;`)
        "needful.h uses `//` outside of a block comment.\n"
        "The header's own comments are slash-star; `//` belongs only inside\n"
        "them (prose, URLs, example code).  Offending lines:\n\n"
        "${violations}"
    )
endif()

message(STATUS "comment style OK: no `//` outside block comments (${lineno} lines)")
