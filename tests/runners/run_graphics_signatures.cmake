# No raylib type stands in a public signature of anti.graphics, by
# "Standard interfaces" in docs/anti-language-additions.md. The check reads
# the source of the module after its comments are gone. A public item, a
# public field and a function of a table, which `pub`, `abstract`,
# `concrete` and `operator` open, runs to its first `{`, `;` or `=`, and
# none of them names the module `rl`, the name the module gives
# anti.raylib. The fields and the functions that are private may. Run with
# cmake -P and SOURCE, src/std/anti/graphics.anti.

cmake_minimum_required(VERSION 3.21)

string(ASCII 1 SEMI)
file(READ "${SOURCE}" text)
string(REPLACE ";" "${SEMI}" text "${text}")
string(REGEX REPLACE "\n[ \t]*//[^\n]*" "\n" text "\n${text}")
if(NOT text MATCHES "\nimport anti\\.raylib as rl${SEMI}")
    message(FATAL_ERROR "${SOURCE} imports anti.raylib under another name")
endif()
string(REGEX MATCHALL
       "\n[ \t]*(pub|abstract|concrete|operator) [^{=${SEMI}]*" items "${text}")
list(LENGTH items count)
if(count LESS 100)
    message(FATAL_ERROR "${SOURCE} holds ${count} public items, too few")
endif()
set(wrong "")
foreach(item IN LISTS items)
    if(item MATCHES "[^A-Za-z_0-9]rl\\.")
        string(STRIP "${item}" item)
        string(REPLACE "${SEMI}" ";" item "${item}")
        string(APPEND wrong "\n  ${item}")
    endif()
endforeach()
if(wrong)
    message(FATAL_ERROR "a public item of anti.graphics names a raylib type:"
                        "${wrong}")
endif()
message(STATUS "${count} public items of anti.graphics name no raylib type")
