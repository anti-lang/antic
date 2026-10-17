# anti license of the package, from the package alone. The notice of a
# program of musl follows the text of musl and of mimalloc with the line
# of each in licenses/sources.txt of the package. The plain form prints
# the licence of antic and anti from LICENSE of the package, then every
# component of licenses/ with its text, and every line of sources.txt
# after the text of its component.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_license.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

set(program "${WORK}/return42-linux-arm64")
run_alone("antic --target linux-arm64" "${WORK}"
          "${package_bin}/antic${suffix}" --target linux-arm64 -o "${program}"
          "${ROOT}/tests/programs/return42.anti")
run_alone("anti license --from of a program of musl" "${WORK}"
          "${package_bin}/anti${suffix}" license --from "${program}")
foreach(name musl mimalloc)
    file(STRINGS "${package_dir}/licenses/sources.txt" line REGEX "^${name} ")
    string(FIND "${alone_out}" "\nsource ${line}\n" at)
    if(line STREQUAL "" OR at EQUAL -1)
        message(FATAL_ERROR "the packed anti license --from prints no `source "
                            "${line}` for ${name}\n${alone_out}")
    endif()
endforeach()

run_alone("anti license" "${WORK}" "${package_bin}/anti${suffix}" license)
if(NOT alone_out MATCHES "^package antic ${package_version_pattern} MIT\npackage anti ${package_version_pattern} MIT\npackage anti\\.rt ${package_version_pattern} 0BSD\n")
    message(FATAL_ERROR "the packed anti license does not start with antic, "
                        "anti and the runtime\n${alone_out}")
endif()
file(READ "${package_dir}/LICENSE" packed_license)
string(FIND "${alone_out}" "\ntext for antic anti\n${packed_license}" at)
if(at EQUAL -1)
    message(FATAL_ERROR "the packed anti license prints no LICENSE of the "
                        "package\n${alone_out}")
endif()
file(GLOB packed_texts RELATIVE "${package_dir}/licenses"
     "${package_dir}/licenses/*.txt")
list(REMOVE_ITEM packed_texts sources.txt)
foreach(file IN LISTS packed_texts)
    string(REGEX REPLACE "\\.txt$" "" name "${file}")
    if(name STREQUAL "anti_rt")
        set(name "anti.rt")
    endif()
    file(READ "${package_dir}/licenses/${file}" text)
    string(FIND "${alone_out}" "\npackage ${name} " line_at)
    string(FIND "${alone_out}" "${text}" text_at)
    if(line_at EQUAL -1 OR text_at EQUAL -1)
        message(FATAL_ERROR "the packed anti license lacks the component "
                            "${name} or its text\n${alone_out}")
    endif()
endforeach()
file(STRINGS "${package_dir}/licenses/sources.txt" packed_sources
     REGEX "^[^#]")
foreach(line IN LISTS packed_sources)
    string(FIND "${alone_out}" "\nsource ${line}\n" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the packed anti license prints no `source "
                            "${line}`\n${alone_out}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
