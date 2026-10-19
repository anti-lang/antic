# The build works out the host of the machine in one place, tools/host.cmake.
# The check refuses three forms that worked it out again. Run with cmake -P
# and ROOT, the root of the repository.
#
# - cmake_host_system_information anywhere but tools/host.cmake.
# - CMAKE_EXECUTABLE_SUFFIX in a script run with cmake -P, a script of
#   tools/ or a runner, where CMake leaves it empty on every host.
# - In the build files and the scripts of tools/, a variable of CMake that
#   names the system, WIN32, APPLE, UNIX, CMAKE_HOST_WIN32,
#   CMAKE_HOST_APPLE, CMAKE_HOST_UNIX, CMAKE_SYSTEM_NAME or
#   CMAKE_SYSTEM_PROCESSOR, and a test of ANTIC_HOST_TARGET against the
#   name of a system. The build files ask ANTIC_HOST_OS, and the scripts
#   antic_machine_host.
# - In the build files, a second question to the C compiler for its
#   resource directory, which CMakeLists.txt asks once as
#   ANTIC_CLANG_RESOURCE_DIR.
#
# A comment may name any of them.

cmake_minimum_required(VERSION 3.21)

set(failures "")
# check(<label> <regex> <file>...) refuses a line of code of a file that
# matches the regex.
function(check label regex)
    foreach(path IN LISTS ARGN)
        # One list item per line, empty lines included, so that the number
        # a finding names is the number of the editor.
        file(READ "${ROOT}/${path}" text)
        string(REPLACE ";" "," text "${text}")
        string(REPLACE "\n" ";" lines "${text}")
        set(number 0)
        foreach(line IN LISTS lines)
            math(EXPR number "${number} + 1")
            string(REGEX REPLACE "#.*" "" code "${line}")
            # The style of a line end is no system.
            string(REPLACE "NEWLINE_STYLE UNIX" "" code "${code}")
            if(code MATCHES "${regex}")
                string(APPEND failures "\n${path}:${number}: ${label}")
            endif()
        endforeach()
    endforeach()
    set(failures "${failures}" PARENT_SCOPE)
endfunction()

file(GLOB tools RELATIVE "${ROOT}" "${ROOT}/tools/*.cmake")
list(REMOVE_ITEM tools tools/host.cmake)
file(GLOB scripts RELATIVE "${ROOT}" "${ROOT}/tests/*.cmake"
     "${ROOT}/tests/runners/*.cmake")
list(REMOVE_ITEM scripts tests/runners/run_host_names.cmake)
file(GLOB native RELATIVE "${ROOT}" "${ROOT}/src/native/*.cmake")
set(build CMakeLists.txt tests/CMakeLists.txt src/native/CMakeLists.txt
    ${native})

check("the host worked out outside tools/host.cmake"
      "cmake_host_system_information" ${build} ${tools} ${scripts})
check("CMAKE_EXECUTABLE_SUFFIX in a script of cmake -P"
      "CMAKE_EXECUTABLE_SUFFIX" ${tools} ${scripts})
set(system "(^|[^A-Za-z0-9_{])(WIN32|APPLE|UNIX|CMAKE_HOST_WIN32|CMAKE_HOST_APPLE|CMAKE_HOST_UNIX|CMAKE_SYSTEM_NAME|CMAKE_SYSTEM_PROCESSOR)([^A-Za-z0-9_]|$)")
check("a variable of CMake that names the system" "${system}"
      ${build} ${tools})
check("ANTIC_HOST_TARGET tested for a system"
      "ANTIC_HOST_TARGET[ \t]+MATCHES[ \t]+\"\\^(windows|linux|macos)"
      ${build} ${tools})
set(asked "")
foreach(path IN LISTS build)
    file(STRINGS "${ROOT}/${path}" lines REGEX "-print-resource-dir")
    foreach(line IN LISTS lines)
        list(APPEND asked "${path}")
    endforeach()
endforeach()
if(NOT asked STREQUAL "CMakeLists.txt")
    string(APPEND failures "\nthe resource directory of the C compiler asked "
                           "in ${asked}")
endif()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "the host worked out again:${failures}")
endif()
