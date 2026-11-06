# Link tests/std/math.anti, which calls every function of anti.math, for
# TARGET with antic, and refuse an executable that takes a function of a
# libm of the system. The libm of musl stands in the runtime under the
# prefix anti_rt_libm_, by Q2 of "Answers of 2026-11-06" in
# docs/decisions.md, so a C name such as sinf or floorf names the libm of
# Apple, of the UCRT or of musl's libc.a. A Mach-O executable lists its
# imports among its symbols, a COFF one in its table of imports, and a
# static ELF one holds every function it links. Run with cmake -P and
# these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCE    tests/std/math.anti
#   OBJDUMP   llvm-objdump of the runtime archive
#   READOBJ   llvm-readobj of the runtime archive
#   TARGET    the target name
#   LEVEL     optional, the processor level of --cpu
#   WORK      a directory for the executable

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

# The functions of a libm that anti.math stands for, and those LLVM calls
# for an operation that a level has no instruction for.
set(names sin cos tan asin acos atan atan2 sinh cosh tanh sqrt cbrt pow exp
    exp2 log log2 log10 hypot floor ceil round trunc fmod fabs fmin fmax
    copysign fminimum_num fmaximum_num)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/math-${TARGET}")
if("${TARGET}" MATCHES "^windows-")
    string(APPEND exe ".exe")
endif()
set(level "")
if(DEFINED LEVEL AND NOT LEVEL STREQUAL "")
    set(level --cpu "${LEVEL}")
endif()
antic_build(--target "${TARGET}" ${level} --runtime "${RUNTIME}" -o "${exe}"
            "${SOURCE}")
if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "antic wrote no executable for ${TARGET}")
endif()

if("${TARGET}" MATCHES "^windows-")
    set(command "${READOBJ}" --coff-imports "${exe}")
    set(form "Symbol: ([A-Za-z0-9_]+)")
else()
    set(command "${OBJDUMP}" --syms "${exe}")
    set(form " _?([A-Za-z0-9_]+)$")
endif()
execute_process(COMMAND ${command}
    RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${command} failed with ${status}\n${text}${err}")
endif()
string(REPLACE "\n" ";" lines "${text}")
set(taken "")
foreach(line IN LISTS lines)
    if(NOT line MATCHES "${form}")
        continue()
    endif()
    set(symbol "${CMAKE_MATCH_1}")
    foreach(name IN LISTS names)
        if(symbol STREQUAL "${name}" OR symbol STREQUAL "${name}f")
            list(APPEND taken "${symbol}")
        endif()
    endforeach()
endforeach()
if(taken)
    list(REMOVE_DUPLICATES taken)
    list(JOIN taken ", " taken)
    message(FATAL_ERROR "${exe} takes ${taken} of a libm of the system")
endif()
if(NOT text MATCHES "anti_rt_libm_" AND NOT "${TARGET}" MATCHES "^windows-")
    message(FATAL_ERROR "${exe} holds no function of the libm of musl\n${text}")
endif()
