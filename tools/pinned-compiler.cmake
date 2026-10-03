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
include("${antic_root}/tools/pinned-tools.cmake")
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

set(antic_exe "")
set(antic_host_os linux)
if(CMAKE_HOST_WIN32)
    set(antic_exe ".exe")
    set(antic_host_os windows)
elseif(CMAKE_HOST_APPLE)
    set(antic_host_os macos)
endif()
# The assembler and the archiver of the pinned release, which the runtime,
# the native libraries and the tests run. Each has this one definition.
set(ANTIC_LLVM_MC "${ANTIC_LLVM_DIR}/bin/llvm-mc${antic_exe}" CACHE FILEPATH
    "llvm-mc of the pinned release" FORCE)
set(ANTIC_LLVM_AR "${ANTIC_LLVM_DIR}/bin/llvm-ar${antic_exe}" CACHE FILEPATH
    "llvm-ar of the pinned release" FORCE)
# opt and llc of the pinned release. The tests of the LLVM back end run the
# verifier of opt over the text antic writes, and both programs on it until
# antic runs them itself.
set(ANTIC_OPT "${ANTIC_LLVM_DIR}/bin/opt${antic_exe}" CACHE FILEPATH
    "opt of the pinned release" FORCE)
set(ANTIC_LLC "${ANTIC_LLVM_DIR}/bin/llc${antic_exe}" CACHE FILEPATH
    "llc of the pinned release" FORCE)

if(NOT ANTIC_SYSTEM_COMPILER)
    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        message(FATAL_ERROR "the Visual Studio generator takes the compiler of "
                            "its toolset. Configure with -G Ninja, or pass "
                            "-DANTIC_SYSTEM_COMPILER=ON.")
    endif()
    # The cache holds each tool, so a look at CMakeCache.txt shows which
    # ones built antic. antic_refuse_host_tools checks them after
    # project(). llvm-ar writes the index of an archive itself, so no
    # ranlib runs.
    set(CMAKE_C_COMPILER "${ANTIC_CLANG_DIR}/bin/clang${antic_exe}" CACHE
        FILEPATH "the pinned clang" FORCE)
    set(CMAKE_AR "${ANTIC_LLVM_AR}" CACHE FILEPATH "llvm-ar of the pinned release"
        FORCE)
    set(CMAKE_RANLIB ":" CACHE INTERNAL "no ranlib, since llvm-ar writes the index")
    set(CMAKE_C_ARCHIVE_FINISH "")
    antic_pinned_linker(antic_linker "${ANTIC_LLVM_DIR}" "${antic_host_os}")
    set(CMAKE_LINKER "${antic_linker}" CACHE FILEPATH
        "the linker of the pinned release" FORCE)
endif()

# The options of a link of a host program, which the build and the test
# scripts that link a C program on this host pass.
# DESIGN: every host links its host programs with the pinned lld, as
# tools/pack-anti.cmake links the programs of a release. The programs the
# suite tests and the ones a release ships then come from one linker.
# Eddie decided this, for the Mac first and then for every host. Apple's
# ld took the -lto_library that the pinned clang passes, which names a
# libLTO.dylib the release of anti-lang/llvm-tools does not carry, and it
# warned at every link. ld64.lld reads the libSystem.tbd of Apple's newest
# SDK as malformed, so a Mac takes the pinned SDK of tools/macos-sdk-pin,
# as the packer does.
set(ANTIC_HOST_LINK_OPTIONS "")
if(NOT ANTIC_SYSTEM_COMPILER)
    if(CMAKE_HOST_APPLE)
        execute_process(COMMAND "${CMAKE_COMMAND}"
                                "-DPIN=${antic_root}/tools/macos-sdk-pin"
                                -P "${antic_root}/tools/macos-sdk.cmake"
                        OUTPUT_VARIABLE antic_sdk
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_VARIABLE antic_sdk_error
                        RESULT_VARIABLE antic_sdk_failed)
        if(antic_sdk_failed)
            message(FATAL_ERROR "${antic_sdk_error}")
        endif()
        set(CMAKE_OSX_SYSROOT "${antic_sdk}" CACHE PATH "the pinned Apple SDK"
            FORCE)
    endif()
    antic_pinned_link_options(ANTIC_HOST_LINK_OPTIONS "${ANTIC_LLVM_DIR}"
                              "${antic_host_os}")
    list(JOIN ANTIC_HOST_LINK_OPTIONS " " antic_link_text)
    # The flags of the cache reach the checks of project() as well, and a
    # tree configured before keeps no other linker.
    foreach(kind EXE SHARED MODULE)
        string(REGEX REPLACE "(--ld-path=|-fuse-ld=|-B)[^ ]*" "" antic_flags
               "${CMAKE_${kind}_LINKER_FLAGS}")
        string(STRIP "${antic_link_text} ${antic_flags}" antic_flags)
        set(CMAKE_${kind}_LINKER_FLAGS "${antic_flags}" CACHE STRING
            "Flags of the linker" FORCE)
    endforeach()
endif()
