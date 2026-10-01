# The runtime and `anti symbols` read the includes of a runtime
# configuration with walks of their own, and both stop at one depth with
# one message. A file may stand below 32 others, and a file below 33
# others ends both. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   ANTI     the anti executable
#   LLVM_MC  the llvm-mc executable
#   RUNTIME  the runtime archive
#   SOURCE   a program that ends with status 0
#   HOST     the target name of this host
#   WORK     a directory this run writes into

set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
set(failures "")

# A chain of count files in dir, c0.toml at its head. Each includes the
# next, and the last one sets a key.
function(write_chain dir count)
    file(MAKE_DIRECTORY "${dir}")
    math(EXPR last "${count} - 1")
    foreach(i RANGE 0 ${last})
        math(EXPR next "${i} + 1")
        if(i LESS last)
            file(WRITE "${dir}/c${i}.toml" "include = \"c${next}.toml\"\n")
        else()
            file(WRITE "${dir}/c${i}.toml" "[runtime]\nthreads = 2\n")
        endif()
    endforeach()
    execute_process(
        COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
                -o "${dir}/report${exe}" "${SOURCE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
    endif()
endfunction()

# Run the program and the inventory over the chain in dir. status is
# the status the program ends with, and message the one both print, or
# empty for none.
function(expect_chain dir status message)
    execute_process(
        COMMAND "${dir}/report${exe}" "--anti.conf=${dir}/c0.toml"
        RESULT_VARIABLE got OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT got EQUAL status OR (message STREQUAL "" AND NOT err STREQUAL ""))
        string(APPEND failures
               "the program over ${dir} ended with ${got}\n${err}\n")
    endif()
    if(NOT message STREQUAL "" AND NOT err MATCHES "${message}")
        string(APPEND failures "the program over ${dir} said\n${err}\n")
    endif()
    execute_process(
        COMMAND "${ANTI}" symbols inventory --conf "${dir}/c0.toml"
                --out "${dir}/all.zip"
        RESULT_VARIABLE got OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(message STREQUAL "" AND NOT got EQUAL 0)
        string(APPEND failures
               "the inventory over ${dir} ended with ${got}\n${out}${err}\n")
    endif()
    if(NOT message STREQUAL "" AND
       (got EQUAL 0 OR NOT err MATCHES "${message}"))
        string(APPEND failures
               "the inventory over ${dir} ended with ${got}\n${err}\n")
    endif()
    set(failures "${failures}" PARENT_SCOPE)
endfunction()

# 33 files: the last one stands below 32 others.
write_chain("${WORK}/deep" 33)
expect_chain("${WORK}/deep" 0 "")
# 34 files: the last one stands below 33 others.
write_chain("${WORK}/deeper" 34)
expect_chain("${WORK}/deeper" 70
             "anti: the configuration files include one another at [^\n]*c33\\.toml")
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
