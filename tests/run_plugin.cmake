# Build a plugin and a host, and run the host. The library is compiled
# with --lib shared --no-runtime, so it carries no runtime and is bound
# against the host that loads it. A Windows library links against the
# import library of its host, so each program there takes a library of
# its own. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
#   EXE       the suffix of an executable on this host
#   WORK      a directory for the objects and the executables
#   EXPECTED  the output of the host
#   PROVIDED  the output of a program whose provider is the library

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "failed with ${status}: ${ARGV}\n${out}${err}")
    endif()
    set(output "${out}" PARENT_SCOPE)
endfunction()

function(same what actual expected)
    file(READ "${expected}" wanted)
    if(NOT actual STREQUAL wanted)
        message(FATAL_ERROR "${what} printed\n${actual}and ${expected} holds\n"
                            "${wanted}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/net/example")
set(interface "net.example.greet.Greeter")

# The directory of the library that program loads, one for all programs
# and one per program on Windows.
function(library_dir program out)
    if(SUFFIX STREQUAL ".dll")
        set(${out} "${WORK}/${program}-lib" PARENT_SCOPE)
    else()
        set(${out} "${WORK}/lib" PARENT_SCOPE)
    endif()
endfunction()

# Build the library that program loads, and on Windows link it against
# the import library of that program. Check the index beside it.
function(plugin program)
    library_dir("${program}" dir)
    set(library "${dir}/libfancy${SUFFIX}")
    if(EXISTS "${library}")
        return()
    endif()
    set(host "")
    if(SUFFIX STREQUAL ".dll")
        set(host "${WORK}/${program}.lib")
    endif()
    file(MAKE_DIRECTORY "${dir}")
    run("${ANTIC}" --lib shared --no-runtime --runtime "${RUNTIME}"
        --llvm-mc "${LLVM_MC}" -I "${SOURCES}" -I "${WORK}" -o "${library}"
        "${SOURCES}/net/example/fancy.anti" ${host})
    if(NOT EXISTS "${dir}/anti-plugins.toml")
        message(FATAL_ERROR "the plugin has no index beside it")
    endif()
    file(READ "${dir}/anti-plugins.toml" index)
    if(NOT index MATCHES "interfaces = \\['${interface}'\\]")
        message(FATAL_ERROR "the index names no interface\n${index}")
    endif()
    file(SHA256 "${library}" digest)
    if(NOT index MATCHES "digest = '${digest}'")
        message(FATAL_ERROR "the index holds another digest\n${index}")
    endif()
endfunction()

# The interface is a library file that both sides import.
run("${ANTIC}" -c -I "${SOURCES}" -o "${WORK}/net/example/greet.antl"
    "${SOURCES}/net/example/greet.anti")

# The host loads the library, asks it for the interface and closes it.
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" -o "${WORK}/host${EXE}" "${SOURCES}/host.anti")
plugin(host)
library_dir(host dir)
run("${WORK}/host${EXE}" "${dir}/libfancy${SUFFIX}")
same("the host" "${output}" "${EXPECTED}")

# `--closed` builds a program without the exports a plugin binds
# against, so the load of the same library fails. It stands in a
# directory of its own, where Windows finds no other program by the name
# the imports of the library give.
file(MAKE_DIRECTORY "${WORK}/closed")
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --closed -o "${WORK}/closed/closed${EXE}"
    "${SOURCES}/host.anti")
execute_process(COMMAND "${WORK}/closed/closed${EXE}" "${dir}/libfancy${SUFFIX}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "a closed program loaded the library\n${out}")
endif()
# It takes no provider from a library either, which the link says.
execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}"
                        --llvm-mc "${LLVM_MC}" -I "${SOURCES}" -I "${WORK}"
                        --closed
                        --inject "${interface}=plugin:${dir}/libfancy${SUFFIX}"
                        -o "${WORK}/shut${EXE}" "${SOURCES}/user.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "names a library, and `--closed` loads")
    message(FATAL_ERROR "a closed build took a library provider\n${err}")
endif()

# `plugin:<path>` of the manifest fills the slot of an injected field
# from that library, and `discover` finds it through the index.
library_dir(named dir)
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=plugin:${dir}/libfancy${SUFFIX}"
    -o "${WORK}/named${EXE}" "${SOURCES}/user.anti")
plugin(named)
run("${WORK}/named${EXE}")
same("the program with a named library" "${output}" "${PROVIDED}")
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=discover" -o "${WORK}/found${EXE}"
    "${SOURCES}/user.anti")
plugin(found)
library_dir(found dir)
run("${WORK}/found${EXE}" "--anti.plugins=${dir}")
same("the program that discovers a library" "${output}" "${PROVIDED}")

# Discovery without a directory to search names the interface, and a
# library whose digest is not the one the index records is refused.
execute_process(COMMAND "${WORK}/found${EXE}" RESULT_VARIABLE status
                ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "no library of the search directories")
    message(FATAL_ERROR "discovery without a directory gave ${status}\n${err}")
endif()
file(READ "${dir}/anti-plugins.toml" index)
file(SHA256 "${dir}/libfancy${SUFFIX}" digest)
string(REPLACE "digest = '${digest}'"
       "digest = '0000000000000000000000000000000000000000000000000000000000000000'"
       broken "${index}")
file(WRITE "${dir}/anti-plugins.toml" "${broken}")
execute_process(COMMAND "${WORK}/found${EXE}" "--anti.plugins=${dir}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "does not match the digest")
    message(FATAL_ERROR "a broken digest gave ${status}\n${err}")
endif()
