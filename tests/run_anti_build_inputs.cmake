# Drive `anti build` over a malformed index.toml of a `file://` repository
# and a malformed anti.lock. Run with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds consumer/
#   WORK     a directory this run writes into
#
# Every string of an index or a lock names a part of a path in the cache,
# a number the resolver compares or a string of the lock file. A name,
# version or module path that is not the grammar of its kind is refused
# before it reaches any of them (S45, S40, M13 of docs/audit/summary.md),
# and nothing is written outside the cache.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(project "${WORK}/project")
set(repo "${WORK}/repo")
set(digest "5b0c2d1e7a94f3c6b8d0e1f2a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6")
file(COPY "${FIXTURE}/consumer/src" DESTINATION "${project}")
file(WRITE "${project}/anti.toml"
     "[package]\n"
     "name = \"com.example.consumer\"\n"
     "version = \"0.1.0\"\n"
     "\n"
     "[repositories]\n"
     "local = \"file://${repo}\"\n"
     "\n"
     "[dependencies]\n"
     "\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n")

function(run_anti out_status out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN} --runtime "${RUNTIME}"
                --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

# Fail unless the build fails with a message that matches pattern, and
# unless nothing was written at WORK/escaped or beside the cache.
function(refused what pattern)
    run_anti(status text build ${ARGN})
    if(status EQUAL 0)
        message(FATAL_ERROR "${what}: the build took it\n${text}")
    endif()
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: the build said\n${text}")
    endif()
    file(GLOB outside LIST_DIRECTORIES true "${WORK}/*")
    list(REMOVE_ITEM outside "${WORK}/project" "${WORK}/repo" "${WORK}/cache")
    if(NOT outside STREQUAL "")
        message(FATAL_ERROR "${what}: the build wrote ${outside}")
    endif()
    file(REMOVE_RECURSE "${WORK}/cache")
endfunction()

# One index of com.example.units, whose single version entry takes the
# three values given. The version stands as TOML, quotes and all.
function(write_index version module dependency)
    file(MAKE_DIRECTORY "${repo}/com.example.units")
    file(WRITE "${repo}/com.example.units/index.toml"
         "name = \"com.example.units\"\n"
         "revision = 1\n"
         "\n"
         "[[version]]\n"
         "version = ${version}\n"
         "modules = [{ path = \"${module}\", sha256 = \"${digest}\" }]\n"
         "dependencies = [${dependency}]\n")
endfunction()

# The traversals climb to the root, where a further `..` stays, and
# come down to WORK/escaped from there, wherever the cache lies.
string(REGEX REPLACE "^[A-Za-z]:" "" down "${WORK}")
string(REGEX REPLACE "^/+" "" down "${down}")
string(REPEAT "../" 40 up)
set(up "${up}${down}/escaped")

# A version that climbs out of the cache.
write_index("\"1.2.0/${up}\"" "com.example.units" "")
refused("a version with a path" "which is no version")

# A version of twenty digits, which overflowed the comparison.
write_index("\"99999999999999999999.0.0\"" "com.example.units" "")
refused("a version of twenty digits" "which is no version")

# A version that closes the string the lock file writes it in, which
# wrote a key of its own there. The TOML of anti reads no escape, so a
# literal string is the one that holds a double quote.
write_index("'1.2.0\", evil = \"x'" "com.example.units" "")
refused("a version with a quote" "which is no version")

# A module path that climbs out of the cache.
write_index("\"1.2.0\"" "${up}" "")
refused("a module path with a path" "which is no module path")

# A dependency whose name climbs out of the cache.
write_index("\"1.2.0\"" "com.example.units"
            "{ name = \"${up}\", version = \"1.0.0\" }")
refused("a dependency name with a path" "which is no package name")

# A dependency whose constraint is no version.
write_index("\"1.2.0\"" "com.example.units"
            "{ name = \"com.example.other\", version = \"1.0/x\" }")
refused("a dependency constraint with a path" "which is no constraint")

# A lock file that answers the manifest names the version and the
# module paths the build fetches, so its strings are checked the same
# way. A lock that holds one that is not the grammar of its kind is not
# read, and --offline then finds no cached index.
function(write_lock version module digest_value)
    file(WRITE "${project}/anti.lock"
         "version = 1\n"
         "\n"
         "[[package]]\n"
         "name = \"com.example.units\"\n"
         "version = \"${version}\"\n"
         "repo = \"file://${repo}\"\n"
         "modules = [\n"
         "    { path = \"${module}\", sha256 = ${digest_value} },\n"
         "]\n")
endfunction()

write_lock("1.2.0/${up}" "com.example.units" "\"${digest}\"")
refused("a locked version with a path" "is not read" --offline)
write_lock("99999999999999999999.0.0" "com.example.units" "\"${digest}\"")
refused("a locked version of twenty digits" "is not read" --offline)
write_lock("1.2.0" "${up}" "\"${digest}\"")
refused("a locked module path with a path" "is not read" --offline)
write_lock("1.2.0" "com.example.units" "'${digest}\"'")
refused("a locked digest with a quote" "is not read" --offline)
file(REMOVE "${project}/anti.lock")
