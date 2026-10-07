cmake_minimum_required(VERSION 3.21)

# The sysroot of a Windows target over the Build Tools of a Windows host.
# tools/get-sysroot.cmake takes the newest version of the MSVC tools and of
# the Windows SDK. Windows Kits/10/Include of a GitHub runner holds `wdf` of
# the Driver Kit beside the versions of the SDK, and the script once took
# it for the newest version. The test lays out a fake installation and a
# fake Windows Kits with `wdf`, an older and a newer SDK, and checks the
# junctions of the sysroot. Run with cmake -P and these values:
#   ROOT   the repository
#   WORK   a directory for the trees

if(NOT CMAKE_HOST_WIN32)
    message("SKIP: the sysroot over the Build Tools needs a Windows host")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
set(vs "${WORK}/vs")
set(kits "${WORK}/kits")
foreach(dir "${vs}/VC/Tools/MSVC/14.29.30133/include"
            "${vs}/VC/Tools/MSVC/14.44.35207/include"
            "${vs}/VC/Tools/MSVC/14.44.35207/lib/x64"
            "${kits}/Include/10.0.19041.0/ucrt"
            "${kits}/Include/wdf/kmdf"
            "${kits}/Lib/wdf/kmdf")
    file(MAKE_DIRECTORY "${dir}")
endforeach()
foreach(part ucrt um shared)
    file(MAKE_DIRECTORY "${kits}/Include/10.0.26100.0/${part}")
endforeach()
foreach(part ucrt um)
    file(MAKE_DIRECTORY "${kits}/Lib/10.0.26100.0/${part}/x64")
endforeach()
file(WRITE "${kits}/Include/10.0.26100.0/ucrt/stdio.h" "/* the SDK */\n")
file(TO_NATIVE_PATH "${vs}" vs_native)
file(WRITE "${WORK}/vswhere.cmd" "@echo ${vs_native}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
            "-DLLVM_BIN=${WORK}" "-DTARGETS=windows-x86_64"
            "-DVSWHERE=${WORK}/vswhere.cmd" "-DWINDOWS_KITS=${kits}"
            -P "${ROOT}/tools/get-sysroot.cmake"
    RESULT_VARIABLE status OUTPUT_FILE "${WORK}/out.txt"
    ERROR_FILE "${WORK}/err.txt")
if(NOT status EQUAL 0)
    file(READ "${WORK}/err.txt" err)
    message(FATAL_ERROR "get-sysroot.cmake failed with ${status}\n${err}")
endif()
set(root "${WORK}/sysroot/windows-x86_64")
if(NOT EXISTS "${root}/sdk/include/ucrt/stdio.h")
    message(FATAL_ERROR "the sysroot takes no headers of SDK 10.0.26100.0")
endif()
if(NOT IS_DIRECTORY "${root}/crt/lib/x86_64")
    message(FATAL_ERROR "the sysroot takes no CRT of MSVC 14.44.35207")
endif()
