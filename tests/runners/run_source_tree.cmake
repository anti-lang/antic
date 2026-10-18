# docs/notes/source-tree.md holds one row per file of the root, of src/,
# of tests/unit/, of tools/ and of tests/runners/, per file directly
# under tests/ and per .cmake script of tests/, and one row per directory
# directly under tests/. A row is a line of a table whose first cell is
# the path in backquotes, a directory ending in /, and whose second cell
# gives its purpose. The check refuses a file or a directory without a
# row, a path with two rows and a row that names nothing of the tree.
#
#   cmake -DROOT=<repository> [-DGIT=<git>] -P tests/runners/run_source_tree.cmake
#
# DESIGN: the paths come from git ls-files, as in run_repo_layout.cmake.
# The suite of ./r runs in an export of the commit, which holds the
# tracked files and no .git, so a tree that is no git work tree is read
# from the disk instead, without build/.

cmake_minimum_required(VERSION 3.21)

set(document "docs/notes/source-tree.md")

if(NOT GIT)
    find_program(GIT git)
endif()
set(listed 1)
if(GIT)
    execute_process(COMMAND "${GIT}" -C "${ROOT}" rev-parse --show-toplevel
                    OUTPUT_VARIABLE top OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE not_git ERROR_QUIET ENCODING NONE)
    get_filename_component(root_real "${ROOT}" REALPATH)
    if(NOT not_git AND NOT top STREQUAL "")
        get_filename_component(top_real "${top}" REALPATH)
    endif()
    if(NOT not_git AND top_real STREQUAL root_real)
        execute_process(COMMAND "${GIT}" -C "${ROOT}" -c core.quotepath=off ls-files
                        OUTPUT_VARIABLE output RESULT_VARIABLE listed
                        ENCODING NONE)
    endif()
endif()
if(listed EQUAL 0)
    string(REPLACE "\n" ";" paths "${output}")
else()
    file(GLOB_RECURSE paths LIST_DIRECTORIES false RELATIVE "${ROOT}"
         "${ROOT}/*" "${ROOT}/.*")
    list(FILTER paths EXCLUDE REGEX "^(build|\\.git)/")
endif()

# The paths the document covers, and the directories of tests/.
set(covered "")
set(directories "")
foreach(path IN LISTS paths)
    if(path MATCHES "^[^/]+$" OR path MATCHES "^(src|tools|tests/unit|tests/runners)/"
       OR path MATCHES "^tests/[^/]+$" OR path MATCHES "^tests/.*\\.cmake$")
        list(APPEND covered "${path}")
    endif()
    if(path MATCHES "^(tests/[^/]+/)")
        list(APPEND directories "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES directories)
list(APPEND covered ${directories})
list(LENGTH covered count)
if(count EQUAL 0)
    message(FATAL_ERROR "no path to check in ${ROOT}")
endif()
if(NOT EXISTS "${ROOT}/${document}")
    message(FATAL_ERROR "${document} does not exist")
endif()

# The first cell of each row, and the first character of its purpose. A
# row whose purpose is empty is no row.
file(READ "${ROOT}/${document}" text)
string(REGEX MATCHALL "\n\\| `[^`\n]+` \\| [^ |\n]" cells "\n${text}")
set(rows "")
set(failures "")
foreach(cell IN LISTS cells)
    string(REGEX REPLACE "^\n\\| `([^`\n]+)`.*" "\\1" row "${cell}")
    if(row IN_LIST rows)
        string(APPEND failures "\n${row}: two rows")
    endif()
    list(APPEND rows "${row}")
    if(NOT row IN_LIST covered)
        string(APPEND failures "\n${row}: a row of nothing the document covers")
    endif()
endforeach()
foreach(path IN LISTS covered)
    if(NOT path IN_LIST rows)
        string(APPEND failures "\n${path}: no row")
    endif()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${document} differs from the tree:${failures}")
endif()
