# Run ./r --dry-run on the copy of tests/release_copy.cmake and check
# what each step wrote: the order of step 5 on the two VMs, the LLVM tools
# of step 1, the packages, the symbols archives and their manifest, the
# plan of the steps a dry run does not run and the downloads page.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DHOST_LINK=<options> -DSDK=<Apple SDK> -DLLVM_BIN=<dir>
#         -P tests/runners/run_release_dry_run.cmake
#
# tests/release_copy.cmake describes the values.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../release_copy.cmake")
if(release_skipped)
    return()
endif()

# DESIGN: step 5 installs the package on each VM, turns the network of
# the VM off, runs the check of the install, turns the network on and runs
# the suite, which Eddie decided on 2026-10-08 in decision 8 of
# docs/work-order-distribution.md. The probe says off before the check
# and on before the suite, so a command that changed nothing stops the
# step. ssh.log of the stand-in holds the calls in their order with the
# state of the network at each.
set(vm_scripts "anti-linux=run\\.sh" "anti-windows=run\\.cmd")
foreach(pair IN LISTS vm_scripts)
    string(REPLACE "=" ";" parts "${pair}")
    list(GET parts 0 vm)
    list(GET parts 1 script)
    file(READ "${WORK}/ssh.log" calls)
    if(NOT calls MATCHES "${vm} on [^\n]*${script} install\n.*${vm} off [^\n]*github\\.com\n.*${vm} off [^\n]*${script} offline\n.*${vm} on [^\n]*github\\.com\n.*${vm} on [^\n]*${script} suite\n")
        message(FATAL_ERROR "step 5 on ${vm} is not the install, the check with "
                            "the network off and the suite with it on, in "
                            "that order:\n${calls}")
    endif()
    foreach(line "${vm}: the network is off" "${vm}: the network is on again"
            "${vm}: the install builds with the network off")
        if(NOT out MATCHES "${line}")
            message(FATAL_ERROR "the dry run does not say `${line}`\n${out}${err}")
        endif()
    endforeach()
    if(EXISTS "${WORK}/offline-${vm}")
        message(FATAL_ERROR "step 5 left ${vm} off the network")
    endif()
endforeach()

# DESIGN: the script of a VM writes its logs beside the files of the run,
# in anti-release of the user, and none into the tree it extracted. The
# tree of a VM has no history, so the test repo_layout reads its directory
# there, and a log at its top level is an entry outside the layout. The
# real step 5 of 2026-10-10 failed on that test, which is how the rule
# got here. A log is named by a variable of the script, never bare.
foreach(script vm-linux.sh vm-windows.cmd)
    file(READ "${copy}/build/dist/dry-run/work/${script}" script_text)
    if(script_text MATCHES "[> ]([A-Za-z][A-Za-z0-9_-]*\\.log)")
        message(FATAL_ERROR "${script} of step 5 names the log "
                            "${CMAKE_MATCH_1} without a directory, so it "
                            "lands in the tree of the VM, where repo_layout "
                            "refuses it")
    endif()
    if(NOT script_text MATCHES "anti-release[^\n]*\n")
        message(FATAL_ERROR "${script} of step 5 names no anti-release")
    endif()
endforeach()

# Step 1 fetches the LLVM tools of every host but this machine into
# llvm-tools/<host> beside the llvm/ of the downloads, through
# tools/get-llvm.cmake, and step 3 hands that directory to the packer.
# The stand-in of the script writes the layout, so what this reads is the
# wiring: one call per host, into the directory the cache names.
foreach(host IN LISTS hosts)
    if(host STREQUAL machine)
        continue()
    endif()
    if(NOT out MATCHES "the LLVM tools of ${host} ")
        message(FATAL_ERROR "the dry run fetched no LLVM tools of ${host}\n"
                            "${out}${err}")
    endif()
    if(NOT EXISTS "${WORK}/deps/llvm-tools/${host}/bin/llvm-version")
        message(FATAL_ERROR "the dry run laid out no llvm-tools/${host}/bin\n"
                            "${out}${err}")
    endif()
endforeach()
if(EXISTS "${WORK}/deps/llvm-tools/${machine}")
    message(FATAL_ERROR "the dry run fetched the LLVM tools of this machine, "
                        "which the runtime archive holds")
endif()

foreach(name state/02-suite state/03-packages state/04-symbols state/05-vms
        logs/mac.log logs/asan.log logs/ubsan.log logs/linux.log
        logs/windows.log packages/SHA256SUMS)
    if(NOT EXISTS "${dist}/${name}")
        message(FATAL_ERROR "the dry run wrote no ${name}\n${out}${err}")
    endif()
endforeach()
# DESIGN: the packages and the symbols archives stand in one directory
# under one manifest, which is the directory tools/check-release.cmake
# reads, step 6 signs and step 7 uploads. A file beside the twelve is a file a user downloads.
file(READ "${dist}/packages/SHA256SUMS" manifest)
foreach(host IN LISTS hosts)
    foreach(name "anti-${version}-${host}.tar.xz"
            "anti-${version}-${host}-symbols.zip")
        if(NOT EXISTS "${dist}/packages/${name}")
            message(FATAL_ERROR "the dry run wrote no packages/${name}\n${out}${err}")
        endif()
        if(NOT manifest MATCHES "  ${name}\n")
            message(FATAL_ERROR "SHA256SUMS names no ${name}\n${manifest}")
        endif()
    endforeach()
endforeach()
string(REGEX MATCHALL "\n" rows "${manifest}")
list(LENGTH rows count)
if(NOT count EQUAL 12)
    message(FATAL_ERROR "SHA256SUMS holds ${count} lines, and a release has "
                        "twelve files\n${manifest}")
endif()
file(GLOB beside RELATIVE "${dist}/packages" "${dist}/packages/*")
foreach(name IN LISTS beside)
    if(NOT name STREQUAL "SHA256SUMS" AND NOT manifest MATCHES "  ${name}\n")
        message(FATAL_ERROR "${name} stands beside the twelve files of the "
                            "release, and SHA256SUMS does not name it")
    endif()
endforeach()

# DESIGN: a Windows program has no symbol table, so its archive holds the
# map of its sections and the PDB that lld-link wrote beside it. Step 4
# reads the CodeView record of the executable against the GUID of that
# PDB before either goes in, and refuses a record that names a path.
foreach(host windows-x86_64 windows-arm64)
    set(archive "${dist}/packages/anti-${version}-${host}-symbols.zip")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                    RESULT_VARIABLE listed OUTPUT_VARIABLE entries
                    ERROR_VARIABLE listed_err ENCODING NONE)
    if(NOT listed EQUAL 0)
        message(FATAL_ERROR "${archive} does not list\n${entries}${listed_err}")
    endif()
    string(APPEND entries "${listed_err}")
    foreach(name antic.exe.syms anti.exe.syms antic.pdb anti.pdb)
        if(NOT entries MATCHES "${name}")
            message(FATAL_ERROR "${archive} holds no ${name}\n${entries}")
        endif()
    endforeach()
endforeach()

# Nothing is signed, tagged or uploaded, and the run says what it would do.
foreach(name packages/SHA256SUMS.sig state/06-digests state/07-release)
    if(EXISTS "${dist}/${name}")
        message(FATAL_ERROR "the dry run wrote ${name}")
    endif()
endforeach()
foreach(line "would sign" "would tag v${version}" "would create the release"
        "would upload 13 files" "would upload no SHA256SUMS.sig"
        "would rsync tools/install.sh"
        "would rsync [^\n]*tools/uninstall\\.sh"
        "would rsync [^\n]*tools/uninstall\\.ps1"
        "would rsync the downloads page" "would rsync SHA256SUMS.sig"
        "tools/keys/release.pem" "would install")
    if(NOT out MATCHES "${line}")
        message(FATAL_ERROR "the dry run does not say what it would do: "
                            "`${line}` is missing\n${out}${err}")
    endif()
endforeach()

# DESIGN: the downloads page shows the manual install with the checks of
# the installers as commands, which Eddie decided on 2026-10-08 in
# decision 7 of docs/work-order-distribution.md: the download of the
# package and the manifest from the release and of the signature and the
# key from the site, the signature check of the installers, the digest,
# the unpacking and the two programs on the path, for both shells. It
# names the uninstallers at the addresses of their headers, where step 9
# puts them by decision 9 of "Decisions of 2026-10-11". The page of the
# dry run has every mark filled.
file(READ "${dist}/downloads-index.html" page)
foreach(text "anti-${version}-macos-arm64.tar.xz"
        "downloads/anti/${version}/SHA256SUMS.sig" "keys/release.pem"
        "openssl dgst -sha256 -binary" "openssl pkeyutl -verify"
        "shasum -a 256" "sha256sum" "tar -xJf" ".local/share/anti"
        ".local/bin" "Get-FileHash" "tar.exe -xJf" "Programs\\anti\\bin"
        "antic --version" "anti --version"
        "https://anti-lang.com/uninstall.sh | sh"
        "https://anti-lang.com/uninstall.ps1 | iex")
    string(FIND "${page}" "${text}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "the downloads page shows no `${text}` in its "
                            "manual install\n${page}")
    endif()
endforeach()
if(page MATCHES "@[A-Z]+@")
    message(FATAL_ERROR "the downloads page keeps a mark unfilled:\n${page}")
endif()
foreach(word cmake sysroot-pin get-sysroot package-api)
    string(FIND "${page}" "${word}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "the downloads page names `${word}`, and nothing "
                            "a user runs needs it\n${page}")
    endif()
endforeach()

# Anti runs no CI, so a release names no runner matrix and no workflow.
if(out MATCHES "matrix|workflow")
    message(FATAL_ERROR "the release still names a runner matrix or a "
                        "workflow\n${out}")
endif()
execute_process(COMMAND "${copy}/r" --matrix WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE matrix_out
                ERROR_VARIABLE matrix_err ENCODING NONE)
if(NOT refused EQUAL 2 OR "${matrix_err}" MATCHES "matrix")
    message(FATAL_ERROR "./r --matrix ended with ${refused} and printed "
                        "`${matrix_out}${matrix_err}`")
endif()

# DESIGN: the assets of the release are the twelve files and the manifest.
# The signature stands on the site, so the plan of step 7 names it only to
# say that it stays off GitHub.
if(out MATCHES "would upload 14 files")
    message(FATAL_ERROR "step 7 still uploads the signature to the host that "
                        "serves the binaries\n${out}")
endif()

file(REMOVE_RECURSE "${WORK}")
