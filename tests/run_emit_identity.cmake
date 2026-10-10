# antic writes the same LLVM text for every program of tests/programs and
# every target on every host. MANIFEST holds the digests of the text that
# --dump-llvm prints, which a run on the Mac wrote. A run on Linux or
# Windows compares its own with them. It finds an antic built by another
# compiler that emits another program, as antic built by clang for Windows
# did while a call passed two arguments that emit code. opt and llc are
# deterministic for the pinned release and one input, so the text decides
# the object.
#
#   cmake -DANTIC=<antic> -DRUNTIME=<dir> -DPROGRAMS=<dir> -DMANIFEST=<file>
#         -DVERSION=<version> -DWORK=<dir> [-DWRITE=yes]
#         -P tests/run_emit_identity.cmake
#
# The text names the version of antic in its ident line, which the digest
# reads as VERSION. It names the version of the standard library in the
# constant anti.lang.package.version too, which the digest keeps, so a new
# version writes the manifest again. WRITE=yes writes the manifest, on the
# Mac, after a change to antic that changes its output.

cmake_minimum_required(VERSION 3.21)

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
        set(out "${WORK}/${name}.${target}.ll")
        execute_process(COMMAND "${ANTIC}" --target ${target}
                                --runtime "${RUNTIME}" --dump-llvm "${source}"
                        RESULT_VARIABLE status OUTPUT_VARIABLE text
                        ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "antic --dump-llvm failed for ${name} on "
                                "${target}\n${err}")
        endif()
        string(REPLACE "antic ${VERSION}" "antic VERSION" text "${text}")
        # DESIGN: the digest is the one of the text as antic means it. On
        # Windows the standard output of antic ends each line with CRLF,
        # the variable loses the CR, and file(WRITE) would write it back,
        # so the digest reads the variable. The file is there to read.
        string(SHA256 digest "${text}")
        file(WRITE "${out}" "${text}")
        string(APPEND lines "${digest}  ${name}.${target}.ll\n")
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
    message(FATAL_ERROR "antic wrote other LLVM text than the Mac for ${count} "
                        "files: ${differ}. A change to antic that alters its "
                        "output writes the manifest on the Mac with -DWRITE=yes "
                        "and the same values.")
endif()
