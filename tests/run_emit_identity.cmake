# antic writes the same assembly for every program of tests/programs and
# every target on every host. MANIFEST holds the digests that a run on the
# Mac wrote. A run on Linux or Windows compares its own with them. It finds
# an antic built by another compiler that emits another program, as antic
# built by clang for Windows did while a call passed two arguments that
# emit code.
#
#   cmake -DANTIC=<antic> -DRUNTIME=<dir> -DPROGRAMS=<dir> -DMANIFEST=<file>
#         -DWORK=<dir> [-DWRITE=yes] -P tests/run_emit_identity.cmake
#
# WRITE=yes writes the manifest, on the Mac, after a change to antic that
# changes its output.

set(targets linux-x86_64 linux-arm64 macos-arm64 macos-x86_64 windows-x86_64
            windows-arm64)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(GLOB sources "${PROGRAMS}/*.anti")
list(SORT sources)
set(lines "")
foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    foreach(target IN LISTS targets)
        set(out "${WORK}/${name}.${target}.s")
        execute_process(COMMAND "${ANTIC}" --target ${target} --runtime "${RUNTIME}"
                                -S -o "${out}" "${source}"
                        RESULT_VARIABLE status ERROR_VARIABLE err)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "antic -S failed for ${name} on ${target}\n${err}")
        endif()
        file(SHA256 "${out}" digest)
        string(APPEND lines "${digest}  ${name}.${target}.s\n")
    endforeach()
endforeach()

if(WRITE STREQUAL "yes")
    if(NOT CMAKE_HOST_APPLE)
        message(FATAL_ERROR "the manifest holds the output of the Mac, so the Mac "
                            "writes it")
    endif()
    file(WRITE "${MANIFEST}" "${lines}")
    return()
endif()
file(READ "${MANIFEST}" expected)
if(NOT lines STREQUAL expected)
    string(REPLACE "\n" ";" got "${lines}")
    string(REPLACE "\n" ";" want "${expected}")
    set(differ "")
    foreach(line IN LISTS got)
        if(line AND NOT line IN_LIST want)
            string(REGEX REPLACE "^[0-9a-f]+  " "" file "${line}")
            list(APPEND differ "${file}")
        endif()
    endforeach()
    list(LENGTH differ count)
    list(JOIN differ ", " differ)
    message(FATAL_ERROR "antic wrote other assembly than the Mac for ${count} "
                        "files: ${differ}. A change to antic that alters its "
                        "output writes the manifest on the Mac with -DWRITE=yes "
                        "and the same values.")
endif()
