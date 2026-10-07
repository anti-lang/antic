# antic takes every path and every variable as UTF-8 on every host, and
# Windows sees UTF-16 at each of its calls. Each name below holds a letter
# outside ASCII and one outside every ANSI code page, which a call of an
# ANSI entry point of Windows cannot name.
#
#   1. A source, an output and a work directory of such a name, given on
#      the command line. antic reads the source, runs llvm-mc and the
#      linker on the paths, and the program exits with 42.
#   2. An antic copied into the bin/ of such a directory, beside a lib/,
#      takes the directory as its runtime archive.
#   3. The data directory of the user under such a name, from HOME or
#      LOCALAPPDATA, is the runtime archive of an antic in no archive.
#   4. anti reads a source of such a name given on its command line, and
#      finds one of such a name in the src/ of a project it made.
#
#   cmake -DANTIC=<antic> -DANTI=<anti> -DLLVM_MC=<llvm-mc> -DRUNTIME=<dir>
#         -DSOURCE=<return42.anti> -DWORK=<dir>
#         -P tests/run_unicode_paths.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(name "pü中")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/${name}")

# The tests of the archive below fail at the first file antic wants from
# it, and the message names the directory antic took. A data directory
# that does not exist leaves nothing else to answer.
if(WIN32)
    set(home_variable LOCALAPPDATA)
    set(data_below "anti")
else()
    set(home_variable HOME)
    set(data_below ".local/share/anti")
endif()

# 1.
set(source "${WORK}/${name}/return42.anti")
file(COPY "${SOURCE}" DESTINATION "${WORK}/${name}")
antic_program("${WORK}/${name}/prog" "${source}")
program_expect("prog" COMMAND "${WORK}/${name}/prog" STATUS 42)

# Run antic with the environment variable of the user's directories set
# to home, and set printed to what it wrote, with every backslash a
# slash. It must fail, since no archive holds more than lib/.
function(archive_run printed antic home)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${home_variable}=${home}"
                --unset=XDG_DATA_HOME --unset=ANTI_HOME
                "${antic}" -o "${WORK}/prog" "${SOURCE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(status EQUAL 0)
        message(FATAL_ERROR "${antic} compiled with an archive of lib/ alone")
    endif()
    string(REPLACE "\\" "/" text "${out}${err}")
    set(${printed} "${text}" PARENT_SCOPE)
endfunction()

# 2.
set(archive "${WORK}/archive-${name}")
file(MAKE_DIRECTORY "${archive}/bin" "${archive}/lib")
file(COPY "${ANTIC}" DESTINATION "${archive}/bin")
get_filename_component(antic_name "${ANTIC}" NAME)
archive_run(printed "${archive}/bin/${antic_name}" "${WORK}/no-home")
if(NOT printed MATCHES "archive-${name}/")
    message(FATAL_ERROR "antic in ${archive}/bin did not take the archive "
                        "above it\n${printed}")
endif()

# 3.
set(home "${WORK}/home-${name}")
file(MAKE_DIRECTORY "${home}/${data_below}/lib")
archive_run(printed "${ANTIC}" "${home}")
if(NOT printed MATCHES "home-${name}/${data_below}")
    message(FATAL_ERROR "antic did not take ${home}/${data_below} as the "
                        "runtime archive\n${printed}")
endif()

# 4.
execute_process(COMMAND "${ANTI}" fmt --check "${source}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti fmt --check did not read ${source}\n${out}${err}")
endif()
execute_process(COMMAND "${ANTI}" new app.demo
    WORKING_DIRECTORY "${WORK}/${name}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti new failed in ${WORK}/${name}\n${out}${err}")
endif()
file(COPY_FILE "${SOURCE}" "${WORK}/${name}/demo/src/${name}.anti")
execute_process(COMMAND "${ANTI}" fmt --check
    WORKING_DIRECTORY "${WORK}/${name}/demo"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti fmt --check did not read src/${name}.anti\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
