# Build what the test of a release builds from a fresh install: a raylib
# program and a plugin host for the host, both run, and a hello program
# for each of the other five targets.
#
#   cmake -DBIN=<dir> -DARCHIVE=<dir> -DROOT=<tree> -DHOST=<target>
#         -DWORK=<dir> -P tools/check-offline.cmake
#
# BIN holds antic and anti of the install, and ARCHIVE is the runtime
# archive the two find: the data directory of an install, or anti/ of an
# unpacked package, whose bin/ is then BIN. ROOT is a tree of this
# repository, for the sources of the programs. HOST is the target of this
# machine, and WORK a directory this script replaces.
#
# DESIGN: Eddie decided the test of a release on 2026-09-27 under "Binary
# distribution" in docs/decisions.md, and decision 8 of
# docs/work-order-distribution.md makes it step 5 of ./r, which turns the
# network of each VM off before it runs this. The script reaches no
# network itself and asks for none, so the test package_keys runs it on
# the package of the host as well. Whether the network is off is the
# question of the release script, which holds the commands.
#
# DESIGN: every tool is named by its path, and each runs with a PATH of
# BIN alone and no LIB, so nothing of LLVM or of Anti comes from the
# machine. No call names the archive: antic and anti find it by the one
# rule of runtime_archive in src/antic/userdirs.c, as a user's call does.
#
# DESIGN: the raylib program asks for a mouse button, which pulls in rcore
# and GLFW and opens no window, since neither VM has a display. It imports
# the binding that anti bind writes from tests/bind/raylib_api.json and
# links the library of raylib from lib/<host>/ of the archive. The plugin
# is the one of tests/plugin, and the host prints what the test
# plugin_host expects.
cmake_minimum_required(VERSION 3.21)

foreach(name BIN ARCHIVE ROOT HOST WORK)
    if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
        message(FATAL_ERROR "check-offline: -D${name}= is missing")
    endif()
endforeach()
foreach(name BIN ARCHIVE ROOT WORK)
    file(TO_CMAKE_PATH "${${name}}" ${name})
endforeach()

set(targets linux-x86_64 linux-arm64 macos-arm64 macos-x86_64
    windows-x86_64 windows-arm64)
if(NOT HOST IN_LIST targets)
    message(FATAL_ERROR "check-offline: ${HOST} is no target")
endif()
set(exe "")
set(shared ".dylib")
set(raylib "libraylib.a")
if(HOST MATCHES "^linux-")
    set(shared ".so")
elseif(HOST MATCHES "^windows-")
    set(exe ".exe")
    set(shared ".dll")
    set(raylib "raylib.lib")
endif()
foreach(program antic anti)
    if(NOT EXISTS "${BIN}/${program}${exe}")
        message(FATAL_ERROR "check-offline: ${BIN} holds no ${program}${exe}, "
                            "so it is no install")
    endif()
endforeach()
set(raylib "${ARCHIVE}/lib/${HOST}/${raylib}")
set(readobj "${ARCHIVE}/bin/llvm-readobj${exe}")
foreach(file "${raylib}" "${readobj}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "check-offline: the archive holds no ${file}")
    endif()
endforeach()

# Run a command with the install alone on the PATH. It must succeed, and
# its output is alone_out.
function(alone what dir)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${BIN}" --unset=LIB ${ARGN}
        WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "check-offline: ${what} failed with ${status}\n"
                            "${out}${err}")
    endif()
    string(REPLACE "\r\n" "\n" out "${out}")
    set(alone_out "${out}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(antic "${BIN}/antic${exe}")
set(anti "${BIN}/anti${exe}")

# The raylib program. The binding names what the library needs of each
# system: the frameworks of a macOS program and the libraries of a Linux
# one, which the link takes as options. The language has no such line for
# Windows, so the link of a Windows program takes the import libraries of
# the four DLLs that raylib reaches by their paths in the sysroot of the
# archive, where a C object would name them in a directive.
set(game "${WORK}/raylib")
file(MAKE_DIRECTORY "${game}/lib/game")
alone("anti bind of raylib_api.json" "${WORK}" "${anti}" bind
      "${ROOT}/tests/bind/raylib_api.json" --module game.raylib
      -o "${game}/src/game")
alone("antic -c of the raylib binding" "${game}" "${antic}" -c
      -I "${game}/src" -o "${game}/lib/game/raylib.antl"
      "${game}/src/game/raylib.anti")
set(link_options "")
if(HOST MATCHES "^linux-")
    file(STRINGS "${game}/src/game/raylib.anti" rows
         REGEX "^link linux \"[^\"]+\";$")
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^link linux \"([^\"]+)\";$" "\\1" name "${row}")
        list(APPEND link_options --linux-lib "${name}")
    endforeach()
elseif(HOST MATCHES "^macos-")
    file(STRINGS "${game}/src/game/raylib.anti" rows
         REGEX "^link framework \"[^\"]+\";$")
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^link framework \"([^\"]+)\";$" "\\1" name
               "${row}")
        list(APPEND link_options --framework "${name}")
    endforeach()
endif()
set(link_inputs "${raylib}")
if(HOST MATCHES "^windows-")
    foreach(name gdi32 user32 shell32 winmm)
        list(APPEND link_inputs "${ARCHIVE}/sysroot/${HOST}/lib/${name}.lib")
    endforeach()
endif()
file(WRITE "${game}/game.anti"
     "import anti.io;\nimport game.raylib;\n\nfn main() -> int\n{\n"
     "    if !raylib.IsMouseButtonPressed(0) {\n"
     "        io.println(\"no click\");\n    }\n    return 0;\n}\n")
alone("the link of a raylib program for ${HOST}" "${game}" "${antic}"
      --target "${HOST}" -I "${game}/lib" ${link_options}
      -o "${WORK}/game${exe}" "${game}/game.anti" ${link_inputs})
alone("the raylib program" "${WORK}" "${WORK}/game${exe}")
if(NOT alone_out MATCHES "no click\n$")
    message(FATAL_ERROR "check-offline: the raylib program prints "
                        "'${alone_out}'")
endif()
message("a raylib program of ${HOST} builds and runs")

# The plugin host. The interface is a library file that both sides import,
# and a Windows plugin links against the import library of its host.
set(sources "${ROOT}/tests/plugin")
set(plugin "${WORK}/plugin")
file(MAKE_DIRECTORY "${plugin}/net/example" "${plugin}/lib")
alone("antic -c of the interface of the plugin" "${plugin}" "${antic}" -c
      -I "${sources}" -o "${plugin}/net/example/greet.antl"
      "${sources}/net/example/greet.anti")
alone("the link of a plugin host for ${HOST}" "${plugin}" "${antic}"
      --target "${HOST}" -I "${sources}" -I "${plugin}"
      -o "${plugin}/host${exe}" "${sources}/host.anti")
set(import_library "")
if(HOST MATCHES "^windows-")
    set(import_library "${plugin}/host.lib")
endif()
alone("the link of a plugin for ${HOST}" "${plugin}" "${antic}"
      --target "${HOST}" --lib shared --no-runtime -I "${sources}"
      -I "${plugin}" -o "${plugin}/lib/libfancy${shared}"
      "${sources}/net/example/fancy.anti" ${import_library})
alone("the plugin host" "${plugin}" "${plugin}/host${exe}"
      "${plugin}/lib/libfancy${shared}")
file(READ "${sources}/host.expected" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT alone_out STREQUAL expected)
    message(FATAL_ERROR "check-offline: the plugin host prints\n${alone_out}"
                        "and tests/plugin/host.expected holds\n${expected}")
endif()
message("a plugin host of ${HOST} loads its plugin")

# A hello program for each of the other five targets. llvm-readobj of the
# archive names the format of the file the link wrote.
set(format_linux-x86_64 "elf64-x86-64")
set(format_linux-arm64 "elf64-littleaarch64")
set(format_macos-arm64 "Mach-O arm64")
set(format_macos-x86_64 "Mach-O 64-bit x86-64")
set(format_windows-x86_64 "COFF-x86-64")
set(format_windows-arm64 "COFF-ARM64")
file(WRITE "${WORK}/hello.anti"
     "import anti.io;\n\nfn main() -> int\n{\n    io.print(\"hello\");\n"
     "    return 0;\n}\n")
foreach(target IN LISTS targets)
    if(target STREQUAL HOST)
        continue()
    endif()
    set(program "${WORK}/hello-${target}")
    if(target MATCHES "^windows-")
        string(APPEND program ".exe")
    endif()
    alone("the link of a hello program for ${target}" "${WORK}" "${antic}"
          --target "${target}" -o "${program}" "${WORK}/hello.anti")
    if(NOT EXISTS "${program}")
        message(FATAL_ERROR "check-offline: antic wrote no hello program for "
                            "${target}")
    endif()
    alone("llvm-readobj of the hello program for ${target}" "${WORK}"
          "${readobj}" --file-headers "${program}")
    if(NOT alone_out MATCHES "\nFormat: ([^\n]+)\n")
        message(FATAL_ERROR "check-offline: llvm-readobj names no format of "
                            "the hello program for ${target}\n${alone_out}")
    endif()
    set(format "${CMAKE_MATCH_1}")
    if(NOT format STREQUAL "${format_${target}}")
        message(FATAL_ERROR "check-offline: the hello program for ${target} is "
                            "${format}, not ${format_${target}}")
    endif()
    message("a hello program links for ${target}: ${format}")
endforeach()
message("the install builds for ${HOST} and links for the other five targets")
