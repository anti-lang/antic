# The anti of the package builds a project of anti.toml with anti build
# --release, and anti symbols inventory of the package finds the symbols
# of that program by its build id. The reader of TOML and the reader of
# the symbols of a binary stand in src/rt, and on a Linux host the packer
# compiled that anti itself, which once left them out.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_build.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

file(COPY "${ROOT}/tests/anti-build/app" DESTINATION "${WORK}")
set(project "${WORK}/app")
run_alone("anti build --release" "${project}" "${package_bin}/anti${suffix}"
          build --release)
set(release "${project}/dist/${HOST}/release")
if(NOT EXISTS "${release}/app${suffix}")
    message(FATAL_ERROR "the packed anti built no program of anti.toml")
endif()
# A Windows program keeps its functions in the PDB, which only DbgHelp
# reads. The inventory runs on the other hosts, as anti_symbols does.
if(NOT HOST MATCHES "^windows-")
    file(MAKE_DIRECTORY "${WORK}/deploy")
    file(COPY "${release}/app" "${release}/app-symbols.zip"
         DESTINATION "${WORK}/deploy")
    file(WRITE "${WORK}/deploy/app.toml" "")
    file(STRINGS "${WORK}/deploy/app" id_line REGEX "^build [0-9a-f]+$")
    list(GET id_line 0 id_line)
    string(SUBSTRING "${id_line}" 6 -1 program_id)
    run_packed("anti symbols inventory" symbols inventory
               --conf "${WORK}/deploy/app.toml" --out "${WORK}/all.zip")
    if(NOT packed_out MATCHES "present [^\n]*app ${program_id}\n")
        message(FATAL_ERROR "the packed anti found no symbols of the "
                            "program\n${packed_out}")
    endif()
endif()

file(REMOVE_RECURSE "${WORK}")
