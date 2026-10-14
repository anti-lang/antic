# Options of antic that the driver reads together, each beside the run
# without it. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   CASE      the case to run, one of the list below
#   RUNTIME   the runtime archive
#   LLVM_MC   the assembler
#   LLVM_AR   the archiver
#   READOBJ   llvm-readobj of the pinned release
#   SOURCES   tests/, which holds programs/, traps/ and errors/
#   TARGET    the target of the case target_cpu
#   EXE       the suffix of a program of this host
#   WORK      a directory this run writes into
#
# The cases:
#   target_cpu            --cpu with --target TARGET: each level of the
#                         architecture gives its attributes, the target
#                         alone gives its default level, and a level of the
#                         other architecture is refused
#   checks_by_mode        --checks and --no-checks beside --dev and
#                         without it
#   checks_last_decides   --checks and --no-checks in one command: the
#                         last of the two decides, in both modes
#   asserts_by_mode       --asserts and --no-asserts beside --dev and
#                         without it
#   warn_undocumented     --warn-undocumented beside --doc-warnings and
#                         without it
#   strict_library        --warnings-as-errors beside -c
#   strict_front_end      --warnings-as-errors beside --front-end
#   strict_dev            --warnings-as-errors beside --dev
#   bundle_static         --bundle-runtime beside --lib static
#   bundle_shared         --bundle-runtime beside --lib shared, which
#                         R132 of "Rules of 2026-10-13" refuses
#   soname_linux          --soname beside --lib shared and
#                         --package-version, for Linux
#   output_assembly       -o beside -S
#   output_library        -o beside -c
#   output_program        -o of a program
#   keep_llvm             --keep-llvm of a program
#   output_static         -o beside --lib static, for Linux and Windows:
#                         the name of the archive without it, and a name
#                         without a directory
#   output_shared         -o beside --lib shared, for the targets this
#                         host links: the name of the library without it
#   output_dev_library    -o beside --dev with a library file as the input
#   output_plugin         -o of a plugin without a directory: the index
#                         stands beside the library and names it
#   linker_lld            --linker lld beside the default
#
# DESIGN: a case reads the LLVM text of --dump-llvm where the option
# decides what the back end writes, so no case needs opt, llc and a link
# to show an option of the front half. The cases that name a file of the
# link build one.

cmake_minimum_required(VERSION 3.21)

foreach(value ANTIC CASE RUNTIME SOURCES WORK)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Run antic in WORK and give its status and its two outputs.
function(antic out_status out_stdout out_stderr)
    execute_process(COMMAND "${ANTIC}" ${ARGN}
                    WORKING_DIRECTORY "${WORK}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_stdout} "${out}" PARENT_SCOPE)
    set(${out_stderr} "${err}" PARENT_SCOPE)
endfunction()

# Run antic in WORK, stop unless it ends with 0 and writes no message, and
# give its standard output.
function(antic_output out)
    antic(status text err ${ARGN})
    if(NOT status STREQUAL "0" OR NOT err STREQUAL "")
        string(REPLACE ";" " " line "${ARGN}")
        message(FATAL_ERROR "antic ${line} ended with ${status}\n${err}")
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(holds text needle what)
    string(FIND "${text}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${what} holds no `${needle}`:\n${text}")
    endif()
endfunction()

function(lacks text needle what)
    string(FIND "${text}" "${needle}" at)
    if(NOT at LESS 0)
        message(FATAL_ERROR "${what} holds `${needle}`:\n${text}")
    endif()
endfunction()

# The files of a directory, sorted, as one text with a space between them.
function(files_of directory out)
    file(GLOB found RELATIVE "${directory}" "${directory}/*")
    list(SORT found)
    list(JOIN found " " line)
    set(${out} "${line}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "target_cpu")
    set(source "${SOURCES}/programs/return42.anti")
    antic_output(levels --print-cpu-levels)
    antic_output(attributes --print-llvm-targets)
    if(NOT levels MATCHES "(^|\n)default ${TARGET} ([^ \n]+)\n")
        message(FATAL_ERROR "--print-cpu-levels names no default level of "
                            "${TARGET}:\n${levels}")
    endif()
    set(default_level "${CMAKE_MATCH_2}")
    if("${TARGET}" MATCHES "arm64$")
        set(arch arm64)
    else()
        set(arch x86_64)
    endif()
    string(REGEX MATCHALL "level [^ \n]+ [^ \n]+ [^ \n]+ [^ \n]+ [^ \n]+\n"
           rows "${attributes}")
    set(taken 0)
    set(refused 0)
    foreach(row IN LISTS rows)
        string(REGEX MATCH "^level ([^ ]+) ([^ ]+) [^ ]+ ([^ ]+) ([^ \n]+)\n$"
               whole "${row}")
        set(level "${CMAKE_MATCH_1}")
        set(level_arch "${CMAKE_MATCH_2}")
        set(cpu "${CMAKE_MATCH_3}")
        set(features "${CMAKE_MATCH_4}")
        antic(status text err --runtime "${RUNTIME}" --target ${TARGET}
              --cpu ${level} --dump-llvm "${source}")
        if(NOT level_arch STREQUAL arch)
            if(NOT status STREQUAL "2" OR NOT text STREQUAL "" OR NOT err
               STREQUAL "antic: ${level} is no processor level of ${TARGET}\n")
                message(FATAL_ERROR "--target ${TARGET} --cpu ${level} "
                                    "ended with ${status}\n${text}${err}")
            endif()
            math(EXPR refused "${refused} + 1")
            continue()
        endif()
        if(NOT status STREQUAL "0" OR NOT err STREQUAL "")
            message(FATAL_ERROR "--target ${TARGET} --cpu ${level} ended "
                                "with ${status}\n${err}")
        endif()
        holds("${text}"
              "\"target-cpu\"=\"${cpu}\" \"target-features\"=\"${features}\""
              "the LLVM text of --target ${TARGET} --cpu ${level}")
        if(level STREQUAL default_level)
            antic_output(plain --runtime "${RUNTIME}" --target ${TARGET}
                         --dump-llvm "${source}")
            if(NOT plain STREQUAL text)
                message(FATAL_ERROR "--target ${TARGET} alone and with "
                                    "--cpu ${level}, its default level, "
                                    "write different LLVM text")
            endif()
        endif()
        math(EXPR taken "${taken} + 1")
    endforeach()
    if(NOT taken EQUAL 3 OR NOT refused EQUAL 3)
        message(FATAL_ERROR "${TARGET} took ${taken} levels and refused "
                            "${refused}, and the test expects three of each")
    endif()
    return()
endif()

# A mark of the LLVM text beside --dev and without it. NEEDLE is the mark,
# ON the option that asks for it and OFF the one that drops it. Release
# mode writes none and dev mode writes it, and each option overrides both.
function(by_mode source needle on off)
    foreach(cell "release||OFF" "release|${on}|ON" "release|${off}|OFF"
            "dev||ON" "dev|${on}|ON" "dev|${off}|OFF")
        string(REPLACE "|" ";" parts "${cell}")
        list(GET parts 0 mode)
        list(GET parts 1 option)
        list(GET parts 2 expected)
        set(arguments "")
        if(mode STREQUAL "dev")
            list(APPEND arguments --dev)
        endif()
        if(NOT option STREQUAL "")
            list(APPEND arguments ${option})
        endif()
        antic_output(text --runtime "${RUNTIME}" ${arguments} --dump-llvm
                     "${source}")
        string(REPLACE ";" " " line "${mode} ${option}")
        if(expected)
            holds("${text}" "${needle}" "the LLVM text of ${line}")
        else()
            lacks("${text}" "${needle}" "the LLVM text of ${line}")
        endif()
    endforeach()
endfunction()

if(CASE STREQUAL "checks_by_mode")
    by_mode("${SOURCES}/traps/bounds_array.anti"
            "call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed("
            --checks --no-checks)
    return()
endif()

# R144 of "Rules of 2026-10-13" in docs/decisions.md: of --checks and
# --no-checks the last one decides, whatever the mode says.
if(CASE STREQUAL "checks_last_decides")
    set(source "${SOURCES}/traps/bounds_array.anti")
    set(needle "call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(")
    foreach(mode release dev)
        set(arguments "")
        if(mode STREQUAL "dev")
            set(arguments --dev)
        endif()
        antic_output(text --runtime "${RUNTIME}" ${arguments} --checks
                     --no-checks --dump-llvm "${source}")
        lacks("${text}" "${needle}"
              "the LLVM text of ${mode} --checks --no-checks")
        antic_output(text --runtime "${RUNTIME}" ${arguments} --no-checks
                     --checks --dump-llvm "${source}")
        holds("${text}" "${needle}"
              "the LLVM text of ${mode} --no-checks --checks")
    endforeach()
    return()
endif()

if(CASE STREQUAL "asserts_by_mode")
    by_mode("${SOURCES}/programs/asserts.anti"
            "call void (ptr, i64) @anti_rt_assert_failed("
            --asserts --no-asserts)
    return()
endif()

if(CASE STREQUAL "warn_undocumented")
    file(WRITE "${WORK}/bare.anti"
         "//! A module with one public item that has no comment.\n\npub fn bare() -> int\n{\n\treturn 1;\n}\n")
    set(warning "bare.anti:3:8: warning: the pub item `bare` has no `///` comment [undocumented]\n")
    # The front end with the options after the text it must write.
    function(front_end expected)
        antic(status out err --runtime "${RUNTIME}" --front-end ${ARGN}
              bare.anti)
        if(NOT status STREQUAL "0" OR NOT out STREQUAL "" OR
           NOT err STREQUAL expected)
            string(REPLACE ";" " " line "${ARGN}")
            message(FATAL_ERROR "--front-end ${line} ended with ${status} "
                                "and wrote:\n${out}${err}\nThe test expects "
                                "0 and:\n${expected}")
        endif()
    endfunction()
    front_end("")
    front_end("" --doc-warnings)
    front_end("" --warn-undocumented)
    front_end("${warning}" --doc-warnings --warn-undocumented)
    return()
endif()

# A program with a warning under the options of one build, without
# --warnings-as-errors and with it. The build is not a release build, so
# it warns and goes on, and the option makes the same line an error.
function(strict)
    file(COPY "${SOURCES}/errors/never_fails.anti" DESTINATION "${WORK}")
    set(line "never_fails.anti:3:21: KIND: `close_it` may fail and never does [never-fails]\n")
    string(REPLACE "KIND" "warning" as_warning "${line}")
    string(REPLACE "KIND" "error" as_error "${line}")
    antic(status out err --runtime "${RUNTIME}" ${ARGN} never_fails.anti)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "the build without the option ended with "
                            "${status}\n${out}${err}")
    endif()
    holds("${err}" "${as_warning}" "the build without the option")
    antic(status out err --runtime "${RUNTIME}" ${ARGN} --warnings-as-errors
          never_fails.anti)
    if(NOT status STREQUAL "1")
        message(FATAL_ERROR "the build with --warnings-as-errors ended "
                            "with ${status}, and the test expects 1\n"
                            "${out}${err}")
    endif()
    holds("${err}" "${as_error}" "the build with --warnings-as-errors")
    lacks("${err}" "warning:" "the build with --warnings-as-errors")
endfunction()

if(CASE STREQUAL "strict_library")
    strict(-c -o "${WORK}/never_fails.antl")
    return()
endif()

if(CASE STREQUAL "strict_front_end")
    strict(--front-end)
    return()
endif()

if(CASE STREQUAL "strict_dev")
    strict(--dev -S -o "${WORK}/never_fails.s")
    return()
endif()

set(library_source "${SOURCES}/errors/modules_library_options.anti")
set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" --llvm-ar "${LLVM_AR}")

if(CASE STREQUAL "bundle_static")
    antic_output(plain_said --lib static ${tools} -o "${WORK}/plain.a"
                 "${library_source}")
    antic_output(bundle_said --lib static --bundle-runtime ${tools}
                 -o "${WORK}/bundle.a" "${library_source}")
    execute_process(COMMAND "${LLVM_AR}" t "${WORK}/plain.a"
                    OUTPUT_VARIABLE plain RESULT_VARIABLE plain_status
                    ENCODING NONE)
    execute_process(COMMAND "${LLVM_AR}" t "${WORK}/bundle.a"
                    OUTPUT_VARIABLE bundle RESULT_VARIABLE bundle_status
                    ENCODING NONE)
    if(NOT plain_status EQUAL 0 OR NOT bundle_status EQUAL 0)
        message(FATAL_ERROR "llvm-ar read no archive")
    endif()
    # check.o holds anti_rt_check_failed, a member of every runtime.
    lacks("${plain}" "check.o" "the archive without --bundle-runtime")
    holds("${bundle}" "check.o" "the archive of --bundle-runtime")
    # The line antic prints names what the C program links beside the
    # archive: the runtime, unless the archive holds it.
    holds("${plain_said}" "libanti_rt.a"
          "the line of the build without --bundle-runtime")
    lacks("${bundle_said}" "libanti_rt.a"
          "the line of the build with --bundle-runtime")
    return()
endif()

if(CASE STREQUAL "bundle_shared")
    antic_output(said --lib shared ${tools} -o "${WORK}/plain.so"
                 "${library_source}")
    antic(status said err --lib shared --bundle-runtime ${tools}
          -o "${WORK}/bundle.so" "${library_source}")
    if(NOT status STREQUAL "2" OR EXISTS "${WORK}/bundle.h")
        message(FATAL_ERROR "--bundle-runtime beside --lib shared ended "
                            "with ${status}, and must be refused\n${err}")
    endif()
    holds("${err}" "--bundle-runtime" "the refusal")
    return()
endif()

if(CASE STREQUAL "soname_linux")
    file(MAKE_DIRECTORY "${WORK}/plain" "${WORK}/option")
    antic_output(said --lib shared --target linux-x86_64
                 --package-version 3.4.5 ${tools}
                 -o "${WORK}/plain/libtwo.so" "${library_source}")
    antic_output(said --lib shared --soname --target linux-x86_64
                 --package-version 3.4.5 ${tools}
                 -o "${WORK}/option/libtwo.so" "${library_source}")
    if(EXISTS "${WORK}/plain/libtwo.so.3" OR
       IS_SYMLINK "${WORK}/plain/libtwo.so")
        message(FATAL_ERROR "the build without --soname wrote a versioned "
                            "library")
    endif()
    if(NOT EXISTS "${WORK}/option/libtwo.so.3" OR
       IS_SYMLINK "${WORK}/option/libtwo.so.3")
        message(FATAL_ERROR "--soname wrote no library libtwo.so.3")
    endif()
    if(NOT IS_SYMLINK "${WORK}/option/libtwo.so")
        message(FATAL_ERROR "libtwo.so of --soname is no link")
    endif()
    file(READ_SYMLINK "${WORK}/option/libtwo.so" target)
    if(NOT target STREQUAL "libtwo.so.3")
        message(FATAL_ERROR "libtwo.so of --soname links to ${target}")
    endif()
    foreach(pair "plain/libtwo.so|without" "option/libtwo.so.3|with")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 file)
        list(GET parts 1 name)
        execute_process(COMMAND "${READOBJ}" --dynamic-table "${WORK}/${file}"
                        OUTPUT_VARIABLE ${name} RESULT_VARIABLE status
                        ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "llvm-readobj read no ${file}")
        endif()
    endforeach()
    lacks("${without}" "SONAME" "the library without --soname")
    holds("${with}" "Library soname: [libtwo.so.3]" "the library of --soname")
    return()
endif()

# A build of return42.anti twice, each in a directory of its own that
# holds the source: without -o in `plain` and with -o <file> in `option`.
# The two directories then hold what each build wrote beside the source.
function(output_beside_source named)
    foreach(copy plain option)
        file(MAKE_DIRECTORY "${WORK}/${copy}" "${WORK}/out")
        file(COPY "${SOURCES}/programs/return42.anti"
             DESTINATION "${WORK}/${copy}")
    endforeach()
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${ARGN}
                            return42.anti
                    WORKING_DIRECTORY "${WORK}/plain"
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the build without -o ended with ${status}")
    endif()
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${ARGN}
                            -o "${WORK}/out/${named}" return42.anti
                    WORKING_DIRECTORY "${WORK}/option"
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the build with -o ended with ${status}")
    endif()
    files_of("${WORK}/plain" plain)
    files_of("${WORK}/option" option)
    files_of("${WORK}/out" out)
    set(plain "${plain}" PARENT_SCOPE)
    set(option "${option}" PARENT_SCOPE)
    set(out "${out}" PARENT_SCOPE)
endfunction()

function(expect_files what found expected)
    if(NOT found STREQUAL expected)
        message(FATAL_ERROR "${what} holds `${found}`, and the test expects "
                            "`${expected}`")
    endif()
endfunction()

if(CASE STREQUAL "output_assembly")
    output_beside_source(named.s -S)
    expect_files("the directory of the build without -o" "${plain}"
                 "return42.anti return42.s")
    expect_files("the directory of the build with -o" "${option}"
                 "return42.anti")
    expect_files("the directory -o names" "${out}" "named.s")
    return()
endif()

if(CASE STREQUAL "output_library")
    output_beside_source(named.antl -c)
    expect_files("the directory of the build without -o" "${plain}"
                 "return42.anti return42.antl")
    expect_files("the directory of the build with -o" "${option}"
                 "return42.anti")
    expect_files("the directory -o names" "${out}" "named.antl")
    return()
endif()

if(CASE STREQUAL "output_program")
    output_beside_source(named${EXE} --lto none --llvm-mc "${LLVM_MC}")
    foreach(name plain option out)
        # The object of the program stays beside it, under a name of the
        # host.
        string(REGEX REPLACE "[^ ]+\\.(o|obj|pdb|lib|exp)( |$)" "" ${name}
               "${${name}}")
        string(STRIP "${${name}}" ${name})
    endforeach()
    expect_files("the directory of the build without -o" "${plain}"
                 "return42${EXE} return42.anti")
    expect_files("the directory of the build with -o" "${option}"
                 "return42.anti")
    expect_files("the directory -o names" "${out}" "named${EXE}")
    return()
endif()

if(CASE STREQUAL "keep_llvm")
    foreach(copy plain option)
        file(MAKE_DIRECTORY "${WORK}/${copy}")
    endforeach()
    set(source "${SOURCES}/programs/return42.anti")
    antic_output(said ${tools} -o "${WORK}/plain/program${EXE}" "${source}")
    antic_output(said ${tools} --keep-llvm
                 -o "${WORK}/option/program${EXE}" "${source}")
    file(GLOB plain "${WORK}/plain/*.ll")
    file(GLOB option RELATIVE "${WORK}/option" "${WORK}/option/*.ll")
    if(NOT plain STREQUAL "")
        message(FATAL_ERROR "the build without --keep-llvm left ${plain}")
    endif()
    if(NOT option STREQUAL "program${EXE}.ll")
        message(FATAL_ERROR "--keep-llvm left `${option}`, and the test "
                            "expects program${EXE}.ll")
    endif()
    file(READ "${WORK}/option/program${EXE}.ll" text)
    holds("${text}" "target triple = " "the file --keep-llvm left")
    return()
endif()

# The library of the cases below, in a directory of its own under WORK.
# antic runs in that directory and names the source without a directory.
function(library_in directory)
    file(MAKE_DIRECTORY "${WORK}/${directory}")
    file(COPY_FILE "${library_source}" "${WORK}/${directory}/named.anti")
endfunction()

# Run antic in a directory under WORK. It must end with 0.
function(antic_in directory)
    execute_process(COMMAND "${ANTIC}" ${tools} ${ARGN}
                    WORKING_DIRECTORY "${WORK}/${directory}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        string(REPLACE ";" " " line "${ARGN}")
        message(FATAL_ERROR "antic ${line} ended with ${status}\n${out}${err}")
    endif()
endfunction()

# Stop unless the directory under WORK holds the file.
function(wrote directory file)
    if(NOT EXISTS "${WORK}/${directory}/${file}")
        files_of("${WORK}/${directory}" found)
        message(FATAL_ERROR "${directory} holds no ${file}: ${found}")
    endif()
endfunction()

if(CASE STREQUAL "output_static")
    foreach(pair "linux-x86_64|libnamed.a" "windows-x86_64|named.lib")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 target)
        list(GET pair 1 archive)
        library_in(plain-${target})
        library_in(option-${target})
        antic_in(plain-${target} --lib static --target ${target} named.anti)
        antic_in(option-${target} --lib static --target ${target} -o other.a
                 named.anti)
        wrote(plain-${target} ${archive})
        wrote(option-${target} other.a)
        if(EXISTS "${WORK}/option-${target}/${archive}")
            message(FATAL_ERROR "-o other.a left ${archive} for ${target}")
        endif()
    endforeach()
    return()
endif()

if(CASE STREQUAL "output_shared")
    set(pairs "")
    if(EXISTS "${RUNTIME}/sysroot/linux-x86_64")
        list(APPEND pairs "linux-x86_64|libnamed.so")
    endif()
    if(EXISTS "${RUNTIME}/sysroot/windows-x86_64")
        list(APPEND pairs "windows-x86_64|named.dll")
    endif()
    if(CMAKE_HOST_APPLE)
        list(APPEND pairs "macos-arm64|libnamed.dylib")
    endif()
    if(pairs STREQUAL "")
        message(FATAL_ERROR "this host links a shared library of no target")
    endif()
    foreach(pair IN LISTS pairs)
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 target)
        list(GET pair 1 shared)
        library_in(plain-${target})
        library_in(option-${target})
        antic_in(plain-${target} --lib shared --target ${target} named.anti)
        antic_in(option-${target} --lib shared --target ${target}
                 -o other.so named.anti)
        wrote(plain-${target} ${shared})
        wrote(option-${target} other.so)
        if(EXISTS "${WORK}/option-${target}/${shared}")
            message(FATAL_ERROR "-o other.so left ${shared} for ${target}")
        endif()
    endforeach()
    return()
endif()

if(CASE STREQUAL "output_dev_library")
    foreach(copy plain option)
        library_in(${copy})
        antic_in(${copy} -c named.anti)
        file(REMOVE "${WORK}/${copy}/named.anti")
    endforeach()
    antic_in(plain --dev named.antl)
    antic_in(option --dev -o other named.antl)
    files_of("${WORK}/plain" plain)
    files_of("${WORK}/option" option)
    string(REGEX REPLACE "\\.obj( |$)" ".o\\1" plain "${plain}")
    string(REGEX REPLACE "\\.obj( |$)" ".o\\1" option "${option}")
    expect_files("the directory of the build without -o" "${plain}"
                 "named.antl named.o")
    expect_files("the directory of the build with -o" "${option}"
                 "named.antl other.o")
    return()
endif()

if(CASE STREQUAL "output_plugin")
    set(plugins "${SOURCES}/plugin")
    file(MAKE_DIRECTORY "${WORK}/plugin" "${WORK}/net/example")
    antic_in(plugin -c -I "${plugins}" -o "${WORK}/net/example/greet.antl"
             "${plugins}/net/example/greet.anti")
    set(host "")
    if(CMAKE_HOST_WIN32)
        # A plugin of Windows links against the import library of its host.
        file(MAKE_DIRECTORY "${WORK}/host")
        antic_in(host -I "${plugins}" -I "${WORK}" -o "${WORK}/host/host.exe"
                 "${plugins}/host.anti" "${WORK}/net/example/greet.antl")
        set(host "${WORK}/host/host.lib")
    endif()
    antic_in(plugin --lib shared --no-runtime -I "${plugins}" -I "${WORK}"
             -o bare.so "${plugins}/net/example/fancy.anti" ${host})
    wrote(plugin bare.so)
    wrote(plugin anti-plugins.toml)
    file(READ "${WORK}/plugin/anti-plugins.toml" index)
    holds("${index}" "path = 'bare.so'" "the index beside the plugin")
    return()
endif()

if(CASE STREQUAL "linker_lld")
    set(source "${SOURCES}/programs/return42.anti")
    foreach(copy plain option)
        file(MAKE_DIRECTORY "${WORK}/${copy}")
    endforeach()
    antic_output(said ${tools} -o "${WORK}/plain/program${EXE}" "${source}")
    antic_output(said ${tools} --linker lld
                 -o "${WORK}/option/program${EXE}" "${source}")
    file(SHA256 "${WORK}/plain/program${EXE}" plain)
    file(SHA256 "${WORK}/option/program${EXE}" option)
    if(NOT plain STREQUAL option)
        message(FATAL_ERROR "--linker lld wrote another program than the "
                            "default, which is lld")
    endif()
    return()
endif()

message(FATAL_ERROR "`${CASE}` is no case of this runner")
