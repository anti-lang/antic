# The tests of anti bind. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   WORK      a directory for the output
#   CASE      header, clang, headers, escaped_path, raymath, raylib,
#             raylib_shipped, api, refusals, api_malformed, clang_malformed,
#             ast_malformed,
#             api_typedef_chain, clang_typedef_chain, clang_of_the_tree,
#             clang_of_the_tree_relative, enum_declared_first, shift_types,
#             cast_types, struct_literal_ranges, clang_sides, macro_sides,
#             ast_sides, ast_refusals, tag_beside_function or
#             probe_anonymous_member
#   SOURCES   tests/clib, for the case header
#   HEADERS   tests/clib/headers, the headers that antic --lib writes
#   BIND      tests/bind, the fixtures of the other cases
#   ABI       tests/abi, whose abi_raymath.expected the case raymath reads
#   RAYLIB    the pinned raylib source
#   RAYLIB_HEADERS, RAYLIB_DEFINES
#             the headers of src/ of RAYLIB and the definitions under which
#             the build binds anti.raylib, separated by commas
#   CC        the C compiler of the build, with its options
#   HOST_LINK the options of a link of a program of this host
#   CLANG_DIR the pinned clang of the build
#   CLANG_VERSION, CLANG_TAG, CLANG_PAGE the version of the pinned clang, the
#             tag of its release and the page of that release, which the
#             refusal of another clang names
#   HOST      the target name of this host

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../pinned_clang.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

# What the refusal of a missing clang or of another major version names: the
# major of the pinned clang, its archive for this host and the page of its
# release. The page holds dots, which stand for themselves.
string(REGEX MATCH "^[0-9]+" clang_major "${CLANG_VERSION}")
string(REPLACE "." "\\." clang_page "${CLANG_PAGE}")
string(REPLACE "." "\\." clang_asset "clang-${CLANG_TAG}-${HOST}.tar.xz")
set(clang_where "needs clang ${clang_major}\\. Get ${clang_asset} from ${clang_page}, or another clang ${clang_major}, and put it first on PATH")
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/warnings.cmake")

# The arguments with which the build binds anti.raylib.
set(raylib_bind "")
string(REPLACE "," ";" raylib_headers "${RAYLIB_HEADERS}")
string(REPLACE "," ";" raylib_defines "${RAYLIB_DEFINES}")
foreach(header IN LISTS raylib_headers)
    list(APPEND raylib_bind "${RAYLIB}/src/${header}")
endforeach()
foreach(define IN LISTS raylib_defines)
    list(APPEND raylib_bind -D "${define}")
endforeach()

# Fail unless file holds the bytes of expected.
function(expect_file file expected)
    file(READ "${file}" got)
    file(READ "${expected}" wanted)
    expect_same_text("${file}, against ${expected}," "${wanted}" "${got}")
endfunction()

# Fail unless the command fails and its standard error matches pattern.
function(expect_refusal pattern)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    string(REGEX REPLACE "[ \n]+" " " said "${err}")
    if(status EQUAL 0 OR NOT said MATCHES "${pattern}")
        message(FATAL_ERROR "${ARGN} was not refused with `${pattern}`: "
                            "${status}\n${out}${err}")
    endif()
endfunction()

# Compile the binding <work>/out/<name>.anti as the module <module> into
# <work>/lib, where a program that imports it finds its library file.
function(compile_binding name module)
    string(REPLACE "." "/" dir "${module}")
    get_filename_component(dir "${dir}" DIRECTORY)
    file(MAKE_DIRECTORY "${WORK}/src/${dir}" "${WORK}/lib/${dir}")
    file(COPY_FILE "${WORK}/out/${name}.anti" "${WORK}/src/${dir}/${name}.anti")
    antic_build(-c ${ARGN} -I "${WORK}/src" --runtime "${RUNTIME}" -o
                "${WORK}/lib/${dir}/${name}.antl"
                "${WORK}/src/${dir}/${name}.anti")
endfunction()

# Build the two probes that anti bind wrote, run both and compare their
# output byte for byte. include is the directory of the header, and the
# arguments after it are more options of the C compiler.
function(compare_probes name include)
    # The probe of C prints through the streams of C, which Windows opens
    # in text mode, so it takes binary_stdio.h as every C test file does.
    tool_run("" ${CC} ${ANTIC_C_WARNINGS} -std=c11 -I "${include}" ${ARGN}
                ${HOST_LINK} -include "${CMAKE_CURRENT_LIST_DIR}/../binary_stdio.h"
                -o "${WORK}/probe_c" "${WORK}/out/probe_${name}.c")
    antic_build(--runtime "${RUNTIME}" -I "${WORK}/lib"
                -o "${WORK}/probe_anti" "${WORK}/out/probe_${name}.anti")
    program_output(c_hex c_status "${WORK}/probe_c.out" "${WORK}/probe_c")
    program_output(anti_hex anti_status "${WORK}/probe_anti.out"
                   "${WORK}/probe_anti")
    if(NOT c_status EQUAL 0 OR NOT anti_status EQUAL 0 OR
       NOT c_hex STREQUAL anti_hex)
        file(READ "${WORK}/probe_c.out" from_c)
        file(READ "${WORK}/probe_anti.out" from_anti)
        message(FATAL_ERROR "the layouts of ${name} differ\nC:\n${from_c}\n"
                            "Anti:\n${from_anti}")
    endif()
    if(c_hex STREQUAL "")
        message(FATAL_ERROR "the probe of ${name} printed nothing")
    endif()
endfunction()

# Compile the shim, link a program that calls through the binding and
# compare its output with the file expected.
function(expect_calls name include program expected)
    tool_run("" ${CC} ${ANTIC_C_WARNINGS} -std=c11 -I "${include}" -c
                -o "${WORK}/shim${CMAKE_C_OUTPUT_EXTENSION}"
                "${WORK}/out/shim_${name}.c")
    antic_build(--runtime "${RUNTIME}" -I "${WORK}/lib"
                -o "${WORK}/calls" "${program}"
                "${WORK}/shim${CMAKE_C_OUTPUT_EXTENSION}")
    program_output(got status "${WORK}/calls.out" "${WORK}/calls")
    file(READ "${expected}" wanted HEX)
    if(NOT status EQUAL 0)
        file(READ "${WORK}/calls.out" text)
        message(FATAL_ERROR "the calls of ${name} ended with ${status}\n${text}")
    endif()
    expect_same_bytes("the output of the calls of ${name}" "${wanted}" "${got}")
endfunction()

# A clang of the shell in WORK/fake that answers --version with the file
# version, -E with the file pre and the read of the AST with the file ast,
# and a copy of anti outside the checkout, which runs it. Sets h to the
# empty header WORK/fake.h and fake to the command that binds it.
function(fake_clang)
    file(MAKE_DIRECTORY "${WORK}/bin" "${WORK}/fake")
    file(COPY_FILE "${ANTI}" "${WORK}/bin/anti")
    file(CHMOD "${WORK}/bin/anti" PERMISSIONS OWNER_READ OWNER_WRITE
         OWNER_EXECUTE)
    set(h "${WORK}/fake.h")
    file(WRITE "${h}" "")
    file(WRITE "${WORK}/fake/clang" "#!/bin/sh
d=`dirname \"$0\"`
for a in \"$@\"; do
    case \"$a\" in
    --version) cat \"$d/version\"; exit 0 ;;
    -print-resource-dir) echo \"$d\"; exit 0 ;;
    -E) cat \"$d/pre\"; exit 0 ;;
    esac
done
cat \"$d/ast\"
")
    file(CHMOD "${WORK}/fake/clang" PERMISSIONS OWNER_READ OWNER_WRITE
         OWNER_EXECUTE)
    set(h "${h}" PARENT_SCOPE)
    set(fake "${CMAKE_COMMAND}" -E env "PATH=${WORK}/fake:$ENV{PATH}"
        "${WORK}/bin/anti" bind --clang "${h}" --module bindtest.fake
        -o "${WORK}/out" --runtime "${RUNTIME}" PARENT_SCOPE)
endfunction()

# Fail unless the command ends with 0 inside 30 seconds and its standard
# error holds warning.
function(expect_bound_in_time warning)
    execute_process(COMMAND ${ARGN} TIMEOUT 30
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    string(REGEX REPLACE "[ \n]+" " " said "${err}")
    string(FIND "${said}" "${warning}" at)
    if(NOT status EQUAL 0 OR at EQUAL -1)
        message(FATAL_ERROR "${ARGN} did not end with `${warning}`: "
                            "${status}\n${out}${err}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT CMAKE_C_OUTPUT_EXTENSION)
    set(CMAKE_C_OUTPUT_EXTENSION ".o")
endif()

if(CASE STREQUAL "header")
    # The header of each library file equals the header that antic --lib
    # writes for the same module, which tests/clib/headers holds.
    foreach(name geo shapes canvas failing tuples flags variants simdlib
            nested forms never)
        antic_build(-c --runtime "${RUNTIME}" -I "${SOURCES}" -o
                    "${WORK}/${name}.antl"
                    "${SOURCES}/com/example/${name}.anti")
        tool_run("" "${ANTI}" bind --header "${WORK}/${name}.antl" -o "${WORK}/out"
                    --runtime "${RUNTIME}" -I "${SOURCES}")
        file(READ "${WORK}/out/${name}.h" got)
        file(READ "${HEADERS}/${name}.h" wanted)
        expect_same_text("${name}.h, against ${HEADERS}/${name}.h," "${wanted}"
                         "${got}")
    endforeach()
    # M26 of the second audit: the header takes the last segment of the module
    # path, whatever the library file is called and however its path is
    # written. A Windows host names it with backslashes as well.
    file(MAKE_DIRECTORY "${WORK}/renamed")
    file(COPY_FILE "${WORK}/geo.antl" "${WORK}/renamed/other.antl")
    set(spellings "${WORK}/renamed/other.antl")
    if(HOST MATCHES "^windows-")
        file(TO_NATIVE_PATH "${WORK}/renamed/other.antl" native)
        list(APPEND spellings "${native}")
    endif()
    foreach(library IN LISTS spellings)
        file(REMOVE_RECURSE "${WORK}/renamed-out")
        tool_run("" "${ANTI}" bind --header "${library}" -o "${WORK}/renamed-out"
                    --runtime "${RUNTIME}" -I "${SOURCES}")
        file(GLOB written RELATIVE "${WORK}/renamed-out" "${WORK}/renamed-out/*")
        if(NOT written STREQUAL "geo.h")
            message(FATAL_ERROR "${library} gave ${written}, not geo.h")
        endif()
        file(READ "${WORK}/renamed-out/geo.h" got)
        file(READ "${HEADERS}/geo.h" wanted)
        expect_same_text("the header of ${library}, against geo.h,"
                         "${wanted}" "${got}")
    endforeach()
elseif(CASE STREQUAL "clang")
    # tests/bind/layout.h holds one of each form. The module, the shim and
    # the warnings are fixed, the module compiles, the two probes agree and
    # the inline functions answer through the shim.
    tool_run("" "${ANTI}" bind --clang "${BIND}/layout.h" --module bindtest.layout
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/layout.anti" "${BIND}/layout.anti.expected")
    expect_file("${WORK}/out/shim_layout.c" "${BIND}/shim_layout.c.expected")
    expect_file("${WORK}/warnings" "${BIND}/layout.warnings")
    compile_binding(layout bindtest.layout)
    compare_probes(layout "${BIND}")
    expect_calls(layout "${BIND}" "${BIND}/layout_calls.anti"
                 "${BIND}/layout_calls.expected")
elseif(CASE STREQUAL "forms")
    # tests/bind/forms.h holds the forms that layout.h leaves out, among
    # them a union a typedef names alone. The
    # header it includes stands in a directory that `-I` names. The module
    # and the warnings are fixed, the module compiles and the two probes
    # agree.
    tool_run("" "${ANTI}" bind --clang "${BIND}/forms.h" -I "${BIND}/include"
                --module bindtest.forms --probe -o "${WORK}/out"
                --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/forms.anti" "${BIND}/forms.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/forms.warnings")
    compile_binding(forms bindtest.forms)
    compare_probes(forms "${BIND}" -I "${BIND}/include")
elseif(CASE STREQUAL "values")
    # tests/bind/values.h holds macros alone: the values that layout.h
    # leaves out, and the ones the evaluator skips. The module and the
    # warnings are fixed, and the module compiles.
    tool_run("" "${ANTI}" bind --clang "${BIND}/values.h" --module bindtest.values
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/values.anti" "${BIND}/values.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/values.warnings")
    compile_binding(values bindtest.values)
elseif(CASE STREQUAL "functions")
    # tests/bind/functions.h holds the forms of a function that layout.h
    # leaves out. The module, the shim and the warnings are fixed, the
    # module compiles and the two probes agree.
    tool_run("" "${ANTI}" bind --clang "${BIND}/functions.h"
                --module bindtest.functions --probe -o "${WORK}/out"
                --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/functions.anti"
                "${BIND}/functions.anti.expected")
    expect_file("${WORK}/out/shim_functions.c"
                "${BIND}/shim_functions.c.expected")
    expect_file("${WORK}/warnings" "${BIND}/functions.warnings")
    compile_binding(functions bindtest.functions)
    compare_probes(functions "${BIND}")
elseif(CASE STREQUAL "headers")
    # Two headers into one module, by Q4 of "Answers of 2026-11-01" in
    # docs/decisions.md: tests/bind/pair_first.h and pair_second.h, read
    # in order in one translation unit. The module holds the struct of the
    # first once and the functions and the constant of both, the comment
    # of the module and the line of the command name both headers, a
    # warning names the header of its macro, the shim and the probe of C
    # include both, the module compiles and the two probes agree.
    tool_run("" "${ANTI}" bind --clang "${BIND}/pair_first.h"
                "${BIND}/pair_second.h" --module bindtest.pair --probe
                -o "${WORK}/out" --runtime "${RUNTIME}")
    if(NOT tool_stdout STREQUAL "bind: bindtest.pair from pair_first.h and pair_second.h, 3 functions and a shim, 3 warnings\n")
        message(FATAL_ERROR "anti bind of two headers printed\n${tool_stdout}")
    endif()
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/pair.anti" "${BIND}/pair.anti.expected")
    expect_file("${WORK}/out/shim_pair.c" "${BIND}/shim_pair.c.expected")
    expect_file("${WORK}/warnings" "${BIND}/pair.warnings")
    compile_binding(pair bindtest.pair)
    compare_probes(pair "${BIND}")
elseif(CASE STREQUAL "huge_enum")
    # An enum with a value above the range of a signed 64-bit integer and
    # one with values below zero and above INT_MAX, in a struct. The two
    # probes agree on the layout of the struct. T117 of
    # docs/audit/tests-findings.md.
    tool_run("" "${ANTI}" bind --clang "${BIND}/huge.h" --module bindtest.huge
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    compile_binding(huge bindtest.huge)
    compare_probes(huge "${BIND}")
elseif(CASE STREQUAL "nested_enum")
    # An enum without a name as the type of a field. Each enumerator is a
    # constant of the module, as for such an enum outside a struct. T118
    # of docs/audit/tests-findings.md.
    tool_run("" "${ANTI}" bind --clang "${BIND}/tagged.h" --module bindtest.tagged
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/tagged.anti" module)
    string(FIND "${module}" "pub const TAGGED_Y: c_int = 1;" at)
    if(at LESS 0)
        message(FATAL_ERROR "the binding holds no constant TAGGED_Y\n"
                            "${module}")
    endif()
elseif(CASE STREQUAL "words")
    # Names of C that are words of Anti. A record, an enum, an enumerator
    # and a constant take a trailing `_` as a field does, and a macro is
    # skipped. The module and the warnings are fixed, the module compiles
    # and the two probes agree.
    tool_run("" "${ANTI}" bind --clang "${BIND}/words.h" --module bindtest.words
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/words.anti" "${BIND}/words.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/words.warnings")
    compile_binding(words bindtest.words)
    compare_probes(words "${BIND}")
elseif(CASE STREQUAL "unsigned_bitfield")
    # A bitfield of one bit of size_t holds 1, so the two probes set it and
    # agree, as for every unsigned type. M339 of docs/audit/summary.md.
    tool_run("" "${ANTI}" bind --clang "${BIND}/bits.h" --module bindtest.bits
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/probe_bits.c" probe)
    string(FIND "${probe}" "dump(\"Bits.one\"" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the probe sets no Bits.one\n${probe}")
    endif()
    compile_binding(bits bindtest.bits)
    compare_probes(bits "${BIND}")
elseif(CASE STREQUAL "fixed_enum")
    # An enum with a fixed base takes that base on every target, and the
    # two probes agree on the structs that hold one. T137 of
    # docs/audit/tests-findings.md.
    tool_run("" "${ANTI}" bind --clang "${BIND}/fixed.h" --module bindtest.fixed
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/fixed.anti" module)
    foreach(line "pub enum Small: c_uchar" "pub enum Wide: c_ulonglong")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "fixed.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
    compile_binding(fixed bindtest.fixed)
    # A fixed base is C23, so the probe of C is compiled as C23.
    compare_probes(fixed "${BIND}" -std=c23)
elseif(CASE STREQUAL "escaped_path")
    # A path given with a backslash, as every native path of Windows is.
    # clang escapes it in the line markers of `-E`, and the macros of the
    # header are still its own. The binding equals that of the case clang.
    # A directory of POSIX may hold a backslash in its name, so the same
    # path stands on every host.
    # file() reads a backslash as a separator, so a POSIX host makes the
    # directory and the copy with its own commands.
    if(HOST MATCHES "^windows-")
        file(MAKE_DIRECTORY "${WORK}/escaped")
        file(COPY_FILE "${BIND}/layout.h" "${WORK}/escaped/layout.h")
        file(TO_NATIVE_PATH "${WORK}/escaped/layout.h" header)
    else()
        set(dir "${WORK}/back\\slash")
        set(header "${dir}/layout.h")
        tool_run("" mkdir -p "${dir}")
        tool_run("" cp "${BIND}/layout.h" "${header}")
    endif()
    tool_run("" "${ANTI}" bind --clang "${header}" --module bindtest.layout
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/layout.anti" "${BIND}/layout.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/layout.warnings")
elseif(CASE STREQUAL "raymath")
    # raymath of the pinned raylib, read through clang. Its probe agrees
    # with C, and the functions of tests/abi/abi_raymath.anti called
    # through the generated binding and its shim print what the
    # hand-written binding prints.
    tool_run("" "${ANTI}" bind --clang "${RAYLIB}/src/raymath.h"
                --module bindtest.raymath --probe -o "${WORK}/out"
                --runtime "${RUNTIME}")
    compile_binding(raymath bindtest.raymath)
    compare_probes(raymath "${RAYLIB}/src")
    file(STRINGS "${ABI}/abi_raymath.expected" lines)
    list(REMOVE_AT lines 0)
    list(JOIN lines "\n" printed)
    # file(WRITE) writes CRLF on Windows, and the program writes LF.
    file(CONFIGURE OUTPUT "${WORK}/raymath_calls.expected"
         CONTENT "${printed}\n" @ONLY NEWLINE_STYLE UNIX)
    expect_calls(raymath "${RAYLIB}/src" "${BIND}/raymath_calls.anti"
                 "${WORK}/raymath_calls.expected")
elseif(CASE STREQUAL "raylib")
    # anti.raylib comes from the headers of the pinned raylib that raylib
    # compiles, bound as the build binds them. It compiles, names its
    # frameworks and its Linux libraries, turns the colour macros into
    # constants of Color, holds the functions of raymath, rcamera and rlgl
    # and no shim, and its two probes agree.
    tool_run("" "${ANTI}" bind --clang ${raylib_bind} --probe
                -o "${WORK}/out" --runtime "${RUNTIME}")
    if(EXISTS "${WORK}/out/shim_raylib.c")
        message(FATAL_ERROR "anti.raylib needs a shim, which no library of "
                            "the runtime archive holds")
    endif()
    file(READ "${WORK}/out/raylib.anti" module)
    foreach(line "link framework \"Cocoa\";" "link linux \"X11\";"
            "pub const RAYWHITE: Color = Color { r: 245, g: 245, b: 245, a: 255 };"
            "pub extern fn SetConfigFlags(flags: c_uint);"
            "pub extern fn Vector3Add(v1: Vector3, v2: Vector3) -> Vector3;"
            "pub extern fn GetCameraForward(camera: ?*Camera3D) -> Vector3;"
            "pub extern fn rlGetVersion() -> c_int;")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "raylib.anti holds no line `${line}`")
        endif()
    endforeach()
    compile_binding(raylib anti.raylib --anti-internal)
    compare_probes(raylib "${RAYLIB}/src")
elseif(CASE STREQUAL "raylib_shipped")
    # anti.raylib of the runtime archive, which the build wrote, against
    # C. The probe of Anti names the module without -I, so it reads
    # std/anti/raylib.antl of the archive, and the two probes agree over
    # every struct of the four headers.
    tool_run("" "${ANTI}" bind --clang ${raylib_bind} --probe
                -o "${WORK}/out" --runtime "${RUNTIME}")
    if(NOT EXISTS "${RUNTIME}/std/anti/raylib.antl")
        message(FATAL_ERROR "the runtime archive holds no anti.raylib")
    endif()
    file(MAKE_DIRECTORY "${WORK}/lib")
    compare_probes(raylib "${RAYLIB}/src")
elseif(CASE STREQUAL "api")
    # A description in the format of rlparser, a part of raylib_api.json,
    # binds as anti.raylib with its frameworks and compiles. The file of
    # the pinned raylib is not JSON, and the refusal names the place.
    tool_run("" "${ANTI}" bind "${BIND}/raylib_api.json" -o "${WORK}/out")
    expect_file("${WORK}/out/raylib.anti" "${BIND}/raylib.anti.expected")
    compile_binding(raylib anti.raylib --anti-internal)
    expect_refusal("raylib_api.json is not JSON: .* at line 4699, column 115"
        "${ANTI}" bind "${RAYLIB}/tools/rlparser/output/raylib_api.json"
        -o "${WORK}/pinned")
elseif(CASE MATCHES "^api_(parts|callbacks|literals|types|words|aliases|docs)$")
    # Four descriptions in the format of rlparser. api_parts holds
    # entries that lack a part: a function without its name, its result
    # or its parameters, a struct without its name or its list of fields,
    # a parameter without a type, an enum without its values and a define
    # without its name, its kind or its value. A struct that lost a field
    # is refused by R172 of "Rules of 2026-10-13", so the cases of
    # tests/bind/messages.cmake hold a field without a part. api_callbacks
    # holds callbacks as the types of parameters, a variadic one and ones
    # that lack a part among them, and an enum as a type. api_literals
    # holds the forms of a struct literal that a define may have. api_types
    # holds one function per spelling of a C type: the words of an integer
    # in the orders no other description has, a qualifier after a name, an
    # empty parameter list, a function as a parameter, which C takes as a
    # pointer, and nine spellings that do not parse. Its struct holds a
    # function as a field, which the binding leaves out. api_aliases
    # holds an alias without its type, a pointer alias whose type does not
    # parse, a name no part declares, a struct and an enum named by their
    # tags, a length in hexadecimal, a callback that takes an array and two
    # spellings of words that do not parse. api_docs holds a description of
    # two lines, which the binding writes as the doc comment of its
    # function. The module and the warnings of each are fixed, and the
    # module compiles.
    tool_run("" "${ANTI}" bind "${BIND}/${CASE}.json" --module bindtest.${CASE}
                -o "${WORK}/out")
    file(WRITE "${WORK}/warnings" "${tool_stderr}")
    expect_file("${WORK}/out/${CASE}.anti" "${BIND}/${CASE}.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/${CASE}.warnings")
    compile_binding(${CASE} bindtest.${CASE})
elseif(CASE STREQUAL "miniaudio")
    # A module whose last segment is miniaudio, a bundled library, names
    # the frameworks of macOS and the libraries of Linux that the library
    # links, bind_frameworks and bind_linux_libraries of bindmodel.c.
    tool_run("" "${ANTI}" bind "${BIND}/miniaudio.json" --module bindtest.miniaudio
                -o "${WORK}/out")
    expect_file("${WORK}/out/miniaudio.anti" "${BIND}/miniaudio.anti.expected")
    compile_binding(miniaudio bindtest.miniaudio)
elseif(CASE STREQUAL "refusals")
    # A pack of two bytes, C++ and a clang of a version the tool was not
    # tested against are refused. The copy of anti stands outside the
    # checkout, so it takes the first clang on PATH, which says 19.
    expect_refusal("`PackTwo` is packed to more than one byte"
        "${ANTI}" bind --clang "${BIND}/pack2.h" -o "${WORK}/out"
        --runtime "${RUNTIME}")
    expect_refusal("clang could not read the header as C"
        "${ANTI}" bind --clang "${BIND}/cplusplus.h" -o "${WORK}/out"
        --runtime "${RUNTIME}")
    file(MAKE_DIRECTORY "${WORK}/bin" "${WORK}/fake")
    file(COPY_FILE "${ANTI}" "${WORK}/bin/anti")
    file(CHMOD "${WORK}/bin/anti" PERMISSIONS OWNER_READ OWNER_WRITE
         OWNER_EXECUTE)
    file(WRITE "${WORK}/fake/clang"
         "#!/bin/sh\necho 'Homebrew clang version 19.1.7'\necho 'Target: x'\n")
    file(CHMOD "${WORK}/fake/clang" PERMISSIONS OWNER_READ OWNER_WRITE
         OWNER_EXECUTE)
    expect_refusal("is clang 19, and bind --clang ${clang_where}"
        "${CMAKE_COMMAND}" -E env "PATH=${WORK}/fake:$ENV{PATH}"
        "${WORK}/bin/anti" bind --clang "${BIND}/layout.h" -o "${WORK}/out"
        --runtime "${RUNTIME}")
    # With no clang on PATH the refusal names the same version and place.
    file(MAKE_DIRECTORY "${WORK}/empty")
    expect_refusal("failed\\. bind --clang ${clang_where}"
        "${CMAKE_COMMAND}" -E env "PATH=${WORK}/empty"
        "${WORK}/bin/anti" bind --clang "${BIND}/layout.h" -o "${WORK}/out"
        --runtime "${RUNTIME}")
elseif(CASE STREQUAL "api_malformed")
    # Descriptions that are cut short, nested deep, repeat a struct or
    # hold values nested deep. Each is refused or bound with warnings,
    # and none crashes the reader.
    set(api "${WORK}/raylib_api.json")
    file(WRITE "${api}" "{\"functions\": [{\"name\": \"f\", \"params\": [")
    expect_refusal("raylib_api.json is not JSON"
        "${ANTI}" bind "${api}" -o "${WORK}/out")
    string(REPEAT "[" 100000 deep)
    file(WRITE "${api}" "${deep}")
    expect_refusal("raylib_api.json is not JSON"
        "${ANTI}" bind "${api}" -o "${WORK}/out")
    set(field "{\"name\": \"x\", \"type\": \"int\"}")
    file(WRITE "${api}" "{\"functions\": [], \"structs\": [
        {\"name\": \"A\", \"fields\": [${field}, ${field}, ${field}]},
        {\"name\": \"A\", \"fields\": [${field}]}]}")
    expect_refusal("names the struct `A` twice"
        "${ANTI}" bind "${api}" -o "${WORK}/out")
    # A name the binding writes into Anti and C is refused where the
    # reader takes it when it is no C identifier.
    set(bad "A { x: int } B")
    set(void_fn "\"returnType\": \"void\"")
    foreach(pair
        "struct|\"structs\": [{\"name\": \"${bad}\", \"fields\": []}]"
        "field|\"structs\": [{\"name\": \"S\", \"fields\": [
            {\"name\": \"${bad}\", \"type\": \"int\"}]}]"
        "enum|\"enums\": [{\"name\": \"${bad}\", \"values\": []}]"
        "value|\"enums\": [{\"name\": \"E\", \"values\": [
            {\"name\": \"${bad}\", \"value\": 1}]}]"
        "function|\"structs\": [], \"functions\": [
            {\"name\": \"${bad}\", ${void_fn}, \"params\": []}]"
        "parameter|\"structs\": [], \"functions\": [{\"name\": \"f\",
            ${void_fn}, \"params\": [{\"name\": \"${bad}\", \"type\": \"int\"}]}]"
        "define|\"defines\": [
            {\"name\": \"${bad}\", \"type\": \"INT\", \"value\": 1}]")
        string(FIND "${pair}" "|" bar)
        string(SUBSTRING "${pair}" 0 ${bar} kind)
        math(EXPR bar "${bar} + 1")
        string(SUBSTRING "${pair}" ${bar} -1 members)
        if(NOT members MATCHES "\"functions\"")
            set(members "\"functions\": [], ${members}")
        endif()
        file(WRITE "${api}" "{${members}}")
        expect_refusal("names the ${kind} `A .* B`, which is no C identifier"
            "${ANTI}" bind "${api}" -o "${WORK}/out")
    endforeach()
    # A struct that holds itself, and values a million deep.
    string(REPEAT "-" 1000000 minus)
    string(REPEAT "*" 1000000 stars)
    string(REPEAT "(" 1000000 open)
    file(WRITE "${api}" "{\"functions\": [
        {\"name\": \"g\", \"returnType\": \"void\", \"params\": [
            {\"name\": \"p\", \"type\": \"void ${open}\"}]}],
        \"structs\": [
        {\"name\": \"Self\", \"fields\": [
            {\"name\": \"s\", \"type\": \"Self\"}]},
        {\"name\": \"Deep\", \"fields\": [
            {\"name\": \"d\", \"type\": \"int ${stars}\"}]},
        {\"name\": \"Pair\", \"fields\": [
            {\"name\": \"a\", \"type\": \"int\"},
            {\"name\": \"b\", \"type\": \"int\"}]}],
        \"aliases\": [{\"name\": \"Loop\", \"type\": \"Loop\"}],
        \"defines\": [
            {\"name\": \"MINUS\", \"type\": \"INT\", \"value\": \"${minus}1\"},
            {\"name\": \"OPEN\", \"type\": \"INT\", \"value\": \"${open}1\"},
            {\"name\": \"NEG\", \"type\": \"UNKNOWN\", \"value\": \"-NOTHING\"},
            {\"name\": \"HUGE\", \"type\": \"COLOR\",
             \"value\": \"CLITERAL(Pair){ 99999999999999999999, 1 }\"},
            {\"name\": \"SMALL\", \"type\": \"COLOR\",
             \"value\": \"CLITERAL(Pair){ 2, 1 }\"}],
        \"enums\": 5, \"callbacks\": {\"x\": 1}}")
    tool_run("" "${ANTI}" bind "${api}" --probe -o "${WORK}/out")
    string(REGEX REPLACE "[ \n]+" " " said "${tool_stderr}")
    foreach(warning "the define `MINUS` is skipped" "the define `OPEN` is skipped"
            "the define `NEG` is skipped" "the define `HUGE` is skipped"
            "`g` is left out")
        string(FIND "${said}" "${warning}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning `${warning}`:\n${tool_stderr}")
        endif()
    endforeach()
    string(FIND "${said}" "the define `SMALL` is skipped" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "the define `SMALL` was skipped:\n${tool_stderr}")
    endif()
elseif(CASE STREQUAL "clang_malformed")
    # Valid C that a reader of the AST mishandled, and macros nested deep
    # or chained long. Each binds with warnings.
    set(h "${WORK}/malformed.h")
    set(text "enum { F = 1 };\nenum Mode { M0 };\nvoid f(enum Mode m);\n")
    string(REPEAT "-" 100000 minus)
    string(REPEAT "(" 100000 open)
    string(REPEAT ")" 100000 close)
    string(APPEND text "#define MINUS ${minus}1\n#define OPEN ${open}1${close}\n")
    foreach(i RANGE 0 4999)
        math(EXPR n "${i} + 1")
        string(APPEND text "#define CHAIN${i} (CHAIN${n} + 1)\n")
    endforeach()
    string(APPEND text "#define CHAIN5000 1\n")
    string(APPEND text "#pragma pack(push, 99999999999)\n"
           "struct After { int a; };\n#pragma pack(pop)\n")
    # Two anonymous records whose names, the record's and the field's
    # joined, agree in their first 256 bytes and differ after them.
    string(REPEAT "x" 260 long)
    string(APPEND text "struct L${long} { struct { int a; } f1; "
           "struct { float b; } f2; };\n"
           "void g(struct L${long} *p);\n")
    # S32 of the second audit. An enumerator without a value after INT64_MAX.
    # The next value is 2^63, as a value written above INT64_MAX is, in an enum
    # of c_ulonglong.
    string(APPEND text "enum Wide : unsigned long long "
           "{ WIDE_TOP = 0x7fffffffffffffff, WIDE_NEXT };\n"
           "void wide(enum Wide w);\n")
    # S31 of the second audit. The last line C allows a #line to name, and
    # lines after it, which a line counter of 32 bits took past its range.
    string(APPEND text "#line 2147483647\n#define LATE 1\n"
           "struct Late { int a; };\nvoid late(struct Late *p);\n")
    file(WRITE "${h}" "${text}")
    tool_run("" "${ANTI}" bind --clang "${h}" --module bindtest.malformed
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/malformed.anti" module)
    foreach(line "pub extern fn f(m: Mode);" "pub const F: c_int = 1;"
            "pub const CHAIN5000: c_int = 1;" "_f1\n{\n\ta: c_int," "_f2\n{\n\tb: c_float,"
            "pub enum Wide: c_ulonglong" "WIDE_NEXT = 9223372036854775808," "pub const LATE: c_int = 1;"
            "pub extern fn late(p: ?*Late);")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "malformed.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
    string(REGEX REPLACE "[ \n]+" " " said "${tool_stderr}")
    foreach(warning "the macro `MINUS` is no constant"
            "the macro `OPEN` is no constant" "the macro `CHAIN0` is no constant")
        string(FIND "${said}" "${warning}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning `${warning}`:\n${tool_stderr}")
        endif()
    endforeach()
elseif(CASE STREQUAL "ast_malformed")
    # A clang that writes a version out of range, an AST cut short or
    # nested deep, an AST whose values have the wrong kinds and a chain
    # of typedefs that the dump of a real clang nests. The copy of anti
    # stands outside the checkout, so it runs the clang of fake.
    fake_clang()
    file(WRITE "${WORK}/fake/version"
         "clang version 99999999999999999999.1.0\nTarget: x\n")
    expect_refusal("cannot read the version of .* from `.* --version`, and bind --clang ${clang_where}" ${fake})
    file(WRITE "${WORK}/fake/version" "clang version 23.1.0\nTarget: x\n")
    # S31 of the second audit. A line marker past the lines clang counts, one
    # that no integer holds, and one at the end of int64_t with a line after
    # it.
    file(WRITE "${WORK}/fake/ast" "{\"kind\": \"TranslationUnitDecl\", \"inner\": []}")
    foreach(marker 4294967296 99999999999999999999 9223372036854775807)
        file(WRITE "${WORK}/fake/pre" "# ${marker} \"${h}\"\nint a;\nint b;\n")
        expect_refusal("the line marker `# ${marker} .*` of clang -E is out of range"
            ${fake})
    endforeach()
    file(WRITE "${WORK}/fake/pre" "# 1 \"${h}\"\n#pragma pack(99999999999999999999)\n")
    file(WRITE "${WORK}/fake/ast" "{\"kind\": \"TranslationUnitDecl\", \"inner\": [")
    expect_refusal("the AST of clang is not JSON" ${fake})
    string(REPEAT "[" 100000 deep)
    file(WRITE "${WORK}/fake/ast" "${deep}")
    expect_refusal("the AST of clang is not JSON" ${fake})
    set(loc "\"loc\": {\"file\": \"${h}\", \"line\": 2}")
    set(ast "{\"kind\": \"TranslationUnitDecl\", \"inner\": [
        {\"kind\": \"EnumDecl\", ${loc}},
        {\"kind\": \"EnumDecl\", \"id\": 5, \"inner\": 7},
        {\"kind\": \"RecordDecl\", \"name\": 3, \"inner\": [1]},
        {\"kind\": \"TypedefDecl\", \"name\": \"T\"},
        {\"kind\": \"FunctionDecl\", \"name\": \"h\", \"type\": 1},
        {\"kind\": \"RecordDecl\", \"name\": \"Packed\", \"id\": \"0x1\",
         \"tagUsed\": \"struct\", \"completeDefinition\": true,
         \"inner\": [{\"kind\": \"MaxFieldAlignmentAttr\"},
                     {\"kind\": \"FieldDecl\", \"name\": \"x\",
                      \"type\": {\"qualType\": \"int\"}}]}")
    foreach(i RANGE 0 4999)
        math(EXPR n "${i} + 1")
        string(APPEND ast ",\n{\"kind\": \"TypedefDecl\", \"name\": \"T${i}\",
            \"type\": {\"qualType\": \"T${n}\"}}")
    endforeach()
    string(APPEND ast ",\n{\"kind\": \"TypedefDecl\", \"name\": \"T5000\",
        \"type\": {\"qualType\": \"int\"}},
        {\"kind\": \"FunctionDecl\", \"name\": \"k\", ${loc},
         \"type\": {\"qualType\": \"void (T0)\"},
         \"inner\": [{\"kind\": \"ParmVarDecl\", \"name\": \"t\",
                     \"type\": {\"qualType\": \"T0\"}}]}]}")
    file(WRITE "${WORK}/fake/ast" "${ast}")
    expect_refusal("`Packed` is packed to more than one byte" ${fake})
elseif(CASE STREQUAL "api_typedef_chain")
    # M312. Thirty aliases, each of which names the one before it three
    # times. The reader resolves each alias once, where it took 3^30
    # parses for the parameter of type F30 and did not end. The function
    # is left out at its first parameter, before the writer spells the
    # second.
    set(aliases "{\"name\": \"F0\", \"type\": \"long double\"}")
    foreach(i RANGE 1 30)
        math(EXPR before "${i} - 1")
        string(APPEND aliases ",\n{\"name\": \"F${i}\", \"type\": "
               "\"F${before} (*)(F${before}, F${before})\"}")
    endforeach()
    file(WRITE "${WORK}/chain.json" "{\"functions\": [{\"name\": \"k\",
        \"returnType\": \"void\", \"params\": [
        {\"name\": \"x\", \"type\": \"long double\"},
        {\"name\": \"t\", \"type\": \"F30\"}]}],
        \"aliases\": [${aliases}]}")
    expect_bound_in_time("`k` is left out: Anti has no form for the type of a parameter"
        "${ANTI}" bind "${WORK}/chain.json" --module bindtest.chain
        -o "${WORK}/out")
elseif(CASE STREQUAL "api_alias_chain_long")
    # T159: forty aliases, each of which names the one before it once, and
    # the first is int. The chain names no alias twice, so the parameter
    # is a c_int, and the reader gives no warning.
    set(aliases "{\"name\": \"G0\", \"type\": \"int\"}")
    foreach(i RANGE 1 40)
        math(EXPR before "${i} - 1")
        string(APPEND aliases ",\n{\"name\": \"G${i}\", \"type\": \"G${before}\"}")
    endforeach()
    file(WRITE "${WORK}/chain.json" "{\"functions\": [{\"name\": \"deep\",
        \"returnType\": \"void\", \"params\": [{\"name\": \"g\", \"type\": \"G40\"}]}],
        \"aliases\": [${aliases}]}")
    tool_run("" "${ANTI}" bind "${WORK}/chain.json" --module bindtest.chain
                -o "${WORK}/out")
    expect_same_text("the warnings of the chain" "" "${tool_stderr}")
    file(READ "${WORK}/out/chain.anti" bound)
    if(NOT bound MATCHES "pub extern fn deep\\(g: c_int\\);")
        message(FATAL_ERROR "the chain bound no `deep`:\n${bound}")
    endif()
elseif(CASE STREQUAL "clang_typedef_chain")
    # M312. Forty typedefs, each of which names the one before it three
    # times. A real clang nests the whole chain in the dump of every
    # typedef, so a clang of the shell writes the AST. The reader resolves
    # each typedef once, where it took 3^40 parses for the parameter of
    # type F40 and did not end.
    fake_clang()
    file(WRITE "${WORK}/fake/version"
         "clang version ${CLANG_VERSION}\nTarget: x\n")
    file(WRITE "${WORK}/fake/pre" "# 1 \"${h}\"\n")
    set(ast "{\"kind\": \"TranslationUnitDecl\", \"inner\": [
        {\"kind\": \"TypedefDecl\", \"name\": \"F0\",
         \"type\": {\"qualType\": \"long double\"}}")
    foreach(i RANGE 1 40)
        math(EXPR before "${i} - 1")
        string(APPEND ast ",\n{\"kind\": \"TypedefDecl\", \"name\": \"F${i}\",
            \"type\": {\"qualType\": \"F${before} (*)(F${before}, F${before})\"}}")
    endforeach()
    string(APPEND ast ",\n{\"kind\": \"FunctionDecl\", \"name\": \"k\",
        \"loc\": {\"file\": \"${h}\", \"line\": 1},
        \"type\": {\"qualType\": \"void (long double, F40)\"},
        \"inner\": [{\"kind\": \"ParmVarDecl\", \"name\": \"x\",
                     \"type\": {\"qualType\": \"long double\"}},
                    {\"kind\": \"ParmVarDecl\", \"name\": \"t\",
                     \"type\": {\"qualType\": \"F40\"}}]}]}")
    file(WRITE "${WORK}/fake/ast" "${ast}")
    expect_bound_in_time("`k` is left out: Anti has no form for the type of a parameter"
        ${fake})
elseif(CASE STREQUAL "clang_of_the_tree")
    # A copy of anti in host/ of a tree whose deps/clang is the pinned
    # clang takes that clang, with no clang on PATH. The tree stands in
    # WORK, and its deps/clang links to the pinned clang of the build.
    # anti looked for the directory above its own at `/` alone, so on
    # Windows, whose path of the executable separates with `\`, it found
    # no tree and took the first clang on PATH.
    if(CLANG_DIR)
        set(pinned "${CLANG_DIR}")
    else()
        get_filename_component(pinned "${ANTI}" DIRECTORY)
        set(pinned "${pinned}/../deps/clang")
    endif()
    get_filename_component(pinned "${pinned}" REALPATH)
    get_filename_component(name "${ANTI}" NAME)
    file(MAKE_DIRECTORY "${WORK}/tree/host" "${WORK}/tree/deps"
         "${WORK}/empty")
    file(COPY_FILE "${ANTI}" "${WORK}/tree/host/${name}")
    file(CREATE_LINK "${pinned}" "${WORK}/tree/deps/clang" SYMBOLIC)
    file(WRITE "${WORK}/api.h" "int count(void);\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=${WORK}/empty"
        "${WORK}/tree/host/${name}" bind --clang api.h -o "${WORK}/out"
        --runtime "${RUNTIME}"
        WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti of the tree took no pinned clang: "
                            "${status}\n${out}${err}")
    endif()
elseif(CASE STREQUAL "clang_of_the_tree_relative")
    # The same copy of anti run by a relative path from a directory below
    # its own, as the build runs anti from src/native/ of a tree. macOS
    # gave the path of the running program as the command line spelled
    # it, so the directory above `../anti` was the one of the command, and
    # anti took the first clang on PATH (T208).
    if(CLANG_DIR)
        set(pinned "${CLANG_DIR}")
    else()
        get_filename_component(pinned "${ANTI}" DIRECTORY)
        set(pinned "${pinned}/../deps/clang")
    endif()
    get_filename_component(pinned "${pinned}" REALPATH)
    get_filename_component(name "${ANTI}" NAME)
    file(MAKE_DIRECTORY "${WORK}/tree/host/below" "${WORK}/tree/deps"
         "${WORK}/empty")
    file(COPY_FILE "${ANTI}" "${WORK}/tree/host/${name}")
    file(CREATE_LINK "${pinned}" "${WORK}/tree/deps/clang" SYMBOLIC)
    file(WRITE "${WORK}/api.h" "int count(void);\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=${WORK}/empty"
        "../${name}" bind --clang "${WORK}/api.h" -o "${WORK}/out"
        --runtime "${RUNTIME}"
        WORKING_DIRECTORY "${WORK}/tree/host/below"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti of the tree run as ../${name} took no "
                            "pinned clang: ${status}\n${out}${err}")
    endif()
elseif(CASE STREQUAL "clang_sides")
    # The sides of the conditions of the reader of a header that a header
    # of C reaches, bound for three targets: a header of another file that
    # packs, two typedefs of one name, declarations before definitions,
    # enums of every base, two typedefs of one anonymous enum, anonymous
    # records and const ones in a directory whose name holds parentheses,
    # an aligned record with a field aligned less, functions declared more
    # than once, macros undefined and defined again, macros of one
    # parameter that are no identity, compound literals that are none and
    # a stack of `#pragma pack` deeper than its 64 entries.
    set(dir "${WORK}/dir(x)")
    file(MAKE_DIRECTORY "${dir}")
    file(WRITE "${dir}/other.h"
         "#pragma pack(push, 1)\nstruct Hidden { char c; int i; };\n"
         "#pragma pack(pop)\n")
    file(WRITE "${dir}/first.h"
         "#pragma pack(push, 1)\nstruct PackedFirst { char a; int b; };\n"
         "#pragma pack(pop)\n")
    string(CONCAT text "#include \"other.h\"\ntypedef struct Hidden HiddenName;\n"
        "typedef int Count;\ntypedef int Count;\n"
        "struct Fwd;\nstruct Fwd;\nstruct Fwd { int a; };\n"
        "enum Low { LOW_MIN = -0x80000001LL, LOW_ZERO = 0 };\n"
        "enum Fixed : int { FIXED_A = 1 };\nenum : __int128 { BIG = 1 };\n"
        "enum __attribute__((deprecated)) Old { OLD_A };\n"
        "enum Huge { HUGE_TOP = 0xFFFFFFFFFFFFFFFFull };\n"
        "typedef enum { TWO_A } TwoA, TwoB;\n"
        "struct Outer {\n    struct { struct { int deep; } inner; };\n"
        "    struct { int x; } named;\n    union { int i; float f; } u;\n};\n"
        "struct Const {\n    const struct { int a; } s;\n"
        "    const union { int i; float f; } u;\n};\n"
        "struct ConstEnum {\n    struct { int a; } s;\n    const enum { CE_A } e;\n};\n"
        "struct __attribute__((aligned(16))) Big { int a __attribute__((aligned(4))); };\n"
        "struct Late { char a; int b; };\n"
        "void redecl(void);\nvoid redecl(void);\n"
        "static int st(void);\nstatic int st(void) { return 1; }\n"
        "extern int ex(void);\nextern int ex(void);\n"
        "inline int inl(void) { return 2; }\nint inl(void);\n"
        "inline int inl2(void) { return 2; }\ninline int inl2(void);\n"
        "int plain(void) { return 3; }\n"
        "void take(struct Outer *o, struct Const *c, struct Late *l, enum Huge h,\n"
        "          HiddenName *n, struct PackedFirst *p, TwoB t, Count k);\n"
        "#define GONE 1\n#undef GONE\n#undef NEVER\n#define USES_GONE GONE\n"
        "#define TWICE 1\n#undef TWICE\n#define TWICE 2\n#define NOBODY(x)\n"
        "#define P(x) (x + 1)\n#define Q(x) (y)\n#define R(x) (x)z\n#define K(x) x + 1\n"
        "#define USEP P(struct Fwd){ 1 }\n#define USEQ Q(struct Fwd){ 1 }\n"
        "#define USER R(struct Fwd){ 1 }\n#define USEK K(struct Fwd){ 1 }\n"
        "#define IDX IDEA\n#define SPACED (struct Fwd) { 7 }\n"
        "#define NO_BRACE (struct Fwd) 7\n#define OPEN_ONLY (1\n"
        "#define NOT_TYPE (Unknown){ 1 }\n#define INT_LIT (int){ 1 }\n#define KK K2\n"
        "struct { int a; } anon_var;\n#pragma pack(push, 2)\n#pragma pack(pop)\n")
    foreach(i RANGE 1 66)
        string(APPEND text "#pragma pack(push, 1)\n")
    endforeach()
    foreach(i RANGE 1 66)
        string(APPEND text "#pragma pack(pop)\n")
    endforeach()
    string(APPEND text "struct { int last; };\n")
    file(WRITE "${dir}/sides.h" "${text}")
    foreach(target macos-x86_64 linux-arm64 windows-x86_64)
        tool_run("" "${ANTI}" bind --clang "${dir}/first.h" "${dir}/sides.h"
                    --module bindtest.sides --target ${target}
                    -o "${WORK}/out-${target}" --runtime "${RUNTIME}")
        file(READ "${WORK}/out-${target}/sides.anti" module)
        set(huge "pub enum Huge: c_ulonglong\n{\n\tHUGE_TOP = 18446744073709551615,")
        if(target MATCHES "^windows-")
            set(huge "pub enum Huge\n{\n\tHUGE_TOP = -1,")
        endif()
        foreach(line "pub const CE_A: c_int = 0;" "pub const TWICE: c_int = 2;"
                "pub const SPACED: Fwd = Fwd { a: 7 };"
                "pub enum Low: c_longlong\n{\n\tLOW_MIN = -2147483649,"
                "pub enum Fixed\n{" "${huge}" "pub enum TwoA\n{\n\tTWO_A = 0,\n}"
                "pub struct Big align(16)\n{\n\ta: c_int,\n}"
                "pub struct Outer\n{\n\tanon0: Outer_anon0,\n\tnamed: Outer_named,\n\tu: Outer_u,\n}"
                "pub struct Const\n{\n\ts: Const_s,\n\tu: Const_u,\n}"
                "pub extern fn take(o: ?*Outer, c: ?*Const, l: ?*Late, h: Huge, n: ?*byte, p: ?*PackedFirst, t: TwoA, k: c_int);")
            string(FIND "${module}" "${line}" at)
            if(at EQUAL -1)
                message(FATAL_ERROR "sides.anti for ${target} holds no "
                                    "`${line}`\n${module}")
            endif()
        endforeach()
        foreach(gone "pub const USES_GONE" "pub const BIG" "pub struct ConstEnum\n" "pub const KK"
                "pub const USEP" "pub const NO_BRACE" "pub const INT_LIT")
            string(FIND "${module}" "${gone}" at)
            if(NOT at EQUAL -1)
                message(FATAL_ERROR "sides.anti for ${target} holds "
                                    "`${gone}`\n${module}")
            endif()
        endforeach()
        file(READ "${WORK}/out-${target}/shim_sides.c" shim)
        if(NOT shim MATCHES "int st\\(void\\)\n" OR
           NOT shim MATCHES "extern __typeof__\\(inl2\\) inl2;" OR
           shim MATCHES "__typeof__\\(inl\\) inl;")
            message(FATAL_ERROR "shim_sides.c for ${target} wraps another "
                                "set of functions\n${shim}")
        endif()
    endforeach()
elseif(CASE STREQUAL "ast_sides")
    # The sides of the conditions of the reader of a header that only an
    # AST or an output of `clang -E` written by hand reaches, by a clang
    # of the shell: locations of the wrong kinds, a record without an id
    # and one defined twice, a typedef that names a record of no header, a
    # constant without its value, enumerators without a name, a base that
    # does not parse, typedefs without a name or a type, nodes without a
    # kind, functions of no function type and with more parameters than
    # their type, and the lines of `-E` that are no marker, a macro whose
    # parameters no `)` closes, a tab before `{` and a marker of a file
    # whose name ends in a backslash. The resource directory ends in
    # `\r\n`, as on Windows, so the target is one whose headers stand
    # there.
    fake_clang()
    file(WRITE "${WORK}/fake/version" "clang version ${CLANG_VERSION}\nTarget: x\n")
    file(WRITE "${WORK}/fake/clang" "#!/bin/sh
d=`dirname \"$0\"`
for a in \"$@\"; do
    case \"$a\" in
    --version) cat \"$d/version\"; exit 0 ;;
    -print-resource-dir) printf '%s\\r\\n' \"$d\"; exit 0 ;;
    -E) cat \"$d/pre\"; exit 0 ;;
    esac
done
cat \"$d/ast\"
")
    file(WRITE "${WORK}/fake/pre" "# 1 \"${h}\"\n# !\n# a\n#define F(x\n"
         "#define G(x)\n#define  SPACE 1\n#pragma pack\n"
         "#define TABBED (struct Twice)\t{ 2 }\n# 1 \"x\\\"\n")
    set(loc "\"loc\": {\"file\": \"${h}\", \"line\": 1}")
    set(int "\"type\": {\"qualType\": \"int\"}")
    set(field "{\"kind\": \"FieldDecl\", \"name\": \"a\", ${int}}")
    file(WRITE "${WORK}/fake/ast" "{\"kind\": \"TranslationUnitDecl\", \"inner\": [
 {\"kind\": \"TypedefDecl\", \"loc\": {\"file\": 5, \"line\": \"x\"}, \"name\": \"Early\", ${int}},
 {\"kind\": \"RecordDecl\", ${loc}, \"name\": \"NoId\", \"tagUsed\": \"struct\",
  \"completeDefinition\": true, \"inner\": [${field}]},
 {\"kind\": \"RecordDecl\", \"name\": \"Twice\", \"id\": \"0x20\", \"tagUsed\": \"struct\",
  \"completeDefinition\": true, \"inner\": [${field}]},
 {\"kind\": \"RecordDecl\", \"name\": \"Twice\", \"id\": \"0x20\", \"tagUsed\": \"struct\",
  \"completeDefinition\": true, \"inner\": [${field}]},
 {\"kind\": \"TypedefDecl\", \"name\": \"Owns\", \"type\": {\"qualType\": \"struct Missing\"},
  \"inner\": [{\"kind\": \"RecordType\", \"decl\": {\"id\": \"0x99\", \"kind\": \"RecordDecl\", \"name\": \"Missing\"}}]},
 {\"kind\": \"EnumDecl\", \"name\": \"Vals\", \"id\": \"0x30\", \"inner\": [
   {\"kind\": \"EnumConstantDecl\", \"name\": \"V0\", \"inner\": [{\"kind\": \"ConstantExpr\"}]},
   {\"kind\": \"EnumConstantDecl\", \"name\": \"V1\", \"inner\": [{\"kind\": \"ConstantExpr\", \"value\": \"\"}]},
   {\"kind\": \"EnumConstantDecl\"}, {\"name\": \"kindless\"}]},
 {\"kind\": \"EnumDecl\", \"name\": \"Odd\", \"id\": \"0x31\", \"fixedUnderlyingType\": {\"qualType\": \"@@\"},
  \"inner\": [{\"kind\": \"EnumConstantDecl\", \"name\": \"ODD_A\"}]},
 {\"kind\": \"TypedefDecl\", ${int}},
 {\"kind\": \"TypedefDecl\", \"name\": \"Empty\", ${int}, \"inner\": []},
 {\"kind\": \"TypedefDecl\", \"name\": \"NoName\", ${int},
  \"inner\": [{\"kind\": \"RecordType\", \"decl\": {\"id\": \"0x40\"}}]},
 {\"name\": \"kindless\"},
 {\"kind\": \"RecordDecl\", \"tagUsed\": \"struct\", \"completeDefinition\": true, \"id\": \"0x50\",
  \"inner\": [${field}]},
 {\"kind\": \"TypedefDecl\", ${int}},
 {\"kind\": \"FunctionDecl\", \"name\": \"not_fn\", ${int}},
 {\"kind\": \"FunctionDecl\", \"name\": \"extra\", \"type\": {\"qualType\": \"void (int)\"},
  \"inner\": [{\"name\": \"kindless\"},
             {\"kind\": \"ParmVarDecl\", \"name\": \"\", ${int}},
             {\"kind\": \"ParmVarDecl\", \"name\": \"b\", ${int}}]},
 {\"kind\": \"FunctionDecl\", \"name\": \"uses\", \"type\": {\"qualType\": \"void (Owns *, struct NoId *)\"},
  \"inner\": [{\"kind\": \"ParmVarDecl\", \"name\": \"o\", \"type\": {\"qualType\": \"Owns *\"}},
             {\"kind\": \"ParmVarDecl\", \"name\": \"n\", \"type\": {\"qualType\": \"struct NoId *\"}}]}
]}")
    set(fake ${fake} --target linux-x86_64)
    tool_run("" ${fake})
    file(READ "${WORK}/out/fake.anti" module)
    foreach(line "pub enum Vals\n{\n\tV0 = 0,\n\tV1 = 1,\n}" "pub struct NoId\n{"
            "pub struct Twice\n{\n\ta: c_int,\n}\n" "pub extern fn extra(p0: c_int);"
            "pub extern fn uses(o: ?*byte, n: ?*NoId);"
            "pub const TABBED: Twice = Twice { a: 2 };")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "fake.anti holds no `${line}`\n${module}")
        endif()
    endforeach()
    foreach(gone "SPACE" "Odd" "ODD_A" "not_fn")
        string(FIND "${module}" "${gone}" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "fake.anti holds `${gone}`\n${module}")
        endif()
    endforeach()
    string(REGEX REPLACE "[ \n]+" " " said "${tool_stderr}")
    foreach(warning "`Odd` is left out: Anti has no form for its base `@@`"
            "the macro `F` takes arguments" "the macro `G` takes arguments")
        string(FIND "${said}" "${warning}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning `${warning}`:\n${tool_stderr}")
        endif()
    endforeach()
elseif(CASE STREQUAL "ast_refusals")
    # A clang of the shell whose version names no number, whose first
    # line names no version, whose `-E` fails, whose AST has no members
    # and whose `-E` defines names that are no identifier of C. A version
    # without a line end is read as well.
    fake_clang()
    file(WRITE "${WORK}/fake/pre" "# 1 \"${h}\"\n")
    file(WRITE "${WORK}/fake/ast" "{\"kind\": \"TranslationUnitDecl\"}")
    foreach(version "clang version x.1\nTarget: x\n" "Target: x\n")
        file(WRITE "${WORK}/fake/version" "${version}")
        expect_refusal("cannot read the version of .* from `.* --version`, and bind --clang ${clang_where}" ${fake})
    endforeach()
    file(WRITE "${WORK}/fake/version" "clang version ${CLANG_VERSION}")
    tool_run("" ${fake})
    file(READ "${WORK}/out/fake.anti" module)
    if(module MATCHES "pub ")
        message(FATAL_ERROR "an AST without members bound items\n${module}")
    endif()
    file(WRITE "${WORK}/fake/clang" "#!/bin/sh
d=`dirname \"$0\"`
for a in \"$@\"; do
    case \"$a\" in
    --version) cat \"$d/version\"; exit 0 ;;
    -E) exit 1 ;;
    esac
done
cat \"$d/ast\"
")
    expect_refusal("clang could not preprocess the header, status 1" ${fake})
    fake_clang()
    file(WRITE "${WORK}/fake/version" "clang version ${CLANG_VERSION}\nTarget: x\n")
    file(WRITE "${WORK}/fake/ast" "{\"kind\": \"TranslationUnitDecl\"}")
    foreach(pair "a{|#define a{ 1" "A\\$|#define A$")
        string(FIND "${pair}" "|" bar)
        string(SUBSTRING "${pair}" 0 ${bar} name)
        math(EXPR bar "${bar} + 1")
        string(SUBSTRING "${pair}" ${bar} -1 define)
        file(WRITE "${WORK}/fake/pre" "# 1 \"${h}\"\n${define}")
        expect_refusal("names the macro `${name}`, which is no C identifier" ${fake})
    endforeach()
elseif(CASE STREQUAL "macro_sides")
    # The sides of the conditions of the evaluator of a macro: the
    # suffixes and the forms of a literal, a name that starts with `_`,
    # the types of mixed operands, the least long long divided, and the
    # elements of a compound literal that nest, close not, are too many,
    # are too long or fill a bitfield, an unnamed bitfield and a struct.
    # A description gives a tab inside an expression and after an element.
    string(REPEAT "1 + " 40 long)
    file(WRITE "${WORK}/macros.h" "#include <stdint.h>\n"
         "struct Pair { int a; int b; };\nstruct Bits { int x : 3; int y; };\n"
         "struct Gap { int a; int : 3; int b; };\nstruct HasPair { struct Pair p; };\n"
         "#define HEXU 0X1F\n#define EXP 1e3\n#define EXPU 1E3\n#define FSUF 1.5F\n"
         "#define LSUF 5l\n#define BIGD 3000000000\n#define DOT .5\n#define _UNDER 4\n"
         "#define USE_UNDER _UNDER\n#define MIX (1.5 + 1)\n#define FD (1.5f + 2.0)\n"
         "#define MINDIV ((-9223372036854775807LL-1) / 2)\n#define USHR (5u >> 1)\n"
         "#define BAR (_UNDER|1)\n#define LT (1 < 2)\n#define TOOBIG ((int)1e30)\n"
         "#define TOOSMALL ((int)-1e30)\n#define STRADD (\"a\" + 1)\n#define TRAIL 1 x\n"
         "#define NESTED (struct Pair){ (1 + 2), 3 }\n#define UNCLOSED (struct Pair){ 1, 2\n"
         "#define TOO_MANY (struct Pair){ 1, 2, 3 }\n#define BITS (struct Bits){ 1, 2 }\n"
         "#define GAP (struct Gap){ 1, 2, 3 }\n#define HAS (struct HasPair){ 1 }\n"
         "#define LONG_ELEMENT (struct Pair){ ${long}1, 2 }\n"
         "#define UNS (struct Pair){ 3u, 4 }\n#define U32 ((uint32_t)5)\n"
         "#define I16 ((int16_t)-2)\n#define U16 ((uint16_t)70000)\n#define I8 ((int8_t)200)\n"
         "#define U32_SUM ((uint32_t)1 + 1)\n#define INT_FLOAT (1 + 1.5)\n#define INT_STR (1 + \"a\")\n"
         "#define FLOAT_SHIFT (1.5 << 1)\n#define SHIFT_FLOAT (1 << 1.5)\n#define BIG_L 5L\n"
         "#define TRUNC ((int)2.5)\n"
         "void take(struct Pair p, struct Bits b, struct Gap g, struct HasPair h);\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/macros.h"
                --module bindtest.macros -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/macros.anti" module)
    foreach(line "pub const HEXU: c_int = 31;" "pub const EXP: c_double = 1000.0;"
            "pub const EXPU: c_double = 1000.0;" "pub const FSUF: c_float = 1.5;"
            "pub const LSUF: c_long = 5;" "pub const BIGD: c_longlong = 3000000000;"
            "pub const DOT: c_double = 0.5;" "pub const USE_UNDER: c_int = 4;"
            "pub const MIX: c_double = 2.5;" "pub const FD: c_double = 3.5;"
            "pub const MINDIV: c_longlong = -4611686018427387904;"
            "pub const USHR: c_uint = 2;" "pub const BAR: c_int = 5;"
            "pub const NESTED: Pair = Pair { a: 3, b: 3 };"
            "pub const UNS: Pair = Pair { a: 3, b: 4 };" "pub const U32: u32 = 5;"
            "pub const I16: i16 = -2;" "pub const U16: u16 = 4464;" "pub const I8: i8 = -56;"
            "pub const U32_SUM: c_uint = 2;" "pub const INT_FLOAT: c_double = 2.5;"
            "pub const BIG_L: c_long = 5;" "pub const TRUNC: c_int = 2;")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "macros.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
    string(REGEX REPLACE "[ \n]+" " " said "${tool_stderr}")
    foreach(name LT TOOBIG TOOSMALL STRADD TRAIL UNCLOSED TOO_MANY BITS GAP HAS
            LONG_ELEMENT INT_STR FLOAT_SHIFT SHIFT_FLOAT)
        string(FIND "${said}" "the macro `${name}` is no constant" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning on `${name}`:\n${tool_stderr}")
        endif()
    endforeach()
    file(WRITE "${WORK}/tabs.json" "{\"functions\": [], \"structs\": [{\"name\": \"Pair\",
        \"fields\": [{\"name\": \"a\", \"type\": \"int\"}, {\"name\": \"b\", \"type\": \"int\"}]}],
        \"defines\": [{\"name\": \"TABBED\", \"type\": \"INT\", \"value\": \"1 +\\t2\"},
        {\"name\": \"PAIR\", \"type\": \"COLOR\", \"value\": \"CLITERAL(Pair){ 1,\\t2 }\"}]}")
    tool_run("" "${ANTI}" bind "${WORK}/tabs.json" --module bindtest.tabs
                -o "${WORK}/out")
    file(READ "${WORK}/out/tabs.anti" module)
    foreach(line "pub const TABBED: c_int = 3;" "pub const PAIR: Pair = Pair { a: 1, b: 2 };")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "tabs.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
elseif(CASE STREQUAL "enum_declared_first")
    # T270: a declaration of an enum before its definition, which C23
    # allows with a fixed base, is the same enum. The binding holds it
    # once, with its values, and compiles.
    file(WRITE "${WORK}/first.h" "enum E : unsigned char;\n"
         "enum E : unsigned char { E_A, E_B };\nvoid take(enum E e);\n"
         "enum F : int { F_A };\nenum F : int;\nvoid give(enum F f);\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/first.h"
                --module bindtest.first -o "${WORK}/out" --runtime "${RUNTIME}")
    compile_binding(first bindtest.first)
    file(READ "${WORK}/out/first.anti" module)
    if(NOT module MATCHES "pub enum E: c_uchar\n{\n\tE_A = 0,\n\tE_B = 1,\n}")
        message(FATAL_ERROR "first.anti holds no enum E with its values\n${module}")
    endif()
    # A declaration after the definition adds nothing.
    if(NOT module MATCHES "pub enum F\n{\n\tF_A = 0,\n}")
        message(FATAL_ERROR "first.anti holds no enum F with its value\n${module}")
    endif()
elseif(CASE STREQUAL "shift_types")
    # T272: a shift has the type of its left operand after the integer
    # promotions, C11 6.5.7, and the type of the count changes neither its
    # type nor its value.
    file(WRITE "${WORK}/shifts.h" "#define SHR (-8 >> 1u)\n"
         "#define SHL (1 << 2ull)\n#define SHU (1u << 2)\n"
         "#define SHLL (1LL << 40u)\n#define SHC ((unsigned char)200 >> 1u)\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/shifts.h"
                --module bindtest.shifts -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/shifts.anti" module)
    foreach(line "pub const SHR: c_int = -4;" "pub const SHL: c_int = 4;"
            "pub const SHU: c_uint = 4;" "pub const SHLL: c_longlong = 1099511627776;"
            "pub const SHC: c_int = 100;")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "shifts.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
elseif(CASE STREQUAL "cast_types")
    # T273: a cast to a typedef of an arithmetic type is a cast to that
    # type, a typedef of the C library as well as one of the header, and
    # the value takes the width of its type.
    file(WRITE "${WORK}/casts.h" "#include <stddef.h>\n#include <stdint.h>\n"
         "typedef int Count;\n#define BYTE ((uint8_t)259)\n"
         "#define WORD ((int32_t)-1)\n#define SIZE ((size_t)3)\n"
         "#define COUNT ((Count)7)\n#define SUM ((uint8_t)250 + 10)\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/casts.h"
                --module bindtest.casts -o "${WORK}/out" --runtime "${RUNTIME}")
    expect_same_text("the warnings of the casts" "" "${tool_stderr}")
    file(READ "${WORK}/out/casts.anti" module)
    foreach(line "pub const BYTE: u8 = 3;" "pub const WORD: i32 = -1;"
            "pub const SIZE: c_size_t = 3;" "pub const COUNT: c_int = 7;"
            "pub const SUM: c_int = 260;")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "casts.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
elseif(CASE STREQUAL "struct_literal_ranges")
    # T275: an element of a compound literal converts to the type of its
    # field, as an initializer of C does, C11 6.7.9, and wraps at its
    # width. The binding compiles.
    file(WRITE "${WORK}/ranges.h" "struct S { unsigned char x; signed char y; };\n"
         "#define BIG (struct S){ 300, 200 }\n#define NEG (struct S){ -1, -1 }\n"
         "void take(struct S s);\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/ranges.h"
                --module bindtest.ranges -o "${WORK}/out" --runtime "${RUNTIME}")
    compile_binding(ranges bindtest.ranges)
    file(READ "${WORK}/out/ranges.anti" module)
    foreach(line "pub const BIG: S = S { x: 44, y: -56 };"
            "pub const NEG: S = S { x: 255, y: -1 };")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "ranges.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
elseif(CASE STREQUAL "tag_beside_function")
    # A struct whose tag a function of the header has as its name binds
    # with a trailing `_`, so the module declares each name once, by item
    # 18 of "Answers of 2026-11-19".
    tool_run("" "${ANTI}" bind --clang "${BIND}/tag.h" --module bindtest.tag
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/tag.anti" module)
    foreach(line "pub struct stat_\n" "pub extern fn stat(path: ?*byte, out: ?*stat_) -> c_int;")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "tag.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
    compile_binding(tag bindtest.tag)
elseif(CASE STREQUAL "probe_anonymous_member")
    # T438: a record that holds an anonymous member of C11 is probed through
    # each field of the member. C names it as a field of the record, and the
    # module through the field `anon0`. The module compiles and the two
    # probes agree.
    file(WRITE "${WORK}/anon.h" "struct Outer {\n    int a;\n"
         "    struct { int x; int y; };\n    union { float f; int i; };\n"
         "    int z;\n};\nvoid take(struct Outer *o);\n")
    tool_run("" "${ANTI}" bind --clang "${WORK}/anon.h" --module bindtest.anon
                --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    compile_binding(anon bindtest.anon)
    compare_probes(anon "${WORK}")
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
