# Build four hosts and seven libraries of an interface of the same name,
# each against another version of it, and load each into a host. A newer
# library and an older one load, and the checks of "Versions" refuse the
# rest: a field added, a version below the floor, a slot the program
# calls and the library lacks, and an interface of another structure. A
# host that calls through reflection loads an older library whose class
# inherits the interface.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
#   EXE       the suffix of an executable on this host
#   WORK      a directory for the objects and the executables
#
# A Windows library links against the import library of its host, so
# each host there takes a library of its own.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

# The library of the version name for the host, built on its first use.
function(library host name out)
    set(dir "${WORK}/${name}")
    set(imports "")
    if(SUFFIX STREQUAL ".dll")
        set(dir "${WORK}/${host}-${name}")
        set(imports "${WORK}/${host}.lib")
    endif()
    set(path "${dir}/libfancy${SUFFIX}")
    if(NOT EXISTS "${path}")
        file(MAKE_DIRECTORY "${dir}")
        antic_build(--lib shared --no-runtime --runtime "${RUNTIME}"
                    --llvm-mc "${LLVM_MC}" -I "${root_${name}}" -I "${WORK}/${name}"
                    -o "${path}" "${root_${name}}/net/example/fancy.anti" ${imports})
    endif()
    set(${out} "${path}" PARENT_SCOPE)
endfunction()

# Load the library of the version name into the host and check what it
# printed against a regular expression. want is "yes" for a load that
# must succeed, and the host exits with 1 on a refusal.
function(loads host name want pattern)
    library("${host}" "${name}" library)
    set(status 1)
    if(want STREQUAL "yes")
        set(status 0)
    endif()
    program_expect("${host} ${name}" COMMAND "${WORK}/${host}${EXE}" "${library}"
                   STATUS ${status} OUT_MATCH "${pattern}")
endfunction()

set(versions "${SOURCES}/versions")
file(REMOVE_RECURSE "${WORK}")

# The interface of the program, and one library file per version of it.
# Each stands in a directory of its own, so a search root names one.
foreach(part "base|${SOURCES}|0.0.0" "one|${versions}/one|0.0.0"
             "three|${versions}/three|0.0.0"
             "inherits|${versions}/inherits|0.0.0"
             "field|${versions}/field|0.0.0"
             "foreign|${versions}/foreign|0.0.0"
             "floor-1.0|${versions}/floor|1.0"
             "floor-1.1|${versions}/floor|1.1")
    string(REPLACE "|" ";" one "${part}")
    list(GET one 0 name)
    list(GET one 1 root)
    list(GET one 2 version)
    file(MAKE_DIRECTORY "${WORK}/${name}/net/example")
    antic_build(-c --package-version "${version}" -I "${root}"
                -o "${WORK}/${name}/net/example/greet.antl"
                "${root}/net/example/greet.anti")
endforeach()

# One library per version, each bound to the interface of its own
# directory and to nothing else. loads builds each on its first use.
set(root_one "${versions}/one")
set(root_three "${versions}/three")
set(root_inherits "${versions}/inherits")
set(root_field "${versions}/field")
set(root_foreign "${versions}/foreign")
set(root_floor-1.0 "${versions}/floor")
set(root_floor-1.1 "${versions}/floor")

# The hosts. reader calls one function of the interface and caller both,
# so the bitmap of the second holds the slot of `asked`.
foreach(part "reader|base" "caller|base" "reflector|base"
             "floor|floor-1.1")
    string(REPLACE "|" ";" one "${part}")
    list(GET one 0 name)
    list(GET one 1 interface)
    if(name STREQUAL "floor")
        set(source "${versions}/reader.anti")
    else()
        set(source "${versions}/${name}.anti")
    endif()
    antic_build(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                -I "${versions}" -I "${SOURCES}" -I "${WORK}/${interface}"
                -o "${WORK}/${name}${EXE}" "${source}")
endforeach()

# A library built against an earlier version loads while the program
# reaches no slot it lacks, and one built against a later version loads
# because the two chains agree wherever both reach.
loads(reader one yes "from the older library")
loads(reader three yes "from the newer library")
# The program calls `asked`, which the earlier interface has not.
loads(caller one no "calls `asked` of `net.example.greet.Greeter`")
loads(caller three yes "from the newer library")
# A field added to the interface ends the compatibility.
loads(reader field no "a field was added to `net.example.greet.Greeter`")
# An interface of the same name whose `words` gives another type is
# another interface, and the chains differ at the first function.
loads(reader foreign no
      "`net.example.greet.Greeter` of .* is not the interface this program carries")
# `compatible 1.1;` refuses a library built for 1.0.
loads(floor floor-1.0 no "this program takes 1.1 and above")
loads(floor floor-1.1 yes "from the floor library")
# A program that calls `reflect.call` gets a stub for a slot that the
# library lacks. A class that inherits the interface holds it at offset
# 0, and its own functions stand in the slots past those of the earlier
# interface. The class keeps its table, a call of its own function
# reaches it, and the library unloads once the object is deleted.
loads(reflector inherits yes
      "^from the inheriting library\nextra 42\nunloaded\n$")
# A class that implements the interface holds it as a sub-object, which
# gets the table of stubs. The object keeps the table of its class, so
# the library unloads.
loads(reflector one yes "^from the older library\nunloaded\n$")
