# Build a plugin and a host, and run the host. The library is compiled
# with --lib shared --no-runtime, so it carries no runtime and is bound
# against the host that loads it. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
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
set(library "${WORK}/libfancy${SUFFIX}")
set(interface "net.example.greet.Greeter")

# The interface is a library file that both sides import.
run("${ANTIC}" -c -I "${SOURCES}" -o "${WORK}/net/example/greet.antl"
    "${SOURCES}/net/example/greet.anti")
run("${ANTIC}" --lib shared --no-runtime --runtime "${RUNTIME}"
    --llvm-mc "${LLVM_MC}" -I "${SOURCES}" -I "${WORK}" -o "${library}"
    "${SOURCES}/net/example/fancy.anti")
if(NOT EXISTS "${WORK}/anti-plugins.toml")
    message(FATAL_ERROR "the plugin has no index beside it")
endif()
file(READ "${WORK}/anti-plugins.toml" index)
if(NOT index MATCHES "interfaces = \\['${interface}'\\]")
    message(FATAL_ERROR "the index names no interface\n${index}")
endif()
file(SHA256 "${library}" digest)
if(NOT index MATCHES "digest = '${digest}'")
    message(FATAL_ERROR "the index holds another digest\n${index}")
endif()

# The host loads the library, asks it for the interface and closes it.
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" -o "${WORK}/host" "${SOURCES}/host.anti")
run("${WORK}/host" "${library}")
same("the host" "${output}" "${EXPECTED}")

# `plugin:<path>` of the manifest fills the slot of an injected field
# from that library, and `discover` finds it through the index.
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=plugin:${library}"
    -o "${WORK}/named" "${SOURCES}/user.anti")
run("${WORK}/named")
same("the program with a named library" "${output}" "${PROVIDED}")
run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=discover" -o "${WORK}/found"
    "${SOURCES}/user.anti")
run("${WORK}/found" "--anti.plugins=${WORK}")
same("the program that discovers a library" "${output}" "${PROVIDED}")

# Discovery without a directory to search names the interface, and a
# library whose digest is not the one the index records is refused.
execute_process(COMMAND "${WORK}/found" RESULT_VARIABLE status
                ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "no library of the search directories")
    message(FATAL_ERROR "discovery without a directory gave ${status}\n${err}")
endif()
string(REPLACE "digest = '${digest}'"
       "digest = '0000000000000000000000000000000000000000000000000000000000000000'"
       broken "${index}")
file(WRITE "${WORK}/anti-plugins.toml" "${broken}")
execute_process(COMMAND "${WORK}/found" "--anti.plugins=${WORK}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "does not match the digest")
    message(FATAL_ERROR "a broken digest gave ${status}\n${err}")
endif()
