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

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

# Run antic, which must succeed and print nothing.
function(run)
    execute_process(COMMAND "${ANTIC}" ${ARGV} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic failed with ${status}: ${ARGV}\n${out}${err}")
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
    run(--lib shared --no-runtime --runtime "${RUNTIME}"
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
run(-c -I "${SOURCES}" -o "${WORK}/net/example/greet.antl"
    "${SOURCES}/net/example/greet.anti")

# The host loads the library, asks it for the interface and closes it.
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" -o "${WORK}/host${EXE}" "${SOURCES}/host.anti")
plugin(host)
library_dir(host dir)
program_expect("the host" COMMAND "${WORK}/host${EXE}" "${dir}/libfancy${SUFFIX}"
               OUT_FILE "${EXPECTED}")

# `--closed` builds a program without the exports a plugin binds
# against, so the load of the same library fails. It stands in a
# directory of its own, where Windows finds no other program by the name
# the imports of the library give.
file(MAKE_DIRECTORY "${WORK}/closed")
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --closed -o "${WORK}/closed/closed${EXE}"
    "${SOURCES}/host.anti")
program_expect("a closed program"
               COMMAND "${WORK}/closed/closed${EXE}" "${dir}/libfancy${SUFFIX}"
               STATUS 1 ERR_MATCH "libfancy")
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

# `unload` counts the objects of a library in the hooks its own code
# calls, so a library built with `--no-hooks` is refused, and so is a
# program that loads libraries. A closed program loads none and may drop
# them.
execute_process(COMMAND "${ANTIC}" --no-hooks --lib shared --no-runtime
                        --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                        -I "${SOURCES}" -I "${WORK}"
                        -o "${WORK}/closed/libfancy${SUFFIX}"
                        "${SOURCES}/net/example/fancy.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "`--no-hooks` drops the hooks that count the objects of a plugin")
    message(FATAL_ERROR "a plugin was built without its hooks\n${err}")
endif()
execute_process(COMMAND "${ANTIC}" --no-hooks --runtime "${RUNTIME}"
                        --llvm-mc "${LLVM_MC}" -I "${SOURCES}" -I "${WORK}"
                        -o "${WORK}/closed/hookless${EXE}"
                        "${SOURCES}/host.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "the program loads libraries, and `--no-hooks` drops")
    message(FATAL_ERROR "a host was built without its hooks\n${err}")
endif()
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --closed --no-hooks -o "${WORK}/closed/hookless${EXE}"
    "${SOURCES}/host.anti")

# `plugin:<path>` of the manifest fills the slot of an injected field
# from that library, and `discover` finds it through the index.
library_dir(named dir)
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=plugin:${dir}/libfancy${SUFFIX}"
    -o "${WORK}/named${EXE}" "${SOURCES}/user.anti")
plugin(named)
program_expect("the program with a named library"
               COMMAND "${WORK}/named${EXE}" OUT_FILE "${PROVIDED}")
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" --inject "${interface}=discover" -o "${WORK}/found${EXE}"
    "${SOURCES}/user.anti")
plugin(found)
library_dir(found dir)
program_expect("the program that discovers a library"
               COMMAND "${WORK}/found${EXE}" "--anti.plugins=${dir}"
               OUT_FILE "${PROVIDED}")

# Discovery without a directory to search names the interface, and a
# library whose digest is not the one the index records is refused.
program_expect("discovery without a directory" COMMAND "${WORK}/found${EXE}"
               STATUS 70 ERR_MATCH "no library of the search directories")
file(READ "${dir}/anti-plugins.toml" index)
file(SHA256 "${dir}/libfancy${SUFFIX}" digest)
string(REPLACE "digest = '${digest}'"
       "digest = '0000000000000000000000000000000000000000000000000000000000000000'"
       broken "${index}")
file(WRITE "${dir}/anti-plugins.toml" "${broken}")
program_expect("a broken digest"
               COMMAND "${WORK}/found${EXE}" "--anti.plugins=${dir}"
               STATUS 70 ERR_MATCH "does not match the digest")

# antic keeps the entries of the other libraries of an index it rewrites,
# whatever form of TOML the runtime reads in it: line ends of Windows, a
# comment and strings in double quotes. The stale entry of the library
# goes, a path that holds a quote of its own comes back in the other
# quotes, and the runtime reads the result.
set(kept "${WORK}/kept")
file(MAKE_DIRECTORY "${kept}")
set(zeros "0000000000000000000000000000000000000000000000000000000000000000")
set(other "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
file(WRITE "${kept}/anti-plugins.toml"
     "# libraries built elsewhere\r\n"
     "[[library]]\r\npath = \"libother${SUFFIX}\"\r\nruntime = \"0.0.1\"\r\n"
     "digest = \"${other}\"\r\ninterfaces = [\"x.Other\", \"x.Y\"]\r\n\r\n"
     "[[library]]\r\npath = \"libfancy${SUFFIX}\"\r\nruntime = \"0.0.1\"\r\n"
     "digest = \"${zeros}\"\r\ninterfaces = [\"${interface}\"]\r\n"
     "[[library]]\r\npath = \"lib'q${SUFFIX}\"\r\nruntime = \"0.0.1\"\r\n"
     "digest = \"${other}\"\r\ninterfaces = [\"x.Q\"]\r\n")
set(host "")
if(SUFFIX STREQUAL ".dll")
    set(host "${WORK}/found.lib")
endif()
run(--lib shared --no-runtime --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
    -I "${SOURCES}" -I "${WORK}" -o "${kept}/libfancy${SUFFIX}"
    "${SOURCES}/net/example/fancy.anti" ${host})
file(READ "${kept}/anti-plugins.toml" index)
file(SHA256 "${kept}/libfancy${SUFFIX}" digest)
string(REGEX MATCHALL "libfancy" fancies "${index}")
list(LENGTH fancies fancy_count)
if(NOT index MATCHES "path = 'libother${SUFFIX}'" OR
   NOT index MATCHES "digest = '${other}'" OR
   NOT index MATCHES "interfaces = \\['x\\.Other', 'x\\.Y'\\]" OR
   NOT index MATCHES "path = \"lib'q${SUFFIX}\"" OR
   NOT index MATCHES "digest = '${digest}'" OR
   index MATCHES "${zeros}" OR NOT fancy_count EQUAL 1)
    message(FATAL_ERROR "the index lost or kept the wrong entries\n${index}")
endif()
program_expect("discovery through a rewritten index"
               COMMAND "${WORK}/found${EXE}" "--anti.plugins=${kept}"
               OUT_FILE "${PROVIDED}")

# An index cut off inside an entry is no TOML, and antic replaces it with
# the entry of its own library.
file(WRITE "${kept}/anti-plugins.toml" "[[library]]\npath = 'libot")
run(--lib shared --no-runtime --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
    -I "${SOURCES}" -I "${WORK}" -o "${kept}/libfancy${SUFFIX}"
    "${SOURCES}/net/example/fancy.anti" ${host})
file(READ "${kept}/anti-plugins.toml" index)
if(index MATCHES "libot" OR NOT index MATCHES "path = 'libfancy${SUFFIX}'")
    message(FATAL_ERROR "a cut index stayed\n${index}")
endif()
