# Compile every C file of src/rt with the warnings of our own code and
# the one of ANTIC_FORMAT_ATTRIBUTE_WARNINGS, and refuse a warning. A
# function that formats like printf then carries the format attribute of
# platform.h, and clang checks the arguments of every call of it. Run
# with cmake -P and these values:
#   CC        the C compiler and its flags for the host, as a list
#   FLAGS     the warning flags, as a list
#   PCRE2     the directory of pcre2.h
#   SOURCES   src/rt
#   WORK      a directory for the log

cmake_minimum_required(VERSION 3.21)

file(MAKE_DIRECTORY "${WORK}")
file(GLOB files "${SOURCES}/*.c")
set(failures "")
foreach(file IN LISTS files)
    execute_process(
        COMMAND ${CC} -std=c11 -fsyntax-only ${FLAGS} -DANTI_CPU_LEVEL_ID=0
            -I "${SOURCES}" -isystem "${PCRE2}" "${file}"
        RESULT_VARIABLE status
        ERROR_VARIABLE errors
        OUTPUT_QUIET
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT errors STREQUAL "")
        string(APPEND failures "${errors}")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    file(WRITE "${WORK}/rt-format.log" "${failures}")
    message(FATAL_ERROR "the runtime does not compile without a warning "
                        "of the format attribute:\n${failures}")
endif()
