# The host of this machine and the names that follow from a target. The
# host is named as the six targets of antic are, so one spelling serves
# the pins, the runtime archive and the option --target. The configure
# step, the fetch of the pinned tools, the packer and the scripts of
# tools/ include this file, and the test host_names refuses the host
# worked out anywhere else.
#
# antic_machine_host(<out>) sets <out> to the host of this machine, such as
# macos-arm64. It reads the machine, not a target of the build, so it
# answers in a script run with cmake -P as well.
#
# antic_executable_suffix(<out> <target>) sets <out> to the suffix of an
# executable of <target>, .exe on Windows and empty elsewhere. CMake
# defines CMAKE_EXECUTABLE_SUFFIX in a build alone, and a script run with
# cmake -P reads it empty on every host.
#
# antic_static_library(<out> <name> <target>) sets <out> to the file name
# of the static library <name> of <target>, <name>.lib on Windows and
# lib<name>.a elsewhere, as anti_rt.lib and libanti_rt.a.

function(antic_machine_host out)
    cmake_host_system_information(RESULT os QUERY OS_NAME)
    cmake_host_system_information(RESULT platform QUERY OS_PLATFORM)
    string(TOLOWER "${os}" os)
    string(TOLOWER "${platform}" platform)
    if(platform MATCHES "^(arm64|aarch64)$")
        set(platform arm64)
    elseif(platform MATCHES "^(x86_64|amd64|x64)$")
        set(platform x86_64)
    endif()
    set(${out} "${os}-${platform}" PARENT_SCOPE)
endfunction()

function(antic_executable_suffix out target)
    if(target MATCHES "^windows-")
        set(${out} ".exe" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

function(antic_static_library out name target)
    if(target MATCHES "^windows-")
        set(${out} "${name}.lib" PARENT_SCOPE)
    else()
        set(${out} "lib${name}.a" PARENT_SCOPE)
    endif()
endfunction()
