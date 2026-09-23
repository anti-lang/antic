# A configure with -DANTI_DEPS_DIR takes the pinned downloads from that
# directory, as a worktree does that shares those of the main checkout. A
# copy of the sources, whose own build/deps does not exist, configures
# against DEPS, downloads nothing and builds.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DDEPS=<deps directory>
#         -DGENERATOR=<generator> -P tests/run_deps_dir.cmake

file(REMOVE_RECURSE "${WORK}")
set(source "${WORK}/source")
file(MAKE_DIRECTORY "${source}")
foreach(entry CMakeLists.txt CMakePresets.json LICENSE README.md CHANGELOG.md
              .github docs LICENSES src tests tools)
    file(COPY "${ROOT}/${entry}" DESTINATION "${source}")
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${WORK}/tree"
                        -G "${GENERATOR}" "-DANTI_DEPS_DIR=${DEPS}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the configure with ANTI_DEPS_DIR failed:\n${out}${err}")
endif()
if(EXISTS "${source}/build/deps")
    message(FATAL_ERROR "the configure wrote ${source}/build/deps instead of "
                        "reading ${DEPS}:\n${out}")
endif()
# tools/fetch-release.cmake says "download <asset>" before it fetches one.
if(out MATCHES "download ")
    message(FATAL_ERROR "the configure downloaded an archive:\n${out}")
endif()
foreach(name CLANG LLVM SYSROOT RAYLIB)
    file(STRINGS "${WORK}/tree/CMakeCache.txt" line
         REGEX "^ANTIC_${name}_DIR:")
    string(FIND "${line}" "=${DEPS}/" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the cache names ${line}, which is not in ${DEPS}")
    endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${WORK}/tree" -j8
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the build with ANTI_DEPS_DIR failed:\n${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
