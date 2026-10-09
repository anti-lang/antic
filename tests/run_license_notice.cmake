# The notice of a program names musl and mimalloc where the program links
# them, which is a program of musl alone. tests/anti-build/app links
# statically against musl on linux-arm64. tests/anti-build/window links
# dynamically against glibc there, and app on macos-arm64 links neither.
# `anti license --from` prints the notice of each program, and NOTICE.txt
# beside it in dist/ holds the same text. After the text of musl and of
# mimalloc stands the line of licenses/sources.txt of the runtime archive
# that names the upstream source of each, as `source <line>`, and a
# program that links neither prints no source line. Run with cmake -P and
# these values:
#   ANTI              the anti executable
#   RUNTIME           the runtime archive
#   LLVM_MC           the assembler
#   FIXTURE           tests/anti-build
#   MUSL_VERSION      the version of musl of the pinned sysroot
#   MIMALLOC_VERSION  the version of the pinned mimalloc
#   WORK              a directory this run writes into

cmake_minimum_required(VERSION 3.21)

foreach(target linux-arm64 macos-arm64)
    if(NOT EXISTS "${RUNTIME}/sysroot/${target}")
        message("SKIP: the runtime archive has no sysroot for ${target}")
        return()
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" "${FIXTURE}/window" DESTINATION "${WORK}")

foreach(name musl mimalloc)
    file(READ "${RUNTIME}/licenses/${name}.txt" ${name}_text)
    file(STRINGS "${RUNTIME}/licenses/sources.txt" ${name}_source
         REGEX "^${name} ")
    if(NOT ${name}_source MATCHES "^${name} [^ ]+ https://[^ ]+$")
        message(FATAL_ERROR "licenses/sources.txt of the runtime archive has "
                            "no line for ${name}: `${${name}_source}`")
    endif()
endforeach()

# Build project for target and check its notice. carries is TRUE where
# the program links musl and mimalloc.
function(check_notice project target carries)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" build --target "${target}"
                --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${WORK}/${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build --target ${target} of ${project} "
                            "failed with ${status}\n${out}${err}")
    endif()
    set(dist "${WORK}/${project}/dist/${target}/dev")
    execute_process(COMMAND "${ANTI}" license --from "${dist}/${project}"
                            --runtime "${RUNTIME}"
        RESULT_VARIABLE status OUTPUT_VARIABLE notice ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti license --from ${dist}/${project} failed "
                            "with ${status}\n${notice}${err}")
    endif()
    set(what "${project} for ${target}")
    if(NOT EXISTS "${dist}/NOTICE.txt")
        message(FATAL_ERROR "${what} has no NOTICE.txt beside it")
    endif()
    file(READ "${dist}/NOTICE.txt" kept)
    if(NOT kept STREQUAL notice)
        message(FATAL_ERROR "NOTICE.txt of ${what} is not what anti license "
                            "--from prints\n${kept}\n---\n${notice}")
    endif()
    if(notice MATCHES "ANTI_LICENSES_|(^|\n)build ")
        message(FATAL_ERROR "the notice of ${what} holds a marker or the "
                            "build id\n${notice}")
    endif()
    if(NOT notice MATCHES "^package anti\\.rt ")
        message(FATAL_ERROR "the notice of ${what} does not start with the "
                            "runtime\n${notice}")
    endif()
    # The package of the compiled module is the last package line.
    string(REGEX MATCHALL "(^|\n)package [^\n]*" packages "${notice}")
    list(GET packages -1 last)
    if(NOT last MATCHES "package com\\.example\\.${project} 0\\.1\\.0 ")
        message(FATAL_ERROR "the last package of ${what} is `${last}`\n"
                            "${notice}")
    endif()
    foreach(name musl mimalloc)
        string(TOUPPER "${name}" key)
        string(REPLACE "." "\\." version "${${key}_VERSION}")
        string(FIND "${notice}" "\ntext for ${name}\n${${name}_text}" text_at)
        if(carries)
            if(NOT notice MATCHES "\npackage ${name} ${version} MIT\n")
                message(FATAL_ERROR "the notice of ${what} lacks the line "
                                    "`package ${name} ${${key}_VERSION} "
                                    "MIT`\n${notice}")
            endif()
            if(text_at EQUAL -1)
                message(FATAL_ERROR "the notice of ${what} lacks the text of "
                                    "${name}\n${notice}")
            endif()
            string(FIND "${notice}" "${${name}_text}" first)
            string(FIND "${notice}" "${${name}_text}" again REVERSE)
            if(NOT first EQUAL again)
                message(FATAL_ERROR "the notice of ${what} holds the text of "
                                    "${name} twice\n${notice}")
            endif()
            string(FIND "${notice}"
                   "\ntext for ${name}\n${${name}_text}source ${${name}_source}\n"
                   source_at)
            if(source_at EQUAL -1)
                message(FATAL_ERROR "the notice of ${what} does not follow the "
                                    "text of ${name} with `source "
                                    "${${name}_source}`\n${notice}")
            endif()
        elseif(notice MATCHES "(^|\n)package ${name} " OR
               notice MATCHES "\ntext for [^\n]*${name}")
            message(FATAL_ERROR "the notice of ${what} names ${name}, which "
                                "it does not link\n${notice}")
        endif()
    endforeach()
    if(NOT carries AND notice MATCHES "(^|\n)source ")
        message(FATAL_ERROR "the notice of ${what} holds a source line, and "
                            "no component of it has one\n${notice}")
    endif()
endfunction()

check_notice(app linux-arm64 TRUE)
check_notice(window linux-arm64 FALSE)
check_notice(app macos-arm64 FALSE)
