# The settings of CMakePresets.json that "Answers of 2026-10-24" in
# docs/decisions.md decides. By Q8 the preset ubsan, which runs
# AddressSanitizer beside UndefinedBehaviorSanitizer, compiles with -g as
# asan does, and its build and test presets set the ASAN_OPTIONS of
# asan's. By Q11, ANTIC_FAULTS is on in the test presets host, asan, ubsan
# and coverage, and never on in release. Run with cmake -P and this value:
#   PRESETS   the file CMakePresets.json

cmake_minimum_required(VERSION 3.21)

file(READ "${PRESETS}" json)

# The member <key> of the entry named <name> of the array <kind>, or
# NOTFOUND when the entry or the member is absent.
function(preset_value out kind name)
    set(value NOTFOUND)
    string(JSON count LENGTH "${json}" "${kind}")
    math(EXPR last "${count} - 1")
    foreach(i RANGE ${last})
        string(JSON entry GET "${json}" "${kind}" ${i} name)
        if(entry STREQUAL name)
            string(JSON value ERROR_VARIABLE missing GET "${json}" "${kind}"
                   ${i} ${ARGN})
            if(missing)
                set(value NOTFOUND)
            endif()
        endif()
    endforeach()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

preset_value(flags configurePresets ubsan cacheVariables CMAKE_C_FLAGS)
separate_arguments(flags UNIX_COMMAND "${flags}")
if(NOT "-g" IN_LIST flags)
    message(FATAL_ERROR "the preset ubsan compiles without -g: ${flags}")
endif()
foreach(kind buildPresets testPresets)
    preset_value(wanted ${kind} asan environment ASAN_OPTIONS)
    preset_value(given ${kind} ubsan environment ASAN_OPTIONS)
    if(wanted STREQUAL "NOTFOUND")
        message(FATAL_ERROR "the entry asan of ${kind} sets no ASAN_OPTIONS")
    endif()
    if(NOT given STREQUAL wanted)
        message(FATAL_ERROR "the entry ubsan of ${kind} sets ASAN_OPTIONS "
                            "to ${given}, and asan sets ${wanted}")
    endif()
endforeach()

foreach(name host asan ubsan coverage)
    preset_value(faults configurePresets ${name} cacheVariables ANTIC_FAULTS)
    if(NOT faults STREQUAL "ON")
        message(FATAL_ERROR "the preset ${name} sets ANTIC_FAULTS to ${faults}")
    endif()
endforeach()
preset_value(faults configurePresets release cacheVariables ANTIC_FAULTS)
if(NOT faults STREQUAL "NOTFOUND" AND faults)
    message(FATAL_ERROR "the preset release turns ANTIC_FAULTS on")
endif()
