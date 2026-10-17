# The package of this host that the test package_pack packed and unpacked
# in PACKAGE, for the runners run_package_*.cmake that read it. Include it
# after tests/program_output.cmake, with PACKAGE, HOST and WORK set. WORK
# is the directory of the one test, made empty here.
#
#   package_archive          the archive in PACKAGE/out
#   package_entries          the lines of `tar tf` of the archive
#   package_dir              the unpacked package, PACKAGE/unpacked/anti
#   package_bin              its bin/
#   suffix                   .exe on a Windows host, empty elsewhere
#   package_version          the version of tools/version
#   package_version_pattern  that version as a regular expression

file(GLOB package_archive "${PACKAGE}/out/anti-*-${HOST}.tar.xz")
if(NOT package_archive OR NOT EXISTS "${PACKAGE}/entries.txt")
    message(FATAL_ERROR "${PACKAGE} holds no package of ${HOST}, which the "
                        "test package_pack packs")
endif()
file(READ "${PACKAGE}/entries.txt" package_entries)
set(package_dir "${PACKAGE}/unpacked/anti")
set(package_bin "${package_dir}/bin")
set(suffix "")
if(HOST MATCHES "^windows-")
    set(suffix ".exe")
endif()
file(STRINGS "${ROOT}/tools/version" package_version LIMIT_COUNT 1)
string(STRIP "${package_version}" package_version)
string(REPLACE "." "\\." package_version_pattern "${package_version}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# DESIGN: the package alone builds a program. The PATH holds bin/ of the
# package and nothing else, so no tool of LLVM or of Anti comes from
# anywhere else, and no --runtime names the archive: antic and anti find
# it above their own bin/, and opt, llc and lld in that bin/, by the one
# rule of runtime_archive in src/antic/userdirs.c.
# Run ARGN in dir with that PATH and a cache in WORK, which must succeed.
# alone_out holds the standard output.
function(run_alone what dir)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${package_bin}"
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                ${ARGN}
        WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the package alone: ${what} failed with "
                            "${status}\n${out}${err}")
    endif()
    set(alone_out "${out}" PARENT_SCOPE)
endfunction()

# Run the anti of the package with ARGN in WORK, through anti_run, which
# must succeed. packed_out holds the standard output.
function(run_packed what)
    set(ANTI "${package_bin}/anti${suffix}")
    anti_run(status out err "${WORK}" ${ARGN})
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the packed anti: ${what} failed with ${status}\n"
                            "${out}${err}")
    endif()
    set(packed_out "${out}" PARENT_SCOPE)
endfunction()
