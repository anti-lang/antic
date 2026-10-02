# M03. antic allocates through src/antic/alloc.c alone. That file calls
# malloc, calloc and realloc, writes the message of a failed allocation
# and ends the run with status 70. No other file of src/antic/ does any of
# the three, and the front end includes no header of the IR for its
# memory. The unit tests that link antic_core take their memory from it
# too, so none of their allocations goes unchecked. Run with cmake -P,
# ROOT, the root of the repository, and UNIT_SOURCES, the sources of the
# programs of tests/ that link antic_core joined with |.

set(src "${ROOT}/src/antic")
set(owner "alloc.c")
set(owners "alloc.c" "alloc.h")
set(failures "")

if(NOT EXISTS "${src}/${owner}")
    string(APPEND failures "\nsrc/antic/${owner} does not exist")
endif()

# Set found to every call of malloc, calloc or realloc in path. A comment
# line or a string literal may name a function, and a call stands in code.
# The tests hold Anti sources in literals. file(STRINGS) drops empty lines,
# so a finding names the line by its text rather than by a number.
function(find_allocations path shown_path)
    set(found "")
    file(STRINGS "${path}" lines)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" shown)
        if(line MATCHES "^[ ]*(/\\*|\\*)")
            continue()
        endif()
        string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "\"\"" code "${line}")
        if(code MATCHES "(^|[^a-z_])(malloc|calloc|realloc)\\(")
            string(APPEND found
                "\n${shown_path}: calls ${CMAKE_MATCH_2}: ${shown}")
        endif()
    endforeach()
    set(found "${found}" PARENT_SCOPE)
endfunction()

file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
foreach(source IN LISTS sources)
    if(source IN_LIST owners)
        continue()
    endif()
    find_allocations("${src}/${source}" "${source}")
    string(APPEND failures "${found}")
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" shown)
        if(line MATCHES "^[ ]*(/\\*|\\*)")
            continue()
        endif()
        if(line MATCHES "\"[^\"]*out of memory")
            string(APPEND failures
                "\n${source}: writes the out-of-memory message: ${shown}")
        endif()
        if(line MATCHES "(^|[^a-z_])exit\\(70\\)")
            string(APPEND failures
                "\n${source}: ends the run with status 70: ${shown}")
        endif()
        if(line MATCHES "(^|[^a-z_])exit\\(1\\)")
            string(APPEND failures
                "\n${source}: ends the run with status 1: ${shown}")
        endif()
    endforeach()
endforeach()

# The front end: the lexer, the parser, the checker, the types, the C
# header and the dumps of the tree. None of it reaches the IR.
file(GLOB front RELATIVE "${src}"
    "${src}/lexer*.[ch]" "${src}/parser*.[ch]" "${src}/sema*.[ch]"
    "${src}/types*.[ch]" "${src}/header*.[ch]" "${src}/ast*.[ch]")
foreach(source IN LISTS front)
    file(STRINGS "${src}/${source}" includes REGEX "^#include \"ir[a-z_]*\\.h\"")
    foreach(include IN LISTS includes)
        string(APPEND failures "\n${source}: ${include}")
    endforeach()
endforeach()

if(UNIT_SOURCES STREQUAL "")
    string(APPEND failures "\nUNIT_SOURCES names no source of a unit test")
endif()
string(REPLACE "|" ";" unit_sources "${UNIT_SOURCES}")
foreach(source IN LISTS unit_sources)
    if(source MATCHES "^unit/.*\\.c$")
        find_allocations("${ROOT}/tests/${source}" "tests/${source}")
        string(APPEND failures "${found}")
    endif()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "allocation outside ${owner}:${failures}")
endif()
message(STATUS "every allocation of antic goes through ${owner}")
