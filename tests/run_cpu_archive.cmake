# The runtime archive holds a runtime for every processor level of every
# target it has one for. A program built with --cpu below the target's
# default links the runtime of its own level, so each level needs its own
# library. Run with cmake -P and these values:
#   RUNTIME   the runtime directory
#   LEVELS    tools/cpu-levels
#   TARGETS   the target names, separated by |

file(STRINGS "${LEVELS}" lines REGEX "^level ")
foreach(line IN LISTS lines)
    string(REPLACE " " ";" fields "${line}")
    list(GET fields 1 name)
    list(GET fields 2 arch)
    list(APPEND levels_${arch} "${name}")
endforeach()

string(REPLACE "|" ";" targets "${TARGETS}")
set(checked 0)
foreach(target IN LISTS targets)
    if(NOT IS_DIRECTORY "${RUNTIME}/lib/${target}")
        continue()
    endif()
    if(target MATCHES "arm64$")
        set(wanted "${levels_arm64}")
    else()
        set(wanted "${levels_x86_64}")
    endif()
    if(target MATCHES "^windows")
        set(library "anti_rt.lib")
    else()
        set(library "libanti_rt.a")
    endif()
    foreach(level IN LISTS wanted)
        if(NOT EXISTS "${RUNTIME}/lib/${target}/${level}/${library}")
            message(FATAL_ERROR
                "the archive has no ${library} for ${target} at ${level}")
        endif()
        math(EXPR checked "${checked} + 1")
    endforeach()
endforeach()
if(checked EQUAL 0)
    message("SKIP: the runtime archive holds no target")
endif()
