# The libm of musl, which the runtime carries for all six targets, so
# every function of anti.math gives the same result on every target. Eddie
# decided this by Q2 of "Answers of 2026-11-06" in docs/decisions.md.
# CMakeLists.txt at the top includes this file before the cross builds of
# the runtime, which compile the files of ANTIC_LIBM_SOURCES at every
# level of every target into anti_rt and its bitcode through
# antic_libm_objects.
#
# The source is the release of musl that tools/sysroot-pins names, the one
# whose headers and libc.a the Linux sysroots hold. antic_get_source of
# get-source.cmake downloads it into build/deps/musl and checks the pinned
# digest.

antic_shared_path(ANTIC_MUSL_DIR "${ANTIC_DEPS_DIR}/musl"
    "the musl source from tools/sysroot-pins")
antic_get_source(ANTIC_MUSL_SOURCE musl "${ANTIC_MUSL_DIR}" SHA256
    src/math/sinf.c PINS sysroot-pins
    FIELDS MUSL_SOURCE_URL MUSL_SOURCE_DIGEST)

# DESIGN: the files of src/math/ of musl that anti.math calls, in f32 and
# f64, and every file they call in turn, and nothing else. Each is the
# generic C of musl, never the file of a processor under src/math/<arch>/,
# so one source gives every target its result. The four exact functions
# of rounding stand here for a level without the instruction, see
# src/rt/libm.c. src/native/libm-names.h renames every function and table
# these files define, and the test libm_symbols reads each object.
set(ANTIC_LIBM_SOURCES
    acos acosf asin asinf atan atan2 atan2f atanf cbrt cbrtf ceil ceilf
    cos cosf cosh coshf exp exp2 exp2f exp2f_data exp_data expf expm1
    expm1f fabs fabsf floor floorf fmod fmodf hypot hypotf log log10
    log10f log2 log2_data log2f log2f_data log_data logf logf_data pow
    pow_data powf powf_data round roundf scalbn sin sinf sinh sinhf sqrt
    sqrt_data sqrtf tan tanf tanh tanhf trunc truncf
    __cos __cosdf __expo2 __expo2f __math_divzero __math_divzerof
    __math_invalid __math_invalidf __math_oflow __math_oflowf __math_uflow
    __math_uflowf __math_xflow __math_xflowf __rem_pio2 __rem_pio2_large
    __rem_pio2f __sin __sindf __tan __tandf)

# The one header of musl that its build generates, bits/alltypes.h, from
# src/native/libm-types.h, in the include directory of the compiles.
set(ANTIC_LIBM_INCLUDE "${CMAKE_BINARY_DIR}/native/libm/include")
configure_file("${PROJECT_SOURCE_DIR}/src/native/libm-types.h"
    "${ANTIC_LIBM_INCLUDE}/bits/alltypes.h" COPYONLY)

# DESIGN: the flags of musl's own build for clang, CFLAGS_C99FSE and the
# -O2 of CFLAGS_AUTO of its configure, with the include directories of
# its Makefile but arch/<arch>: the headers of clang come first, so
# stdint.h and float.h give the widths of each target, and
# ANTIC_LIBM_INCLUDE gives bits/alltypes.h. No header of a system is read,
# so the code of every target comes from the same text. The warnings are
# ANTIC_LIBM_WARNINGS of warnings.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/warnings.cmake")
set(ANTIC_LIBM_FLAGS -std=c99 -nostdinc -ffreestanding
    -fexcess-precision=standard -frounding-math -fno-strict-aliasing
    -D_XOPEN_SOURCE=700 -O2 -Qunused-arguments ${ANTIC_LIBM_WARNINGS}
    "-I${ANTIC_CLANG_RESOURCE_DIR}/include" "-I${ANTIC_LIBM_INCLUDE}"
    "-I${ANTIC_MUSL_SOURCE}/arch/generic" "-I${ANTIC_MUSL_SOURCE}/src/include"
    "-I${ANTIC_MUSL_SOURCE}/src/internal" "-I${ANTIC_MUSL_SOURCE}/include"
    -include "${PROJECT_SOURCE_DIR}/src/native/libm-names.h"
    "-ffile-prefix-map=${ANTIC_MUSL_SOURCE}=."
    "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
    "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")

# antic_libm_objects(<out> <name> <target> <triple> <level> <work>
#                    [MODE <mode>] [RETRIPLE <triple>])
# Compile every file of ANTIC_LIBM_SOURCES for <target> at <level> into
# <work> as libm-<file>.o, the name of its member of anti_rt, and give the
# objects in <out>. MODE compiles bitcode for that
# mode of --lto, and RETRIPLE writes it again for that triple, as the
# bitcode of anti_rt of a Windows target is. Each compile is noted under
# <name> for the test runtime_fp_contract.
#
# DESIGN: the code options of a target come from the runtime of the same
# target: its level, its sections and alignment, the code options of
# Linux, -fPIC on Linux, -fno-auto-import on Windows and ANTIC_FP_OPTIONS.
# The bitcode runs clang in the musl source on the path below it, since
# bitcode records the path of its source as given and the test no_paths
# reads every library of the archive.
function(antic_libm_objects out name target triple level work)
    cmake_parse_arguments(PARSE_ARGV 6 arg "" "MODE;RETRIPLE" "")
    antic_cpu_march(march "${level}")
    antic_runtime_sections(sections "${target}")
    antic_linux_code_options(code_options "${target}")
    set(options "")
    if(target MATCHES "arm64" AND NOT arg_MODE)
        list(APPEND options -falign-functions=16)
    endif()
    if(target MATCHES "^linux-")
        list(APPEND options -fPIC)
    elseif(target MATCHES "^windows-")
        list(APPEND options -fno-auto-import)
    endif()
    if(arg_MODE)
        set(sections -flto=${arg_MODE})
        if(triple MATCHES "-linux-")
            list(APPEND options -fno-asynchronous-unwind-tables
                -fno-unwind-tables)
        endif()
    endif()
    set(objects "")
    foreach(source IN LISTS ANTIC_LIBM_SOURCES)
        set(object "${work}/libm-${source}.o")
        set(compiled "${object}")
        if(arg_RETRIPLE)
            set(compiled "${work}/libm-${source}.gnu.o")
        endif()
        set(command "${CMAKE_C_COMPILER}" --target=${triple}
            ${ANTIC_LIBM_FLAGS} ${sections} ${options} "${march}"
            ${code_options} ${ANTIC_FP_OPTIONS}
            -c "src/math/${source}.c" -o "${compiled}")
        antic_runtime_compiled(${name} "${target}" "${level}" ${command})
        set(retriple "")
        if(arg_RETRIPLE)
            set(retriple COMMAND "${CMAKE_C_COMPILER}" -x ir
                "--target=${arg_RETRIPLE}" -O2 -flto=${arg_MODE}
                ${ANTIC_OVERRIDE_MODULE_WARNINGS}
                -c "${compiled}" -o "${object}")
        endif()
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND ${command}
            ${retriple}
            WORKING_DIRECTORY "${ANTIC_MUSL_SOURCE}"
            DEPENDS "${ANTIC_MUSL_SOURCE}/src/math/${source}.c"
                "${PROJECT_SOURCE_DIR}/src/native/libm-names.h"
                "${ANTIC_LIBM_INCLUDE}/bits/alltypes.h"
            VERBATIM)
        list(APPEND objects "${object}")
    endforeach()
    set(${out} "${objects}" PARENT_SCOPE)
endfunction()
