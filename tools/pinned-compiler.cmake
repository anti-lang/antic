# Pick the C compiler of a build of antic, and install the LLVM tools that
# the build and its tests take. CMakeLists.txt and libs/CMakeLists.txt
# include this file before their project() command.
#
# DESIGN: every build of antic that is not a reader's compiles with the
# clang of the pinned release from anti-lang/llvm-tools, and so does every
# binary Anti ships. The configure step downloads it with
# tools/get-clang.cmake. ANTIC_SYSTEM_COMPILER=ON takes the compiler of the
# machine instead, for a reader who only builds antic from source. The
# release scripts refuse a build made that way.
option(ANTIC_SYSTEM_COMPILER
       "Build with the C compiler of this machine instead of the pinned clang"
       OFF)
get_filename_component(antic_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(ANTIC_LLVM_DIR "${antic_root}/build/llvm" CACHE PATH
    "LLVM tools from tools/get-llvm.cmake")
set(ANTIC_CLANG_DIR "${antic_root}/build/clang" CACHE PATH
    "the pinned clang from tools/get-clang.cmake")

set(antic_fetch llvm)
if(NOT ANTIC_SYSTEM_COMPILER)
    list(APPEND antic_fetch clang)
endif()
foreach(name IN LISTS antic_fetch)
    string(TOUPPER "${name}" upper)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_${upper}_DIR}"
                            -P "${antic_root}/tools/get-${name}.cmake"
                    RESULT_VARIABLE fetched)
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR "tools/get-${name}.cmake failed, so the build has "
                            "no pinned ${name}")
    endif()
endforeach()

if(NOT ANTIC_SYSTEM_COMPILER)
    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        message(FATAL_ERROR "the Visual Studio generator takes the compiler of "
                            "its toolset. Configure with -G Ninja, or pass "
                            "-DANTIC_SYSTEM_COMPILER=ON.")
    endif()
    set(antic_exe "")
    if(CMAKE_HOST_WIN32)
        set(antic_exe ".exe")
    endif()
    # The cache holds the compiler, so a look at CMakeCache.txt shows which
    # one built antic.
    set(CMAKE_C_COMPILER "${ANTIC_CLANG_DIR}/bin/clang${antic_exe}" CACHE
        FILEPATH "the pinned clang" FORCE)
endif()
