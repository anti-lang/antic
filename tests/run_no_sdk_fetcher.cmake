# The Windows sysroots come from mingw-w64, as the entry of the Windows
# targets under "Binary distribution" in docs/decisions.md says. The tool
# that fetched the CRT and SDK of Microsoft before, and the variable of the
# installers that accepted their licence, left with the step after `mingw`
# of docs/work-order-distribution.md. The test refuses the name of the tool
# in src/, tests/, tools/, docs/notes/, docs/site/, the documents directly
# under docs/ and CLAUDE.md, in either case, and the variable in src/,
# tests/ and tools/. docs/decisions.md and docs/reports/ keep both, since
# they are history.
# Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

# DESIGN: both words are joined here from two halves, so this file passes
# its own check.
string(CONCAT tool "xw" "in")
string(CONCAT variable "ANTI_" "MICROSOFT")

# Every file under the directories of ROOT that `dirs` names, and the files
# of `files`, relative to ROOT.
function(tree_files out_variable dirs files)
    set(paths "")
    foreach(dir IN LISTS dirs)
        file(GLOB_RECURSE found LIST_DIRECTORIES false RELATIVE "${ROOT}"
             "${ROOT}/${dir}/*")
        list(APPEND paths ${found})
    endforeach()
    foreach(file IN LISTS files)
        if(EXISTS "${ROOT}/${file}")
            list(APPEND paths "${file}")
        endif()
    endforeach()
    set("${out_variable}" "${paths}" PARENT_SCOPE)
endfunction()

# The paths of `paths` whose text holds `word`. `fold` compares in lower
# case, as grep -i does.
function(files_with out_variable word fold paths)
    set(hits "")
    foreach(path IN LISTS paths)
        file(READ "${ROOT}/${path}" text)
        if(fold)
            string(TOLOWER "${text}" text)
        endif()
        string(FIND "${text}" "${word}" at)
        if(NOT at EQUAL -1)
            list(APPEND hits "${path}")
        endif()
    endforeach()
    set("${out_variable}" "${hits}" PARENT_SCOPE)
endfunction()

file(GLOB documents LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/docs/*.md")
list(REMOVE_ITEM documents docs/decisions.md)
list(LENGTH documents count)
if(count EQUAL 0)
    message(FATAL_ERROR "no document to check in ${ROOT}/docs")
endif()

tree_files(named "src;tests;tools;docs/notes;docs/site"
           "${documents};CLAUDE.md")
files_with(with_tool "${tool}" TRUE "${named}")
if(NOT with_tool STREQUAL "")
    list(JOIN with_tool "\n  " listed)
    message(FATAL_ERROR "the Windows sysroots come from mingw-w64, and "
                        "these files still name ${tool}:\n  ${listed}")
endif()

tree_files(code "src;tests;tools" "")
files_with(with_variable "${variable}" FALSE "${code}")
if(NOT with_variable STREQUAL "")
    list(JOIN with_variable "\n  " listed)
    message(FATAL_ERROR "no installer reads ${variable}, and these files "
                        "still name it:\n  ${listed}")
endif()
