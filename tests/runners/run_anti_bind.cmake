# The tests of anti bind. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   WORK      a directory for the output
#   CASE      header, clang, escaped_path, raymath, raylib, api, refusals,
#             api_malformed, clang_malformed, ast_malformed,
#             api_typedef_chain, clang_typedef_chain or clang_of_the_tree
#   SOURCES   tests/clib, for the case header
#   DUMP      tests/dump, the headers that antic --lib writes
#   BIND      tests/bind, the fixtures of the other cases
#   ABI       tests/abi, whose abi_raymath.expected the case raymath reads
#   RAYLIB    the pinned raylib source
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
    # writes for the same module, which tests/dump holds.
    foreach(name geo shapes canvas failing tuples flags variants simdlib
            nested forms never)
        antic_build(-c --runtime "${RUNTIME}" -I "${SOURCES}" -o
                    "${WORK}/${name}.antl"
                    "${SOURCES}/com/example/${name}.anti")
        tool_run("" "${ANTI}" bind --header "${WORK}/${name}.antl" -o "${WORK}/out"
                    --runtime "${RUNTIME}" -I "${SOURCES}")
        file(READ "${WORK}/out/${name}.h" got)
        file(READ "${DUMP}/${name}.h" wanted)
        expect_same_text("${name}.h, against ${DUMP}/${name}.h," "${wanted}"
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
        file(READ "${DUMP}/geo.h" wanted)
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
    # anti.raylib comes from raylib.h of the pinned raylib, the header that
    # raylib compiles. It compiles, names its frameworks and its Linux
    # libraries, turns the colour macros into constants of Color, and its
    # two probes agree.
    tool_run("" "${ANTI}" bind --clang "${RAYLIB}/src/raylib.h" --probe
                -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/raylib.anti" module)
    foreach(line "link framework \"Cocoa\";" "link linux \"X11\";"
            "pub const RAYWHITE: Color = Color { r: 245, g: 245, b: 245, a: 255 };"
            "pub extern fn SetConfigFlags(flags: c_uint);")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "raylib.anti holds no line `${line}`")
        endif()
    endforeach()
    compile_binding(raylib anti.raylib --anti-internal)
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
    # empty parameter list and nine spellings that do not parse. api_aliases
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
    expect_refusal("is clang -1, and bind --clang ${clang_where}" ${fake})
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
    # is a c_int. The reader stops at a depth of 32 and says that the
    # alias it stopped at names itself.
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
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
