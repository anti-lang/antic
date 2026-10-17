# Drive `anti build` over one case of what it delivers and caches. Run with
# cmake -P and these values:
#   ANTI     the anti executable
#   CASE     the case
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURES the directory tests/
#   HOST     the target name of this host
#   OTHER    a target name that is not this host's
#   WORK     a directory this run writes into
#
# The cases, each a defect of docs/audit/anti-tool.md:
#   windows_import_library  dist/ of a Windows DLL holds the import library
#                           a C program links against (defect 4)
#   path_target             a path dependency whose manifest names another
#                           default target is built for the host, where
#                           its consumer reads it (defect 2)
#   path_target_cpu         and a build for a target of the other processor
#                           with --cpu builds it at the level of the host
#                           (T144)

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

foreach(value ANTI CASE RUNTIME LLVM_MC FIXTURES HOST WORK)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()

# A project at directory with the manifest text after its [package]
# lines, and one module of path module whose text is source.
function(write_project directory name manifest module source)
    file(WRITE "${directory}/anti.toml"
         "[package]\nname = \"${name}\"\nversion = \"1.0.0\"\n${manifest}")
    string(REPLACE "." "/" file "${module}")
    file(WRITE "${directory}/src/${file}.anti" "${source}")
endfunction()

# Run anti in a directory. The cache of the repositories is the user's,
# so the run names its own. ENV names more of the environment.
function(anti directory out_status out_text)
    cmake_parse_arguments(PARSE_ARGV 3 run "" "" "ENV")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                ${run_ENV} "${ANTI}" ${run_UNPARSED_ARGUMENTS}
        WORKING_DIRECTORY "${directory}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

# Run anti in a directory, and stop unless it ends with 0.
function(anti_passes what directory)
    anti("${directory}" status text ${ARGN})
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${what} ended with ${status}\n${text}")
    endif()
    set(anti_text "${text}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "windows_import_library")
    file(COPY "${FIXTURES}/anti-build/shapes" DESTINATION "${WORK}")
    set(project "${WORK}/shapes")
    anti_passes("anti build --lib shared" "${project}" build --lib shared
                --target windows-x86_64 ${tools})
    set(dist "${project}/dist/windows-x86_64/dev")
    if(NOT EXISTS "${project}/build/windows-x86_64/dev/shapes.lib")
        message(FATAL_ERROR "the link wrote no import library shapes.lib")
    endif()
    file(GLOB found RELATIVE "${dist}" "${dist}/*")
    foreach(file shapes.dll shapes.h shapes.lib)
        if(NOT EXISTS "${dist}/${file}")
            message(FATAL_ERROR "dist/ of a Windows DLL holds no ${file}. It "
                                "holds: ${found}")
        endif()
    endforeach()
    return()
endif()

if(CASE STREQUAL "path_target")
    string(CONCAT dep_source
           "//! A library whose project builds for another target.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    string(CONCAT app_source
           "//! A program over the library of a path.\n\n"
           "import com.example.dep;\n\n"
           "fn main() -> int\n{\n\treturn dep.one();\n}\n")
    write_project("${WORK}/dep" "com.example.dep"
                  "\n[targets]\ndefault = [\"${OTHER}\"]\n"
                  "com.example.dep" "${dep_source}")
    write_project("${WORK}/app" "com.example.app"
                  "\n[dependencies]\n\"com.example.dep\" = { path = \"../dep\" }\n"
                  "com.example.app" "${app_source}")
    anti_passes("the build of the consumer" "${WORK}/app" build ${tools})
    if(NOT EXISTS "${WORK}/dep/dist/${HOST}/dev/com/example/dep.antl")
        message(FATAL_ERROR "the dependency was not built for the host")
    endif()
    program_expect("the program" COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 1 OUT "")
    return()
endif()

# DESIGN: the consumer is a library project, which links nothing, so the
# case needs no sysroot of the other target.
if(CASE STREQUAL "path_target_cpu")
    if(OTHER MATCHES "x86_64$")
        set(level v1)
    else()
        set(level armv8.0)
    endif()
    string(CONCAT dep_source
           "//! A library of a path.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    string(CONCAT app_source
           "//! A library over the library of a path.\n\n"
           "import com.example.dep;\n\n"
           "/// Two.\npub fn two() -> int\n{\n\treturn dep.one() + 1;\n}\n")
    write_project("${WORK}/dep" "com.example.dep" "" "com.example.dep"
                  "${dep_source}")
    write_project("${WORK}/app" "com.example.app"
                  "\n[dependencies]\n\"com.example.dep\" = { path = \"../dep\" }\n"
                  "com.example.app" "${app_source}")
    anti_passes("the build of the consumer for ${OTHER} at ${level}"
                "${WORK}/app" build --target ${OTHER} --cpu ${level} ${tools})
    foreach(file "dep/dist/${HOST}/dev/com/example/dep.antl"
                 "app/dist/${OTHER}/dev/com/example/app.antl")
        if(NOT EXISTS "${WORK}/${file}")
            message(FATAL_ERROR "the build wrote no ${file}")
        endif()
    endforeach()
    return()
endif()

message(FATAL_ERROR "no case ${CASE}")
