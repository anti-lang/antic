# The layout rule of "Repository layout" in CLAUDE.md. Every tracked path
# stands in an allowed entry of the top level, and every directory directly
# under src/, tests/ and docs/ is one of the lists below. Nothing under
# build/ is tracked. Adding to any list is Eddie's decision, and the rule
# in CLAUDE.md names the same entries.
#
#   cmake -DROOT=<repository> [-DGIT=<git>] -P tests/run_repo_layout.cmake
#
# DESIGN: the paths come from git ls-files. The suite of ./r runs in an
# export of the commit, which holds the tracked files and no .git, so a
# tree that is no git work tree is read from the disk instead, without
# build/.

set(top_directories build docs LICENSES src tests tools)
set(top_files CLAUDE.md README.md CHANGELOG.md LICENSE CMakeLists.txt
    CMakePresets.json .gitignore)
# The files that tools require in the root, and the release link r. A
# tracked .claude/settings.json is one, when there is one.
set(tool_entries .github .gitattributes .editorconfig .claude/settings.json r)
set(src_directories antic anti rt std native)
set(tests_directories abi anti-build anti-symbols anti-test bind check checks
    clib conf doc dump emit-identity errors fmt framework inject link-identity
    modules opt plugin programs raw std trace traps unit)
set(docs_directories notes reports site)

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
list(LENGTH paths count)
if(count EQUAL 0)
    message(FATAL_ERROR "no path to check in ${ROOT}")
endif()

set(wrong "")
foreach(path IN LISTS paths)
    if(path STREQUAL "")
        continue()
    endif()
    string(REGEX MATCH "^[^/]+" top "${path}")
    set(allowed OFF)
    if(path MATCHES "/" AND top IN_LIST top_directories)
        set(allowed ON)
    elseif(NOT path MATCHES "/" AND top IN_LIST top_files)
        set(allowed ON)
    endif()
    foreach(entry IN LISTS tool_entries)
        string(REPLACE "." "\\." entry_pattern "${entry}")
        if(path MATCHES "^${entry_pattern}(/|$)")
            set(allowed ON)
        endif()
    endforeach()
    if(top STREQUAL "build")
        list(APPEND wrong "${path}: nothing under build/ is committed")
    elseif(NOT allowed)
        list(APPEND wrong "${path}: ${top} is no entry of the top level")
    elseif(path MATCHES "^(src|tests|docs)/([^/]+)/")
        set(kind "${CMAKE_MATCH_1}")
        set(directory "${CMAKE_MATCH_2}")
        if(NOT directory IN_LIST ${kind}_directories)
            list(APPEND wrong "${path}: ${kind}/${directory}/ is no directory of ${kind}/")
        endif()
    endif()
endforeach()

list(REMOVE_DUPLICATES wrong)
if(wrong)
    list(JOIN wrong "\n" wrong)
    message(FATAL_ERROR "paths outside the repository layout of CLAUDE.md:\n"
                        "${wrong}\nA new entry is Eddie's decision.")
endif()
message(STATUS "${count} paths follow the repository layout")
