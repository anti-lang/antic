# Without --runtime, antic takes the runtime archive of the user's data
# directory when it does not sit in the bin/ of an archive itself. An
# install puts the two executables on the PATH and the archive under
# ~/.local/share/anti or %LOCALAPPDATA%\anti, so the directory above antic
# holds no lib/ any more.
#
#   cmake -DANTIC=<antic> -DSOURCE=<file.anti> -DWORK=<dir>
#         -P tests/run_runtime_from_data_dir.cmake

file(REMOVE_RECURSE "${WORK}")
if(WIN32)
    set(data "${WORK}/home/anti")
    set(env "LOCALAPPDATA=${WORK}/home")
else()
    set(data "${WORK}/home/.local/share/anti")
    set(env "HOME=${WORK}/home")
endif()
# The lookup asks for lib/, which is what tells an archive from a build
# tree. Nothing else of the archive is there, so the compile stops at the
# first file it wants and names the directory it looked in.
file(MAKE_DIRECTORY "${data}/lib")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "${env}"
            --unset=XDG_DATA_HOME --unset=ANTI_HOME
            "${ANTIC}" -o "${WORK}/prog" "${SOURCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
# antic joins the data directory of Windows with backslashes and the
# rest of a path with slashes, so both sides are read with one separator.
string(REPLACE "\\" "/" printed "${out}${err}")
string(REPLACE "\\" "/" want "${data}")
if(NOT printed MATCHES "${want}")
    message(FATAL_ERROR "antic did not take ${data} as the runtime archive\n"
                        "${printed}")
endif()

# The directory above antic holds no lib/, so nothing else could have
# answered. A run with neither an archive nor a data directory says what
# it needs.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "${env}-empty"
            --unset=XDG_DATA_HOME --unset=ANTI_HOME
            "${ANTIC}" -o "${WORK}/prog" "${SOURCE}"
    RESULT_VARIABLE refused OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "--runtime")
    message(FATAL_ERROR "antic without an archive does not ask for --runtime\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
