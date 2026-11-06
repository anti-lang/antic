# The notice of a program names every native library of the runtime
# archive that the program links, by Q2 of "Answers of 2026-11-01" in
# docs/decisions.md. A program of anti.regex names PCRE2, one of
# anti.raylib names raylib and miniaudio, and tests/anti-build/app, which
# imports neither, names none of the three. Each line is
# `package <name> <version> <identifier>`, with the version of the line
# of licenses/sources.txt of the runtime archive, and the text of
# licenses/ follows `text for <name>` with the line of the record after it
# as `source <line>`. `anti license --from` prints the notice of the
# program, and NOTICE.txt beside it in dist/ holds the same. A program of
# anti.math names musl, whose libm the runtime carries, for every target,
# by Q2 of "Answers of 2026-11-06", with the version of MUSL_VERSION and
# the text of licenses/musl.txt. On macOS and Windows the app names no
# musl. Run with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   FIXTURE  tests/anti-build
#   RAYLIB   tests/std/raylib.anti, a program of anti.raylib
#   HOST     the target name of this host
#   TARGETS  the targets of the runtime archive, separated by commas
#   MUSL_VERSION  the version of musl of tools/sysroot-pins
#   WORK     a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" DESTINATION "${WORK}")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()

# A project of one module whose program is source.
function(project_of name source)
    write_bytes("${WORK}/${name}/anti.toml"
                "[package]\nname = \"com.example.${name}\"\nversion = \"0.1.0\"\n")
    file(MAKE_DIRECTORY "${WORK}/${name}/src/com/example")
    file(COPY_FILE "${source}" "${WORK}/${name}/src/com/example/${name}.anti")
endfunction()
write_bytes("${WORK}/pattern.anti" "//! A program of anti.regex.

import anti.regex;

fn main() -> int
{
	if \"due 2026\".matches(re\"[0-9]+\") {
		return 0;
	}
	return 1;
}
")
write_bytes("${WORK}/sine.anti" "//! A program of anti.math.

import anti.math;

fn main() -> int
{
	let x: f32 = 0.0;
	return math.sin(x) as int;
}
")
project_of(pattern "${WORK}/pattern.anti")
project_of(sine "${WORK}/sine.anti")
project_of(game "${RAYLIB}")

set(identifier_pcre2 "BSD-3-Clause WITH PCRE2-exception")
set(identifier_raylib "Zlib")
set(identifier_miniaudio "Unlicense OR MIT-0")
foreach(name pcre2 raylib miniaudio)
    file(READ "${RUNTIME}/licenses/${name}.txt" text_${name})
    file(STRINGS "${RUNTIME}/licenses/sources.txt" source_${name}
         REGEX "^${name} ")
    if(NOT source_${name} MATCHES "^${name} ([^ ]+) https://[^ ]+$")
        message(FATAL_ERROR "licenses/sources.txt of the runtime archive has "
                            "no line for ${name}: `${source_${name}}`")
    endif()
    set(version_${name} "${CMAKE_MATCH_1}")
    # The line of the source starts a line of its own, so a text without
    # a line feed at its end gets one, by R189 of "Rules of 2026-10-13".
    if(NOT text_${name} MATCHES "\n$")
        string(APPEND text_${name} "\n")
    endif()
endforeach()

# Build project and check that its notice names the libraries of linked
# and no other native library.
function(check_notice project)
    set(linked ${ARGN})
    anti_run(status out err "${WORK}/${project}" build --runtime "${RUNTIME}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build of ${project} failed with ${status}\n"
                            "${out}${err}")
    endif()
    set(dist "${WORK}/${project}/dist/${HOST}/dev")
    execute_process(COMMAND "${ANTI}" license --from "${dist}/${project}${exe}"
                            --runtime "${RUNTIME}"
        RESULT_VARIABLE status OUTPUT_VARIABLE notice ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti license --from the program of ${project} "
                            "failed with ${status}\n${notice}${err}")
    endif()
    file(READ "${dist}/NOTICE.txt" kept)
    if(NOT kept STREQUAL notice)
        message(FATAL_ERROR "NOTICE.txt of ${project} is not what anti "
                            "license --from prints\n${kept}\n---\n${notice}")
    endif()
    foreach(name pcre2 raylib miniaudio)
        string(FIND "${notice}"
               "\npackage ${name} ${version_${name}} ${identifier_${name}}\n"
               line_at)
        string(FIND "${notice}"
               "\ntext for ${name}\n${text_${name}}source ${source_${name}}\n"
               text_at)
        if(name IN_LIST linked)
            if(line_at EQUAL -1 OR text_at EQUAL -1)
                message(FATAL_ERROR "the notice of ${project} lacks the line "
                                    "`package ${name} ${version_${name}} "
                                    "${identifier_${name}}`, or its text and "
                                    "its source after `text for ${name}`\n"
                                    "${notice}")
            endif()
        elseif(notice MATCHES "(^|\n)package ${name} " OR
               notice MATCHES "\ntext for [^\n]*${name}")
            message(FATAL_ERROR "the notice of ${project} names ${name}, "
                                "which it does not link\n${notice}")
        endif()
    endforeach()
endfunction()

check_notice(pattern pcre2)
check_notice(game raylib miniaudio)
check_notice(app)

# The notice of a program of anti.math for every target names musl with
# its text. Without anti.math only a program of musl names it.
file(READ "${RUNTIME}/licenses/musl.txt" text_musl)
if(NOT text_musl MATCHES "\n$")
    string(APPEND text_musl "\n")
endif()
string(REPLACE "," ";" targets "${TARGETS}")
foreach(target IN LISTS targets)
    foreach(project sine app)
        anti_run(status out err "${WORK}/${project}" build --runtime
                 "${RUNTIME}" --target "${target}")
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "anti build of ${project} for ${target} "
                                "failed with ${status}\n${out}${err}")
        endif()
        set(program "${WORK}/${project}/dist/${target}/dev/${project}")
        if(target MATCHES "^windows-")
            string(APPEND program ".exe")
        endif()
        execute_process(COMMAND "${ANTI}" license --from "${program}"
                                --runtime "${RUNTIME}"
            RESULT_VARIABLE status OUTPUT_VARIABLE notice ERROR_VARIABLE err
            ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "anti license --from ${program} failed with "
                                "${status}\n${notice}${err}")
        endif()
        string(FIND "${notice}" "\npackage musl ${MUSL_VERSION} MIT\n" line_at)
        string(FIND "${notice}" "\ntext for musl\n${text_musl}" text_at)
        if(project STREQUAL "sine" OR target MATCHES "^linux-")
            if(line_at EQUAL -1 OR text_at EQUAL -1)
                message(FATAL_ERROR "the notice of ${project} for ${target} "
                                    "lacks `package musl ${MUSL_VERSION} MIT` "
                                    "or its text\n${notice}")
            endif()
        elseif(NOT line_at EQUAL -1)
            message(FATAL_ERROR "the notice of ${project} for ${target} names "
                                "musl, which it does not link\n${notice}")
        endif()
    endforeach()
endforeach()
