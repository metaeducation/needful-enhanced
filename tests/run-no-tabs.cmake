# run-no-tabs.cmake
#
# Indentation is spaces, everywhere, in every file type.
#
# Tabs are the definitive invisible convention: they render differently for
# every reader, survive review unnoticed, and arrive by accident from an editor
# setting rather than by intent.  A single tab in a header whose whole selling
# point is "read it and see there is no magic" is a small but real tax on the
# reader it is trying to win over.
#
# Usage:  cmake -DROOTS=<dir>[;<dir>...] -P run-no-tabs.cmake

cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED ROOTS)
    message(FATAL_ERROR "ROOTS must be set")
endif()

set(files "")
foreach(d IN LISTS ROOTS)          # each entry may be a directory or one file
    if(IS_DIRECTORY "${d}")
        file(GLOB_RECURSE more
            "${d}/*.h" "${d}/*.hpp" "${d}/*.c" "${d}/*.cpp"
            "${d}/*.md" "${d}/*.cmake" "${d}/*.py" "${d}/*.yml"
            "${d}/CMakeLists.txt"
        )
        list(APPEND files ${more})
    elseif(EXISTS "${d}")
        list(APPEND files "${d}")
    else()
        message(FATAL_ERROR "ROOTS entry does not exist: ${d}")
    endif()
endforeach()
list(REMOVE_DUPLICATES files)

set(offenders "")
foreach(f IN LISTS files)
    # Generated and build trees are not ours to police.
    if(f MATCHES "/(build|build-verify|doctest-extracted)/")
        continue()
    endif()
    file(READ "${f}" body)
    if(body MATCHES "\t")
        file(RELATIVE_PATH rel "${CMAKE_CURRENT_LIST_DIR}" "${f}")
        list(APPEND offenders "${rel}")
    endif()
endforeach()

list(LENGTH files n_files)

if(offenders)
    string(REPLACE ";" "\n  " pretty "${offenders}")
    message(FATAL_ERROR
        "Tab characters found.  Indentation is spaces everywhere.\n\n"
        "  ${pretty}\n"
    )
endif()

message(STATUS "no tabs OK (${n_files} files scanned)")
