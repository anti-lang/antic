# The pre-commit hook of tools/hooks refuses a commit whose staged .md files
# fail the docs-style checker, and does nothing when no .md file is staged.
# The configure step points core.hooksPath of the checkout at the hook.
#
#   cmake -DROOT=<repository> -DHOOKS=<hooks directory, relative to ROOT>
#         -DWORK=<scratch directory> [-DGIT=<git>] [-DPYTHON=<python3>]
#         -P tests/run_pre_commit.cmake
#
# DESIGN: every commit happens in a scratch repository under WORK, whose
# core.hooksPath names the hook of ROOT. The global and system git
# configurations are cut off, so a signing key or a hooks path of the user
# does not change the result.

cmake_minimum_required(VERSION 3.21)

if(NOT GIT)
    find_program(GIT git)
endif()
if(NOT GIT)
    message(FATAL_ERROR "no git to run the hook with")
endif()
if(NOT PYTHON)
    find_program(PYTHON python3)
endif()
if(NOT PYTHON)
    message("SKIP: no python3 to run the docs-style checker with")
    return()
endif()

# The checkout itself. The suite of ./r runs in an export without .git,
# where there is no configuration to read.
execute_process(COMMAND "${GIT}" -C "${ROOT}" rev-parse --show-toplevel
                OUTPUT_VARIABLE top OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE not_git ERROR_QUIET
                ENCODING NONE)
if(NOT not_git)
    get_filename_component(top_real "${top}" REALPATH)
    get_filename_component(root_real "${ROOT}" REALPATH)
endif()
if(NOT not_git AND top_real STREQUAL root_real)
    execute_process(COMMAND "${GIT}" -C "${ROOT}" config core.hooksPath
                    OUTPUT_VARIABLE hooks_path OUTPUT_STRIP_TRAILING_WHITESPACE
                    ENCODING NONE)
    if(NOT hooks_path STREQUAL HOOKS)
        message(FATAL_ERROR "core.hooksPath of ${ROOT} is '${hooks_path}', "
                            "not ${HOOKS}. Run the configure step.")
    endif()
endif()

set(repo "${WORK}/repo")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${repo}")
file(WRITE "${WORK}/gitconfig" "")
set(git "${CMAKE_COMMAND}" -E env "GIT_CONFIG_GLOBAL=${WORK}/gitconfig"
    GIT_CONFIG_NOSYSTEM=1 "${GIT}" -C "${repo}")

function(git_ok)
    execute_process(COMMAND ${git} ${ARGN} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE out
                    ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} failed with ${status}:\n${out}")
    endif()
endfunction()

function(head_of out_var)
    execute_process(COMMAND ${git} rev-parse -q --verify HEAD
                    OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE
                    ENCODING NONE)
    set(${out_var} "${head}" PARENT_SCOPE)
endfunction()

# Commits with message WHAT and expects it to be made, or with REFUSED
# set, refused with a finding at each of the lines FINDINGS names and with
# the text SAYING in its output.
function(commit what)
    cmake_parse_arguments(PARSE_ARGV 1 arg "REFUSED;ALL" "SAYING" "FINDINGS")
    head_of(before)
    set(all "")
    if(arg_ALL)
        set(all -a)
    endif()
    execute_process(COMMAND ${git} commit -q ${all} -m "${what}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE out
                    ENCODING NONE)
    head_of(after)
    if(arg_REFUSED)
        if(status EQUAL 0 OR NOT before STREQUAL after)
            message(FATAL_ERROR "the hook let through: ${what}\n${out}")
        endif()
        foreach(finding IN LISTS arg_FINDINGS)
            string(FIND "${out}" "${finding}: error:" at)
            if(at EQUAL -1)
                message(FATAL_ERROR "refused without the finding ${finding}: "
                                    "${what}\n${out}")
            endif()
        endforeach()
        if(arg_SAYING)
            string(FIND "${out}" "${arg_SAYING}" at)
            if(at EQUAL -1)
                message(FATAL_ERROR "refused without '${arg_SAYING}': "
                                    "${what}\n${out}")
            endif()
        endif()
    elseif(NOT status EQUAL 0 OR before STREQUAL after)
        message(FATAL_ERROR "the hook refused: ${what}\n${out}")
    endif()
endfunction()

set(clean "The parser reads one token at a time.\n")
set(failing "The parser is robust.\n")

git_ok(init -q)
git_ok(config user.name "Hook Test")
git_ok(config user.email "hook@test.invalid")
git_ok(config commit.gpgsign false)
git_ok(config core.hooksPath "${ROOT}/${HOOKS}")

file(WRITE "${repo}/good.md" "${clean}")
git_ok(add good.md)
commit("a clean .md file")

file(WRITE "${repo}/docs/bad.md" "${clean}${failing}")
git_ok(add docs/bad.md)
commit("a failing .md file" REFUSED FINDINGS docs/bad.md:2)

# The staged text is checked, not the text in the work tree.
file(WRITE "${repo}/docs/bad.md" "${clean}")
commit("a failing .md file staged and fixed unstaged" REFUSED
       FINDINGS docs/bad.md:2)
git_ok(add docs/bad.md)
file(WRITE "${repo}/docs/bad.md" "${failing}")
commit("a clean .md file staged and broken unstaged")
git_ok(checkout -- docs/bad.md)

file(WRITE "${repo}/UPPER.MD" "${failing}")
file(WRITE "${repo}/with space.md" "${failing}")
git_ok(add UPPER.MD "with space.md")
commit("two failing .md files" REFUSED
       FINDINGS UPPER.MD:1 "with space.md:1")
git_ok(rm -q --cached UPPER.MD "with space.md")
file(REMOVE "${repo}/UPPER.MD" "${repo}/with space.md")

file(WRITE "${repo}/good.md" "${failing}")
commit("a failing tracked .md file under commit -a" REFUSED ALL
       FINDINGS good.md:1)
git_ok(checkout -- good.md)

git_ok(rm -q good.md)
commit("a deleted .md file")

# A hook whose checker is missing fails the moment it would run it, so a
# commit it lets through with this copy never ran the checker.
set(no_checker "${WORK}/no-checker/hooks")
file(COPY "${ROOT}/${HOOKS}/pre-commit" DESTINATION "${no_checker}")
git_ok(config core.hooksPath "${no_checker}")
file(WRITE "${repo}/notes.txt" "${failing}")
file(WRITE "${repo}/main.c" "/* ${failing} */\n")
git_ok(add notes.txt main.c)
commit("no .md file staged")
file(WRITE "${repo}/docs/bad.md" "${clean}${clean}")
git_ok(add docs/bad.md)
commit("a .md file with no checker to read it" REFUSED
       SAYING "no docs-style checker")
