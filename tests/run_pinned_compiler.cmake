# Every build of antic that is not a reader's compiles with the clang of the
# pinned release. The cache of the build names the compiler, and no test
# script calls a C compiler of the machine by name.
#
#   cmake -DROOT=<repository> -DCACHE=<CMakeCache.txt> -P tests/run_pinned_compiler.cmake

# Set <out> to the value of <key> in the cache.
function(cached key out)
    file(STRINGS "${CACHE}" line REGEX "^${key}:[A-Z]+=")
    string(REGEX REPLACE "^${key}:[A-Z]+=" "" value "${line}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

cached(ANTIC_SYSTEM_COMPILER system)
cached(ANTIC_CLANG_DIR clang_dir)
cached(CMAKE_C_COMPILER compiler)
if(NOT system)
    set(pinned "${clang_dir}/bin/clang")
    if(CMAKE_HOST_WIN32)
        set(pinned "${pinned}.exe")
    endif()
    if(NOT compiler STREQUAL pinned)
        message(FATAL_ERROR "CMAKE_C_COMPILER is ${compiler}, and the pinned "
                            "clang is ${pinned}")
    endif()
    file(READ "${ROOT}/tools/llvm-version" version)
    string(STRIP "${version}" version)
    execute_process(COMMAND "${compiler}" --version OUTPUT_VARIABLE out
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0 OR NOT out MATCHES "clang version ${version}")
        message(FATAL_ERROR "${compiler} is not clang ${version}: ${out}")
    endif()
    message(STATUS "CMAKE_C_COMPILER is the pinned clang ${version}")
endif()

# A test compiles C and C++ with the compiler that the build hands it.
file(GLOB scripts "${ROOT}/tests/*.cmake")
foreach(script IN LISTS scripts)
    file(STRINGS "${script}" lines
         REGEX "(COMMAND|run\\()[ \t]+(cc|c\\+\\+|gcc|g\\+\\+|clang|clang\\+\\+)[ \t)]")
    if(lines)
        get_filename_component(name "${script}" NAME)
        message(FATAL_ERROR "tests/${name} calls a compiler of the machine: ${lines}")
    endif()
endforeach()
