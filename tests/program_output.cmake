# The functions every runner includes. They run a program, antic and anti
# with the output in files, write bytes and compare bytes and texts, build
# a program and check its output and status, and fake a release for the
# tests of the installer.

# DESIGN: the raw-bytes rule. The harness reads the standard output and
# the standard error of a program as the bytes of a file, on every host.
# An OUTPUT_VARIABLE or an ERROR_VARIABLE loses the CR of each CRLF and
# every NUL, and a file keeps them. The test raw_output refuses a runner
# that reads a program it built into a variable.

# Run the command after <file> with its standard output in <file>. Set
# <hex> to those bytes in hexadecimal and <status> to its exit status.
function(program_output hex status file)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_FILE "${file}")
    file(READ "${file}" bytes HEX)
    set(${hex} "${bytes}" PARENT_SCOPE)
    set(${status} "${code}" PARENT_SCOPE)
endfunction()

# Write <text> into <path> as its bytes, and the directories above it.
# DESIGN: file(WRITE) writes CRLF for each LF on Windows, so a file whose
# bytes a case compares, hashes or counts the lines of differs there
# alone. `cmake -E echo_append` into an OUTPUT_FILE writes the bytes of
# its argument on every host. A CMake text holds no NUL, and a case that
# needs one writes it with poke_byte.
function(write_bytes path text)
    get_filename_component(directory "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo_append "${text}"
                    OUTPUT_FILE "${path}" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "cannot write ${path}: ${status}")
    endif()
endfunction()

# Set <out> to the line of <hex> made readable: the bytes of printable
# characters as they are and every other byte as \xHH, without the line
# feed at its end. A line that has none ends in " (no line end)". A line
# longer than 300 bytes is cut there.
function(readable_line out hex)
    set(end "")
    if(hex MATCHES "^(.*)0a$")
        set(hex "${CMAKE_MATCH_1}")
    else()
        set(end " (no line end)")
    endif()
    string(LENGTH "${hex}" length)
    if(length GREATER 600)
        string(SUBSTRING "${hex}" 0 600 hex)
        set(end "...${end}")
    endif()
    string(REGEX MATCHALL ".." bytes "${hex}")
    set(text "")
    foreach(byte IN LISTS bytes)
        math(EXPR code "0x${byte}")
        if(code GREATER_EQUAL 32 AND code LESS 127 AND NOT code EQUAL 92)
            string(ASCII ${code} char)
            string(APPEND text "${char}")
        else()
            string(APPEND text "\\x${byte}")
        endif()
    endforeach()
    set(${out} "${text}${end}" PARENT_SCOPE)
endfunction()

# Set <out> to the empty text when the hexadecimal <got> holds the bytes
# of <wanted>, both as file(READ ... HEX) and string(HEX) write them, and
# otherwise to a text that names <what> and the first line that differs,
# as each side has it, with the line before it and the line after it,
# once when the two sides agree on it.
# DESIGN: one comparison for every runner. A runner that printed the whole
# new text, or both sides in hexadecimal, left the reader to find the
# line, and a golden was written again without that reading.
function(bytes_difference out what wanted got)
    set(${out} "" PARENT_SCOPE)
    if(got STREQUAL wanted)
        return()
    endif()
    foreach(side wanted got)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\1 " spaced "${${side}}")
        string(REPLACE "0a " "0a;" spaced "${spaced}")
        string(REPLACE " " "" ${side}_lines "${spaced}")
        list(LENGTH ${side}_lines ${side}_count)
    endforeach()
    set(number 0)
    set(before "")
    foreach(w g IN ZIP_LISTS wanted_lines got_lines)
        math(EXPR number "${number} + 1")
        if(NOT w STREQUAL g)
            break()
        endif()
        set(before "${w}")
    endforeach()
    string(CONCAT text "${what} differs at line ${number}:\n")
    if(number GREATER 1)
        math(EXPR previous "${number} - 1")
        readable_line(shown "${before}")
        string(APPEND text "  line ${previous}:    ${shown}\n")
    endif()
    foreach(side wanted got)
        if(side STREQUAL "wanted")
            set(label "expected:")
        else()
            set(label "got:     ")
        endif()
        math(EXPR index "${number} - 1")
        set(line "")
        if(index LESS ${side}_count)
            list(GET ${side}_lines ${index} line)
        endif()
        # Only the element after the last line feed is empty.
        if(line STREQUAL "")
            set(shown "the end of the text")
        else()
            readable_line(shown "${line}")
        endif()
        string(APPEND text "  ${label}  ${shown}\n")
    endforeach()
    # The line after it, once when both sides hold the same.
    math(EXPR next "${number} + 1")
    foreach(side wanted got)
        set(after_${side} "")
        if(number LESS ${side}_count)
            list(GET ${side}_lines ${number} after_${side})
        endif()
    endforeach()
    if(after_wanted STREQUAL after_got)
        if(NOT after_wanted STREQUAL "")
            readable_line(shown "${after_wanted}")
            string(APPEND text "  line ${next}:    ${shown}\n")
        endif()
    else()
        foreach(side wanted got)
            if(NOT after_${side} STREQUAL "")
                readable_line(shown "${after_${side}}")
                if(side STREQUAL "wanted")
                    string(APPEND text "  then expected:  ${shown}\n")
                else()
                    string(APPEND text "  then got:       ${shown}\n")
                endif()
            endif()
        endforeach()
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Fail unless the hexadecimal <got> holds the bytes of <wanted>, with the
# text of bytes_difference.
function(expect_same_bytes what wanted got)
    bytes_difference(text "${what}" "${wanted}" "${got}")
    if(NOT text STREQUAL "")
        message(FATAL_ERROR "${text}")
    endif()
endfunction()

# Fail unless the text <got> equals <wanted>, through expect_same_bytes.
function(expect_same_text what wanted got)
    string(HEX "${wanted}" wanted_hex)
    string(HEX "${got}" got_hex)
    expect_same_bytes("${what}" "${wanted_hex}" "${got_hex}")
endfunction()

# DESIGN: an expected exit status is the one POSIX shows, which is the
# low eight bits of what the program returned. Windows reports all
# thirty-two, so a program that returns 521 gives 521 there and 9 on
# Linux. Set <out> to <code> with the low eight bits taken on a Windows
# host, and to <code> unchanged otherwise.
function(exit_status out code)
    if(CMAKE_HOST_WIN32 AND code MATCHES "^-?[0-9]+$")
        math(EXPR code "((${code}) % 256 + 256) % 256")
    endif()
    set(${out} "${code}" PARENT_SCOPE)
endfunction()

# Run antic and set <status>, <out> and <err> to its exit status, its
# standard output and its standard error. The arguments of antic follow
# COMMAND, and these keywords stand before it:
#   IN <dir>             run it in dir
#   TIMEOUT <s>          stop it after s seconds
#   OUTPUT_FILE <file>   its standard output goes to file, and <out> is empty
#   THROUGH <command>... run it as the last word of command, such as
#                        `cmake -E env` with the settings of the case
# DESIGN: the one way a runner runs antic. antic_build holds a build that
# must succeed, and a call of antic_run reads what antic said. 49 builds
# in 31 runners read the status alone and passed a warning of antic or
# of the linker, which fails the same build of one module. The test
# antic_calls refuses an execute_process of antic in a runner.
function(antic_run status out err)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "IN;TIMEOUT;OUTPUT_FILE"
                          "THROUGH;COMMAND")
    if(DEFINED arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "antic_run takes the arguments of antic after "
                            "COMMAND: ${arg_UNPARSED_ARGUMENTS}")
    endif()
    set(options "")
    if(DEFINED arg_IN)
        list(APPEND options WORKING_DIRECTORY "${arg_IN}")
    endif()
    if(DEFINED arg_TIMEOUT)
        list(APPEND options TIMEOUT "${arg_TIMEOUT}")
    endif()
    set(stdout "")
    if(DEFINED arg_OUTPUT_FILE)
        execute_process(COMMAND ${arg_THROUGH} "${ANTIC}" ${arg_COMMAND}
                        ${options} RESULT_VARIABLE code
                        OUTPUT_FILE "${arg_OUTPUT_FILE}"
                        ERROR_VARIABLE stderr ENCODING NONE)
    else()
        execute_process(COMMAND ${arg_THROUGH} "${ANTIC}" ${arg_COMMAND}
                        ${options} RESULT_VARIABLE code
                        OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
                        ENCODING NONE)
    endif()
    set(${status} "${code}" PARENT_SCOPE)
    set(${out} "${stdout}" PARENT_SCOPE)
    set(${err} "${stderr}" PARENT_SCOPE)
endfunction()

# Run antic with the arguments, which must end with 0 and print nothing:
# warnings are errors, as run_program.cmake says. The keywords of
# antic_run stand before the arguments, which then follow COMMAND, and
#   OUTPUT <var>         sets var to the standard output, which a dump
#                        prints, and only the standard error must be empty
function(antic_build)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "IN;TIMEOUT;OUTPUT_FILE;OUTPUT"
                          "THROUGH;COMMAND")
    set(arguments ${arg_UNPARSED_ARGUMENTS} ${arg_COMMAND})
    set(options "")
    foreach(keyword IN ITEMS IN TIMEOUT OUTPUT_FILE)
        if(DEFINED arg_${keyword})
            list(APPEND options ${keyword} "${arg_${keyword}}")
        endif()
    endforeach()
    if(DEFINED arg_THROUGH)
        list(APPEND options THROUGH ${arg_THROUGH})
    endif()
    antic_run(code stdout stderr ${options} COMMAND ${arguments})
    # DESIGN: a sanitizer build of antic run without the programs of the
    # system on PATH finds no symbolizer, and the runtime of the sanitizer
    # starts with two lines of its own. They are no output of antic and
    # leave, as in run_anti_messages.cmake. Every other line stays.
    string(REGEX REPLACE
           "==[0-9]+==WARN: No external symbolizers found[^\n]*\n==[0-9]+==HINT: Is PATH set[^\n]*\n"
           "" stderr "${stderr}")
    set(printed "${stdout}")
    if(DEFINED arg_OUTPUT)
        set(${arg_OUTPUT} "${stdout}" PARENT_SCOPE)
        set(printed "")
    endif()
    if(NOT code EQUAL 0 OR NOT printed STREQUAL "" OR NOT stderr STREQUAL "")
        list(JOIN arguments " " line)
        message(FATAL_ERROR "antic ${line} gave ${code} and printed:\n"
                            "${printed}${stderr}")
    endif()
endfunction()

# Run the command after <what>, a tool of the harness, which must end with
# 0. <what> names it in the message of a failure, and the empty text names
# it by its command line. QUIET after <what> also refuses a tool that
# writes on its standard error, and IN <dir> runs it in dir. The caller
# gets the two outputs in tool_stdout and tool_stderr.
# DESIGN: one helper for the tools a runner runs beside antic. Eight
# runners each had a function run of their own, with four contracts.
function(tool_run what)
    set(command ${ARGN})
    set(quiet FALSE)
    set(where "")
    if(command MATCHES "^QUIET(;|$)")
        set(quiet TRUE)
        list(REMOVE_AT command 0)
    endif()
    if(command MATCHES "^IN;")
        list(GET command 1 directory)
        set(where WORKING_DIRECTORY "${directory}")
        list(REMOVE_AT command 0 1)
    endif()
    execute_process(COMMAND ${command} ${where} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(what STREQUAL "")
        list(JOIN command " " what)
    endif()
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} ended with ${status}\n${out}${err}")
    endif()
    if(quiet AND NOT err STREQUAL "")
        message(FATAL_ERROR "${what} wrote on its standard error\n${err}")
    endif()
    set(tool_stdout "${out}" PARENT_SCOPE)
    set(tool_stderr "${err}" PARENT_SCOPE)
endfunction()

# Run the ANTI of the runner in <dir> with the arguments after it, with a
# cache of its own in <WORK>/cache, and set <status>, <out> and <err> to
# its exit status, its standard output and its standard error. With ENV
# <var=value>... first, those settings join the environment, and the
# arguments follow COMMAND.
# DESIGN: the one call of anti with a cache of its own. Ten places wrote
# it out under four signatures, and one of them left out an option its
# neighbour passed.
function(anti_run status out err dir)
    set(arguments ${ARGN})
    set(settings "")
    if(arguments MATCHES "^ENV;")
        cmake_parse_arguments(PARSE_ARGV 4 arg "" "" "ENV;COMMAND")
        set(settings ${arg_ENV})
        set(arguments ${arg_COMMAND})
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
                            "XDG_CACHE_HOME=${WORK}/cache"
                            "LOCALAPPDATA=${WORK}/cache" ${settings}
                            "${ANTI}" ${arguments}
                    WORKING_DIRECTORY "${dir}" RESULT_VARIABLE code
                    OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
                    ENCODING NONE)
    set(${status} "${code}" PARENT_SCOPE)
    set(${out} "${stdout}" PARENT_SCOPE)
    set(${err} "${stderr}" PARENT_SCOPE)
endfunction()

# The fake release of the installer tests, which install.sh takes from
# file:// addresses. fake_release_host sets version, host and asset of
# the caller to a package of version 99.0.0 for this machine, and skip to
# the reason the test cannot run here, or to the empty text. A script of
# cmake -P carries no processor, so it comes from uname, as it does in the
# installer.
# DESIGN: one fake release for the three tests of the installer, which
# each wrote it out.
function(fake_release_host)
    set(skip "" PARENT_SCOPE)
    if(CMAKE_HOST_WIN32)
        set(skip "install.sh needs a shell" PARENT_SCOPE)
        return()
    endif()
    find_program(OPENSSL openssl)
    find_program(CURL curl)
    foreach(tool OPENSSL CURL)
        if(NOT ${tool})
            string(TOLOWER "${tool}" name)
            set(skip "${name} is missing" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(version "99.0.0")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(os macos)
    else()
        set(os linux)
    endif()
    execute_process(COMMAND uname -m OUTPUT_VARIABLE arch
                    OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING NONE)
    if(arch STREQUAL "aarch64")
        set(arch arm64)
    endif()
    if(NOT arch MATCHES "^(arm64|x86_64)$")
        set(skip "no package for the processor ${arch}" PARENT_SCOPE)
        return()
    endif()
    set(version "${version}" PARENT_SCOPE)
    set(host "${os}-${arch}" PARENT_SCOPE)
    set(asset "anti-${version}-${os}-${arch}.tar.xz" PARENT_SCOPE)
endfunction()

# Write <area>/<asset> and <area>/SHA256SUMS: a package of the two
# programs the installer runs as the check of the install, as scripts in
# <tree> that print <printed> as their version, and nothing else. The
# installer reads no script and no pin of a package, so the test reaches
# no server.
function(fake_release_package area tree printed)
    file(MAKE_DIRECTORY "${tree}/anti/bin" "${area}")
    foreach(program antic anti)
        file(WRITE "${tree}/anti/bin/${program}"
             "#!/bin/sh\necho \"${program} ${printed}\"\n")
    endforeach()
    file(CHMOD "${tree}/anti/bin/antic" "${tree}/anti/bin/anti"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                     GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    tool_run("the package did not pack" "${CMAKE_COMMAND}" -E chdir "${tree}"
             "${CMAKE_COMMAND}" -E tar cJf "${area}/${asset}" anti)
    file(SHA256 "${area}/${asset}" digest)
    file(WRITE "${area}/SHA256SUMS" "${digest}  ${asset}\n")
endfunction()

# Write the key of the test, <WORK>/private.pem and <WORK>/public.pem, and
# <WORK>/install.sh, the installer with the public half in place of the
# key of the release, whose text the caller gets in installer.
function(fake_release_key)
    tool_run("openssl wrote no private key" "${OPENSSL}" ecparam
             -name prime256v1 -genkey -noout -out "${WORK}/private.pem")
    tool_run("openssl wrote no public key" "${OPENSSL}" ec
             -in "${WORK}/private.pem" -pubout -out "${WORK}/public.pem")
    file(READ "${WORK}/public.pem" public)
    string(STRIP "${public}" public)
    file(READ "${ROOT}/tools/install.sh" text)
    set(block
        "-----BEGIN PUBLIC KEY-----[A-Za-z0-9+/=\n]+-----END PUBLIC KEY-----")
    if(NOT text MATCHES "${block}")
        message(FATAL_ERROR "tools/install.sh holds no public key to replace")
    endif()
    string(REGEX REPLACE "${block}" "${public}" text "${text}")
    file(WRITE "${WORK}/install.sh" "${text}")
    set(installer "${text}" PARENT_SCOPE)
endfunction()

# Sign <manifest> with the key of the test into <signature>, as step 6 of
# ./r signs SHA256SUMS.
function(fake_release_sign manifest signature)
    tool_run("openssl hashed nothing" "${OPENSSL}" dgst -sha256 -binary
             -out "${WORK}/manifest.sha256" "${manifest}")
    tool_run("openssl signed nothing" "${OPENSSL}" pkeyutl -sign
             -inkey "${WORK}/private.pem" -in "${WORK}/manifest.sha256"
             -out "${signature}")
endfunction()

# Build <exe> from <source> with the ANTIC and RUNTIME of the runner,
# passing the arguments after <source> to antic first, through
# antic_build.
function(antic_program exe source)
    antic_build(${ARGN} --runtime "${RUNTIME}"
                -o "${exe}" "${source}")
endfunction()

# Run a program and check what it did. Its standard output and standard
# error go to <WORK>/<label>.stdout and .stderr, with the label made an
# identifier, and each is compared as bytes. Without a keyword for it,
# the program exits with 0 and both outputs are empty. The caller gets
# the two outputs in program_stdout and program_stderr.
#   COMMAND <program> <arguments>...
#   STATUS <n>       the exit status, as exit_status gives it
#   ABORTS           the program aborts, as the failure routine of the
#                    runtime does, instead of exiting
#   OUT <text>       the whole standard output
#   OUT_FILE <file>  a file that holds the whole standard output
#   OUT_MATCH <re>   a pattern the standard output matches
#   ANY_OUT          the caller reads the standard output itself
#   EXPECTED <file>  an expected file: the line "exit N" with the status,
#                    or the line "abort" for ABORTS, then every byte of the
#                    standard output
#   ERR, ERR_FILE, ERR_MATCH and ANY_ERR the same for the standard error
#   TIMEOUT <s>      the seconds the program may run. One that runs longer
#                    is stopped, and its status is then no number
#   IN <dir>         the directory the program runs in
function(program_expect label)
    cmake_parse_arguments(PARSE_ARGV 1 arg "ABORTS;ANY_OUT;ANY_ERR"
        "STATUS;OUT;OUT_FILE;OUT_MATCH;ERR;ERR_FILE;ERR_MATCH;EXPECTED;TIMEOUT;IN"
        "COMMAND")
    set(limit "")
    if(DEFINED arg_TIMEOUT)
        set(limit TIMEOUT "${arg_TIMEOUT}")
    endif()
    if(DEFINED arg_IN)
        list(APPEND limit WORKING_DIRECTORY "${arg_IN}")
    endif()
    set(skip_OUT 0)
    set(skip_ERR 0)
    if(DEFINED arg_EXPECTED)
        file(READ "${arg_EXPECTED}" expected)
        if(expected MATCHES "^abort\n")
            set(arg_ABORTS TRUE)
            string(LENGTH "abort\n" skip_OUT)
        elseif(expected MATCHES "^exit ([0-9]+)\n")
            set(arg_STATUS "${CMAKE_MATCH_1}")
            string(LENGTH "exit ${arg_STATUS}\n" skip_OUT)
        else()
            message(FATAL_ERROR "${arg_EXPECTED} does not start with "
                                "'exit N' or 'abort'")
        endif()
        set(arg_OUT_FILE "${arg_EXPECTED}")
    endif()
    string(MAKE_C_IDENTIFIER "${label}" name)
    set(base "${WORK}/${name}")
    execute_process(COMMAND ${arg_COMMAND} RESULT_VARIABLE code ${limit}
                    OUTPUT_FILE "${base}.stdout" ERROR_FILE "${base}.stderr")
    file(READ "${base}.stdout" out)
    file(READ "${base}.stderr" err)
    set(program_stdout "${out}" PARENT_SCOPE)
    set(program_stderr "${err}" PARENT_SCOPE)
    string(CONCAT what "${label} gave `${code}`\nstandard output:\n${out}\n"
                       "standard error:\n${err}")

    # DESIGN: abort() raises SIGABRT on POSIX, which CMake reports as
    # "Subprocess aborted". abort() of the C runtime of Windows ends the
    # process with a fast fail, which CMake reports as the status
    # 0xc0000409.
    if(arg_ABORTS)
        if(NOT code STREQUAL "Subprocess aborted" AND
           NOT code STREQUAL "Exit code 0xc0000409")
            message(FATAL_ERROR "${what}\nexpected an abort")
        endif()
    else()
        if(NOT DEFINED arg_STATUS)
            set(arg_STATUS 0)
        endif()
        exit_status(code "${code}")
        if(NOT code STREQUAL arg_STATUS)
            message(FATAL_ERROR "${what}\nexpected the status ${arg_STATUS}")
        endif()
    endif()

    foreach(stream OUT ERR)
        if(stream STREQUAL "OUT")
            set(got "${out}")
            set(file "${base}.stdout")
            set(suffix output)
        else()
            set(got "${err}")
            set(file "${base}.stderr")
            set(suffix error)
        endif()
        if(arg_ANY_${stream})
            continue()
        endif()
        if(DEFINED arg_${stream}_MATCH)
            if(NOT got MATCHES "${arg_${stream}_MATCH}")
                message(FATAL_ERROR "${what}\nthe standard ${suffix} does "
                                    "not match `${arg_${stream}_MATCH}`")
            endif()
            continue()
        endif()
        if(DEFINED arg_${stream}_FILE)
            file(READ "${arg_${stream}_FILE}" wanted OFFSET ${skip_${stream}}
                 HEX)
        else()
            string(HEX "${arg_${stream}}" wanted)
        endif()
        file(READ "${file}" bytes HEX)
        expect_same_bytes("${what}\nthe standard ${suffix}" "${wanted}"
                          "${bytes}")
    endforeach()
endfunction()
