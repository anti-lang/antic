# `anti build` of a project whose manifest has no [package] or a
# [package] without `name`, which Q3 and Q6 of "Answers of 2026-11-01" in
# docs/decisions.md allow for a project that requires no packages. Such a
# project is the package of its root module. The project is
# tests/anti-build/internal, whose modules share an internal item, copied
# into a directory whose name no module path holds. Run with cmake -P and
# these values:
#   ANTI     the anti executable
#   CASE     the case, one of those below
#   RUNTIME  the runtime archive
#   FIXTURE  the directory that holds internal/
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#
# program: an empty manifest. The package is the module that holds main,
# com.example.app. The program of both modes takes the name of the project
# directory, internal, and returns the internal item of com.example.core,
# so the modules share their package. The notice names com.example.app
# last.
#
# named: [package] holds `program` alone, which names the program of both
# modes tree.
#
# version: [package] holds `version` alone. The program is named after
# the directory, and the notice inside it names the version, which anti
# passes to antic as --package-version.
#
# license: [package] holds `license` and `license_text` alone, and the
# notice names the licence.
#
# library: without app.anti the project is a library project. Its root
# module is the last of the order, com.example.near, which the notice of
# `anti license --project` names last. The build writes both library files
# and no warning of a missing root module.
#
# directory_space and directory_device: the project directory is `my
# tree`, or `con`, a name of a device on Windows. Neither is a file name
# on every target, so the build of an empty manifest is refused with the
# message the case holds and writes no program.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(directory internal)
if(CASE STREQUAL "directory_space")
    set(directory "my tree")
elseif(CASE STREQUAL "directory_device")
    set(directory con)
endif()
set(project "${WORK}/${directory}")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/internal/" DESTINATION "${project}")
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

# Fail unless the last package line of notice is `package <line>`.
function(expect_last_package what notice line)
    string(REGEX MATCHALL "(^|\n)package [^\n]*" packages "${notice}")
    list(GET packages -1 last)
    string(STRIP "${last}" last)
    if(NOT last STREQUAL "package ${line}")
        message(FATAL_ERROR "the last package of ${what} is `${last}`, and "
                            "not `package ${line}`\n${notice}")
    endif()
endfunction()

# Build the project in dev and release mode. Each program is
# dist/<host>/<mode>/<name>, ends with 41, and the last package line of its
# notice is `package <line>`.
function(expect_programs name line)
    foreach(mode dev release)
        set(flag "")
        if(mode STREQUAL "release")
            set(flag --release)
        endif()
        anti_quiet("anti build ${flag}" build ${flag} --runtime "${RUNTIME}")
        set(program "${project}/dist/${HOST}/${mode}/${name}${exe}")
        program_expect("${mode}" COMMAND "${program}" STATUS 41)
        anti_quiet("anti license --from" license --from "${program}"
                   --runtime "${RUNTIME}")
        expect_last_package("the program of ${mode}" "${out}" "${line}")
        file(GLOB written RELATIVE "${project}/dist/${HOST}/${mode}"
             "${project}/dist/${HOST}/${mode}/*")
        list(REMOVE_ITEM written "${name}${exe}" NOTICE.txt
             "${name}-symbols.zip")
        if(NOT written STREQUAL "")
            message(FATAL_ERROR "dist/${HOST}/${mode} holds ${written} "
                                "beside the program ${name}${exe}")
        endif()
    endforeach()
endfunction()

if(CASE STREQUAL "program")
    expect_programs(internal "com.example.app 0.0.0")
elseif(CASE STREQUAL "named")
    write_bytes("${project}/anti.toml" "[package]\nprogram = \"tree\"\n")
    expect_programs(tree "com.example.app 0.0.0")
elseif(CASE STREQUAL "version")
    write_bytes("${project}/anti.toml" "[package]\nversion = \"1.2.3\"\n")
    expect_programs(internal "com.example.app 1.2.3")
elseif(CASE STREQUAL "license")
    write_bytes("${project}/anti.toml"
                "[package]\nlicense = \"MIT\"\nlicense_text = \"LICENSE\"\n")
    write_bytes("${project}/LICENSE" "The licence of the project.\n")
    expect_programs(internal "com.example.app 0.0.0 MIT")
elseif(CASE MATCHES "^directory_")
    anti_run(status text err "${project}" build --runtime "${RUNTIME}")
    set(expected "anti: the project directory `${directory}` is no file name on every target, and `program` of [package] names the program")
    if(NOT status EQUAL 1 OR NOT err STREQUAL "${expected}\n")
        message(FATAL_ERROR "the build in ${directory} ended with ${status}, "
                            "and must be refused with\n${expected}\n"
                            "It wrote\n${text}${err}")
    endif()
    if(EXISTS "${project}/dist")
        message(FATAL_ERROR "the refused build wrote ${project}/dist")
    endif()
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
    expect_last_package("the library project" "${out}"
                        "com.example.near 0.0.0")
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
