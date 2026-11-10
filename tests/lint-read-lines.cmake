# lint-read-lines.cmake
#
# Shared by the source lints: read a file as a CMake list of its lines,
# exactly one element per line, blank lines included.
#
# The obvious ways to do this both lose lines, silently:
#
# * A CMake list is a string with `;` separators, and a separator is not a
#   separator if it is escaped by a preceding backslash or sits inside an
#   unclosed `[`.  So every line ending in `\` -- every multi-line #define in
#   needful.h -- welds itself to the line after it, and one stray `[` in a
#   Markdown file can weld the whole rest of the page into one "line".
#   file(STRINGS) produces exactly such a list, so it has this problem.
#
# * Blank lines vanish from foreach(... IN LISTS ...) unless CMP0007 is NEW.
#
# So read_lines() swaps `\` `[` `]` `;` for control characters before
# splitting on newline, and each line must go through decode_line() after it
# is taken out of the list and before it is inspected.
#
# Usage:
#     include("${CMAKE_CURRENT_LIST_DIR}/lint-read-lines.cmake")
#     read_lines("${file}" lines)
#     ...
#     list(GET lines ${i} line)    (or: foreach(line IN LISTS lines))
#     decode_line(line)

cmake_policy(SET CMP0007 NEW)  # do not silently drop empty list elements

string(ASCII 1 _LINT_BACKSLASH)
string(ASCII 2 _LINT_LBRACKET)
string(ASCII 3 _LINT_RBRACKET)
string(ASCII 4 _LINT_SEMICOLON)

function(read_lines file out_var)
    file(READ "${file}" raw)
    string(REPLACE "\r" "" raw "${raw}")
    string(REPLACE "\\" "${_LINT_BACKSLASH}" raw "${raw}")
    string(REPLACE "[" "${_LINT_LBRACKET}" raw "${raw}")
    string(REPLACE "]" "${_LINT_RBRACKET}" raw "${raw}")
    string(REPLACE ";" "${_LINT_SEMICOLON}" raw "${raw}")
    string(REGEX REPLACE "\n$" "" raw "${raw}")  # no phantom last line
    string(REPLACE "\n" ";" raw "${raw}")
    set(${out_var} "${raw}" PARENT_SCOPE)
endfunction()

macro(decode_line var)
    string(REPLACE "${_LINT_BACKSLASH}" "\\" ${var} "${${var}}")
    string(REPLACE "${_LINT_LBRACKET}" "[" ${var} "${${var}}")
    string(REPLACE "${_LINT_RBRACKET}" "]" ${var} "${${var}}")
    string(REPLACE "${_LINT_SEMICOLON}" ";" ${var} "${${var}}")
endmacro()
