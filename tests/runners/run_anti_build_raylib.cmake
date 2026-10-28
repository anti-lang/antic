# `anti build` of a project that imports anti.raylib, in dev mode and in
# release mode. The manifest holds the lines of [package] alone, with no
# dependency and no option of a link, and the project holds no binding of
# its own: the module of the runtime archive and its libraries are found
# without them. The program of each mode is tests/std/raylib.anti, which
# runs where no display is, and prints what tests/std/raylib.expected
# holds. Run with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   SOURCE   tests/std/raylib.anti
#   EXPECTED tests/std/raylib.expected
#   HOST     the target name of this host
#   WORK     a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
set(project "${WORK}/game")
file(MAKE_DIRECTORY "${project}/src/com/example")
write_bytes("${project}/anti.toml"
            "[package]\nname = \"com.example.game\"\nversion = \"0.1.0\"\n")
file(COPY_FILE "${SOURCE}" "${project}/src/com/example/game.anti")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()

foreach(mode dev release)
    set(flag "")
    if(mode STREQUAL "release")
        set(flag --release)
    endif()
    anti_run(status out err "${project}" build ${flag} --runtime "${RUNTIME}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build ${flag} failed with ${status}\n"
                            "${out}${err}")
    endif()
    program_expect("game ${mode}"
                   COMMAND "${project}/dist/${HOST}/${mode}/game${exe}"
                   EXPECTED "${EXPECTED}")
endforeach()
