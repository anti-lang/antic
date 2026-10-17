# The package of this host carries no key and nothing of tools/keys/.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_keys.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

string(REGEX MATCHALL "[^\n]+\\.(pem|gpg|asc)\n" keys "${package_entries}")
if(keys)
    message(FATAL_ERROR "${package_archive} carries ${keys}")
endif()
# Nothing of tools/keys/ goes in, whatever its name or form.
if(package_entries MATCHES "(^|\n)anti/tools/keys/")
    message(FATAL_ERROR "${package_archive} carries tools/keys/")
endif()
file(GLOB_RECURSE key_files LIST_DIRECTORIES false "${ROOT}/tools/keys/*")
foreach(key_file IN LISTS key_files)
    get_filename_component(key_name "${key_file}" NAME)
    string(REPLACE "." "\\." key_pattern "${key_name}")
    if(package_entries MATCHES "(^|[\n/])${key_pattern}\n")
        message(FATAL_ERROR "${package_archive} carries ${key_name} of "
                            "tools/keys/")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
