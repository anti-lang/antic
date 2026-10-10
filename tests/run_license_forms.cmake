# The four forms of `anti license` beside --from, one per run. Run with
# cmake -P and these values:
#   FORM              plain, project, notice or archive
#   ANTI              the anti executable
#   RUNTIME           the runtime archive
#   LLVM_MC           the assembler
#   LLVM_AR           the archiver of a static library
#   ROOT              the repository, whose LICENSE is the one of antic
#                     and anti
#   FIXTURE           tests/anti-build, which holds shapes/, tones/,
#                     notices/ and units/
#   HOST              the target name of this host
#   VERSION           the version of antic and anti
#   MUSL_VERSION      the version of musl of the pinned sysroot
#   MIMALLOC_VERSION  the version of the pinned mimalloc
#   WORK              a directory this run writes into
#
# plain: `anti license` prints the licence of antic and anti, then every
# component of licenses/ of the runtime archive with its name, its
# version, its identifier and its text, and the line of sources.txt after
# the text of each component the record names.
#
# project: `anti license --project` prints the packages the project
# links, from anti.lock and the imported bundled modules: the runtime,
# musl and mimalloc where the program links them, both packages of the
# lock with their identifiers and attributions, the standard library and
# the package of the project last, then each text. The program it built
# names the same packages in its own notice.
#
# notice: `anti license --project --notice` writes the same content as
# dist/<target>/<mode>/NOTICE.txt, which is the file `anti build` writes.
# The file names every package of the lock, also one that no module
# imports, and it comes from the project and not from a binary: a static
# library, which carries no notice, gets one.
#
# archive: `anti license --from-archive` prints the licence fields of the
# copy of the package header in a static archive that `anti build --lib
# static` wrote, in each of the three archive formats.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run_anti where out_status out_text out_err)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN}
        WORKING_DIRECTORY "${where}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}" PARENT_SCOPE)
    set(${out_err} "${err}" PARENT_SCOPE)
endfunction()

# Run anti in where and return its standard output. It must succeed.
function(anti_output what where out_text)
    run_anti("${where}" status out err ${ARGN})
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${out}${err}")
    endif()
    set(${out_text} "${out}" PARENT_SCOPE)
endfunction()

# The options that name the runtime archive and its tools to a build.
set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" --llvm-ar "${LLVM_AR}")

# Fail unless text holds part.
function(expect_part what text part)
    string(FIND "${text}" "${part}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${what} lacks `${part}`\n${text}")
    endif()
endfunction()

# The `package` lines of a notice, sorted, in out.
function(package_lines text out)
    string(REGEX MATCHALL "(^|\n)package [^\n]*" lines "${text}")
    set(found "")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        list(APPEND found "${line}")
    endforeach()
    list(SORT found)
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# A cross build needs the sysroot of its target.
function(need_sysroots)
    foreach(target IN LISTS ARGN)
        if(NOT EXISTS "${RUNTIME}/sysroot/${target}")
            message("SKIP: the runtime archive has no sysroot for ${target}")
            set(skip TRUE PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

string(REPLACE "." "\\." version_pattern "${VERSION}")

if(FORM STREQUAL "plain")
    anti_output("anti license" "${WORK}" out license --runtime "${RUNTIME}")
    if(out MATCHES "ANTI_LICENSES_|(^|\n)build ")
        message(FATAL_ERROR "anti license prints a marker or a build id\n${out}")
    endif()
    # The licence of antic and anti comes first, then the runtime.
    if(NOT out MATCHES "^package antic ${version_pattern} MIT\npackage anti ${version_pattern} MIT\npackage anti\\.rt ${version_pattern} 0BSD\n")
        message(FATAL_ERROR "anti license does not start with antic, anti and "
                            "the runtime\n${out}")
    endif()
    file(READ "${ROOT}/LICENSE" own)
    expect_part("anti license" "${out}" "\ntext for antic anti\n${own}")
    file(READ "${RUNTIME}/LICENSE" kept)
    if(NOT kept STREQUAL own)
        message(FATAL_ERROR "LICENSE of the runtime archive is not the one of "
                            "the repository")
    endif()
    # Every text of licenses/ is a component with a name, a version and an
    # identifier, and its text follows.
    file(GLOB texts RELATIVE "${RUNTIME}/licenses" "${RUNTIME}/licenses/*.txt")
    list(REMOVE_ITEM texts sources.txt)
    list(LENGTH texts count)
    if(count LESS 4)
        message(FATAL_ERROR "licenses/ of ${RUNTIME} holds ${count} texts")
    endif()
    file(STRINGS "${RUNTIME}/licenses/sources.txt" sources REGEX "^[^#]")
    foreach(file IN LISTS texts)
        string(REGEX REPLACE "\\.txt$" "" name "${file}")
        set(component "${name}")
        if(name STREQUAL "anti_rt")
            set(component "anti.rt")
        endif()
        string(REPLACE "." "\\." pattern "${component}")
        string(REPLACE "+" "\\+" pattern "${pattern}")
        if(NOT out MATCHES "(^|\n)package ${pattern} ([^ \n]+) ([^\n]+)\n")
            message(FATAL_ERROR "anti license has no line `package "
                                "${component} <version> <identifier>`\n${out}")
        endif()
        set(version "${CMAKE_MATCH_2}")
        set(source "")
        foreach(line IN LISTS sources)
            if(line MATCHES "^${pattern} ([^ ]+) ")
                set(source "${line}")
                set(recorded "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        if(NOT source STREQUAL "")
            if(NOT version STREQUAL recorded)
                message(FATAL_ERROR "anti license gives ${component} the "
                                    "version ${version}, and sources.txt "
                                    "${recorded}")
            endif()
            expect_part("anti license" "${out}" "\nsource ${source}\n")
        elseif(NOT component STREQUAL "anti.rt" AND NOT version STREQUAL "-")
            message(FATAL_ERROR "anti license gives ${component} the version "
                                "${version}, and no record names one")
        endif()
        if(NOT out MATCHES "\ntext for ([^\n]* )?${pattern}( [^\n]*)?\n")
            message(FATAL_ERROR "anti license has no `text for` line that "
                                "names ${component}\n${out}")
        endif()
        file(READ "${RUNTIME}/licenses/${file}" text)
        expect_part("anti license" "${out}" "${text}")
    endforeach()
    # Every line of the record follows a text, once.
    foreach(line IN LISTS sources)
        string(FIND "${out}" "\nsource ${line}\n" first)
        string(FIND "${out}" "\nsource ${line}\n" last REVERSE)
        if(first EQUAL -1 OR NOT first EQUAL last)
            message(FATAL_ERROR "anti license does not print `source ${line}` "
                                "once\n${out}")
        endif()
    endforeach()
    # The identifier of a component is the one its licence names, and
    # LicenseRef-<name> where its text holds several.
    foreach(pair
            "musl ${MUSL_VERSION} MIT"
            "mimalloc ${MIMALLOC_VERSION} MIT"
            "glibc [^ ]+ LGPL-2.1-or-later"
            "linux-headers [^ ]+ GPL-2.0-only"
            "pcre2 [^ ]+ BSD-3-Clause WITH PCRE2-exception"
            "compiler-rt - Apache-2.0 WITH LLVM-exception"
            "libx11-6 [^ ]+ LicenseRef-libx11-6")
        string(REGEX REPLACE " .*$" "" name "${pair}")
        if(EXISTS "${RUNTIME}/licenses/${name}.txt" AND
           NOT out MATCHES "\npackage ${pair}\n")
            message(FATAL_ERROR "anti license has no line `package ${pair}`\n"
                                "${out}")
        endif()
    endforeach()
    # An archive without licenses/ is refused, and --notice belongs to
    # --project.
    file(MAKE_DIRECTORY "${WORK}/empty/lib")
    run_anti("${WORK}" status out err license --runtime "${WORK}/empty")
    if(status EQUAL 0 OR NOT err MATCHES "licenses")
        message(FATAL_ERROR "anti license took an archive without licenses/: "
                            "${status}\n${out}${err}")
    endif()
    run_anti("${WORK}" status out err license --notice)
    if(NOT status EQUAL 2)
        message(FATAL_ERROR "anti license --notice without --project ended "
                            "with ${status}\n${out}${err}")
    endif()
    return()
endif()

file(COPY "${FIXTURE}/shapes" "${FIXTURE}/tones" "${FIXTURE}/notices"
          "${FIXTURE}/units" DESTINATION "${WORK}")
foreach(name shapes tones notices)
    file(READ "${FIXTURE}/${name}/LICENSE" ${name}_text)
endforeach()

# The name of the program of the fixture notices built for target.
function(program_of target out)
    set(name "notices")
    if(target MATCHES "^windows-")
        string(APPEND name ".exe")
    endif()
    set(${out} "${WORK}/notices/dist/${target}/dev/${name}" PARENT_SCOPE)
endfunction()

if(FORM STREQUAL "project")
    set(skip FALSE)
    need_sysroots(linux-arm64 macos-arm64)
    if(skip)
        return()
    endif()
    foreach(name musl mimalloc)
        file(READ "${RUNTIME}/licenses/${name}.txt" ${name}_text)
        file(STRINGS "${RUNTIME}/licenses/sources.txt" ${name}_source
             REGEX "^${name} ")
    endforeach()
    set(targets "${HOST}" linux-arm64 macos-arm64)
    list(REMOVE_DUPLICATES targets)
    foreach(target IN LISTS targets)
        set(what "anti license --project --target ${target}")
        anti_output("${what}" "${WORK}/notices" out license --project
                    --target "${target}" ${tools})
        if(out MATCHES "ANTI_LICENSES_|(^|\n)build ")
            message(FATAL_ERROR "${what} prints a marker or a build id\n${out}")
        endif()
        # The runtime first, then what the link of the target took of the
        # C library, both packages of the lock with identifier and
        # attributions, the bundled modules the project imports, and the
        # package of the project last.
        set(head "package anti.rt ${VERSION} 0BSD\n")
        if(target MATCHES "^linux-")
            string(APPEND head "package musl ${MUSL_VERSION} MIT\n"
                               "package mimalloc ${MIMALLOC_VERSION} MIT\n")
        elseif(out MATCHES "(^|\n)package (musl|mimalloc) " OR
               out MATCHES "(^|\n)source ")
            message(FATAL_ERROR "${what} names musl, mimalloc or a source, "
                                "and the program links neither\n${out}")
        endif()
        string(APPEND head
            "package com.example.shapes 2.0.1 BSD-3-Clause\n"
            "attribution Copyright 2026 The Shapes Authors\n"
            "package com.example.tones 1.4.0 MIT\n"
            "attribution Copyright 2026 The Tones Authors\n"
            "attribution Contains tables from Example Corp, CC0-1.0\n"
            "package anti ${VERSION} 0BSD\n"
            "package com.example.notices 0.3.0 MIT\n"
            "text for ")
        string(FIND "${out}" "${head}" at)
        if(NOT at EQUAL 0)
            message(FATAL_ERROR "${what} does not start with\n${head}\n${out}")
        endif()
        foreach(name shapes tones notices)
            expect_part("${what}" "${out}"
                        "\ntext for com.example.${name}\n${${name}_text}")
        endforeach()
        if(target MATCHES "^linux-")
            foreach(name musl mimalloc)
                expect_part("${what}" "${out}"
                    "\ntext for ${name}\n${${name}_text}source ${${name}_source}\n")
            endforeach()
        endif()
        # The command built the project, and the notice of the program it
        # linked names the same packages.
        program_of("${target}" program)
        anti_output("anti license --from ${program}" "${WORK}" linked license
                    --from "${program}" --runtime "${RUNTIME}")
        package_lines("${out}" of_project)
        package_lines("${linked}" of_binary)
        if(NOT of_project STREQUAL of_binary)
            message(FATAL_ERROR "${what} names\n${of_project}\nand the "
                                "program it built\n${of_binary}")
        endif()
    endforeach()
    file(READ "${WORK}/notices/anti.lock" lock)
    if(NOT lock MATCHES "com.example.shapes" OR
       NOT lock MATCHES "com.example.tones")
        message(FATAL_ERROR "anti.lock names no package:\n${lock}")
    endif()
    # One notice is one text, so the form takes one target.
    run_anti("${WORK}/notices" status out err license --project --target all
             ${tools})
    if(status EQUAL 0 OR NOT err MATCHES "one target")
        message(FATAL_ERROR "anti license --project --target all ended with "
                            "${status}\n${out}${err}")
    endif()
    # A directory without a manifest is no project.
    run_anti("${WORK}" status out err license --project ${tools})
    if(status EQUAL 0 OR NOT err MATCHES "anti.toml")
        message(FATAL_ERROR "anti license --project outside a project ended "
                            "with ${status}\n${out}${err}")
    endif()
    return()
endif()

if(FORM STREQUAL "notice")
    set(skip FALSE)
    need_sysroots(linux-arm64)
    if(skip)
        return()
    endif()
    set(targets "${HOST}" linux-arm64)
    list(REMOVE_DUPLICATES targets)
    foreach(target IN LISTS targets)
        set(file "${WORK}/notices/dist/${target}/dev/NOTICE.txt")
        anti_output("anti build --target ${target}" "${WORK}/notices" out
                    build --target "${target}" ${tools})
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "anti build wrote no ${file}")
        endif()
        file(READ "${file}" built)
        anti_output("anti license --project" "${WORK}/notices" printed
                    license --project --target "${target}" ${tools})
        if(NOT printed STREQUAL built)
            message(FATAL_ERROR "anti license --project for ${target} prints\n"
                                "${printed}\nand NOTICE.txt of anti build "
                                "holds\n${built}")
        endif()
        file(REMOVE "${file}")
        anti_output("anti license --project --notice" "${WORK}/notices" out
                    license --project --notice --target "${target}" ${tools})
        if(NOT out STREQUAL "")
            message(FATAL_ERROR "anti license --project --notice prints\n"
                                "${out}")
        endif()
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "anti license --project --notice wrote no "
                                "${file}")
        endif()
        file(READ "${file}" written)
        if(NOT written STREQUAL built)
            message(FATAL_ERROR "anti license --project --notice for "
                                "${target} wrote\n${written}\nand anti build\n"
                                "${built}")
        endif()
    endforeach()
    # NOTICE.txt names every package of the lock, also one that the
    # manifest names and no module imports.
    file(APPEND "${WORK}/notices/anti.toml"
         "\"com.example.units\" = { path = \"../units\" }\n")
    anti_output("anti build with a third package" "${WORK}/notices" out
                build ${tools})
    file(READ "${WORK}/notices/dist/${HOST}/dev/NOTICE.txt" built)
    expect_part("NOTICE.txt" "${built}" "\npackage com.example.units 1.2.0 \n")
    # NOTICE.txt comes from the project and not from a binary. A static
    # library carries no notice, and `anti build` writes no file beside
    # it. --project --notice writes the one of its project, which names
    # no musl, since no link of the library took it.
    set(file "${WORK}/shapes/dist/linux-arm64/dev/NOTICE.txt")
    anti_output("anti build --lib static" "${WORK}/shapes" out build --lib
                static --target linux-arm64 ${tools})
    if(EXISTS "${file}")
        message(FATAL_ERROR "anti build --lib static wrote ${file}")
    endif()
    anti_output("anti license --project --notice --lib static"
                "${WORK}/shapes" out license --project --notice --lib static
                --target linux-arm64 ${tools})
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "anti license --project --notice --lib static "
                            "wrote no ${file}")
    endif()
    file(READ "${file}" written)
    if(NOT written MATCHES "^package anti\\.rt ${version_pattern} 0BSD\n" OR
       NOT written MATCHES "\npackage com\\.example\\.shapes 2\\.0\\.1 BSD-3-Clause\nattribution Copyright 2026 The Shapes Authors\ntext for " OR
       written MATCHES "(^|\n)package (musl|mimalloc) ")
        message(FATAL_ERROR "NOTICE.txt of the static library holds\n"
                            "${written}")
    endif()
    expect_part("NOTICE.txt of the static library" "${written}"
                "\ntext for com.example.shapes\n${shapes_text}")
    return()
endif()

if(FORM STREQUAL "archive")
    set(skip FALSE)
    need_sysroots(linux-x86_64 macos-arm64 windows-x86_64)
    if(skip)
        return()
    endif()
    set(expected "package com.example.shapes 2.0.1 BSD-3-Clause\n")
    string(APPEND expected "attribution Copyright 2026 The Shapes Authors\n"
                           "text for com.example.shapes\n${shapes_text}")
    # One archive per format: ELF in the GNU form, Mach-O in the form of
    # BSD and COFF.
    foreach(target linux-x86_64 macos-arm64 windows-x86_64)
        anti_output("anti build --lib static --target ${target}"
                    "${WORK}/shapes" out build --lib static --target
                    "${target}" ${tools})
        set(archive "${WORK}/shapes/dist/${target}/dev/libshapes.a")
        if(target MATCHES "^windows-")
            set(archive "${WORK}/shapes/dist/${target}/dev/shapes.lib")
        endif()
        if(NOT EXISTS "${archive}")
            message(FATAL_ERROR "anti build --lib static wrote no ${archive}")
        endif()
        anti_output("anti license --from-archive ${archive}" "${WORK}" out
                    license --from-archive "${archive}")
        if(NOT out STREQUAL expected)
            message(FATAL_ERROR "anti license --from-archive ${archive} "
                                "prints\n${out}\nand not\n${expected}")
        endif()
    endforeach()
    # An archive that bundles the runtime keeps the header as a member.
    anti_output("anti build --lib static --bundle-runtime" "${WORK}/shapes"
                out build --lib static --bundle-runtime --target macos-arm64
                ${tools})
    anti_output("anti license --from-archive of a bundle" "${WORK}" out
                license --from-archive
                "${WORK}/shapes/dist/macos-arm64/dev/libshapes.a")
    if(NOT out STREQUAL expected)
        message(FATAL_ERROR "anti license --from-archive of a bundle prints\n"
                            "${out}")
    endif()
    # A file that is no archive, and an archive without a package header.
    run_anti("${WORK}" status out err license --from-archive
             "${FIXTURE}/shapes/LICENSE")
    if(status EQUAL 0 OR NOT err MATCHES "is no archive")
        message(FATAL_ERROR "anti license --from-archive took a text file: "
                            "${status}\n${out}${err}")
    endif()
    file(GLOB_RECURSE runtimes "${RUNTIME}/lib/macos-arm64/*/libanti_rt.a")
    list(GET runtimes 0 runtime_library)
    run_anti("${WORK}" status out err license --from-archive
             "${runtime_library}")
    if(status EQUAL 0 OR NOT err MATCHES "carries no package header")
        message(FATAL_ERROR "anti license --from-archive took the runtime "
                            "library: ${status}\n${out}${err}")
    endif()
    # A malformed archive is refused and read no further than its bytes
    # go: a size past the end of the file, a size that is no number, a
    # long name without its table and one past the end of the table, a
    # name of the BSD form longer than its member, and a member of the
    # package header that holds the first bytes of a library file alone.
    # The header of a member is its name in 16 bytes, 32 bytes of date,
    # owner and mode, the size in 10 and the two bytes that end it.
    function(ar_member out name size data)
        foreach(field name size)
            set(width 16)
            if(field STREQUAL "size")
                set(width 10)
            endif()
            string(LENGTH "${${field}}" length)
            math(EXPR pad "${width} - ${length}")
            string(REPEAT " " ${pad} spaces)
            string(APPEND ${field} "${spaces}")
        endforeach()
        string(REPEAT " " 32 middle)
        set(${out} "${name}${middle}${size}`\n${data}" PARENT_SCOPE)
    endfunction()
    ar_member(past "s.package.o/" 9999999999 "ANTL")
    ar_member(no_number "s.package.o/" "4x" "ANTL")
    ar_member(no_table "/20" 4 "ANTL")
    ar_member(table "//" 18 "shapes.package.o/\n")
    ar_member(past_table "/9999" 4 "ANTL")
    ar_member(long_bsd "#1/9999" 4 "ANTL")
    ar_member(start_alone "s.package.o/" 4 "ANTL")
    ar_member(start_named "/0" 4 "ANTL")
    set(malformed_1 "${past}")
    set(malformed_2 "${no_number}")
    set(malformed_3 "${no_table}")
    set(malformed_4 "${table}${past_table}")
    set(malformed_5 "${long_bsd}")
    set(malformed_6 "${start_alone}")
    set(malformed_7 "${table}${start_named}")
    set(malformed_8 "")
    foreach(case RANGE 1 8)
        file(WRITE "${WORK}/malformed-${case}.a" "!<arch>\n${malformed_${case}}")
        run_anti("${WORK}" status out err license --from-archive
                 "${WORK}/malformed-${case}.a")
        if(NOT status EQUAL 1 OR NOT out STREQUAL "" OR
           NOT err MATCHES "carries no package header")
            message(FATAL_ERROR "anti license --from-archive of the malformed "
                                "archive ${case} ended with ${status}\n"
                                "${out}${err}")
        endif()
    endforeach()
    return()
endif()

message(FATAL_ERROR "FORM is `${FORM}`, and it is plain, project, notice or "
                    "archive")
