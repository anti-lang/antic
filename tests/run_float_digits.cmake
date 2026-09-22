# The runtime writes the text of a float from the digits of its exact
# value and never through the C library, whose rounding of a tie depends on
# its version. The object of rt/text.c in every runtime of the archive
# therefore names no function that prints or reads a number.
#
#   cmake -DRUNTIME=<runtime archive> -DLLVM_AR=<llvm-ar> -DWORK=<dir>
#         -P tests/run_float_digits.cmake

file(GLOB_RECURSE archives "${RUNTIME}/lib/libanti_rt.a"
     "${RUNTIME}/lib/anti_rt.lib")
if(archives STREQUAL "")
    message(FATAL_ERROR "no runtime library under ${RUNTIME}/lib")
endif()
set(found "")
set(count 0)
foreach(archive IN LISTS archives)
    string(MD5 key "${archive}")
    set(dir "${WORK}/${key}")
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}")
    # The runtime of the host is built by CMake, which names the member
    # text.c.o or text.c.obj. The one of another target is text.o.
    execute_process(COMMAND "${LLVM_AR}" t "${archive}"
        RESULT_VARIABLE status OUTPUT_VARIABLE members ERROR_VARIABLE err
        ENCODING NONE)
    string(REPLACE "\r" "" members "${members}")
    string(REPLACE "\n" ";" members "${members}")
    set(member "")
    foreach(name IN LISTS members)
        if(name MATCHES "^text(\\.c)?\\.o(bj)?$")
            set(member "${name}")
        endif()
    endforeach()
    if(NOT status EQUAL 0 OR member STREQUAL "")
        message(FATAL_ERROR "${archive} holds no object of rt/text.c\n${err}")
    endif()
    execute_process(COMMAND "${LLVM_AR}" x "${archive}" "${member}"
        WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status
        ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT EXISTS "${dir}/${member}")
        message(FATAL_ERROR "${member} of ${archive} does not extract\n${err}")
    endif()
    file(STRINGS "${dir}/${member}" names REGEX "printf|scanf|strto[dfl]")
    if(NOT names STREQUAL "")
        string(APPEND found "${archive}: ${names}\n")
    endif()
    math(EXPR count "${count} + 1")
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "the text of a float calls the C library\n${found}")
endif()
message("${count} runtimes write the digits of a float themselves")
