# The configure step of a fresh checkout installs the sysroot of every
# target and the raylib source, as it installs clang and the LLVM tools,
# so a first ./c builds. That is decision 3 of "Decisions of 2026-10-11".
#
# A copy of the sources configures against a directory of downloads that
# holds no sysroot and no raylib tree. That directory holds the archives
# alone, copied from the downloads of the tree, so the configure has to
# unpack and lay out each one, and the network stays out of the test: a
# configure that says "download " fails it. clang, the LLVM tools and the
# sources of the native libraries come from the directories that TREE, the
# build tree, names in its cache.
#
#   cmake -DROOT=<repository> -DTREE=<build tree> -DWORK=<dir>
#         -DGENERATOR=<generator> -P tests/runners/run_first_build.cmake

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
set(source "${WORK}/source")
set(deps "${WORK}/deps")
file(MAKE_DIRECTORY "${source}" "${deps}")
foreach(entry CMakeLists.txt CMakePresets.json LICENSE README.md CHANGELOG.md
              docs LICENSES src tests tools)
    file(COPY "${ROOT}/${entry}" DESTINATION "${source}")
endforeach()

# The directories of the tree, from its cache. The sysroots and raylib are
# the subject, and every other one is passed on.
file(STRINGS "${TREE}/CMakeCache.txt" lines REGEX "^ANTIC_[A-Z0-9]+_DIR:")
set(passed "")
foreach(line IN LISTS lines)
    if(NOT line MATCHES "^ANTIC_([A-Z0-9]+)_DIR:[A-Z]+=(.*)$")
        continue()
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(value "${CMAKE_MATCH_2}")
    set(dir_${name} "${value}")
    if(NOT name MATCHES "^(SYSROOT|RAYLIB)$")
        list(APPEND passed "-DANTIC_${name}_DIR=${value}")
    endif()
endforeach()
foreach(name SYSROOT RAYLIB)
    if(NOT DEFINED dir_${name})
        message(FATAL_ERROR "${TREE}/CMakeCache.txt names no ANTIC_${name}_DIR")
    endif()
endforeach()

# Copy each archive below <from>, to <depth> levels, to the same path below
# <to>. A tree an archive unpacked to stays behind.
function(copy_archives from to depth)
    set(patterns "${from}/*")
    set(pattern "${from}/*")
    foreach(level RANGE 2 ${depth})
        string(APPEND pattern "/*")
        list(APPEND patterns "${pattern}")
    endforeach()
    file(GLOB found LIST_DIRECTORIES false RELATIVE "${from}" ${patterns})
    set(copied 0)
    foreach(file IN LISTS found)
        if(file MATCHES "\\.(deb|apk|zip|tar\\.[a-z0-9]+)$|^apsl\\.txt$")
            get_filename_component(directory "${to}/${file}" DIRECTORY)
            file(MAKE_DIRECTORY "${directory}")
            file(COPY_FILE "${from}/${file}" "${to}/${file}")
            math(EXPR copied "${copied} + 1")
        endif()
    endforeach()
    if(copied EQUAL 0)
        message(FATAL_ERROR "${from} holds no archive to start from")
    endif()
endfunction()
copy_archives("${dir_SYSROOT}/.download" "${deps}/sysroot/.download" 2)
copy_archives("${dir_RAYLIB}" "${deps}/raylib" 1)

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${WORK}/tree"
                        -G "${GENERATOR}" "-DANTI_DEPS_DIR=${deps}" ${passed}
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the configure of a fresh checkout failed:\n${out}${err}")
endif()
if(out MATCHES "download ")
    message(FATAL_ERROR "the configure downloaded what the archives held:\n${out}")
endif()

set(missing "")
include("${ROOT}/tools/cpu-levels.cmake")
antic_read_cpu_levels("${ROOT}/tools/cpu-levels")
set(sysroots ${ANTIC_TARGETS})
foreach(target IN LISTS ANTIC_TARGETS)
    if(target MATCHES "^linux-")
        list(APPEND sysroots "${target}-glibc")
    endif()
endforeach()
foreach(target IN LISTS sysroots)
    file(GLOB entries "${WORK}/tree/runtime/sysroot/${target}/*")
    if(NOT entries)
        string(APPEND missing " the sysroot of ${target},")
    endif()
endforeach()
file(GLOB raylib "${deps}/raylib/*/src/rcore.c")
if(NOT raylib)
    string(APPEND missing " the raylib source,")
endif()
if(NOT missing STREQUAL "")
    message(FATAL_ERROR "the configure of a fresh checkout installed no"
                        "${missing} in ${deps}:\n${out}")
endif()
file(REMOVE_RECURSE "${WORK}")
