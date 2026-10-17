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
#         -DVERSION=<version> -DWORK=<dir> -DPINNED=<dir> [-DWRITE=yes]
#         -P tests/runners/run_emit_identity.cmake
#
# The text names the version of antic in its ident line, which the digest
# reads as VERSION. It names the version of the standard library in the
# constant anti.lang.package.version too, which the digest keeps, so a new
# version writes the manifest again. WRITE=yes writes the manifest, on the
# Mac, after a change to antic that changes its output.
#
# DESIGN: PINNED, under the build tree, keeps the texts of the manifest
# that a run last wrote or passed, with the digest of that manifest. A
# run that differs, and a run that writes the manifest again, print the
# first line that moved in each text that differs from a kept one. The
# diff of the manifest names the programs that moved and nothing of how,
# and "Pinned outputs" of docs/notes/value-rules.md asks for that reading.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

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

# The files of <manifest> whose line <lines> lacks, in <out>.
function(moved out manifest)
    string(REPLACE "\n" ";" got "${lines}")
    string(REPLACE "\n" ";" want "${manifest}")
    set(differ "")
    foreach(line IN LISTS got)
        if(line AND NOT line IN_LIST want)
            string(REGEX REPLACE "^[0-9a-f]+  " "" file "${line}")
            list(APPEND differ "${file}")
        endif()
    endforeach()
    set(${out} "${differ}" PARENT_SCOPE)
endfunction()

# Set <out> to the first line that moved in each file of <files>, against
# the text PINNED keeps of <manifest>. The first twenty files are shown.
function(show_moved out manifest files)
    set(${out} "" PARENT_SCOPE)
    string(SHA256 stamp "${manifest}")
    if(NOT DEFINED PINNED OR NOT EXISTS "${PINNED}/manifest.sha256")
        return()
    endif()
    file(STRINGS "${PINNED}/manifest.sha256" kept LIMIT_COUNT 1)
    if(NOT kept STREQUAL stamp)
        return()
    endif()
    set(text "")
    set(shown 0)
    foreach(file IN LISTS files)
        if(shown EQUAL 20)
            string(APPEND text "and more files\n")
            break()
        endif()
        if(EXISTS "${PINNED}/${file}")
            file(READ "${PINNED}/${file}" old HEX)
            file(READ "${WORK}/${file}" new HEX)
            bytes_difference(difference "${file}" "${old}" "${new}")
            string(APPEND text "${difference}\n")
            math(EXPR shown "${shown} + 1")
        endif()
    endforeach()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Keep the texts of this run in PINNED as those of the manifest <lines>.
function(keep_texts)
    if(NOT DEFINED PINNED)
        return()
    endif()
    file(REMOVE_RECURSE "${PINNED}")
    file(COPY "${WORK}/" DESTINATION "${PINNED}")
    string(SHA256 stamp "${lines}")
    write_bytes("${PINNED}/manifest.sha256" "${stamp}\n")
endfunction()

if(WRITE STREQUAL "yes")
    if(NOT CMAKE_HOST_APPLE)
        message(FATAL_ERROR "the manifest holds the output of the Mac, so the Mac "
                            "writes it")
    endif()
    if(EXISTS "${MANIFEST}")
        file(READ "${MANIFEST}" old)
        moved(differ "${old}")
        show_moved(text "${old}" "${differ}")
        if(NOT text STREQUAL "")
            message("The texts that moved:\n${text}")
        endif()
    endif()
    file(WRITE "${MANIFEST}" "${lines}")
    keep_texts()
    return()
endif()
file(READ "${MANIFEST}" expected)
if(lines STREQUAL expected)
    string(SHA256 stamp "${lines}")
    set(kept "")
    if(EXISTS "${PINNED}/manifest.sha256")
        file(STRINGS "${PINNED}/manifest.sha256" kept LIMIT_COUNT 1)
    endif()
    if(NOT kept STREQUAL stamp)
        keep_texts()
    endif()
    return()
endif()
moved(differ "${expected}")
show_moved(text "${expected}" "${differ}")
list(LENGTH differ count)
list(JOIN differ ", " names)
message(FATAL_ERROR "antic wrote other LLVM text than the Mac for ${count} "
                    "files: ${names}. A change to antic that alters its "
                    "output writes the manifest on the Mac with -DWRITE=yes "
                    "and the same values.\n${text}")
