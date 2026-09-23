# Drive `anti symbols inventory`, `check` and `resolve` over a deployment
# of the release program of tests/anti-symbols/tracer. Run with cmake -P
# and these values:
#   ANTI     the anti executable
#   ANTIC    the antic executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  tests/anti-symbols
#   PLUGIN   tests/plugin
#   SUFFIX   the suffix of a plugin on this host, empty where a program
#            loads none
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#
# The deployment is the program, its symbols archive and a runtime
# configuration that includes another file. On a host that loads
# plugins it also holds a plugin in a `plugins` directory, named once
# more by `[injections]`, with an archive of its own.

set(project "${WORK}/tracer")
set(deploy "${WORK}/deploy")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${deploy}/plugins" "${WORK}/net/example")
file(COPY "${FIXTURE}/tracer" DESTINATION "${WORK}")

function(run what)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "${what} failed with ${status}\n${out}${err}")
    endif()
    set(output "${out}" PARENT_SCOPE)
endfunction()

# Run `anti symbols` in the deployment, and give its status and output.
function(symbols out_status out_text)
    execute_process(COMMAND "${ANTI}" symbols ${ARGN}
                    WORKING_DIRECTORY "${deploy}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

# The build id in the notice of a binary.
function(build_id file out)
    file(STRINGS "${file}" lines REGEX "^build [0-9a-f]+$")
    list(GET lines 0 line)
    string(SUBSTRING "${line}" 6 -1 id)
    set(${out} "${id}" PARENT_SCOPE)
endfunction()

function(release)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" build --release --runtime "${RUNTIME}"
                --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the release build failed with ${status}\n"
                            "${out}${err}")
    endif()
endfunction()

release()
set(dist "${project}/dist/${HOST}/release")
file(COPY "${dist}/tracer" "${dist}/tracer-symbols.zip"
     DESTINATION "${deploy}")
build_id("${deploy}/tracer" program_id)

file(WRITE "${deploy}/base.toml" "[runtime]\nplugins = ['plugins']\n")
if(SUFFIX STREQUAL "")
    file(WRITE "${deploy}/app.toml" "include = 'base.toml'\n")
    file(WRITE "${deploy}/plugins/anti-plugins.toml" "")
    set(modules 1)
else()
    # The plugin and its debug twin, which carry one build id. Its archive
    # is packed by another tool, which compresses what it holds.
    set(library "libfancy${SUFFIX}")
    run("the interface" "${ANTIC}" -c -I "${PLUGIN}"
        -o "${WORK}/net/example/greet.antl"
        "${PLUGIN}/net/example/greet.anti")
    run("the plugin" "${ANTIC}" --lib shared --no-runtime
        --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${PLUGIN}"
        -I "${WORK}" -o "${deploy}/plugins/${library}"
        "${PLUGIN}/net/example/fancy.anti")
    file(MAKE_DIRECTORY "${WORK}/twin")
    run("the twin of the plugin" "${ANTIC}" -g --lib shared --no-runtime
        --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${PLUGIN}"
        -I "${WORK}" -o "${WORK}/twin/libfancy.debug"
        "${PLUGIN}/net/example/fancy.anti")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf
                            "${deploy}/plugins/libfancy-symbols.zip"
                            --format=zip libfancy.debug
                    WORKING_DIRECTORY "${WORK}/twin" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the archive of the plugin was not written")
    endif()
    build_id("${deploy}/plugins/${library}" plugin_id)
    file(WRITE "${deploy}/app.toml"
         "include = 'base.toml'\n\n[injections]\n"
         "'net.example.greet.Greeter' = 'plugins/${library}'\n")
    set(modules 2)
endif()

# The inventory folds each archive into one, keyed by build id, and
# names the plugin once although two keys reach it.
symbols(status text inventory --conf app.toml --out "${WORK}/all.zip")
if(NOT status EQUAL 0 OR NOT text MATCHES "present [^\n]*tracer ${program_id}\n"
   OR NOT text MATCHES "with ${modules} of ${modules} modules")
    message(FATAL_ERROR "the inventory gave ${status}\n${text}")
endif()
if(DEFINED plugin_id)
    string(REGEX MATCHALL "${library}" named "${text}")
    list(LENGTH named times)
    if(NOT text MATCHES "present [^\n]*${library} ${plugin_id}\n" OR
       NOT times EQUAL 1)
        message(FATAL_ERROR "the inventory missed the plugin\n${text}")
    endif()
endif()
file(ARCHIVE_EXTRACT INPUT "${WORK}/all.zip" DESTINATION "${WORK}/all")
if(NOT EXISTS "${WORK}/all/${program_id}/tracer.debug" OR
   NOT EXISTS "${WORK}/all/${program_id}/tracer.map")
    message(FATAL_ERROR "the inventory holds no symbols of the program")
endif()
file(READ "${WORK}/all/index.toml" index)
if(NOT index MATCHES "module = 'tracer'\nid = '${program_id}'\nversion = '1.2.0'\nsource = '[^\n]*tracer-symbols.zip'")
    message(FATAL_ERROR "the index names the program wrongly\n${index}")
endif()
if(DEFINED plugin_id AND
   NOT EXISTS "${WORK}/all/${plugin_id}/libfancy.debug")
    message(FATAL_ERROR "the inventory holds no symbols of the plugin")
endif()

# Every module has its symbols, beside it and in the inventory.
symbols(status text check --conf app.toml)
if(NOT status EQUAL 0 OR text MATCHES "stale|missing")
    message(FATAL_ERROR "the check beside the binaries gave ${status}\n${text}")
endif()
symbols(status text check --conf app.toml --symbols "${WORK}/all.zip")
if(NOT status EQUAL 0 OR text MATCHES "stale|missing")
    message(FATAL_ERROR "the check of the inventory gave ${status}\n${text}")
endif()

# A release trace comes out with the function and the file and line of
# every frame of the program. The frame outside it stays raw, and so
# does every frame when no archive holds its module.
execute_process(COMMAND "${deploy}/tracer" RESULT_VARIABLE status
                OUTPUT_FILE "${WORK}/trace.txt" ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the program ended with ${status}\n${err}")
endif()
file(READ "${WORK}/trace.txt" trace)
set(archives --symbols "${WORK}/all.zip")
if(DEFINED plugin_id)
    list(APPEND archives --symbols "${deploy}/plugins/libfancy-symbols.zip")
endif()
symbols(status text resolve "${WORK}/trace.txt" ${archives})
set(frame "0x[0-9a-f]+ 0\\+0x[0-9a-f]+")
foreach(name inner outer main)
    if(NOT text MATCHES "\n${frame} com\\.example\\.tracer\\.${name} tracer\\.anti:[0-9]+\n")
        message(FATAL_ERROR "the resolved trace names no ${name}\n${text}")
    endif()
endforeach()
string(REGEX MATCHALL "\n[^\n]*" raw_lines "\n${trace}")
string(REGEX MATCHALL "\n[^\n]*" resolved_lines "\n${text}")
list(LENGTH raw_lines raw_count)
list(LENGTH resolved_lines resolved_count)
if(NOT status EQUAL 0 OR NOT raw_count EQUAL resolved_count)
    message(FATAL_ERROR "resolve gave ${status} and other lines\n${trace}\n"
                        "became\n${text}")
endif()
foreach(line IN LISTS raw_lines)
    if(line MATCHES "^\nmodule " OR line MATCHES " [1-9][0-9]*\\+0x")
        string(FIND "\n${text}" "${line}\n" at)
        if(at LESS 0)
            message(FATAL_ERROR "resolve changed the line '${line}'\n${text}")
        endif()
    endif()
endforeach()
if(DEFINED plugin_id)
    symbols(status text resolve "${WORK}/trace.txt" --symbols
            "${deploy}/plugins/libfancy-symbols.zip")
    if(NOT status EQUAL 0 OR NOT text STREQUAL trace)
        message(FATAL_ERROR "an archive that matches no frame changed the "
                            "trace\n${text}")
    endif()
endif()

# A rebuilt program leaves both archives stale, and a program without an
# archive is missing. Either stops a rollout.
file(READ "${project}/src/com/example/tracer.anti" source)
string(REPLACE "return outer(1) - 3;" "return outer(2) - 4;" source
       "${source}")
file(WRITE "${project}/src/com/example/tracer.anti" "${source}")
release()
file(COPY_FILE "${dist}/tracer" "${deploy}/tracer")
build_id("${deploy}/tracer" rebuilt_id)
if(rebuilt_id STREQUAL program_id)
    message(FATAL_ERROR "the changed program kept its build id")
endif()
symbols(status text check --conf app.toml)
if(status EQUAL 0 OR NOT text MATCHES "stale [^\n]*tracer ${rebuilt_id}\n")
    message(FATAL_ERROR "the check beside a rebuilt program gave ${status}\n"
                        "${text}")
endif()
symbols(status text check --conf app.toml --symbols "${WORK}/all.zip")
if(status EQUAL 0 OR NOT text MATCHES "stale [^\n]*tracer ${rebuilt_id}\n")
    message(FATAL_ERROR "the check of an old inventory gave ${status}\n"
                        "${text}")
endif()
file(REMOVE "${deploy}/tracer-symbols.zip")
symbols(status text check --conf app.toml)
if(status EQUAL 0 OR NOT text MATCHES "missing [^\n]*tracer ${rebuilt_id}\n")
    message(FATAL_ERROR "the check without an archive gave ${status}\n"
                        "${text}")
endif()
