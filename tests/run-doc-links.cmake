# run-doc-links.cmake
#
# Resolves every documentation link against the docs/ tree, so that a renamed
# heading or a retired page turns the build red instead of silently becoming
# a 404 for a reader.
#
# needful.h leans on these links harder than most headers do: the whole reason
# it can stay short is that rationale lives at needful.metaeducation.com and
# the header just points there.  A dead pointer is a missing explanation.
#
# Three link shapes are checked:
#
#     https://needful.metaeducation.com/page#anchor    (needful.h, *.hpp, docs)
#     ](/page#anchor)                                  (docs only)
#     ](#anchor)                                       (docs only, same page)
#
# A page resolves if some docs/*.md declares it as its `permalink:`.  An anchor
# resolves if that page pins it explicitly with {#anchor}, or if a heading
# derives it the way kramdown would.  When those two disagree the fix is to
# pin the anchor -- deriving is what makes links quietly fragile.
#
# Usage:  cmake -DDOCS_DIR=<dir> -DSCAN_DIRS=<dir>[;<dir>...] -P run-doc-links.cmake

cmake_minimum_required(VERSION 3.14)

# Files are read with file(STRINGS ... ENCODING UTF-8).  The encoding is not
# optional: without it, the em dashes in this project's prose come back as
# semicolons, which CMake then reads as list separators -- welding a page's
# whole tail into one "line" and mangling the links inside it into nonsense
# targets.  That failure looks like broken links rather than a broken reader,
# which is a genuinely confusing hour.
#
if(NOT DEFINED DOCS_DIR)
    message(FATAL_ERROR "DOCS_DIR must be set")
endif()

file(GLOB doc_files "${DOCS_DIR}/*.md")
if(NOT doc_files)
    message(FATAL_ERROR "no .md files found under '${DOCS_DIR}'")
endif()

# Approximates kramdown's auto-generated heading id.
function(derive_anchor text out_var)
    string(REGEX REPLACE "{#[^}]*}" "" text "${text}")     # drop explicit tag
    string(REGEX REPLACE "[`*\\[\\]]" "" text "${text}")   # markdown noise
    string(TOLOWER "${text}" text)
    string(REGEX REPLACE "[^a-z0-9 -]" "" text "${text}")  # incl. UTF-8 dashes
    string(STRIP "${text}" text)
    string(REGEX REPLACE " +" "-" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

#=[ INDEX PAGES AND ANCHORS ]=================================================

set(known_pages "")
set(known_anchors "")   # entries look like  /result#result-none

foreach(f IN LISTS doc_files)
    file(STRINGS "${f}" body ENCODING UTF-8)   # UTF-8 matters: see below

    set(perm "")
    foreach(line IN LISTS body)
        if(line MATCHES "^permalink:[ \t]*(.+)$")
            set(perm "${CMAKE_MATCH_1}")
            string(STRIP "${perm}" perm)
            break()
        endif()
    endforeach()
    if(perm STREQUAL "")
        continue()
    endif()
    list(APPEND known_pages "${perm}")

    foreach(line IN LISTS body)
        if(line MATCHES "{#([A-Za-z0-9_-]+)}")
            list(APPEND known_anchors "${perm}#${CMAKE_MATCH_1}")
        endif()
        if(line MATCHES "^#+[ \t]+(.+)$")
            derive_anchor("${CMAKE_MATCH_1}" a)
            if(NOT a STREQUAL "")
                list(APPEND known_anchors "${perm}#${a}")
            endif()
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES known_pages)
list(REMOVE_DUPLICATES known_anchors)

#=[ CHECK ONE LINK ]==========================================================

set(problems "")

function(check_link target where)
    string(REGEX REPLACE "#.*$" "" page "${target}")
    set(anchor "")
    if(target MATCHES "#(.+)$")
        set(anchor "${CMAKE_MATCH_1}")
    endif()
    if(page STREQUAL "")
        message(FATAL_ERROR "check_link called with no page: ${target}")
    endif()

    if(NOT page IN_LIST known_pages)
        set(problems "${problems};dead page   ${page}  (linked from ${where})"
            PARENT_SCOPE)
        return()
    endif()
    if(NOT anchor STREQUAL "")
        if(NOT "${page}#${anchor}" IN_LIST known_anchors)
            set(problems
                "${problems};dead anchor ${page}#${anchor}  (linked from ${where})"
                PARENT_SCOPE)
        endif()
    endif()
endfunction()

#=[ SCAN ]====================================================================

set(scan_files "")
foreach(d IN LISTS SCAN_DIRS)
    file(GLOB more "${d}/*.h" "${d}/*.hpp" "${d}/*.md")
    list(APPEND scan_files ${more})
endforeach()
list(APPEND scan_files ${doc_files})
list(REMOVE_DUPLICATES scan_files)

foreach(f IN LISTS scan_files)
    get_filename_component(base "${f}" NAME)
    file(STRINGS "${f}" body ENCODING UTF-8)   # UTF-8 matters: see below

    get_filename_component(ext "${f}" EXT)
    set(self_perm "")
    if(ext STREQUAL ".md")
        foreach(line IN LISTS body)
            if(line MATCHES "^permalink:[ \t]*(.+)$")
                set(self_perm "${CMAKE_MATCH_1}")
                string(STRIP "${self_perm}" self_perm)
                break()
            endif()
        endforeach()
    endif()

    foreach(line IN LISTS body)
        # absolute site URLs (may appear more than once per line)
        string(REGEX MATCHALL "https://needful\\.metaeducation\\.com(/[A-Za-z0-9_#/-]*)?"
               hits "${line}")
        foreach(h IN LISTS hits)
            string(REPLACE "https://needful.metaeducation.com" "" t "${h}")
            if(t STREQUAL "")
                set(t "/")
            endif()
            check_link("${t}" "${base}")
        endforeach()

        if(ext STREQUAL ".md")
            # Strip inline code spans first.  Jekyll does not render a link
            # inside backticks either, so `](/page#anchor)` written *about*
            # link syntax is illustration, not a live link.  (This lint's own
            # documentation is the first thing that hits this.)
            string(REGEX REPLACE "`[^`]*`" "" md_line "${line}")

            string(REGEX MATCHALL "\\]\\(/[A-Za-z0-9_#/-]*\\)" hits "${md_line}")
            foreach(h IN LISTS hits)
                string(REGEX REPLACE "^\\]\\(" "" t "${h}")
                string(REGEX REPLACE "\\)$" "" t "${t}")
                check_link("${t}" "${base}")
            endforeach()

            if(NOT self_perm STREQUAL "")
                string(REGEX MATCHALL "\\]\\(#[A-Za-z0-9_-]+\\)" hits "${md_line}")
                foreach(h IN LISTS hits)
                    string(REGEX REPLACE "^\\]\\(" "" t "${h}")
                    string(REGEX REPLACE "\\)$" "" t "${t}")
                    check_link("${self_perm}${t}" "${base}")
                endforeach()
            endif()
        endif()
    endforeach()
endforeach()

list(REMOVE_ITEM problems "")
list(REMOVE_DUPLICATES problems)

list(LENGTH known_pages n_pages)
list(LENGTH known_anchors n_anchors)

if(problems)
    string(REPLACE ";" "\n  " pretty "${problems}")
    message(FATAL_ERROR
        "Documentation links do not resolve.\n"
        "Pin the target with an explicit {#anchor} and link to that.\n\n"
        "  ${pretty}\n"
    )
endif()

message(STATUS
    "doc links OK (${n_pages} pages, ${n_anchors} anchors known)")
