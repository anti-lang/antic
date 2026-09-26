# Pick the C compiler of a build of antic, and install the LLVM tools that
# the build and its tests take. CMakeLists.txt and src/native/CMakeLists.txt
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
include("${antic_root}/tools/deps-dir.cmake")
set(ANTIC_LLVM_DIR "${ANTIC_DEPS_DIR}/llvm" CACHE PATH
    "LLVM tools from tools/get-llvm.cmake")
set(ANTIC_CLANG_DIR "${ANTIC_DEPS_DIR}/clang" CACHE PATH
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

# The options of a link of a host program, which the build and the test
# scripts that link a C program on this host pass.
# DESIGN: a Mac links its host programs with the pinned ld64.lld, as
# tools/pack-anti.cmake links the macOS programs of a release. The
# programs the suite tests and the ones a release ships then come from
# one linker. Eddie decided this. Apple's ld took the -lto_library that
# the pinned clang passes, which names a libLTO.dylib the release of
# anti-lang/llvm-tools does not carry, and it warned at every link.
# ld64.lld reads the libSystem.tbd of Apple's newest SDK as malformed, so
# the build takes the pinned SDK of tools/macos-sdk-pin, as the packer
# does.
set(ANTIC_HOST_LINK_OPTIONS "")
if(CMAKE_HOST_APPLE AND NOT ANTIC_SYSTEM_COMPILER)
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            "-DPIN=${antic_root}/tools/macos-sdk-pin"
                            -P "${antic_root}/tools/macos-sdk.cmake"
                    OUTPUT_VARIABLE antic_sdk OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_VARIABLE antic_sdk_error
                    RESULT_VARIABLE antic_sdk_failed)
    if(antic_sdk_failed)
        message(FATAL_ERROR "${antic_sdk_error}")
    endif()
    set(CMAKE_OSX_SYSROOT "${antic_sdk}" CACHE PATH "the pinned Apple SDK"
        FORCE)
    set(ANTIC_HOST_LINK_OPTIONS "--ld-path=${ANTIC_LLVM_DIR}/bin/ld64.lld")
    # The flags of the cache reach the checks of project() as well, and a
    # tree configured before keeps no other linker.
    foreach(kind EXE SHARED MODULE)
        string(REGEX REPLACE "--ld-path=[^ ]*" "" antic_flags
               "${CMAKE_${kind}_LINKER_FLAGS}")
        string(STRIP "${ANTIC_HOST_LINK_OPTIONS} ${antic_flags}" antic_flags)
        set(CMAKE_${kind}_LINKER_FLAGS "${antic_flags}" CACHE STRING
            "Flags of the linker" FORCE)
    endforeach()
endif()
