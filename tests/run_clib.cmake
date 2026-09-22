# The tests of libraries for C. Build the libraries geo and other with
# antic --lib, then compile and run C programs against them. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   LLVM_AR   the llvm-ar executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/clib
#   DUMP      tests/dump, with the headers that chapter 25 prints
#   WORK      a directory for the output
#   CASE      static, shared, exports, two, loader, header, bundle,
#             classes, failing or tuples
#   CC        the C compiler of the build, with its options
#   CXX       the same compiler for C++, which checks the headers
#   TARGET    the target of this host

# The file names of a library and a program on this host, and what the
# library driver prints as the compiler of C.
set(PREFIX lib)
set(STATIC_SUFFIX ".a")
set(HOST_SHARED_SUFFIX ".so")
set(EXE "")
set(DRIVER cc)
set(LINK "")
if(APPLE)
    set(HOST_SHARED_SUFFIX ".dylib")
elseif(CMAKE_HOST_WIN32)
    set(PREFIX "")
    set(STATIC_SUFFIX ".lib")
    set(HOST_SHARED_SUFFIX ".dll")
    set(EXE ".exe")
    set(DRIVER cl)
endif()
set(RUNTIME_LIBRARY "${PREFIX}anti_rt${STATIC_SUFFIX}")

# DESIGN: on Windows the pinned clang compiles and links each C program
# against the xwin sysroot of the runtime archive, as antic links an Anti
# program, and not against the Build Tools of the machine. The pinned
# archive ships no C++ library, so a C++17 check compiles the header with
# -x c++ and -fsyntax-only and links nothing.
if(CMAKE_HOST_WIN32)
    set(win "${RUNTIME}/sysroot/${TARGET}")
    set(arch x86_64)
    if("${TARGET}" STREQUAL "windows-arm64")
        set(arch aarch64)
    endif()
    list(APPEND CC -fms-runtime-lib=dll -nostdlibinc
         -isystem "${win}/crt/include" -isystem "${win}/sdk/include/ucrt"
         -isystem "${win}/sdk/include/um" -isystem "${win}/sdk/include/shared")
    set(CXX ${CC} -x c++)
    set(LINK -fuse-ld=lld -B "${RUNTIME}/bin" -L "${win}/crt/lib/${arch}"
             -L "${win}/sdk/lib/ucrt/${arch}" -L "${win}/sdk/lib/um/${arch}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${ARGN} failed with ${status}\n${out}${err}")
    endif()
    set(run_out "${out}" PARENT_SCOPE)
endfunction()

# Build library name as kind, static or shared, into dir. Set
# library_file to the file and library_link to what a program links: the
# library itself, or the import library of a DLL.
function(library name kind dir)
    file(MAKE_DIRECTORY "${dir}")
    if(kind STREQUAL "shared")
        set(file "${dir}/${PREFIX}${name}${HOST_SHARED_SUFFIX}")
    else()
        set(file "${dir}/${PREFIX}${name}${STATIC_SUFFIX}")
    endif()
    set(link "${file}")
    if(kind STREQUAL "shared" AND CMAKE_HOST_WIN32)
        set(link "${dir}/${name}.lib")
    endif()
    run("${ANTIC}" --lib ${kind} ${ARGN} --llvm-mc "${LLVM_MC}"
        --llvm-ar "${LLVM_AR}" --runtime "${RUNTIME}" -I "${SOURCES}"
        -o "${file}" "${SOURCES}/com/example/${name}.anti")
    set(run_out "${run_out}" PARENT_SCOPE)
    set(library_file "${file}" PARENT_SCOPE)
    set(library_link "${link}" PARENT_SCOPE)
endfunction()

# Compare the generated header with the one that chapter 25 prints.
function(expect_header header expected)
    file(READ "${header}" got)
    file(READ "${DUMP}/${expected}" wanted)
    if(NOT got STREQUAL wanted)
        message(FATAL_ERROR "${header} differs from ${DUMP}/${expected}\n${got}")
    endif()
endfunction()

# Fail unless the program of ARGN exits with 0 and prints <wanted>, byte
# for byte.
function(expect_printed wanted)
    program_output(got status "${dir}/printed.stdout" ${ARGN})
    string(HEX "${wanted}" want)
    if(NOT status EQUAL 0 OR NOT got STREQUAL want)
        file(READ "${dir}/printed.stdout" text)
        message(FATAL_ERROR "${ARGN} exited with ${status} and printed\n${text}"
                            "expected\n${wanted}")
    endif()
endfunction()

function(expect_output program expected)
    file(READ "${expected}" wanted)
    expect_printed("${wanted}" "${program}")
endfunction()

set(dir "${WORK}/${CASE}")
file(REMOVE_RECURSE "${dir}")

if(CASE STREQUAL "static")
    library(geo static "${dir}")
    # The printed line links main.c with the archive and the runtime.
    string(STRIP "${run_out}" line)
    if(NOT line MATCHES "^${DRIVER} main.c ${library_file} ${RUNTIME}/lib/[a-z0-9_-]+/[a-z0-9.]+/${RUNTIME_LIBRARY}")
        message(FATAL_ERROR "unexpected link line: ${line}")
    endif()
    # The archive keeps the copy of the package header in an object.
    run("${LLVM_AR}" t "${library_file}")
    if(NOT run_out MATCHES "geo.package.o")
        message(FATAL_ERROR "the archive holds no package header\n${run_out}")
    endif()
    string(REPLACE "${DRIVER} main.c " "" inputs "${line}")
    separate_arguments(inputs UNIX_COMMAND "${inputs}")
    run(${CC} -I "${dir}" "${SOURCES}/roundtrip.c" ${inputs} ${LINK}
        -o "${dir}/roundtrip${EXE}")
    expect_output("${dir}/roundtrip${EXE}" "${SOURCES}/roundtrip.expected")
elseif(CASE STREQUAL "classes")
    # A C program builds an Anti class, calls through its table and ends
    # it. The header compiles as C11 and as C++17.
    library(canvas static "${dir}")
    expect_header("${dir}/canvas.h" canvas.h)
    string(STRIP "${run_out}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    run(${CC} -std=c11 -I "${dir}" "${SOURCES}/canvas.c" "${library_file}"
        "${runtime_library}" ${LINK} -o "${dir}/canvas${EXE}")
    expect_output("${dir}/canvas${EXE}" "${SOURCES}/canvas.expected")
    # The copy finds ../binary_stdio.h through the directory of canvas.c.
    configure_file("${SOURCES}/canvas.c" "${dir}/canvas.cpp" COPYONLY)
    if(CMAKE_HOST_WIN32)
        run(${CXX} -std=c++17 -fsyntax-only -I "${dir}" -I "${SOURCES}"
            "${dir}/canvas.cpp")
    else()
        run(${CXX} -std=c++17 -I "${dir}" -I "${SOURCES}" "${dir}/canvas.cpp"
            "${library_file}" "${runtime_library}" -o "${dir}/canvaspp")
        expect_output("${dir}/canvaspp" "${SOURCES}/canvas.expected")
    endif()
elseif(CASE STREQUAL "failing")
    # Two `may fail` functions cross to C as `?*Error f(args, R *out)`, and
    # the header says in a comment that each may fail.
    library(failing static "${dir}")
    expect_header("${dir}/failing.h" failing.h)
    string(STRIP "${run_out}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    run(${CC} -std=c11 -Wall -Werror -I "${dir}" "${SOURCES}/failing.c"
        "${library_file}" "${runtime_library}" ${LINK}
        -o "${dir}/failing${EXE}")
    expect_output("${dir}/failing${EXE}" "${SOURCES}/failing.expected")
elseif(CASE STREQUAL "tuples")
    # A tuple of an exported signature crosses as the struct the header
    # writes for it, one per distinct tuple, and C builds one of its own.
    library(tuples static "${dir}")
    expect_header("${dir}/tuples.h" tuples.h)
    string(STRIP "${run_out}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    run(${CC} -std=c11 -Wall -Werror -I "${dir}" "${SOURCES}/tuples.c"
        "${library_file}" "${runtime_library}" ${LINK}
        -o "${dir}/tuples${EXE}")
    expect_output("${dir}/tuples${EXE}" "${SOURCES}/tuples.expected")
elseif(CASE STREQUAL "shared")
    library(geo shared "${dir}")
    run(${CC} -I "${dir}" "${SOURCES}/roundtrip.c" "${library_link}" ${LINK}
        -o "${dir}/roundtrip${EXE}")
    expect_output("${dir}/roundtrip${EXE}" "${SOURCES}/roundtrip.expected")
elseif(CASE STREQUAL "exports")
    library(geo shared "${dir}")
    if(APPLE)
        run(nm -gU "${library_file}")
    elseif(CMAKE_HOST_WIN32)
        run("${RUNTIME}/bin/llvm-readobj" --coff-exports "${library_file}")
        string(REGEX MATCHALL "Name: [A-Za-z_][A-Za-z0-9_.]*" names
               "${run_out}")
        string(REPLACE "Name: " "" run_out "${names}")
        string(REPLACE ";" "\n" run_out "${run_out}\n")
    else()
        run(nm -D --defined-only "${library_file}")
    endif()
    string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_.]*\n" symbols "${run_out}")
    set(names "")
    foreach(symbol IN LISTS symbols)
        string(STRIP "${symbol}" symbol)
        string(REGEX REPLACE "^_" "" symbol "${symbol}")
        list(APPEND names "${symbol}")
    endforeach()
    list(SORT names)
    set(wanted anti_licenses geo_apply geo_dot geo_half geo_layer geo_ready
        geo_scale)
    if(NOT names STREQUAL wanted)
        message(FATAL_ERROR "exports: ${names}\nexpected: ${wanted}")
    endif()
elseif(CASE STREQUAL "two")
    library(geo shared "${dir}/shared")
    set(geo "${library_link}")
    library(other shared "${dir}/shared")
    # Windows finds a DLL in the directory of the program.
    set(two_shared "${dir}/two_shared${EXE}")
    if(CMAKE_HOST_WIN32)
        set(two_shared "${dir}/shared/two_shared${EXE}")
    endif()
    run(${CC} -I "${dir}/shared" "${SOURCES}/twolibs.c" "${geo}"
        "${library_link}" ${LINK} -o "${two_shared}")
    expect_printed("10\n" "${two_shared}")
    library(geo static "${dir}/static")
    set(geo "${library_file}")
    library(other static "${dir}/static")
    # The printed link line names the runtime library of the host.
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${run_out}")
    run(${CC} -I "${dir}/static" "${SOURCES}/twolibs.c" "${geo}"
        "${library_file}" ${runtime_library} ${LINK}
        -o "${dir}/two_static${EXE}")
    expect_printed("10\n" "${dir}/two_static${EXE}")
elseif(CASE STREQUAL "loader")
    library(geo shared "${dir}")
    run(${CC} "${SOURCES}/loader.c" ${LINK} -o "${dir}/loader${EXE}")
    expect_printed("1\n" "${dir}/loader${EXE}" "${library_file}")
elseif(CASE STREQUAL "header")
    library(geo static "${dir}")
    expect_header("${dir}/geo.h" geo.h)
    library(shapes static "${dir}")
    expect_header("${dir}/shapes.h" shapes.h)
    run(${CC} -std=c11 -Wall -Werror -I "${dir}" -fsyntax-only
        "${SOURCES}/header.c")
    run(${CXX} -std=c++17 -Wall -Werror -I "${dir}" -fsyntax-only
        "${SOURCES}/header.cpp")
    run(${CC} -std=c11 -Wall -Werror -I "${dir}" -fsyntax-only
        "${SOURCES}/shapes.c")
    run(${CXX} -std=c++17 -Wall -Werror -I "${dir}" -fsyntax-only
        "${SOURCES}/shapes.cpp")
elseif(CASE STREQUAL "bundle")
    library(geo static "${dir}" --bundle-runtime)
    set(geo "${library_file}")
    expect_header("${dir}/geo.h" geo.bundle.h)
    string(STRIP "${run_out}" line)
    # The driver puts the system libraries of the target on the line, and
    # a Linux program links pthread and the maths library.
    set(system "")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(system " -lpthread -lm")
    endif()
    if(NOT line STREQUAL "${DRIVER} main.c ${geo}${system}")
        message(FATAL_ERROR "unexpected link line: ${line}")
    endif()
    library(other static "${dir}" --bundle-runtime)
    execute_process(
        COMMAND ${CC} -I "${dir}" "${SOURCES}/twolibs.c" "${geo}"
                "${library_file}" ${LINK} -o "${dir}/two_bundled${EXE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(status EQUAL 0 OR NOT "${out}${err}" MATCHES "duplicate symbol|multiple definition")
        message(FATAL_ERROR "two bundled runtimes linked: ${status}\n${out}${err}")
    endif()
    if(NOT "${out}${err}" MATCHES "anti_rt")
        message(FATAL_ERROR "the duplicate does not name the runtime\n${out}${err}")
    endif()
    # A library that reads the notice links against the stub of the bundle
    # and reports an empty notice.
    library(notice static "${dir}/notice" --bundle-runtime)
    run(${CC} -I "${dir}/notice" "${SOURCES}/notice.c" "${library_file}"
        ${LINK} -o "${dir}/notice/notice${EXE}")
    expect_printed("0\n" "${dir}/notice/notice${EXE}")
else()
    message(FATAL_ERROR "unknown CASE ${CASE}")
endif()
