# The commands that take a VM off the network, put it back and ask
# whether it reaches the network stand in tools/release.sh, and
# docs/vm-setup.md shows each of them as a command of the Mac.
#
#   cmake -DROOT=<repository> -P tests/runners/run_release_vm_commands.cmake

cmake_minimum_required(VERSION 3.21)

# DESIGN: the commands that take a VM off the network, put it back and ask
# whether it reaches the network stand in tools/release.sh, one variable
# each, and docs/vm-setup.md shows the same six as commands of the Mac.
# The page is what Eddie runs by hand when a release stopped with a VM off
# the network, so it shows the commands of the script and no others.
file(READ "${ROOT}/tools/release.sh" release_text)
file(READ "${ROOT}/docs/vm-setup.md" vm_setup)
foreach(system linux windows)
    foreach(kind off on probe)
        if(NOT release_text MATCHES "\n${system}_network_${kind}='([^\n]*)'\n")
            message(FATAL_ERROR "tools/release.sh holds no "
                                "${system}_network_${kind}='...' on one line")
        endif()
        set(shown "ssh anti-${system} '${CMAKE_MATCH_1}'")
        string(FIND "${vm_setup}" "${shown}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "docs/vm-setup.md does not show the command of "
                                "${system}_network_${kind} of tools/release.sh:\n"
                                "${shown}")
        endif()
    endforeach()
endforeach()
