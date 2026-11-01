# `anti build` of a project whose manifest is empty, which Q3 of "Answers
# of 2026-11-01" in docs/decisions.md allows for a project that requires
# no packages. Such a project is the package of its root module. The
# project is tests/anti-build/internal, whose modules share an internal
# item, with an empty anti.toml. Run with cmake -P and these values:
#   ANTI     the anti executable
#   CASE     program or library
#   RUNTIME  the runtime archive
#   FIXTURE  the directory that holds internal/
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#
# program: the package is the module that holds main, com.example.app. The
# program of both modes is named app, as antic names it without -o, and
# returns the internal item of com.example.core, so the modules share
# their package. The notice names com.example.app last.
#
# library: without app.anti the project is a library project. Its root
# module is the last of the order, com.example.near, which the notice of
# `anti license --project` names last. The build writes both library files
# and no warning of a missing root module.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(project "${WORK}/internal")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/internal" DESTINATION "${WORK}")
write_bytes("${project}/anti.toml" "")

# Run anti in the project. It must succeed and write nothing to the
# standard error. Sets out to its standard output.
function(anti_quiet what)
    anti_run(status text err "${project}" ${ARGN})
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "${what} ended with ${status}\n${text}${err}")
    endif()
    set(out "${text}" PARENT_SCOPE)
endfunction()

# Fail unless the last package line of notice names the package.
function(expect_last_package what notice package)
    string(REGEX MATCHALL "(^|\n)package [^\n]*" packages "${notice}")
    list(GET packages -1 last)
    string(REPLACE "." "\\." pattern "${package}")
    if(NOT last MATCHES "^\n?package ${pattern} 0\\.0\\.0 ")
        message(FATAL_ERROR "the last package of ${what} is `${last}`\n"
                            "${notice}")
    endif()
endfunction()

if(CASE STREQUAL "program")
    foreach(mode dev release)
        set(flag "")
        if(mode STREQUAL "release")
            set(flag --release)
        endif()
        anti_quiet("anti build ${flag}" build ${flag} --runtime "${RUNTIME}")
        set(program "${project}/dist/${HOST}/${mode}/app${exe}")
        program_expect("${mode}" COMMAND "${program}" STATUS 41)
        anti_quiet("anti license --from" license --from "${program}"
                   --runtime "${RUNTIME}")
        expect_last_package("the program of ${mode}" "${out}"
                            com.example.app)
    endforeach()
elseif(CASE STREQUAL "library")
    file(REMOVE "${project}/src/com/example/app.anti")
    anti_quiet("anti build" build --runtime "${RUNTIME}")
    foreach(module core near)
        set(file "${project}/dist/${HOST}/dev/com/example/${module}.antl")
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "the library project wrote no ${file}")
        endif()
    endforeach()
    anti_quiet("anti license --project" license --project
               --runtime "${RUNTIME}")
    expect_last_package("the library project" "${out}" com.example.near)
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
