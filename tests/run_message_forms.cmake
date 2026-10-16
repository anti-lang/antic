# The forms of a message of antic. A message at a position of a source is
# `file:line:column: kind: message`, with the name of a warning after it,
# and diagnostics_print of src/antic/diagnostic.c is the one printer that
# writes it. No other file of src/antic/ spells the format of a position.
# The back-end audit found three printers.
# Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

set(failures "")
file(GLOB sources RELATIVE "${ROOT}" "${ROOT}/src/antic/*.c")
foreach(source IN LISTS sources)
    if(source STREQUAL "src/antic/diagnostic.c")
        continue()
    endif()
    file(STRINGS "${ROOT}/${source}" lines REGEX "%s:%d:%d: ")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        string(APPEND failures
               "${source} prints a position of its own: ${line}\n")
    endforeach()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
