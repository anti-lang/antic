cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

# A Windows program of `anti build --memory-checks` loads the DLL of
# AddressSanitizer from its own directory, so the build puts the DLL into
# dist/ beside the program, in dev and release mode. `anti run` started
# the program from dist/ without it on a runner of windows-x86_64, and
# Windows ended it with 0xC0000135. The build is a cross build, so every
# host whose runtime archive holds the sysroot runs it. Run with cmake -P
# and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   FIXTURE   the directory that holds app/
#   WORK      a directory this run writes into

set(target windows-x86_64)
if(NOT EXISTS "${RUNTIME}/sysroot/${target}")
    message("SKIP: the runtime archive has no sysroot for ${target}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" DESTINATION "${WORK}")
foreach(mode dev release)
    set(release "")
    if(mode STREQUAL "release")
        set(release --release)
    endif()
    anti_run(status out err "${WORK}/app" build ${release} --memory-checks
             --target ${target} --runtime "${RUNTIME}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build of ${mode} failed with ${status}\n${err}")
    endif()
    set(dist "${WORK}/app/dist/${target}/${mode}")
    if(NOT EXISTS "${dist}/app.exe")
        message(FATAL_ERROR "the build of ${mode} wrote no ${dist}/app.exe")
    endif()
    if(NOT EXISTS "${dist}/clang_rt.asan_dynamic.dll")
        message(FATAL_ERROR "${dist} holds the program without the DLL of "
                            "AddressSanitizer")
    endif()
endforeach()
