# The tests of libraries for C. Build the libraries geo and other with
# antic --lib, then compile and run C programs against them. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_AR   the llvm-ar executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/clib
#   DUMP      tests/dump, with the headers antic must write
#   WORK      a directory for the output
#   CASE      static, shared, exports, two, loader, header, bundle, twice,
#             simd, classes, failing, tuples, flags, ledger, variants,
#             nested, names, handlers, generics, optional, forms, views,
#             never, never_cpp, ctypes, ctypes_shared, ctypes_bundle,
#             tight or tight_aligned
#   CC        the C compiler of the build, with its options
#   HOST_LINK the options of a link of a program of this host
#   CXX       the same compiler for C++, which checks the headers
#   TARGET    the target of this host, or with CROSS the Windows target
#   LLVM_OBJDUMP  llvm-objdump, which lists the symbols of the runtime
#   CROSS     ON for a target of another host. The case builds and links
#             the programs and runs none, and only bundle and twice have
#             one.
#   SYSROOT   ON for a macOS target whose C programs compile and link
#             against the sysroot of the runtime archive, on every host
#   OPTIONS   further options of antic, with | between them

# The file names of a library and a program on this host, and what the
# library driver prints as the compiler of C.

cmake_minimum_required(VERSION 3.21)

set(PREFIX lib)
set(STATIC_SUFFIX ".a")
set(HOST_SHARED_SUFFIX ".so")
set(EXE "")
set(DRIVER cc)
set(LINK ${HOST_LINK})
set(TARGET_OPTION "")
if("${TARGET}" MATCHES "^windows-")
    set(PREFIX "")
    set(STATIC_SUFFIX ".lib")
    set(HOST_SHARED_SUFFIX ".dll")
    set(EXE ".exe")
    # DRIVER is the word the link line of link_line in src/antic/linker.c
    # starts with, which the cases compare. The C programs of the cases
    # compile with the pinned clang, as the DESIGN below says.
    set(DRIVER cl)
elseif(APPLE)
    set(HOST_SHARED_SUFFIX ".dylib")
endif()
if(CROSS OR SYSROOT)
    if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
        message("SKIP: the runtime archive has no sysroot for ${TARGET}")
        return()
    endif()
    set(TARGET_OPTION --target "${TARGET}")
endif()
string(REPLACE "|" ";" options "${OPTIONS}")
list(APPEND TARGET_OPTION ${options})
set(RUNTIME_LIBRARY "${PREFIX}anti_rt${STATIC_SUFFIX}")

# DESIGN: on Windows the pinned clang compiles each C program against the
# mingw-w64 sysroot of the runtime archive, with the triple and the flags
# of tools/target-compile.cmake, and lld-link links it against the
# sysroot as antic links an Anti program, never against the Build Tools
# of the machine. The pinned archive ships no C++ library, so a C++17
# check compiles the header with -x c++ and -fsyntax-only and links
# nothing.
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/target-compile.cmake")
if("${TARGET}" MATCHES "^windows-")
    set(win "${RUNTIME}/sysroot/${TARGET}")
    list(GET CC 0 compiler)
    execute_process(COMMAND "${compiler}" -print-resource-dir
                    OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE
                    ENCODING NONE)
    antic_target_triple(triple "${TARGET}")
    antic_target_c_options(windows_options "${TARGET}" "${win}" "${resource}")
    list(APPEND CC "--target=${triple}" ${windows_options})
    set(CXX ${CC} -x c++)
    set(LLD_LINK "${RUNTIME}/bin/lld-link")
    if(CMAKE_HOST_WIN32)
        set(LLD_LINK "${RUNTIME}/bin/lld-link.exe")
    endif()
    antic_windows_link_options(WINDOWS_LINK "${TARGET}" "${win}")
endif()

# DESIGN: with SYSROOT the pinned clang compiles each C program of a macOS
# target against the sysroot of the runtime archive, and ld64.lld of the
# archive links it against the stubs of that sysroot, as antic links an
# Anti program. Every host then builds the programs of both macOS targets,
# and no driver of C stands between the test and what ld64.lld reports.
# The triple and the versions are the ones of the cross build of the
# runtime and of macos_start in src/antic/linker.c.
set(MACOS_LINK "")
if(SYSROOT AND "${TARGET}" MATCHES "^macos-(.+)$")
    set(arch "${CMAKE_MATCH_1}")
    set(macos "${RUNTIME}/sysroot/${TARGET}")
    file(STRINGS "${macos}/sdk-version" sdk_version LIMIT_COUNT 1)
    list(GET CC 0 compiler)
    antic_target_triple(triple "${TARGET}")
    set(CC "${compiler}" "--target=${triple}" -isysroot "${macos}")
    set(CXX ${CC} -x c++)
    set(LD64 "${RUNTIME}/bin/ld64.lld")
    if(CMAKE_HOST_WIN32)
        set(LD64 "${RUNTIME}/bin/ld64.lld.exe")
    endif()
    set(MACOS_LINK -arch "${arch}" -platform_version macos 11.0
        "${sdk_version}" -syslibroot "${macos}" -lSystem)
endif()

# Every C and C++ file of the tests compiles under the warnings of the
# repository, and a warning is an error.
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/warnings.cmake")
list(APPEND CXX ${ANTIC_CXX_WARNINGS})
list(APPEND CC ${ANTIC_C_WARNINGS})

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

# Compile the C sources of SOURCES with the options of OPTIONS, and link
# them with the inputs of INPUTS into output. On Windows the pinned clang
# compiles each source for the target and lld-link links the objects
# against the sysroot, with the libraries every Windows program links.
# With SYSROOT the same holds for a macOS target and ld64.lld.
# A program whose inputs name no runtime of Anti takes the one of the
# lowest level of the target, for the start of a program it holds.
# A library of macOS has the install name @rpath/<file name>, by R151 of
# "Rules of 2026-10-13", so the program takes the directory of each as an
# rpath.
function(program output)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "OPTIONS;SOURCES;INPUTS")
    set(objects "")
    set(rpaths "")
    set(cc_rpaths "")
    foreach(input IN LISTS arg_INPUTS)
        if(input MATCHES "\\.dylib$")
            get_filename_component(where "${input}" DIRECTORY)
            list(APPEND rpaths -rpath "${where}")
            list(APPEND cc_rpaths "-Wl,-rpath,${where}")
        endif()
    endforeach()
    if("${TARGET}" MATCHES "^windows-" OR MACOS_LINK)
        foreach(source IN LISTS arg_SOURCES)
            get_filename_component(name "${source}" NAME_WE)
            set(object "${output}-${name}.o")
            tool_run("" ${CC} ${arg_OPTIONS} -c "${source}" -o "${object}")
            list(APPEND objects "${object}")
        endforeach()
    endif()
    if("${TARGET}" MATCHES "^windows-")
        set(inputs ${arg_INPUTS})
        if(NOT "${inputs}" MATCHES "anti_rt\\.lib")
            file(GLOB runtimes "${RUNTIME}/lib/${TARGET}/*/anti_rt.lib")
            list(SORT runtimes)
            list(GET runtimes 0 lowest)
            list(APPEND inputs "${lowest}")
        endif()
        tool_run("" "${LLD_LINK}" ${WINDOWS_LINK} "/OUT:${output}" ${objects}
                    ${inputs} ${ANTIC_WINDOWS_LIBRARIES})
    elseif(MACOS_LINK)
        tool_run("" "${LD64}" ${MACOS_LINK} ${objects} ${arg_INPUTS} ${rpaths}
                    -o "${output}")
    else()
        tool_run("" ${CC} ${arg_OPTIONS} ${arg_SOURCES} ${arg_INPUTS} ${cc_rpaths}
                    ${LINK} -o "${output}")
    endif()
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
    antic_build(OUTPUT tool_stdout COMMAND --lib ${kind} ${ARGN} ${TARGET_OPTION}
                --llvm-ar "${LLVM_AR}" --runtime
                "${RUNTIME}" -I "${SOURCES}" -o "${file}"
                "${SOURCES}/com/example/${name}.anti")
    set(tool_stdout "${tool_stdout}" PARENT_SCOPE)
    set(library_file "${file}" PARENT_SCOPE)
    set(library_link "${link}" PARENT_SCOPE)
endfunction()

# Compare the generated header with the one of tests/dump.
function(expect_header header expected)
    file(READ "${header}" got)
    file(READ "${DUMP}/${expected}" wanted)
    expect_same_text("${header}, against ${DUMP}/${expected}," "${wanted}"
                     "${got}")
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
    string(STRIP "${tool_stdout}" line)
    if(NOT line MATCHES "^${DRIVER} main.c ${library_file} ${RUNTIME}/lib/[a-z0-9_-]+/[a-z0-9.]+/${RUNTIME_LIBRARY}")
        message(FATAL_ERROR "unexpected link line: ${line}")
    endif()
    # The archive keeps the copy of the package header in an object.
    tool_run("" "${LLVM_AR}" t "${library_file}")
    if(NOT tool_stdout MATCHES "geo.package.o")
        message(FATAL_ERROR "the archive holds no package header\n${tool_stdout}")
    endif()
    string(REPLACE "${DRIVER} main.c " "" inputs "${line}")
    separate_arguments(inputs UNIX_COMMAND "${inputs}")
    program("${dir}/roundtrip${EXE}" OPTIONS -I "${dir}"
            SOURCES "${SOURCES}/roundtrip.c" INPUTS ${inputs})
    expect_output("${dir}/roundtrip${EXE}" "${SOURCES}/roundtrip.expected")
elseif(CASE STREQUAL "classes")
    # A C program builds an Anti class, calls through its table and ends
    # it. The header compiles as C11 and as C++17.
    library(canvas static "${dir}")
    expect_header("${dir}/canvas.h" canvas.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/canvas${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/canvas.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/canvas${EXE}" "${SOURCES}/canvas.expected")
    # The copy finds ../binary_stdio.h through the directory of canvas.c.
    configure_file("${SOURCES}/canvas.c" "${dir}/canvas.cpp" COPYONLY)
    if(CMAKE_HOST_WIN32)
        tool_run("" ${CXX} -std=c++17 -fsyntax-only -I "${dir}" -I "${SOURCES}"
                    "${dir}/canvas.cpp")
    else()
        tool_run("" ${CXX} -std=c++17 -I "${dir}" -I "${SOURCES}" "${dir}/canvas.cpp"
                    "${library_file}" "${runtime_library}" ${LINK} -o "${dir}/canvaspp")
        expect_output("${dir}/canvaspp" "${SOURCES}/canvas.expected")
    endif()
elseif(CASE STREQUAL "failing")
    # Two `may fail` functions cross to C as `?*Error f(args, R *out)`, and
    # the header says in a comment that each may fail.
    library(failing static "${dir}")
    expect_header("${dir}/failing.h" failing.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/failing${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/failing.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/failing${EXE}" "${SOURCES}/failing.expected")
elseif(CASE STREQUAL "generics")
    # The copy of a generic that `export type` names crosses to C as the
    # export class of that name, with its table, its init and one symbol
    # per public function. The header compiles as C11 and as C++17.
    library(stacks static "${dir}")
    expect_header("${dir}/stacks.h" stacks.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/stacks${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/stacks.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/stacks${EXE}" "${SOURCES}/stacks.expected")
    configure_file("${SOURCES}/stacks.c" "${dir}/stacks.cpp" COPYONLY)
    tool_run("" ${CXX} -std=c++17 -fsyntax-only -I "${dir}" -I "${SOURCES}"
                "${dir}/stacks.cpp")
elseif(CASE STREQUAL "tuples")
    # A tuple of an exported signature crosses as the struct the header
    # writes for it, one per distinct tuple, and C builds one of its own.
    library(tuples static "${dir}")
    expect_header("${dir}/tuples.h" tuples.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/tuples${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/tuples.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/tuples${EXE}" "${SOURCES}/tuples.expected")
elseif(CASE STREQUAL "optional")
    # A `?T` of an exported signature crosses as the struct of the value
    # and a bool that the header writes for it, one per T.
    library(optional static "${dir}")
    expect_header("${dir}/optional.h" optional.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/optional${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/optional.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/optional${EXE}" "${SOURCES}/optional.expected")
elseif(CASE STREQUAL "simd")
    # A simd struct of 16 bytes crosses as the vector type of C, which
    # the header writes per architecture, and one of another size as the
    # struct of its lanes.
    library(simdlib static "${dir}")
    expect_header("${dir}/simdlib.h" simdlib.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/simd${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/simd.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/simd${EXE}" "${SOURCES}/simd.expected")
elseif(CASE STREQUAL "forms")
    # The forms of a header that no other library holds: a constant of
    # bool, a text with a control character, a vector type per kind of
    # lane, a union, a tuple with pointers and an array, and a parameter
    # that is a function with its context. The header compiles as C++17
    # as well.
    library(forms static "${dir}")
    expect_header("${dir}/forms.h" forms.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/forms${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/forms.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/forms${EXE}" "${SOURCES}/forms.expected")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only "${SOURCES}/forms.cpp")
elseif(CASE STREQUAL "views")
    # More forms of a header that no other library holds: an enum, an
    # optional result, types that name themselves, a field named as a
    # macro of <stdint.h>, vectors of 8 and of 32 bytes, a lent parameter,
    # the error in a signature, an interface of a base class, a protected
    # field, a packed type of a class and a synchronized class with a
    # construct and a destruct, which run without the lock note. The
    # header compiles as C++17 as well.
    library(views static "${dir}")
    expect_header("${dir}/views.h" views.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/views${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/views.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/views${EXE}" "${SOURCES}/views.expected")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only "${SOURCES}/views.cpp")
elseif(CASE STREQUAL "never" OR CASE STREQUAL "never_cpp")
    # An export function that never returns. The header declares it so,
    # and a C program ends inside the call. never_cpp compiles the header
    # as C++17.
    library(never static "${dir}")
    expect_header("${dir}/never.h" never.h)
    if(CASE STREQUAL "never")
        string(STRIP "${tool_stdout}" line)
        string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library
               "${line}")
        program("${dir}/never${EXE}" OPTIONS -std=c11 -I "${dir}"
                SOURCES "${SOURCES}/never.c"
                INPUTS "${library_file}" "${runtime_library}")
        expect_output("${dir}/never${EXE}" "${SOURCES}/never.expected")
    else()
        tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only
                    "${SOURCES}/never.cpp")
    endif()
elseif(CASE STREQUAL "flags")
    # Flags crosses as the struct of four bools the header writes for it,
    # passed and returned by value and held as a field.
    library(flags static "${dir}")
    expect_header("${dir}/flags.h" flags.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/flags${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/flags.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/flags${EXE}" "${SOURCES}/flags.expected")
elseif(CASE STREQUAL "tight")
    # The struct the header writes for a packed class has the size Anti
    # gives the class.
    library(tight static "${dir}")
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/tight${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/tight.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/tight${EXE}" "${SOURCES}/tight.expected")
elseif(CASE STREQUAL "tight_aligned")
    # The struct the header writes for a packed struct with align(16) has
    # the size and the alignment Anti gives the struct.
    library(tight static "${dir}")
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/tight_aligned${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/tight_aligned.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/tight_aligned${EXE}"
                  "${SOURCES}/tight_aligned.expected")
elseif(CASE STREQUAL "ledger")
    # A synchronized class crosses with the bytes of its hidden lock, and
    # two threads of C call it without losing an addition.
    library(ledger static "${dir}")
    expect_header("${dir}/ledger.h" ledger.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/ledger${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/ledger.c"
            INPUTS "${library_file}" "${runtime_library}" -lpthread)
    expect_output("${dir}/ledger${EXE}" "${SOURCES}/ledger.expected")
elseif(CASE STREQUAL "handlers")
    # An `own fn` field crosses as the struct of its code and its
    # snapshot. C calls the code with the snapshot, and the delete of the
    # object frees the snapshot.
    library(handlers static "${dir}")
    expect_header("${dir}/handlers.h" handlers.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/handlers${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/handlers.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/handlers${EXE}" "${SOURCES}/handlers.expected")
elseif(CASE STREQUAL "names")
    # The header gives two fields whose names share a long prefix one C
    # name each, writes a whole float constant as a float of its type, and
    # declares every entry of a table of more than 64 public functions.
    library(names static "${dir}")
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/names${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/names.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/names${EXE}" "${SOURCES}/names.expected")
elseif(CASE STREQUAL "variants")
    # An export variant crosses as the enum of its tags and the struct of
    # its tag and the union of its cases. C sets and reads both, by value
    # as a parameter, a result and a field. The header compiles as C++17
    # as well.
    library(variants static "${dir}")
    expect_header("${dir}/variants.h" variants.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/variants${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/variants.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/variants${EXE}" "${SOURCES}/variants.expected")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only
                "${SOURCES}/variants.cpp")
elseif(CASE STREQUAL "nested")
    # An export class whose layout holds types nested in it. The header
    # writes each one the layout reaches before the class, under the name
    # `PeopleList_Node`, and C reads the fields of each. The header
    # compiles as C++17 as well.
    library(nested static "${dir}")
    expect_header("${dir}/nested.h" nested.h)
    string(STRIP "${tool_stdout}" line)
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
    program("${dir}/nested${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/nested.c"
            INPUTS "${library_file}" "${runtime_library}")
    expect_output("${dir}/nested${EXE}" "${SOURCES}/nested.expected")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only
                "${SOURCES}/nested.cpp")
elseif(CASE MATCHES "^ctypes")
    # Every C type Anti names, in a library built three ways: as an
    # archive beside the runtime, as a shared library and as an archive
    # that bundles the runtime. One C program calls each build and prints
    # the same lines.
    if(CASE STREQUAL "ctypes_shared")
        library(ctypes shared "${dir}")
        set(inputs "${library_link}")
    elseif(CASE STREQUAL "ctypes_bundle")
        library(ctypes static "${dir}" --bundle-runtime)
        set(inputs "${library_file}")
    else()
        library(ctypes static "${dir}")
        string(STRIP "${tool_stdout}" line)
        string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${line}")
        set(inputs "${library_file}" "${runtime_library}")
    endif()
    program("${dir}/ctypes${EXE}" OPTIONS -std=c11 -I "${dir}"
            SOURCES "${SOURCES}/ctypes.c" INPUTS ${inputs})
    expect_output("${dir}/ctypes${EXE}" "${SOURCES}/ctypes.expected")
elseif(CASE STREQUAL "shared")
    library(geo shared "${dir}")
    program("${dir}/roundtrip${EXE}" OPTIONS -I "${dir}"
            SOURCES "${SOURCES}/roundtrip.c" INPUTS "${library_link}")
    expect_output("${dir}/roundtrip${EXE}" "${SOURCES}/roundtrip.expected")
elseif(CASE STREQUAL "exports")
    library(geo shared "${dir}")
    # The names a library exports, read with the LLVM tools of the runtime
    # archive. A dylib lists them in its exports trie, one per line after
    # its address.
    if(APPLE)
        tool_run("" "${LLVM_OBJDUMP}" --macho --exports-trie "${library_file}")
    elseif(CMAKE_HOST_WIN32)
        tool_run("" "${RUNTIME}/bin/llvm-readobj" --coff-exports "${library_file}")
        string(REGEX MATCHALL "Name: [A-Za-z_][A-Za-z0-9_.]*" names
               "${tool_stdout}")
        string(REPLACE "Name: " "" tool_stdout "${names}")
        string(REPLACE ";" "\n" tool_stdout "${tool_stdout}\n")
    else()
        # A global or a weak name of the dynamic symbol table that is not
        # undefined is an export of a shared object.
        tool_run("" "${LLVM_OBJDUMP}" --dynamic-syms "${library_file}")
        string(REGEX MATCHALL "[^\n]*\n" lines "${tool_stdout}")
        set(tool_stdout "")
        foreach(line IN LISTS lines)
            if(line MATCHES "^[0-9a-f]+ (g | w)" AND NOT line MATCHES "\\*UND\\*")
                string(APPEND tool_stdout "${line}")
            endif()
        endforeach()
    endif()
    string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_.]*\n" symbols "${tool_stdout}")
    set(names "")
    foreach(symbol IN LISTS symbols)
        string(STRIP "${symbol}" symbol)
        string(REGEX REPLACE "^_" "" symbol "${symbol}")
        list(APPEND names "${symbol}")
    endforeach()
    list(SORT names)
    set(wanted anti_licenses geo_apply geo_dot geo_half geo_layer geo_ready
        geo_scale geo_sum_by)
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
    program("${two_shared}" OPTIONS -I "${dir}/shared"
            SOURCES "${SOURCES}/twolibs.c" INPUTS "${geo}" "${library_link}")
    expect_printed("10\n" "${two_shared}")
    library(geo static "${dir}/static")
    set(geo "${library_file}")
    library(other static "${dir}/static")
    # The printed link line names the runtime library of the host.
    string(REGEX MATCH "[^ ]*${RUNTIME_LIBRARY}" runtime_library "${tool_stdout}")
    program("${dir}/two_static${EXE}" OPTIONS -I "${dir}/static"
            SOURCES "${SOURCES}/twolibs.c"
            INPUTS "${geo}" "${library_file}" ${runtime_library})
    expect_printed("10\n" "${dir}/two_static${EXE}")
elseif(CASE STREQUAL "loader")
    library(geo shared "${dir}")
    program("${dir}/loader${EXE}" SOURCES "${SOURCES}/loader.c")
    expect_printed("1\n" "${dir}/loader${EXE}" "${library_file}")
elseif(CASE STREQUAL "header")
    library(geo static "${dir}")
    expect_header("${dir}/geo.h" geo.h)
    library(shapes static "${dir}")
    expect_header("${dir}/shapes.h" shapes.h)
    tool_run("" ${CC} -std=c11 -I "${dir}" -fsyntax-only
                "${SOURCES}/header.c")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only
                "${SOURCES}/header.cpp")
    tool_run("" ${CC} -std=c11 -I "${dir}" -fsyntax-only
                "${SOURCES}/shapes.c")
    tool_run("" ${CXX} -std=c++17 -I "${dir}" -fsyntax-only
                "${SOURCES}/shapes.cpp")
elseif(CASE STREQUAL "bundle")
    library(geo static "${dir}" --bundle-runtime)
    set(geo "${library_file}")
    expect_header("${dir}/geo.h" geo.bundle.h)
    string(STRIP "${tool_stdout}" line)
    # The driver puts the system libraries of the target on the line, and
    # a Linux program links pthread and the maths library.
    set(system "")
    if("${TARGET}" MATCHES "^linux-")
        set(system " -lpthread -lm")
    endif()
    if(NOT line STREQUAL "${DRIVER} main.c ${geo}${system}")
        message(FATAL_ERROR "unexpected link line: ${line}")
    endif()
    # A C program links the bundled library and nothing else of Anti,
    # and runs on the runtime inside it.
    program("${dir}/roundtrip${EXE}" OPTIONS -I "${dir}"
            SOURCES "${SOURCES}/roundtrip.c" INPUTS "${geo}")
    if(NOT CROSS)
        expect_output("${dir}/roundtrip${EXE}" "${SOURCES}/roundtrip.expected")
    endif()
    # Two bundled libraries of ELF or COFF in one program. A Mach-O bundle
    # is an archive, whose duplicate is the marker that the case twice
    # reads.
    if(NOT "${TARGET}" MATCHES "^macos-")
        library(other static "${dir}" --bundle-runtime)
        if("${TARGET}" MATCHES "^windows-")
            tool_run("" ${CC} -I "${dir}" -c "${SOURCES}/twolibs.c"
                        -o "${dir}/two_bundled.o")
            execute_process(
                COMMAND "${LLD_LINK}" ${WINDOWS_LINK}
                        "/OUT:${dir}/two_bundled${EXE}" "${dir}/two_bundled.o"
                        "${geo}" "${library_file}" ${ANTIC_WINDOWS_LIBRARIES}
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
        else()
            execute_process(
                COMMAND ${CC} -I "${dir}" "${SOURCES}/twolibs.c" "${geo}"
                        "${library_file}" ${LINK} -o "${dir}/two_bundled${EXE}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
        endif()
        if(status EQUAL 0 OR NOT "${out}${err}" MATCHES "duplicate symbol|multiple definition")
            message(FATAL_ERROR "two bundled runtimes linked: ${status}\n${out}${err}")
        endif()
        # One of the duplicates is a symbol that the runtime library defines,
        # and not only one that antic writes into the library object. Every
        # linker spells a duplicate its own way, and llvm-objdump lists a
        # definition of COFF, ELF and Mach-O each its own way. ld64.lld names
        # a Mach-O symbol without the underscore that llvm-objdump prints.
        file(GLOB runtime_libraries "${RUNTIME}/lib/${TARGET}/*/${RUNTIME_LIBRARY}")
        list(GET runtime_libraries 0 runtime_library)
        set(linked "${out}${err}")
        tool_run("" "${LLVM_OBJDUMP}" --syms "${runtime_library}")
        set(defined "${tool_stdout}")
        set(spelled "(duplicate symbol:? '?|multiple definition of `)")
        string(REGEX MATCHALL "${spelled}[^' \n]+" reported "${linked}")
        set(runtime_duplicate "")
        foreach(item IN LISTS reported)
            string(REGEX REPLACE "^${spelled}" "" name "${item}")
            string(REGEX REPLACE "([][+.*()^$?|\\])" "\\\\\\1" pattern "${name}")
            if(defined MATCHES "(\\(sec +[1-9][0-9]*\\)\\(fl 0x[0-9a-f]+\\)\\(ty +[0-9a-f]+\\)\\(scl +2\\) \\(nx [0-9]+\\) 0x[0-9a-f]+ |\n[0-9a-f]+ g [^\n]*[ \t])_?${pattern}\r?\n")
                set(runtime_duplicate "${name}")
                break()
            endif()
        endforeach()
        if(runtime_duplicate STREQUAL "")
            message(FATAL_ERROR "no duplicate is a symbol of the runtime\n${linked}")
        endif()
    endif()
    # A library that reads the notice links against the stub of the bundle
    # and reports an empty notice.
    library(notice static "${dir}/notice" --bundle-runtime)
    program("${dir}/notice/notice${EXE}" OPTIONS -I "${dir}/notice"
            SOURCES "${SOURCES}/notice.c" INPUTS "${library_file}")
    if(NOT CROSS)
        expect_printed("0\n" "${dir}/notice/notice${EXE}")
    endif()
elseif(CASE STREQUAL "twice")
    # Two bundled libraries of a macOS target in one C program. A Mach-O
    # bundle is an archive: the library object, the members of the runtime
    # library without the one of src/rt/start.c, and a marker member. The
    # library object refers to anti_rt_bundle_<package> of its own marker
    # member, so the link loads the marker of each bundle, and each
    # defines anti_rt_bundle, which ld64.lld reports as a duplicate.
    if(NOT MACOS_LINK)
        message(FATAL_ERROR "the case twice takes a macOS target and SYSROOT")
    endif()
    library(geo static "${dir}" --bundle-runtime)
    set(geo "${library_file}")
    library(other static "${dir}" --bundle-runtime)
    tool_run("" "${LLVM_AR}" t "${geo}")
    string(REPLACE "\r" "" listed "\n${tool_stdout}")
    foreach(member geo.o geo.bundle.o geo.package.o anti_rt_license_stub.o
            init.o text.o)
        if(NOT listed MATCHES "\n${member}\n")
            message(FATAL_ERROR "${geo} holds no ${member}:${listed}")
        endif()
    endforeach()
    foreach(member start.o license.o geo.bundled.o)
        if(listed MATCHES "\n${member}\n")
            message(FATAL_ERROR "${geo} holds ${member}:${listed}")
        endif()
    endforeach()
    # The marker member defines both names, and the library object leaves
    # the one of its package undefined.
    tool_run("" "${LLVM_OBJDUMP}" --syms "${geo}")
    string(REPLACE "\r" "" symbols "${tool_stdout}")
    foreach(defined _anti_rt_bundle _anti_rt_bundle_com_example_geo)
        if(NOT symbols MATCHES "\n[0-9a-f]+ g [^\n]*[ \t]${defined}\n")
            message(FATAL_ERROR "${geo} defines no ${defined}\n${symbols}")
        endif()
    endforeach()
    if(NOT symbols MATCHES "\\*UND\\* _anti_rt_bundle_com_example_geo\n")
        message(FATAL_ERROR "the library object of ${geo} names no marker\n"
                            "${symbols}")
    endif()
    tool_run("" ${CC} -I "${dir}" -c "${SOURCES}/twolibs.c" -o "${dir}/twolibs.o")
    execute_process(
        COMMAND "${LD64}" ${MACOS_LINK} "${dir}/twolibs.o" "${geo}"
                "${library_file}" -o "${dir}/two_bundled"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    string(REPLACE "\r" "" linked "${out}${err}")
    if(status EQUAL 0 OR EXISTS "${dir}/two_bundled")
        message(FATAL_ERROR "two bundled runtimes linked: ${status}\n${linked}")
    endif()
    if(NOT linked MATCHES "duplicate symbol: _?anti_rt_bundle\n>>> defined in [^\n]*libgeo\\.a\\(geo\\.bundle\\.o\\)\n>>> defined in [^\n]*libother\\.a\\(other\\.bundle\\.o\\)\n")
        message(FATAL_ERROR "ld64.lld reports no duplicate marker\n${linked}")
    endif()
else()
    message(FATAL_ERROR "unknown CASE ${CASE}")
endif()
