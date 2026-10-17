# The package alone links a Linux program against glibc, from any host:
# a program of `link linux` with anti build, and a raylib program that
# links libraylib.a of the package. A Linux host runs both.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_glibc.cmake

# DESIGN: the package alone links a Linux program against glibc, from any
# host, which the step glibc of docs/work-order-distribution.md built. A
# program of `link linux`, tests/anti-build/window, builds with anti build.
# A raylib program imports the binding that the packed anti bind writes,
# compiled into a library file, and links libraylib.a of the package with
# the Linux libraries the binding names. Both name libX11 and libGL of the
# glibc sysroot. A Linux host runs the two programs of its own target. No
# display serves them, so the raylib program asks for a mouse button, which
# reads the state of rcore and opens no window.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

file(COPY "${ROOT}/tests/anti-build/window" DESTINATION "${WORK}")
set(game_dir "${WORK}/game")
file(MAKE_DIRECTORY "${game_dir}/lib/game")
run_alone("anti bind --module game.raylib" "${WORK}"
          "${package_bin}/anti${suffix}" bind "${ROOT}/tests/bind/raylib_api.json" --module game.raylib
          -o "${game_dir}/src/game")
run_alone("antic -c of the raylib binding" "${game_dir}"
          "${package_bin}/antic${suffix}" -c -I "${game_dir}/src" -o "${game_dir}/lib/game/raylib.antl"
          "${game_dir}/src/game/raylib.anti")
file(WRITE "${game_dir}/game.anti"
     "import anti.io;\nimport game.raylib;\n\nfn main() -> int\n{\n"
     "    if !raylib.IsMouseButtonPressed(0) {\n"
     "        io.println(\"no click\");\n    }\n    return 0;\n}\n")
file(STRINGS "${game_dir}/src/game/raylib.anti" raylib_linux
     REGEX "^link linux \"[^\"]+\";$")
set(linux_libs)
foreach(line IN LISTS raylib_linux)
    string(REGEX REPLACE "^link linux \"([^\"]+)\";$" "\\1" name "${line}")
    list(APPEND linux_libs --linux-lib "${name}")
endforeach()
if(NOT linux_libs MATCHES "X11" OR NOT linux_libs MATCHES "GL")
    message(FATAL_ERROR "the binding of raylib names no X11 and GL: "
                        "${raylib_linux}")
endif()
foreach(target linux-x86_64 linux-arm64)
    run_alone("anti build --target ${target} of a program of link linux"
              "${WORK}/window" "${package_bin}/anti${suffix}" build
              --target "${target}")
    set(window "${WORK}/window/dist/${target}/dev/window")
    set(game "${WORK}/game-${target}")
    run_alone("a raylib program for ${target}" "${game_dir}"
              "${package_bin}/antic${suffix}" --target "${target}"
              -I "${game_dir}/lib" ${linux_libs} -o "${game}"
              "${game_dir}/game.anti"
              "${package_dir}/lib/${target}/libraylib.a")
    foreach(exe "${window}" "${game}")
        run_alone("llvm-readobj of ${exe}" "${WORK}"
                  "${package_bin}/llvm-readobj${suffix}" --needed-libs
                  --program-headers "${exe}")
        foreach(needed PT_INTERP libX11\\.so\\.6 libGL\\.so\\.1 libc\\.so\\.6)
            if(NOT alone_out MATCHES "${needed}")
                message(FATAL_ERROR "${exe} of the package lacks ${needed}\n"
                                    "${alone_out}")
            endif()
        endforeach()
    endforeach()
    if(target STREQUAL HOST)
        run_alone("the program of link linux" "${WORK}" "${window}")
        if(NOT alone_out STREQUAL "no display\nno context\n")
            message(FATAL_ERROR "the program of link linux of the package "
                                "prints '${alone_out}'")
        endif()
        run_alone("the raylib program" "${WORK}" "${game}")
        if(NOT alone_out MATCHES "no click\n$")
            message(FATAL_ERROR "the raylib program of the package prints "
                                "'${alone_out}'")
        endif()
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
