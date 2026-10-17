# The anti of the package runs anti bind, whose reader of JSON stands in
# src/rt. On a Linux host the packer compiled that anti itself, which once
# left the readers of src/rt out. anti bind --clang of the package, with
# bin/ of the package and the pinned clang alone on the PATH, binds
# include/raylib/raylib.h of the package. It writes anti.raylib as the
# anti of the tree writes it from raylib.h of the pinned raylib source,
# which the test anti_bind_raylib compiles and probes.
#
#   cmake -DROOT=<repository> -DANTI=<anti> -DHOST=<host> -DRUNTIME=<dir>
#         -DCLANG=<clang> -DRAYLIB=<dir> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_bind.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

run_packed("anti bind" bind "${ROOT}/tests/bind/raylib_api.json"
           -o "${WORK}/bound")
if(NOT EXISTS "${WORK}/bound/raylib.anti")
    message(FATAL_ERROR "the packed anti bind wrote no raylib.anti")
endif()

# The PATH is set as run_anti_bind.cmake sets it, for the two runs of
# --clang, and put back after them. Each directory is made native alone,
# since file(TO_NATIVE_PATH) writes `;` for the `:` of a joined PATH.
get_filename_component(clang_bin "${CLANG}" DIRECTORY)
file(TO_NATIVE_PATH "${package_bin}" bind_path)
file(TO_NATIVE_PATH "${clang_bin}" clang_bin)
if(HOST MATCHES "^windows-")
    string(APPEND bind_path ";${clang_bin}")
else()
    string(APPEND bind_path ":${clang_bin}")
endif()
set(saved_path "$ENV{PATH}")
set(ENV{PATH} "${bind_path}")
set(tree_anti "${ANTI}")
set(ANTI "${package_bin}/anti${suffix}")
anti_run(packed_status packed_out packed_err "${WORK}" bind --clang
         "${package_dir}/include/raylib/raylib.h" -o "${WORK}/bound-clang")
set(ANTI "${tree_anti}")
anti_run(tree_status tree_out tree_err "${WORK}" bind --clang
         "${RAYLIB}/src/raylib.h" -o "${WORK}/bound-tree"
         --runtime "${RUNTIME}")
set(ENV{PATH} "${saved_path}")
if(NOT packed_status EQUAL 0)
    message(FATAL_ERROR "the packed anti bind --clang of include/raylib/raylib.h "
                        "failed with ${packed_status}\n${packed_out}${packed_err}")
endif()
if(NOT tree_status EQUAL 0)
    message(FATAL_ERROR "anti bind --clang of the pinned raylib.h failed with "
                        "${tree_status}\n${tree_out}${tree_err}")
endif()
file(GLOB written RELATIVE "${WORK}/bound-clang" "${WORK}/bound-clang/*")
file(GLOB wanted RELATIVE "${WORK}/bound-tree" "${WORK}/bound-tree/*")
if(NOT "raylib.anti" IN_LIST written OR NOT written STREQUAL wanted)
    message(FATAL_ERROR "the packed anti bind --clang wrote '${written}', and "
                        "the anti of the tree '${wanted}'")
endif()
foreach(file IN LISTS written)
    file(READ "${WORK}/bound-clang/${file}" got)
    file(READ "${WORK}/bound-tree/${file}" want)
    if(NOT got STREQUAL want)
        message(FATAL_ERROR "${file} that the packed anti bind --clang wrote "
                            "from include/raylib/raylib.h differs from anti.raylib "
                            "of the tree")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
