# The installers and the uninstallers of the two shells take the same
# options, because a reader of one page uses either. A spelling that only
# one of them knows fails the other with no message worth reading.
#
#   cmake -DROOT=<repository> -P tests/run_installer_options.cmake

set(scripts install.sh install.ps1 uninstall.sh uninstall.ps1)
# The processor of the package, which defaults to the processor of this
# machine. Each spelling stands in every script.
set(options --arm --arm64 --intel --x86_64 ANTI_ARCH)

foreach(script IN LISTS scripts)
    file(READ "${ROOT}/tools/${script}" text)
    foreach(option IN LISTS options)
        string(FIND "${text}" "${option}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "tools/${script} does not know `${option}`, "
                                "which the other installers take")
        endif()
    endforeach()
endforeach()

# A package of another processor installs beside the one of this machine
# rather than over it, so both directories stay usable.
foreach(script install.sh uninstall.sh install.ps1 uninstall.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    string(FIND "${text}" "anti-$" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "tools/${script} writes no directory of its own "
                            "for a package of another processor")
    endif()
endforeach()

# DESIGN: an install of Anti follows the conventions of the platform, and
# the four roots are spelled in five places: the two installers, the two
# uninstallers, src/antic/userdirs.c for antic, src/std/anti/os.anti for a program
# and docs/decisions.md for a reader. A script necessarily repeats what
# the binary uses, so the seam is pinned here rather than trusted. A
# spelling that moves in one place fails this test.
#
# The four roots stand in src/antic/userdirs.c, which antic reads them from,
# and in docs/decisions.md, which a reader does.
foreach(root XDG_BIN_HOME .local/bin XDG_DATA_HOME .local/share
        XDG_CONFIG_HOME .config XDG_CACHE_HOME .cache)
    foreach(file src/antic/userdirs.c docs/decisions.md)
        file(READ "${ROOT}/${file}" text)
        string(FIND "${text}" "${root}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${file} does not name ${root}, which is a "
                                "root of an install on Linux and macOS")
        endif()
    endforeach()
endforeach()
# anti.os gives a program the three directories of its own files. The bin
# directory is of an install and no program asks for it.
foreach(root XDG_DATA_HOME .local/share XDG_CONFIG_HOME .config
        XDG_CACHE_HOME .cache)
    file(READ "${ROOT}/src/std/anti/os.anti" text)
    string(FIND "${text}" "${root}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "src/std/anti/os.anti does not name ${root}, which is "
                            "a root it gives to a program")
    endif()
endforeach()
# The two installers of the shell write the bin and the data root, and
# read the XDG variables of both.
foreach(script install.sh uninstall.sh)
    file(READ "${ROOT}/tools/${script}" text)
    foreach(root XDG_BIN_HOME .local/bin XDG_DATA_HOME .local/share)
        string(FIND "${text}" "${root}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "tools/${script} does not name ${root}")
        endif()
    endforeach()
endforeach()
# Windows keeps everything under LOCALAPPDATA, with the executables in
# Programs\anti\bin.
foreach(file tools/install.ps1 tools/uninstall.ps1 src/antic/userdirs.c
        src/std/anti/os.anti docs/decisions.md)
    file(READ "${ROOT}/${file}" text)
    string(FIND "${text}" "LOCALAPPDATA" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${file} does not name LOCALAPPDATA, which is the "
                            "root of an install on Windows")
    endif()
endforeach()
foreach(file tools/install.ps1 tools/uninstall.ps1 src/antic/userdirs.c
        docs/decisions.md)
    file(READ "${ROOT}/${file}" text)
    string(FIND "${text}" "Programs" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${file} does not put the executables of a "
                            "Windows install under Programs")
    endif()
endforeach()

# DESIGN: an uninstaller removes a tree only when the installer's marker
# stands in it. The four scripts spell the same file name.
foreach(script install.sh uninstall.sh install.ps1 uninstall.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    string(FIND "${text}" ".anti-install" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "tools/${script} does not know the marker "
                            ".anti-install, which is what an uninstaller "
                            "checks before it removes a directory")
    endif()
endforeach()

# A package that carries Zig's stubs links for macOS with no SDK, so
# install.sh asks for the Command Line Tools only for a package without
# them.
file(READ "${ROOT}/tools/install.sh" text)
string(FIND "${text}" "/sysroot/macos-$arch/usr/lib/libSystem.tbd" found)
if(found EQUAL -1)
    message(FATAL_ERROR "tools/install.sh takes the SDK stubs of a Mac even "
                        "for a package that carries Zig's")
endif()
