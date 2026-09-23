# Drive `anti build` over a project that takes its dependency from a path
# and from a `file://` repository, and over a library for C. Run with
# cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   LLVM_AR  the archiver of a static library
#   FIXTURE  the directory that holds units/ and consumer/
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#
# It covers the two shapes of `[dependencies]` in docs/tooling.md, the
# library files a library project writes, the repository layout and the
# lock file that names a version and its digest.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/units" "${FIXTURE}/consumer" DESTINATION "${WORK}")

function(run_anti where out_status out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN}
        WORKING_DIRECTORY "${where}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(build what where)
    run_anti("${where}" status text ${ARGN} --runtime "${RUNTIME}"
             --llvm-mc "${LLVM_MC}" --llvm-ar "${LLVM_AR}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${text}")
    endif()
endfunction()

# A project without `main` is a library project, whose build writes the
# library file of each of its modules.
build("the library project" "${WORK}/units" build)
set(library "${WORK}/units/dist/${HOST}/dev/com/example/units.antl")
if(NOT EXISTS "${library}")
    message(FATAL_ERROR "the library project wrote no ${library}")
endif()

# A dependency of a path names a project, which is built first.
build("the consumer of a path" "${WORK}/consumer" build)
set(program "${WORK}/consumer/dist/${HOST}/dev/consumer")
execute_process(COMMAND "${program}" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 8 OR NOT out MATCHES "^consumer")
    message(FATAL_ERROR "the consumer ended with ${status} and wrote ${out}${err}")
endif()
file(READ "${WORK}/consumer/anti.lock" lock)
if(NOT lock MATCHES "com.example.units")
    message(FATAL_ERROR "the lock file names no dependency:\n${lock}")
endif()

# The same package from a repository, which is a URL prefix over static
# files. The index names every version with the digest of each module.
set(repo "${WORK}/repo/com.example.units")
file(MAKE_DIRECTORY "${repo}/1.2.0")
file(COPY "${library}" DESTINATION "${repo}/1.2.0")
file(RENAME "${repo}/1.2.0/units.antl" "${repo}/1.2.0/com.example.units.antl")
file(SHA256 "${repo}/1.2.0/com.example.units.antl" digest)
file(WRITE "${repo}/index.toml"
     "name = \"com.example.units\"\n"
     "revision = 1\n"
     "\n"
     "[[version]]\n"
     "version = \"1.1.0\"\n"
     "yanked = true\n"
     "modules = [{ path = \"com.example.units\", sha256 = \"${digest}\" }]\n"
     "dependencies = []\n"
     "\n"
     "[[version]]\n"
     "version = \"1.2.0\"\n"
     "modules = [{ path = \"com.example.units\", sha256 = \"${digest}\" }]\n"
     "dependencies = []\n")
file(REMOVE_RECURSE "${WORK}/fetched")
file(COPY "${FIXTURE}/consumer" DESTINATION "${WORK}/fetched")
file(WRITE "${WORK}/fetched/consumer/anti.toml"
     "[package]\n"
     "name = \"com.example.consumer\"\n"
     "version = \"0.1.0\"\n"
     "\n"
     "[repositories]\n"
     "local = \"file://${WORK}/repo\"\n"
     "\n"
     "[dependencies]\n"
     "\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n")
build("the consumer of a repository" "${WORK}/fetched/consumer" build)
execute_process(COMMAND "${WORK}/fetched/consumer/dist/${HOST}/dev/consumer"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 8)
    message(FATAL_ERROR "the fetched consumer ended with ${status}\n${err}")
endif()
file(READ "${WORK}/fetched/consumer/anti.lock" lock)
if(NOT lock MATCHES "${digest}" OR NOT lock MATCHES "file://")
    message(FATAL_ERROR "the lock file names no repository and digest:\n${lock}")
endif()

# The second build reads the lock file and contacts no repository, which
# --offline proves.
build("the offline build" "${WORK}/fetched/consumer" build --offline)

# A constraint that no version of the index satisfies is refused, and a
# yanked version is no answer to one.
file(WRITE "${WORK}/fetched/consumer/anti.lock" "version = 1\n")
file(APPEND "${WORK}/fetched/consumer/anti.toml" "")
file(READ "${WORK}/fetched/consumer/anti.toml" manifest)
string(REPLACE "version = \"1.2.0\", repo" "version = \"=1.1.0\", repo"
       manifest "${manifest}")
file(WRITE "${WORK}/fetched/consumer/anti.toml" "${manifest}")
run_anti("${WORK}/fetched/consumer" status text build --runtime "${RUNTIME}"
         --llvm-mc "${LLVM_MC}")
if(status EQUAL 0 OR NOT text MATCHES "satisfies every constraint")
    message(FATAL_ERROR "the yanked version answered the constraint: ${text}")
endif()

# A library for C: the archive or the shared library, and the header
# beside it.
build("the static library" "${WORK}/units" build --lib static)
if(NOT EXISTS "${WORK}/units/dist/${HOST}/dev/libunits.a" AND
   NOT EXISTS "${WORK}/units/dist/${HOST}/dev/units.lib")
    message(FATAL_ERROR "--lib static wrote no archive")
endif()
if(NOT EXISTS "${WORK}/units/dist/${HOST}/dev/units.h")
    message(FATAL_ERROR "--lib static wrote no header")
endif()
build("the shared library" "${WORK}/units" build --lib shared)
set(shared "")
foreach(name libunits.dylib libunits.so units.dll)
    if(EXISTS "${WORK}/units/dist/${HOST}/dev/${name}")
        set(shared "${name}")
    endif()
endforeach()
if(shared STREQUAL "")
    message(FATAL_ERROR "--lib shared wrote no library")
endif()
