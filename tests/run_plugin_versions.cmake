# Build one host and five libraries of the same interface, each against
# another version of it, and load each into the host. A newer library and
# an older one load, and the three checks of "Versions" refuse the rest.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
#   WORK      a directory for the objects and the executables

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "failed with ${status}: ${ARGV}\n${out}${err}")
    endif()
    set(output "${out}" PARENT_SCOPE)
endfunction()

# Load the library into the host and check what it printed against a
# regular expression. want is "yes" for a load that must succeed.
function(loads host library want pattern)
    execute_process(COMMAND "${host}" "${library}" RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(want STREQUAL "yes" AND NOT status EQUAL 0)
        message(FATAL_ERROR "${host} refused ${library}\n${out}${err}")
    endif()
    if(NOT want STREQUAL "yes" AND status EQUAL 0)
        message(FATAL_ERROR "${host} took ${library}\n${out}")
    endif()
    if(NOT out MATCHES "${pattern}")
        message(FATAL_ERROR "${host} printed\n${out}and none of it matches "
                            "${pattern}")
    endif()
endfunction()

set(versions "${SOURCES}/versions")
file(REMOVE_RECURSE "${WORK}")

# The interface of the program, and one library file per version of it.
# Each stands in a directory of its own, so a search root names one.
foreach(part "base|${SOURCES}|0.0.0" "one|${versions}/one|0.0.0"
             "three|${versions}/three|0.0.0"
             "field|${versions}/field|0.0.0"
             "floor-1.0|${versions}/floor|1.0"
             "floor-1.1|${versions}/floor|1.1")
    string(REPLACE "|" ";" one "${part}")
    list(GET one 0 name)
    list(GET one 1 root)
    list(GET one 2 version)
    file(MAKE_DIRECTORY "${WORK}/${name}/net/example")
    run("${ANTIC}" -c --package-version "${version}" -I "${root}"
        -o "${WORK}/${name}/net/example/greet.antl"
        "${root}/net/example/greet.anti")
endforeach()

# One library per version, each bound to the interface of its own
# directory and to nothing else.
foreach(part "one|${versions}/one" "three|${versions}/three"
             "field|${versions}/field" "floor-1.0|${versions}/floor"
             "floor-1.1|${versions}/floor")
    string(REPLACE "|" ";" one "${part}")
    list(GET one 0 name)
    list(GET one 1 root)
    run("${ANTIC}" --lib shared --no-runtime --runtime "${RUNTIME}"
        --llvm-mc "${LLVM_MC}" -I "${root}" -I "${WORK}/${name}"
        -o "${WORK}/${name}/libfancy${SUFFIX}" "${root}/net/example/fancy.anti")
endforeach()

# The hosts. reader calls one function of the interface and caller both,
# so the bitmap of the second holds the slot of `asked`.
foreach(part "reader|base" "caller|base" "floor|floor-1.1")
    string(REPLACE "|" ";" one "${part}")
    list(GET one 0 name)
    list(GET one 1 interface)
    if(name STREQUAL "floor")
        set(source "${versions}/reader.anti")
    else()
        set(source "${versions}/${name}.anti")
    endif()
    run("${ANTIC}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
        -I "${versions}" -I "${SOURCES}" -I "${WORK}/${interface}"
        -o "${WORK}/${name}" "${source}")
endforeach()

# A library built against an earlier version loads while the program
# reaches no slot it lacks, and one built against a later version loads
# because the two chains agree wherever both reach.
loads("${WORK}/reader" "${WORK}/one/libfancy${SUFFIX}" yes
      "from the older library")
loads("${WORK}/reader" "${WORK}/three/libfancy${SUFFIX}" yes
      "from the newer library")
# The program calls `asked`, which the earlier interface has not.
loads("${WORK}/caller" "${WORK}/one/libfancy${SUFFIX}" no
      "calls `asked` of `net.example.greet.Greeter`")
loads("${WORK}/caller" "${WORK}/three/libfancy${SUFFIX}" yes
      "from the newer library")
# A field added to the interface ends the compatibility.
loads("${WORK}/reader" "${WORK}/field/libfancy${SUFFIX}" no
      "a field was added to `net.example.greet.Greeter`")
# `compatible 1.1;` refuses a library built for 1.0.
loads("${WORK}/floor" "${WORK}/floor-1.0/libfancy${SUFFIX}" no
      "this program takes 1.1 and above")
loads("${WORK}/floor" "${WORK}/floor-1.1/libfancy${SUFFIX}" yes
      "from the floor library")
