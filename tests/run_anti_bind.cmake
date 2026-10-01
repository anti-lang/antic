# The tests of anti bind. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   WORK      a directory for the output
#   CASE      header, clang, escaped_path, raymath, raylib, api, refusals,
#             api_malformed, clang_malformed or ast_malformed
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

# anti bind takes the pinned clang of build/deps beside its tree, and
# otherwise the first clang on PATH. A tree configured with a pinned clang
# elsewhere, as on the test machines, puts that one first on PATH.
if(CLANG_DIR)
    file(TO_NATIVE_PATH "${CLANG_DIR}/bin" clang_bin)
    if(WIN32)
        set(ENV{PATH} "${clang_bin};$ENV{PATH}")
    else()
        set(ENV{PATH} "${clang_bin}:$ENV{PATH}")
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

# What the refusal of a missing clang or of another major version names: the
# major of the pinned clang, its archive for this host and the page of its
# release. The page holds dots, which stand for themselves.
string(REGEX MATCH "^[0-9]+" clang_major "${CLANG_VERSION}")
string(REPLACE "." "\\." clang_page "${CLANG_PAGE}")
string(REPLACE "." "\\." clang_asset "clang-${CLANG_TAG}-${HOST}.tar.xz")
set(clang_where "needs clang ${clang_major}\\. Get ${clang_asset} from ${clang_page}, or another clang ${clang_major}, and put it first on PATH")
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")

function(run)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${ARGN} failed with ${status}\n${out}${err}")
    endif()
    set(run_err "${err}" PARENT_SCOPE)
endfunction()

# Fail unless file holds the bytes of expected.
function(expect_file file expected)
    file(READ "${file}" got)
    file(READ "${expected}" wanted)
    if(NOT got STREQUAL wanted)
        message(FATAL_ERROR "${file} differs from ${expected}\n${got}")
    endif()
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
    run("${ANTIC}" -c ${ARGN} -I "${WORK}/src" --runtime "${RUNTIME}"
        -o "${WORK}/lib/${dir}/${name}.antl" "${WORK}/src/${dir}/${name}.anti")
endfunction()

# Build the two probes that anti bind wrote, run both and compare their
# output byte for byte. include is the directory of the header.
function(compare_probes name include)
    # The probe of C prints through the streams of C, which Windows opens
    # in text mode, so it takes binary_stdio.h as every C test file does.
    run(${CC} ${ANTIC_C_WARNINGS} -std=c11 -I "${include}" ${HOST_LINK}
        -include "${CMAKE_CURRENT_LIST_DIR}/binary_stdio.h"
        -o "${WORK}/probe_c" "${WORK}/out/probe_${name}.c")
    run("${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
        -I "${WORK}/lib" -o "${WORK}/probe_anti" "${WORK}/out/probe_${name}.anti")
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
    run(${CC} ${ANTIC_C_WARNINGS} -std=c11 -I "${include}" -c
        -o "${WORK}/shim${CMAKE_C_OUTPUT_EXTENSION}"
        "${WORK}/out/shim_${name}.c")
    run("${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
        -I "${WORK}/lib" -o "${WORK}/calls" "${program}"
        "${WORK}/shim${CMAKE_C_OUTPUT_EXTENSION}")
    program_output(got status "${WORK}/calls.out" "${WORK}/calls")
    file(READ "${expected}" wanted HEX)
    if(NOT status EQUAL 0 OR NOT got STREQUAL wanted)
        file(READ "${WORK}/calls.out" text)
        message(FATAL_ERROR "the calls of ${name} printed\n${text}")
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
            nested)
        run("${ANTIC}" -c --runtime "${RUNTIME}" -I "${SOURCES}"
            -o "${WORK}/${name}.antl" "${SOURCES}/com/example/${name}.anti")
        run("${ANTI}" bind --header "${WORK}/${name}.antl" -o "${WORK}/out"
            --runtime "${RUNTIME}" -I "${SOURCES}")
        file(READ "${WORK}/out/${name}.h" got)
        file(READ "${DUMP}/${name}.h" wanted)
        if(NOT got STREQUAL wanted)
            message(FATAL_ERROR "${name}.h differs from ${DUMP}/${name}.h\n${got}")
        endif()
    endforeach()
elseif(CASE STREQUAL "clang")
    # tests/bind/layout.h holds one of each form. The module, the shim and
    # the warnings are fixed, the module compiles, the two probes agree and
    # the inline functions answer through the shim.
    run("${ANTI}" bind --clang "${BIND}/layout.h" --module bindtest.layout
        --probe -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${run_err}")
    expect_file("${WORK}/out/layout.anti" "${BIND}/layout.anti.expected")
    expect_file("${WORK}/out/shim_layout.c" "${BIND}/shim_layout.c.expected")
    expect_file("${WORK}/warnings" "${BIND}/layout.warnings")
    compile_binding(layout bindtest.layout)
    compare_probes(layout "${BIND}")
    expect_calls(layout "${BIND}" "${BIND}/layout_calls.anti"
                 "${BIND}/layout_calls.expected")
elseif(CASE STREQUAL "escaped_path")
    # A path given with a backslash, as every native path of Windows is.
    # clang escapes it in the line markers of `-E`, and the macros of the
    # header are still its own. The binding equals that of the case clang.
    # A directory of POSIX may hold a backslash in its name, so the same
    # path stands on every host.
    # file() reads a backslash as a separator, so a POSIX host makes the
    # directory and the copy with its own commands.
    if(WIN32)
        file(MAKE_DIRECTORY "${WORK}/escaped")
        file(COPY_FILE "${BIND}/layout.h" "${WORK}/escaped/layout.h")
        file(TO_NATIVE_PATH "${WORK}/escaped/layout.h" header)
    else()
        set(dir "${WORK}/back\\slash")
        set(header "${dir}/layout.h")
        run(mkdir -p "${dir}")
        run(cp "${BIND}/layout.h" "${header}")
    endif()
    run("${ANTI}" bind --clang "${header}" --module bindtest.layout
        -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/warnings" "${run_err}")
    expect_file("${WORK}/out/layout.anti" "${BIND}/layout.anti.expected")
    expect_file("${WORK}/warnings" "${BIND}/layout.warnings")
elseif(CASE STREQUAL "raymath")
    # raymath of the pinned raylib, read through clang. Its probe agrees
    # with C, and the functions of tests/abi/abi_raymath.anti called
    # through the generated binding and its shim print what the
    # hand-written binding prints.
    run("${ANTI}" bind --clang "${RAYLIB}/src/raymath.h"
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
    run("${ANTI}" bind --clang "${RAYLIB}/src/raylib.h" --probe
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
    run("${ANTI}" bind "${BIND}/raylib_api.json" -o "${WORK}/out")
    expect_file("${WORK}/out/raylib.anti" "${BIND}/raylib.anti.expected")
    compile_binding(raylib anti.raylib --anti-internal)
    expect_refusal("raylib_api.json is not JSON: .* at line 4699, column 115"
        "${ANTI}" bind "${RAYLIB}/tools/rlparser/output/raylib_api.json"
        -o "${WORK}/pinned")
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
    run("${ANTI}" bind "${api}" --probe -o "${WORK}/out")
    string(REGEX REPLACE "[ \n]+" " " said "${run_err}")
    foreach(warning "the define `MINUS` is skipped" "the define `OPEN` is skipped"
            "the define `NEG` is skipped" "the define `HUGE` is skipped"
            "`g` is left out")
        string(FIND "${said}" "${warning}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning `${warning}`:\n${run_err}")
        endif()
    endforeach()
    string(FIND "${said}" "the define `SMALL` is skipped" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "the define `SMALL` was skipped:\n${run_err}")
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
    # S32. An enumerator without a value after INT64_MAX. The next value
    # has the bits of 2^63, as a value written above INT64_MAX has.
    string(APPEND text "enum Wide : unsigned long long "
           "{ WIDE_TOP = 0x7fffffffffffffff, WIDE_NEXT };\n"
           "void wide(enum Wide w);\n")
    # S31. The last line C allows a #line to name, and lines after it,
    # which a line counter of 32 bits took past its range.
    string(APPEND text "#line 2147483647\n#define LATE 1\n"
           "struct Late { int a; };\nvoid late(struct Late *p);\n")
    file(WRITE "${h}" "${text}")
    run("${ANTI}" bind --clang "${h}" --module bindtest.malformed
        -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/malformed.anti" module)
    foreach(line "pub extern fn f(m: Mode);" "pub const F: c_int = 1;"
            "pub const CHAIN5000: c_int = 1;" "_f1\n{\n\ta: c_int," "_f2\n{\n\tb: c_float,"
            "WIDE_NEXT = -9223372036854775808," "pub const LATE: c_int = 1;"
            "pub extern fn late(p: ?*Late);")
        string(FIND "${module}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "malformed.anti holds no line `${line}`\n${module}")
        endif()
    endforeach()
    string(REGEX REPLACE "[ \n]+" " " said "${run_err}")
    foreach(warning "the macro `MINUS` is no constant"
            "the macro `OPEN` is no constant" "the macro `CHAIN0` is no constant")
        string(FIND "${said}" "${warning}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "no warning `${warning}`:\n${run_err}")
        endif()
    endforeach()
elseif(CASE STREQUAL "ast_malformed")
    # A clang that writes a version out of range, an AST cut short or
    # nested deep, an AST whose values have the wrong kinds and a chain
    # of typedefs that the dump of a real clang nests. The copy of anti
    # stands outside the checkout, so it runs the clang of fake.
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
    set(fake "${CMAKE_COMMAND}" -E env "PATH=${WORK}/fake:$ENV{PATH}"
        "${WORK}/bin/anti" bind --clang "${h}" --module bindtest.fake
        -o "${WORK}/out" --runtime "${RUNTIME}")
    file(WRITE "${WORK}/fake/version"
         "clang version 99999999999999999999.1.0\nTarget: x\n")
    expect_refusal("is clang -1, and bind --clang ${clang_where}" ${fake})
    file(WRITE "${WORK}/fake/version" "clang version 23.1.0\nTarget: x\n")
    # S31. A line marker past the lines clang counts, one that no integer
    # holds, and one at the end of int64_t with a line after it.
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
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
