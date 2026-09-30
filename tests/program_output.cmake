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

# Build <exe> from <source> with the ANTIC, LLVM_MC and RUNTIME of the
# runner, passing the arguments after <source> to antic first. antic must
# succeed and print nothing: warnings are errors, as run_program.cmake
# says.
function(antic_program exe source)
    execute_process(COMMAND "${ANTIC}" ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${exe}" "${source}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic gave ${status} for ${exe}\n${out}${err}")
    endif()
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
#                    then every byte of the standard output
#   ERR, ERR_FILE, ERR_MATCH and ANY_ERR the same for the standard error
function(program_expect label)
    cmake_parse_arguments(PARSE_ARGV 1 arg "ABORTS;ANY_OUT;ANY_ERR"
        "STATUS;OUT;OUT_FILE;OUT_MATCH;ERR;ERR_FILE;ERR_MATCH;EXPECTED"
        "COMMAND")
    set(skip_OUT 0)
    set(skip_ERR 0)
    if(DEFINED arg_EXPECTED)
        file(READ "${arg_EXPECTED}" expected)
        if(NOT expected MATCHES "^exit ([0-9]+)\n")
            message(FATAL_ERROR "${arg_EXPECTED} does not start with 'exit N'")
        endif()
        set(arg_STATUS "${CMAKE_MATCH_1}")
        set(arg_OUT_FILE "${arg_EXPECTED}")
        string(LENGTH "exit ${arg_STATUS}\n" skip_OUT)
    endif()
    string(MAKE_C_IDENTIFIER "${label}" name)
    set(base "${WORK}/${name}")
    execute_process(COMMAND ${arg_COMMAND} RESULT_VARIABLE code
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
        if(NOT bytes STREQUAL wanted)
            message(FATAL_ERROR "${what}\nthe standard ${suffix} differs: "
                                "expected the bytes ${wanted}, got ${bytes}")
        endif()
    endforeach()
endfunction()
