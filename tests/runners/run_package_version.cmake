# antic and anti of the package print the version of tools/version with
# nothing but bin/ of the package on the PATH. VERSION of the package
# holds the same version, and bin/llvm-version the version of
# tools/llvm-version.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_version.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

foreach(program antic anti)
    run_alone("${program} --version" "${WORK}"
              "${package_bin}/${program}${suffix}" --version)
    if(NOT alone_out MATCHES "^${program} ${package_version_pattern}\r?\n$")
        message(FATAL_ERROR "the packed ${program} prints '${alone_out}', not "
                            "${program} ${package_version}")
    endif()
endforeach()
file(READ "${package_dir}/VERSION" packed_version)
string(STRIP "${packed_version}" packed_version)
if(NOT packed_version STREQUAL package_version)
    message(FATAL_ERROR "VERSION of the package holds '${packed_version}', and "
                        "tools/version ${package_version}")
endif()
file(READ "${package_bin}/llvm-version" packed_llvm)
file(READ "${ROOT}/tools/llvm-version" llvm_version)
if(NOT packed_llvm STREQUAL llvm_version)
    message(FATAL_ERROR "bin/llvm-version of the package holds "
                        "'${packed_llvm}', and tools/llvm-version "
                        "'${llvm_version}'")
endif()

file(REMOVE_RECURSE "${WORK}")
