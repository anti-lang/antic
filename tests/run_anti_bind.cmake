# The tests of anti bind. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   WORK      a directory for the output
#   CASE      header, clang, raymath, raylib, api or refusals
#   SOURCES   tests/clib, for the case header
#   DUMP      tests/dump, the headers that antic --lib writes
#   BIND      tests/bind, the fixtures of the other cases
#   ABI       tests/abi, whose abi_raymath.expected the case raymath reads
#   RAYLIB    the pinned raylib source
#   CC        the C compiler of the build, with its options

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

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
    run(${CC} -std=c11 -I "${include}" -o "${WORK}/probe_c"
        "${WORK}/out/probe_${name}.c")
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
    run(${CC} -std=c11 -I "${include}" -c -o "${WORK}/shim${CMAKE_C_OUTPUT_EXTENSION}"
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
    foreach(name geo shapes canvas failing tuples flags variants simdlib)
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
    file(WRITE "${WORK}/raymath_calls.expected" "${printed}\n")
    expect_calls(raymath "${RAYLIB}/src" "${BIND}/raymath_calls.anti"
                 "${WORK}/raymath_calls.expected")
elseif(CASE STREQUAL "raylib")
    # anti.raylib comes from raylib.h of the pinned raylib, the header that
    # raylib compiles. It compiles, names its frameworks, turns the colour
    # macros into constants of Color, and its two probes agree.
    run("${ANTI}" bind --clang "${RAYLIB}/src/raylib.h" --probe
        -o "${WORK}/out" --runtime "${RUNTIME}")
    file(READ "${WORK}/out/raylib.anti" module)
    foreach(line "link framework \"Cocoa\";"
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
    expect_refusal("clang 19, and anti bind was tested against clang 23 only"
        "${CMAKE_COMMAND}" -E env "PATH=${WORK}/fake:$ENV{PATH}"
        "${WORK}/bin/anti" bind --clang "${BIND}/layout.h" -o "${WORK}/out"
        --runtime "${RUNTIME}")
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
