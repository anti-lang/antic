# Drive `anti doc` over the fixture of tests/doc and read what it writes.
# Run with cmake -P and these values:
#   ANTI     the anti executable
#   ANTIC    the antic executable
#   RUNTIME  the runtime archive, which holds the library file of anti.lang
#   FIXTURE  the directory that holds src/ and the expected pages
#   WORK     a directory for the pages this run writes
#
# The run covers the doc-equivalence test of docs/tooling.md, which asks
# that `anti doc` on a library file equal `anti doc` on the source for the
# public interface. It also covers the four doc markers, the Markdown
# subset in both forms and the refusal of a library file under --dev.

set(SRC "${FIXTURE}/src")
set(SHAPES "${SRC}/com/example/shapes.anti")
set(APP "${SRC}/com/example/app.anti")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/lib/com/example")

function(run_doc arguments out_status out_text)
    execute_process(
        COMMAND "${ANTI}" doc --runtime "${RUNTIME}" ${arguments}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(same_file got expected what)
    file(READ "${got}" a)
    file(READ "${expected}" b)
    if(NOT a STREQUAL b)
        message(FATAL_ERROR "${what} differs from ${expected}:\n${a}")
    endif()
endfunction()

# User docs from the two sources.
run_doc("-I;${SRC};--work;${WORK}/w1;-o;${WORK}/user;${SHAPES};${APP}"
        status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "doc failed with ${status}\n${text}")
endif()
same_file("${WORK}/user/com.example.shapes.html"
          "${FIXTURE}/shapes.expected.html" "the page of the source")
same_file("${WORK}/user/index.html" "${FIXTURE}/index.expected.html"
          "the index")

# The same two modules as library files. `anti doc` on one equals
# `anti doc` on the source for the public interface.
execute_process(
    COMMAND "${ANTIC}" -c --runtime "${RUNTIME}" -I "${SRC}"
            -o "${WORK}/lib/com/example/shapes.antl" "${SHAPES}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c on shapes failed\n${out}${err}")
endif()
execute_process(
    COMMAND "${ANTIC}" -c --runtime "${RUNTIME}" -I "${SRC}" -I "${WORK}/lib"
            -o "${WORK}/lib/com/example/app.antl" "${APP}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c on app failed\n${out}${err}")
endif()
run_doc("-I;${WORK}/lib;--work;${WORK}/w2;-o;${WORK}/library;${WORK}/lib/com/example/shapes.antl;${WORK}/lib/com/example/app.antl"
        status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "doc on a library file failed with ${status}\n${text}")
endif()
foreach(page com.example.shapes.html com.example.app.html index.html)
    same_file("${WORK}/library/${page}" "${WORK}/user/${page}"
              "the page of the library file ${page}")
endforeach()

# Markdown passes the doc text through unchanged.
run_doc("--markdown;-I;${SRC};--work;${WORK}/w3;-o;${WORK}/md;${SHAPES};${APP}"
        status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "doc --markdown failed with ${status}\n${text}")
endif()
same_file("${WORK}/md/com.example.shapes.md" "${FIXTURE}/shapes.expected.md"
          "the Markdown page")

# An `internal` item stands in the developer docs and not in the user
# docs, which the object model asks for.
file(READ "${WORK}/user/com.example.shapes.html" page)
string(FIND "${page}" "START" at)
if(NOT at LESS 0)
    message(FATAL_ERROR "the user docs carry the internal item")
endif()

# Dev docs carry the private items and the `//#` notes.
run_doc("--dev;-I;${SRC};--work;${WORK}/w4;-o;${WORK}/dev;${SHAPES};${APP}"
        status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "doc --dev failed with ${status}\n${text}")
endif()
same_file("${WORK}/dev/com.example.shapes.html"
          "${FIXTURE}/shapes.dev.expected.html" "the developer's page")

# `--private` keeps the private items in the user docs and writes no
# Internals section, which the `//#` notes fill.
run_doc("--private;-I;${SRC};--work;${WORK}/w5;-o;${WORK}/private;${SHAPES}"
        status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "doc --private failed with ${status}\n${text}")
endif()
file(READ "${WORK}/private/com.example.shapes.html" page)
foreach(name helper secret private_note START)
    string(FIND "${page}" "${name}" at)
    if(at LESS 0)
        message(FATAL_ERROR "--private left out ${name}")
    endif()
endforeach()
string(FIND "${page}" "internals" at)
if(NOT at LESS 0)
    message(FATAL_ERROR "--private wrote an Internals section")
endif()

# A library file holds no private item and no note, so both options
# refuse one rather than write a page that is short of them.
run_doc("--dev;--work;${WORK}/w6;-o;${WORK}/refused;${WORK}/lib/com/example/shapes.antl"
        status text)
if(status EQUAL 0)
    message(FATAL_ERROR "--dev took a library file")
endif()
string(FIND "${text}" "needs the source" at)
if(at LESS 0)
    message(FATAL_ERROR "--dev on a library file said: ${text}")
endif()
