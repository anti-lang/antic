# Every `anti` block of the syntax overview goes through the front end of
# antic, so an example that stops compiling fails the suite. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   OVERVIEW  docs/anti-syntax-overview.md
#   WORK      a directory the run may write into
#
# A block is a mix of items and statements. A line that starts with a
# word that opens an item, such as `fn`, `class` or `import`, begins an
# item, which runs until its braces close on a line that ends in `}` or
# `;`. Every other line is a statement. The items stand at module level
# and the statements go into the body of one function. That function may
# fail when a statement forwards with `try` or writes `fail`.
# `import anti.lang;` stands first unless the block writes it.
#
# Two marks steer a block:
#   ```anti not-built
#       opens a block that shows a feature not built yet. The run skips it
#   <!-- overview: context, docs-style:ignore
#   ```anti
#   ...
#   ```
#   -->
#       hides a block from a reader. Its lines go before the next `anti`
#       block and are split the same way, so they declare the names the
#       example uses without showing them. `docs-style:ignore` keeps the
#       checker off the `!` of the comment.
# A context that no `anti` block follows fails the run.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${OVERVIEW}" content)

# The lines become a list. A `;`, `[` or `]` of the text would split or
# join elements, so each stands as a control character until it is
# written out.
string(ASCII 1 semicolon)
string(ASCII 2 open_bracket)
string(ASCII 3 close_bracket)
string(REPLACE ";" "${semicolon}" content "${content}")
string(REPLACE "[" "${open_bracket}" content "${content}")
string(REPLACE "]" "${close_bracket}" content "${content}")
string(REPLACE "\n" ";" lines "${content}")

set(item_start "^(import|pub|internal|fn|extern|export|struct|packed|union|enum|variant|class|abstract|final|singleton|simd|trace|worker|provides|link|tests|fixtures|constraint|type)( |$)")
set(checked 0)
set(skipped 0)
set(failures "")

# Split `text`, a list of lines, into items and statements, and write the
# module to `path`.
function(write_module text path)
    set(items "")
    set(statements "")
    set(in_item FALSE)
    set(depth 0)
    foreach(line IN LISTS text)
        if(NOT in_item AND line MATCHES "${item_start}")
            set(in_item TRUE)
            set(depth 0)
        endif()
        if(in_item)
            string(APPEND items "${line}\n")
            string(REGEX MATCHALL "{" opened "${line}")
            string(REGEX MATCHALL "}" closed "${line}")
            list(LENGTH opened n_opened)
            list(LENGTH closed n_closed)
            math(EXPR depth "${depth} + ${n_opened} - ${n_closed}")
            string(STRIP "${line}" stripped)
            if(depth EQUAL 0 AND stripped MATCHES "[}${semicolon}]$")
                set(in_item FALSE)
            endif()
        elseif(NOT line STREQUAL "")
            string(APPEND statements "\t${line}\n")
        endif()
    endforeach()
    set(source "")
    if(NOT items MATCHES "(^|\n)import anti\\.lang${semicolon}")
        string(APPEND source "import anti.lang${semicolon}\n\n")
    endif()
    string(APPEND source "${items}")
    if(NOT statements STREQUAL "")
        set(fails "")
        if(statements MATCHES "(^|[^a-z_.])(try [^{]|fail[ ${semicolon}])")
            set(fails " may fail")
        endif()
        string(APPEND source
            "\nfn overview_example() -> int${fails}\n{\n${statements}\treturn 0${semicolon}\n}\n")
    endif()
    string(REPLACE "${semicolon}" ";" source "${source}")
    string(REPLACE "${open_bracket}" "[" source "${source}")
    string(REPLACE "${close_bracket}" "]" source "${source}")
    file(WRITE "${path}" "${source}")
endfunction()

set(number 0)
set(state outside)
set(skip FALSE)
set(context_line 0)
set(context "")
set(block "")
set(block_line 0)
foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    if(state STREQUAL "context")
        if(line STREQUAL "-->")
            set(state outside)
        elseif(NOT line MATCHES "^```")
            list(APPEND context "${line}")
        endif()
    elseif(state STREQUAL "block")
        if(line STREQUAL "```")
            set(state outside)
            if(skip)
                math(EXPR skipped "${skipped} + 1")
            else()
                math(EXPR checked "${checked} + 1")
                set(module "${WORK}/overview_${block_line}.anti")
                write_module("${context};${block}" "${module}")
                execute_process(
                    COMMAND "${ANTIC}" --front-end --tests
                            --runtime "${RUNTIME}" "${module}"
                    RESULT_VARIABLE status
                    OUTPUT_VARIABLE out
                    ERROR_VARIABLE err
                    ENCODING NONE)
                if(NOT status EQUAL 0 OR err MATCHES "warning")
                    string(APPEND failures
                        "\nthe block at line ${block_line} of the overview, written as ${module}:\n${err}")
                endif()
            endif()
            set(context_line 0)
            set(context "")
            set(block "")
        else()
            list(APPEND block "${line}")
        endif()
    elseif(line MATCHES "^<!-- overview: context")
        set(context_line ${number})
        set(state context)
    elseif(line MATCHES "^```anti( not-built)?$")
        set(skip FALSE)
        if(CMAKE_MATCH_1)
            set(skip TRUE)
        endif()
        set(state block)
        math(EXPR block_line "${number} + 1")
    elseif(line MATCHES "^```" AND NOT context_line EQUAL 0)
        message(FATAL_ERROR
            "the context at line ${context_line} of the overview stands before a block that is not `anti`")
    endif()
endforeach()
if(NOT context_line EQUAL 0)
    message(FATAL_ERROR
        "the context at line ${context_line} of the overview has no block after it")
endif()
if(checked EQUAL 0)
    message(FATAL_ERROR "no `anti` block in ${OVERVIEW}")
endif()
if(failures)
    message(FATAL_ERROR "an example of the overview does not compile:${failures}")
endif()
message(STATUS "${checked} blocks compiled, ${skipped} skipped as not built")
