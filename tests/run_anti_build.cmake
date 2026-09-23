# Drive `anti build`, `anti run` and `anti new` over the two-module
# project of tests/anti-build/app. Run with cmake -P and these values:
#   ANTI     the anti executable
#   OBJDUMP  llvm-objdump of the pinned release
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds app/ and units/
#   HOST     the target name of this host
#   OTHER    a target name that is not this host's
#   CPU      a processor level of this host's architecture
#   WORK     a directory this run writes into
#
# It covers the dev mode of docs/tooling-addendum.md with its cache,
# release mode, the `-g` rule of docs/tooling.md, --target, --cpu, the
# lock file, `anti run` and the project `anti new` writes.

set(project "${WORK}/app")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" DESTINATION "${WORK}")

# The cache of the repositories is the user's, so the run names its own
# and leaves the one of this machine alone.
function(run_anti out_status out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN}
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(build what)
    run_anti(status text ${ARGN} --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${text}")
    endif()
endfunction()

# The dev build of the two modules. The program prints the word of the
# second module and ends with its status.
build("the dev build" build)
set(program "${project}/dist/${HOST}/dev/app")
if(NOT EXISTS "${program}")
    message(FATAL_ERROR "the dev build wrote no ${program}")
endif()
execute_process(COMMAND "${program}" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 7 OR NOT out MATCHES "^one")
    message(FATAL_ERROR "the program ended with ${status} and wrote ${out}${err}")
endif()

# The lock file stands beside the manifest, even for a project with no
# dependency.
if(NOT EXISTS "${project}/anti.lock")
    message(FATAL_ERROR "the build wrote no anti.lock")
endif()

# The cache key of a module is the digest of its input, the compiler
# version and the target, so a build that changes nothing writes no
# object again.
set(object "${project}/build/${HOST}/dev/obj/com/example/greet.o")
if(NOT EXISTS "${object}")
    message(FATAL_ERROR "the dev build wrote no ${object}")
endif()
file(TIMESTAMP "${object}" before "%Y%m%d%H%M%S" UTC)
file(SHA256 "${object}" digest_before)
build("the second dev build" build)
file(SHA256 "${object}" digest_after)
file(TIMESTAMP "${object}" after "%Y%m%d%H%M%S" UTC)
if(NOT before STREQUAL after OR NOT digest_before STREQUAL digest_after)
    message(FATAL_ERROR "the second build wrote ${object} again")
endif()

# A changed module is compiled again, and the program follows it.
file(READ "${project}/src/com/example/greet.anti" source)
string(REPLACE "return 7;" "return 9;" source "${source}")
file(WRITE "${project}/src/com/example/greet.anti" "${source}")
build("the build after a change" build)
file(SHA256 "${object}" digest_changed)
if(digest_before STREQUAL digest_changed)
    message(FATAL_ERROR "the changed module wrote the same ${object}")
endif()
execute_process(COMMAND "${program}" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 9)
    message(FATAL_ERROR "the changed program ended with ${status}")
endif()

# `anti run` builds for the host and runs what it wrote.
run_anti(status text run --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
if(NOT status EQUAL 9 OR NOT text MATCHES "one")
    message(FATAL_ERROR "anti run ended with ${status} and wrote ${text}")
endif()

# Release mode compiles the whole program in one call.
build("the release build" build --release)
set(release "${project}/dist/${HOST}/release/app")
execute_process(COMMAND "${release}" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 9 OR NOT out MATCHES "^one")
    message(FATAL_ERROR "the release program ended with ${status}")
endif()

# A release binary carries no symbol data, so the build writes the
# archive that names it: the same link with the debug sections kept and
# the map of the program, which carries the build id of the binary.
set(archive "${project}/dist/${HOST}/release/app-symbols.zip")
if(NOT EXISTS "${archive}")
    message(FATAL_ERROR "the release build wrote no ${archive}")
endif()
file(REMOVE_RECURSE "${WORK}/symbols")
file(MAKE_DIRECTORY "${WORK}/symbols")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
    WORKING_DIRECTORY "${WORK}/symbols"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the symbols archive did not unpack\n${err}")
endif()
foreach(name app.debug app.map)
    if(NOT EXISTS "${WORK}/symbols/${name}")
        message(FATAL_ERROR "the symbols archive holds no ${name}")
    endif()
endforeach()
# The link with the debug sections is the same program at the same
# addresses, so the map answers for the binary beside it.
execute_process(COMMAND "${WORK}/symbols/app.debug" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 9 OR NOT out MATCHES "^one")
    message(FATAL_ERROR "app.debug ended with ${status} and wrote ${out}${err}")
endif()
file(STRINGS "${release}" lines REGEX "^build [0-9a-f]+$")
string(REGEX MATCH "^build ([0-9a-f]+)$" line "${lines}")
set(id "${CMAKE_MATCH_1}")
file(READ "${WORK}/symbols/app.map" map)
if(NOT map MATCHES "# build ${id}")
    message(FATAL_ERROR "the map names no build id of the program:\n${map}")
endif()
foreach(name com.example.app.main com.example.greet.word)
    if(NOT map MATCHES "${name}")
        message(FATAL_ERROR "the map names no ${name}")
    endif()
endforeach()
if(NOT map MATCHES "greet.anti:[0-9]+")
    message(FATAL_ERROR "the map carries no file and line:\n${map}")
endif()

# `anti build` passes -g in dev mode and never in release, so the
# assembly of a dev build carries the line of every statement.
file(READ "${project}/build/${HOST}/dev/app.s" dev_assembly)
file(READ "${project}/build/${HOST}/release/app.s" release_assembly)
string(FIND "${dev_assembly}" ".loc" at)
if(at LESS 0)
    message(FATAL_ERROR "the dev build passed no -g")
endif()
string(FIND "${release_assembly}" ".loc" at)
if(NOT at LESS 0)
    message(FATAL_ERROR "the release build passed -g")
endif()

# --target builds for another target, and --cpu takes a level of this
# host's architecture.
build("the build for ${OTHER}" build --target "${OTHER}")
if(NOT EXISTS "${project}/dist/${OTHER}/dev/app" AND
   NOT EXISTS "${project}/dist/${OTHER}/dev/app.exe")
    message(FATAL_ERROR "the build for ${OTHER} wrote no program")
endif()
build("the build at ${CPU}" build --cpu "${CPU}")

# A level of the other architecture is refused.
run_anti(status text build --cpu nonesuch --runtime "${RUNTIME}"
         --llvm-mc "${LLVM_MC}")
if(status EQUAL 0 OR NOT text MATCHES "no processor level")
    message(FATAL_ERROR "--cpu nonesuch was taken: ${text}")
endif()

# `anti new` writes a project of the default layout that builds and runs.
file(MAKE_DIRECTORY "${WORK}/new")
execute_process(
    COMMAND "${ANTI}" new com.example.demo
    WORKING_DIRECTORY "${WORK}/new"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti new failed with ${status}\n${out}${err}")
endif()
foreach(file anti.toml src/com/example/demo.anti)
    if(NOT EXISTS "${WORK}/new/demo/${file}")
        message(FATAL_ERROR "anti new wrote no ${file}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${WORK}/cache"
            "LOCALAPPDATA=${WORK}/cache"
            "${ANTI}" run --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
    WORKING_DIRECTORY "${WORK}/new/demo"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out MATCHES "hello")
    message(FATAL_ERROR "the project of anti new wrote ${out}${err}")
endif()

# A package name of one segment is no module path of a package.
execute_process(
    COMMAND "${ANTI}" new demo
    WORKING_DIRECTORY "${WORK}/new"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "one segment")
    message(FATAL_ERROR "anti new took a name of one segment: ${out}${err}")
endif()
