# Lay out bin/ of the runtime archive: copy every tool that
# antic_llvm_archive_tools of tools/pinned-tools.cmake names from LLVM_BIN
# to DEST, and write llvm-version beside them. A tool missing from LLVM_BIN
# is an error that names it, and nothing is written then. The tools of
# ANTIC_LLVM_UNPACKED stay out, as tools/check-llvm.cmake states, and the
# copy removes one that an older build left in DEST. The target llvm_tools of
# CMakeLists.txt runs this after that check. Run with cmake -P and these
# values:
#   LLVM_BIN  the directory of the installed tools
#   DEST      the directory of the runtime archive's tools

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/pinned-tools.cmake")
file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" pin)
string(STRIP "${pin}" pin)
include("${CMAKE_CURRENT_LIST_DIR}/host.cmake")
antic_machine_host(machine)
antic_executable_suffix(exe "${machine}")
antic_llvm_archive_tools(names)
set(missing "")
foreach(tool IN LISTS names)
    if(NOT EXISTS "${LLVM_BIN}/${tool}${exe}")
        list(APPEND missing "${LLVM_BIN}/${tool}${exe} is missing")
    endif()
endforeach()
if(missing)
    list(JOIN missing "\n" text)
    message(FATAL_ERROR "${text}")
endif()
file(MAKE_DIRECTORY "${DEST}")
foreach(tool IN LISTS ANTIC_LLVM_UNPACKED)
    file(REMOVE "${DEST}/${tool}${exe}")
endforeach()
foreach(tool IN LISTS names)
    file(COPY "${LLVM_BIN}/${tool}${exe}" DESTINATION "${DEST}")
endforeach()
file(WRITE "${DEST}/llvm-version" "${pin}\n")
