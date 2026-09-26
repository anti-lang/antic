# The tools a build of this tree runs, all from the pinned release of
# anti-lang/llvm-tools. tools/pinned-compiler.cmake includes this file,
# sets the tools with antic_pinned_link_options, and CMakeLists.txt calls
# antic_refuse_host_tools after project(). tests/run_pinned_tools.cmake
# reads a tree's cache through antic_pinned_tool_problems.
#
# DESIGN: every host builds with the pinned clang, lld, llvm-mc and llvm-ar
# alone. Eddie decided this. No compiler, linker, assembler or archiver of
# the host takes part: no system gcc or clang, no Apple ld or ar, no GNU ld
# or ar, no MSVC. A host tool differs per machine, so the same tree built
# on two hosts gave two programs, and the Linux runtime built by the host
# linked helpers of libgcc that a musl program does not have. CMake also
# finds nm, strip, objcopy, mt and rc of the host, and no rule of this
# build runs them.

# The options of the clang driver that make it link with the pinned
# linker, for a host of the operating system os (macos, linux, windows).
# The Mac and Linux name the linker with --ld-path. clang ignores
# --ld-path for an MSVC target, so Windows asks for lld and puts the
# pinned directory first with -B, which finds lld-link there.
function(antic_pinned_link_options out llvm os)
    if(os STREQUAL "macos")
        set(options "--ld-path=${llvm}/bin/ld64.lld")
    elseif(os STREQUAL "windows")
        set(options -fuse-ld=lld "-B${llvm}/bin")
    else()
        set(options "--ld-path=${llvm}/bin/ld.lld")
    endif()
    set(${out} "${options}" PARENT_SCOPE)
endfunction()

# The pinned linker itself, which CMAKE_LINKER names.
function(antic_pinned_linker out llvm os)
    if(os STREQUAL "macos")
        set(${out} "${llvm}/bin/ld64.lld" PARENT_SCOPE)
    elseif(os STREQUAL "windows")
        set(${out} "${llvm}/bin/lld-link.exe" PARENT_SCOPE)
    else()
        set(${out} "${llvm}/bin/ld.lld" PARENT_SCOPE)
    endif()
endfunction()

# Whether path lies under dir.
function(antic_path_under out path dir)
    string(FIND "${path}" "${dir}/" at)
    if(at EQUAL 0)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# The list of problems of the tools the variables of the caller name:
# CMAKE_C_COMPILER, CMAKE_AR, CMAKE_RANLIB, CMAKE_LINKER, ANTIC_LLVM_AR,
# ANTIC_LLVM_MC and the three CMAKE_<kind>_LINKER_FLAGS. It is empty when
# each is a tool of clang or llvm, the two pinned directories. CMAKE_RANLIB
# may be `:`, since llvm-ar writes the index of an archive itself.
function(antic_pinned_tool_problems out clang llvm os)
    set(problems "")
    antic_path_under(pinned "${CMAKE_C_COMPILER}" "${clang}")
    if(NOT pinned)
        list(APPEND problems
             "CMAKE_C_COMPILER is ${CMAKE_C_COMPILER}, not the clang of ${clang}")
    endif()
    foreach(name CMAKE_AR CMAKE_LINKER ANTIC_LLVM_AR ANTIC_LLVM_MC)
        antic_path_under(pinned "${${name}}" "${llvm}")
        if(NOT pinned)
            list(APPEND problems "${name} is ${${name}}, not a tool of ${llvm}")
        endif()
    endforeach()
    if(NOT CMAKE_RANLIB STREQUAL ":")
        list(APPEND problems "CMAKE_RANLIB is ${CMAKE_RANLIB}, not `:`")
    endif()
    antic_pinned_link_options(options "${llvm}" "${os}")
    foreach(kind EXE SHARED MODULE)
        separate_arguments(flags NATIVE_COMMAND
                           "${CMAKE_${kind}_LINKER_FLAGS}")
        foreach(option IN LISTS options)
            list(FIND flags "${option}" at)
            if(at EQUAL -1)
                list(APPEND problems
                     "CMAKE_${kind}_LINKER_FLAGS lacks ${option}")
            endif()
        endforeach()
    endforeach()
    set(${out} "${problems}" PARENT_SCOPE)
endfunction()

# Put the pinned tools in the scope of the caller after project(). CMake
# keeps the tools it found in the compiler information of an earlier
# configure, as plain variables that hide the cache, so a tree configured
# before this rule would go on running the host's ar and ranlib.
macro(antic_use_pinned_tools)
    if(NOT ANTIC_SYSTEM_COMPILER)
        set(CMAKE_AR "${ANTIC_LLVM_AR}")
        set(CMAKE_RANLIB ":")
        set(CMAKE_C_ARCHIVE_FINISH "")
        antic_pinned_linker(CMAKE_LINKER "${ANTIC_LLVM_DIR}" "${ANTIC_HOST_OS}")
    endif()
endmacro()

# Stop the configure step when a tool of the host would take part. A
# reader's build with ANTIC_SYSTEM_COMPILER takes the tools of the machine
# on purpose, and the release scripts refuse it.
function(antic_refuse_host_tools)
    if(ANTIC_SYSTEM_COMPILER)
        return()
    endif()
    antic_pinned_tool_problems(problems "${ANTIC_CLANG_DIR}"
                               "${ANTIC_LLVM_DIR}" "${ANTIC_HOST_OS}")
    if(problems)
        list(JOIN problems "\n  " text)
        message(FATAL_ERROR "the build would run a tool of this host, and "
                            "every tool comes from the pinned release:\n"
                            "  ${text}")
    endif()
endfunction()
