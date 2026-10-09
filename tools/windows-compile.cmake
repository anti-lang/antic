# The compile of C for a Windows target and the link of a program of it,
# against the sysroot of mingw-w64 that tools/get-sysroot.cmake lays out.
# One definition serves the cross builds of the runtime and of the native
# libraries, the C of the tests and the packer. src/antic/linker.c spells
# the same link for the programs antic builds, and the test
# windows_sysroot reads the layout both name.
#
# DESIGN: the C of a Windows target is compiled for the gnu triple of its
# processor, x86_64-w64-windows-gnu or aarch64-w64-windows-gnu, because
# the headers of mingw-w64 refuse the msvc triple: _mingw.h defines
# __attribute__ away for a compiler without __GNUC__, and the intrinsics
# of clang fail under it. -fms-extensions keeps the pragmas of the linker
# and of the libraries, and -fno-auto-import keeps the stubs of the
# automatic import out, so that an external variable is reached as the
# msvc triple reaches it. The C carries no debug information, as before,
# so no -g option is given: -gcodeview alone, which decision 4 named,
# adds the records of every function and the build information with the
# paths of the machine, which the tests no_paths and dead_code refuse.
# The text of antic keeps the msvc triple, and both are COFF of one
# calling convention. Eddie decided the two triples on 2026-10-08 in
# decision 4 of docs/work-order-distribution.md, and the step mingw found
# the msvc triple refused.

# The triple the C of <target> is compiled for.
function(antic_windows_triple out target)
    if(target STREQUAL "windows-arm64")
        set(${out} aarch64-w64-windows-gnu PARENT_SCOPE)
    elseif(target STREQUAL "windows-x86_64")
        set(${out} x86_64-w64-windows-gnu PARENT_SCOPE)
    else()
        message(FATAL_ERROR "${target} is no Windows target")
    endif()
endfunction()

# The triple the text of antic carries for <target>, which the bitcode of
# the runtime takes as well.
function(antic_windows_msvc_triple out target)
    if(target STREQUAL "windows-arm64")
        set(${out} aarch64-pc-windows-msvc PARENT_SCOPE)
    elseif(target STREQUAL "windows-x86_64")
        set(${out} x86_64-pc-windows-msvc PARENT_SCOPE)
    else()
        message(FATAL_ERROR "${target} is no Windows target")
    endif()
endfunction()

# The options of a compile of C for a Windows target against <sysroot>,
# the directory of the target among the sysroots, after --target=. The
# headers of clang in <resource>, its resource directory, come first, as
# the driver of clang orders them for mingw-w64, and -nostdinc keeps
# every header of the host out. __USE_MINGW_ANSI_STDIO at 0 keeps the
# printf family on the one of the UCRT, which the runtime defines: a
# source that defines _GNU_SOURCE or _POSIX_C_SOURCE, as sha256.c of
# Mbed TLS does, would otherwise take the one of mingw-w64's own library,
# which no sysroot holds.
# Each -isystem is one word with its directory, since CMake drops a
# repeated word from the compile options of a target.
function(antic_windows_compile_options out sysroot resource)
    set(${out} -fms-extensions -fno-auto-import -nostdinc
        -D__USE_MINGW_ANSI_STDIO=0
        "-isystem${resource}/include" "-isystem${sysroot}/include"
        PARENT_SCOPE)
endfunction()

# The libraries every program of a Windows target links after its own
# objects, by name from lib/ of the sysroot: the compiler helpers of the
# pinned clang, ucrtbase.dll, the C library of every Windows since 10,
# ntdll.dll and kernel32.dll. The entry point, the stack probe and the
# printf family, which the static libraries of Microsoft gave, stand in
# the runtime, in src/rt/platform_windows.c.
set(ANTIC_WINDOWS_LIBRARIES clang_rt.builtins.lib ucrtbase.lib ntdll.lib
    kernel32.lib)

# The options of an lld-link command for a program of <target> against
# <sysroot>, before its output and its inputs. /lldmingw ties the unwind
# data of the gnu objects to their functions, as GNU ld does, and looks
# for no Visual Studio of the machine. /lldignoreenv keeps the library
# directories of LIB out. /NODEFAULTLIB drops the four libraries of
# Microsoft's C runtime and SDK that the runtimes of compiler-rt name in
# their directives, which no sysroot of ours holds, and lib/ of the sysroot is
# the one library directory. A directive that names another library, as
# a C object that names user32.lib does, reaches lld-link.
function(antic_windows_link_options out target sysroot)
    set(machine X64)
    if(target STREQUAL "windows-arm64")
        set(machine ARM64)
    endif()
    set(${out} /NOLOGO /lldmingw /lldignoreenv /SUBSYSTEM:CONSOLE
        "/MACHINE:${machine}" /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmt.lib
        /NODEFAULTLIB:oldnames.lib /NODEFAULTLIB:uuid.lib
        "/LIBPATH:${sysroot}/lib" PARENT_SCOPE)
endfunction()
