# The runtime writes the text of a float from the digits of its exact
# value and reads one back with its own reader, never through the C
# library, whose rounding of a tie depends on its version. In every runtime
# of the archive the object of rt/text.c, which holds both, and of
# rt/object.c, which serialize writes with, name no function that prints
# or reads a number. The object of rt/registry.c, which deserialize reads
# with, names none that reads a float.
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
    # The runtime of the host is built by CMake, which names a member
    # text.c.o or text.c.obj. The one of another target is text.o.
    execute_process(COMMAND "${LLVM_AR}" t "${archive}"
        RESULT_VARIABLE status OUTPUT_VARIABLE members ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${archive} does not list\n${err}")
    endif()
    string(REPLACE "\r" "" members "${members}")
    string(REPLACE "\n" ";" members "${members}")
    foreach(source text object registry)
        if(source STREQUAL "registry")
            set(refused "scanf|strto[df]")
        else()
            set(refused "printf|scanf|strto[dfl]")
        endif()
        set(member "")
        foreach(name IN LISTS members)
            if(name MATCHES "^${source}(\\.c)?\\.o(bj)?$")
                set(member "${name}")
            endif()
        endforeach()
        if(member STREQUAL "")
            message(FATAL_ERROR "${archive} holds no object of rt/${source}.c")
        endif()
        execute_process(COMMAND "${LLVM_AR}" x "${archive}" "${member}"
            WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status
            ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0 OR NOT EXISTS "${dir}/${member}")
            message(FATAL_ERROR
                "${member} of ${archive} does not extract\n${err}")
        endif()
        file(STRINGS "${dir}/${member}" names REGEX "${refused}")
        if(NOT names STREQUAL "")
            string(APPEND found "${archive} rt/${source}.c: ${names}\n")
        endif()
    endforeach()
    math(EXPR count "${count} + 1")
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "the text of a float calls the C library\n${found}")
endif()
message("${count} runtimes write and read the digits of a float themselves")
