# Every message of antic and of anti is expected by a test, or stands in
# the list of the open ones. Run with cmake -P and these values:
#   ROOT  the source tree, which holds src/antic, src/anti and tests
#   OPEN  tests/diagnostics-open.txt
#
# The messages are read from the sources:
#   - of src/antic/, the text every call of diagnostics_add,
#     diagnostics_warn, diagnostics_check and diagnostics_doc gives, and
#     the text of every function that hands one on to them
#   - of src/anti/, the text every fprintf, fputs and vfprintf writes to
#     stderr, and the text of every function that hands one on to them
# The expected outputs are read from tests/: every .err, .expected, .pattern
# and .warnings file, every CMake file, and the C files of tests/unit/.
#
# The test fails when
#   - a message has no line in OPEN and no expected output holds it
#   - a line of OPEN names a message that an expected output holds
#   - a line of OPEN names no message of the sources, or stands twice
#   - the text of a call is a variable, in a function TEXTS does not name
#
# DESIGN: a message is its format, as the C source writes it, with the
# pieces of the literal joined. A conditional gives one message per
# branch. The format turns into a pattern in which a conversion matches
# any text of one line. A message of antic must stand whole in an expected
# output: after `error: `, `warning: ` or a `"`, and before the end of the
# line, the ` [name]` of a warning or a `"`. A message of anti has no such
# frame around it, so its pattern is looked for anywhere, and one that ends
# its line must end there. An expected output that holds a part of a message
# does not hold the message.
#
# DESIGN: the runner reads C with regular expressions and no parser. It
# reads the calls of the functions TEXTS names and takes the argument at
# the position given. A call it cannot read fails the test, so a function
# that hands a format on cannot hide its callers.

cmake_minimum_required(VERSION 3.21)

# The functions that take the text of a message:
#   <function>:<file or nothing>:<position of the text>:<position of the stream>[:<prefix>]
# A file limits the row to that file of the directory. The stream, where a
# row has one, must be `stderr`. Positions count from 0. A prefix is the
# text the function writes before every message, which the message then
# holds as the C source of an fprintf would write it.
set(TEXTS_ANTIC
    "diagnostics_add::3:"
    "diagnostics_warn::4:"
    "diagnostics_check::4:"
    "diagnostics_doc::4:"
    "sema_error_at::2:"
    "sema_check_at::3:"
    "sema_declare::4:"
    "parser_error_here::1:"
    "error_at:lexer.c:3:"
    "number_error:lexer.c:4:"
    "arm_body:parser_stmt.c:1:")
set(TEXTS_ANTI
    "say_anti::0::anti: "
    "fprintf::1:0"
    "vfprintf::1:0"
    "fputs::0:1"
    "bind_warn::1:"
    "refuse:bindclang.c:1:"
    "fail:jsontree.c:1:")
# A text that reaches its call through a table of the source, which no
# call spells: the file, the expression at the call and the literal.
#   <file>|<expression>|<the literal as the source writes it>
set(TEXTS_INDIRECT
    "lexer.c|prefix->refused|`fr\\\"` is not a prefix, write `rf\\\"`")

# A `;`, a `[`, a `]` and a `\` are bytes a CMake list reads, so every
# text is held with four other bytes in their place.
string(ASCII 1 SEMI)
string(ASCII 2 LEFT)
string(ASCII 3 RIGHT)
string(ASCII 4 BS)
# Where a conversion stood in a format, where a message may start in an
# expected output, and two bytes for the steps between.
string(ASCII 5 ANY)
string(ASCII 6 START)
string(ASCII 7 KEEP)
string(ASCII 8 KEEP2)

function(encode out text)
    string(REPLACE ";" "${SEMI}" text "${text}")
    string(REPLACE "[" "${LEFT}" text "${text}")
    string(REPLACE "]" "${RIGHT}" text "${text}")
    string(REPLACE "\\" "${BS}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(decode out text)
    string(REPLACE "${SEMI}" ";" text "${text}")
    string(REPLACE "${LEFT}" "[" text "${text}")
    string(REPLACE "${RIGHT}" "]" text "${text}")
    string(REPLACE "${BS}" "\\" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(read_text out file)
    file(READ "${file}" text)
    encode(text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(STRING "\"([^\"${BS}\n]|${BS}.)*\"")
set(CHAR "'([^'${BS}\n]|${BS}.)*'")

# A C source without its comments. A literal is kept whole, so a `/*`
# inside one starts no comment.
function(read_c out file)
    read_text(text "${file}")
    string(REGEX REPLACE "(${STRING}|${CHAR})|/\\*([^*]|\\*+[^*/])*\\*+/"
           "\\1" text "${text}")
    # Every line starts after a line break, the first one too.
    set(${out} "\n${text}" PARENT_SCOPE)
endfunction()

# The macros that are a literal, which a format may hold between its
# pieces: define_<name> is the text.
function(read_defines)
    foreach(file IN LISTS ARGN)
        read_c(text "${file}")
        set(pieces "(${STRING}([ \t]|${BS}\n)*)+")
        string(REGEX MATCHALL
               "#define[ \t]+[A-Za-z_0-9]+([ \t]|${BS}\n)+${pieces}"
               defines "${text}")
        foreach(define IN LISTS defines)
            string(REGEX MATCH "#define[ \t]+([A-Za-z_0-9]+)" name "${define}")
            set(name "${CMAKE_MATCH_1}")
            string(REGEX MATCHALL "${STRING}" literals "${define}")
            set(value "")
            foreach(literal IN LISTS literals)
                string(REGEX REPLACE "^\"(.*)\"$" "\\1" literal "${literal}")
                string(APPEND value "${literal}")
            endforeach()
            set(define_${name} "${value}" PARENT_SCOPE)
            set(defined_${name} ON PARENT_SCOPE)
        endforeach()
    endforeach()
endfunction()

# The texts an argument gives: one per branch of a conditional. `unreadable`
# is ON for an argument that holds a name that is no macro.
function(texts_of out_texts out_unreadable argument)
    set(texts "")
    set(unreadable OFF)
    set(current "")
    set(has OFF)
    set(unknown OFF)
    set(rest "${argument}")
    while(NOT "${rest}" STREQUAL "")
        set(close OFF)
        if(rest MATCHES "^[ \t\n()]+")
            set(piece "${CMAKE_MATCH_0}")
        elseif(rest MATCHES "^\"(([^\"${BS}\n]|${BS}.)*)\"")
            set(piece "${CMAKE_MATCH_0}")
            string(APPEND current "${CMAKE_MATCH_1}")
            set(has ON)
        elseif(rest MATCHES "^[A-Za-z_][A-Za-z_0-9]*")
            set(piece "${CMAKE_MATCH_0}")
            if(defined_${piece})
                string(APPEND current "${define_${piece}}")
                set(has ON)
            else()
                set(unknown ON)
            endif()
        elseif(rest MATCHES "^[?]")
            # What stood before was the condition.
            set(piece "?")
            set(current "")
            set(has OFF)
            set(unknown OFF)
        elseif(rest MATCHES "^:")
            set(piece ":")
            set(close ON)
        else()
            string(SUBSTRING "${rest}" 0 1 piece)
            set(unknown ON)
        endif()
        string(LENGTH "${piece}" length)
        string(SUBSTRING "${rest}" ${length} -1 rest)
        if("${rest}" STREQUAL "")
            set(close ON)
        endif()
        if(close)
            if(unknown)
                set(unreadable ON)
            elseif(has AND NOT "${current}" STREQUAL "")
                list(APPEND texts "${current}")
            endif()
            set(current "")
            set(has OFF)
            set(unknown OFF)
        endif()
    endwhile()
    set(${out_texts} "${texts}" PARENT_SCOPE)
    set(${out_unreadable} ${unreadable} PARENT_SCOPE)
endfunction()

# The arguments of the call whose `(` stands before `text`: argument_<n>
# and their count.
function(arguments_of out_count text length)
    string(SUBSTRING "${text}" 0 ${length} rest)
    set(depth 0)
    set(count 0)
    set(current "")
    set(closed OFF)
    while(NOT "${rest}" STREQUAL "")
        if(rest MATCHES "^[^\"'(),]+")
        elseif(rest MATCHES "^${STRING}")
        elseif(rest MATCHES "^${CHAR}")
        elseif(rest MATCHES "^[(),]")
        else()
            break()
        endif()
        set(piece "${CMAKE_MATCH_0}")
        string(LENGTH "${piece}" length)
        string(SUBSTRING "${rest}" ${length} -1 rest)
        if("${piece}" STREQUAL "(")
            math(EXPR depth "${depth} + 1")
        elseif("${piece}" STREQUAL ")")
            if(depth EQUAL 0)
                set(closed ON)
                break()
            endif()
            math(EXPR depth "${depth} - 1")
        elseif("${piece}" STREQUAL "," AND depth EQUAL 0)
            set(argument_${count} "${current}" PARENT_SCOPE)
            math(EXPR count "${count} + 1")
            set(current "")
            continue()
        endif()
        string(APPEND current "${piece}")
    endwhile()
    if(NOT closed)
        set(${out_count} -1 PARENT_SCOPE)
        return()
    endif()
    string(STRIP "${current}" current)
    set(argument_${count} "${current}" PARENT_SCOPE)
    math(EXPR count "${count} + 1")
    set(${out_count} ${count} PARENT_SCOPE)
endfunction()

# A format of anti with fewer than two words of its own beside `anti:`
# hands a text on and is no message.
function(is_frame out text)
    string(REGEX REPLACE
           "%[-+ #0]*[0-9*]*([.][0-9*]+)?(hh|h|ll|l|z|j|t)?[diouxXcsfgeEp]"
           " " text "${text}")
    string(REGEX REPLACE "${BS}." " " text "${text}")
    string(REGEX REPLACE "^anti:" "" text "${text}")
    string(REGEX MATCHALL "[A-Za-z]+" words "${text}")
    list(LENGTH words count)
    if(count LESS 2)
        set(${out} ON PARENT_SCOPE)
    else()
        set(${out} OFF PARENT_SCOPE)
    endif()
endfunction()

set(faults "")
# The messages in the order the sources give them, with the file and the
# tool of each.
set(messages "")
set(message_files "")
set(message_tools "")

function(add_message text file tool)
    if(tool STREQUAL "anti")
        is_frame(frame "${text}")
        if(frame)
            return()
        endif()
    endif()
    list(FIND messages "${text}" at)
    if(at EQUAL -1)
        list(APPEND messages "${text}")
        list(APPEND message_files "${file}")
        list(APPEND message_tools "${tool}")
        set(messages "${messages}" PARENT_SCOPE)
        set(message_files "${message_files}" PARENT_SCOPE)
        set(message_tools "${message_tools}" PARENT_SCOPE)
    endif()
endfunction()

# Read the calls of one directory.
function(read_messages tool directory)
    file(GLOB files "${ROOT}/${directory}/*.c")
    list(SORT files)
    # The functions that hand a text on, whatever their file.
    set(known "")
    foreach(row IN LISTS ARGN)
        string(REGEX REPLACE ":.*" "" function "${row}")
        list(APPEND known "${function}")
    endforeach()
    foreach(file IN LISTS files)
        get_filename_component(name "${file}" NAME)
        set(shown "${directory}/${name}")
        set(functions "")
        foreach(row IN LISTS ARGN)
            if(NOT row MATCHES "^([^:]*):([^:]*):([0-9]+):([0-9]*)(:(.*))?$")
                message(FATAL_ERROR "the row `${row}` of TEXTS has another form")
            endif()
            # DESIGN: a group that matched nothing leaves its
            # CMAKE_MATCH_<n> unset, and `if` reads an unset name as its own
            # text. Every group is therefore read inside quotes.
            set(row_file "${CMAKE_MATCH_2}")
            if("${row_file}" STREQUAL "" OR "${row_file}" STREQUAL "${name}")
                list(APPEND functions "${CMAKE_MATCH_1}")
                set(text_at_${CMAKE_MATCH_1} "${CMAKE_MATCH_3}")
                set(stream_at_${CMAKE_MATCH_1} "${CMAKE_MATCH_4}")
                set(prefix_of_${CMAKE_MATCH_1} "${CMAKE_MATCH_6}")
            endif()
        endforeach()
        list(JOIN functions "|" names)
        read_c(rest "${file}")
        set(enclosing "")
        while(rest MATCHES "(^|[^A-Za-z0-9_])(${names})[ \t\n]*\\(")
            set(hit "${CMAKE_MATCH_0}")
            set(function "${CMAKE_MATCH_2}")
            string(FIND "${rest}" "${hit}" at)
            string(LENGTH "${hit}" length)
            math(EXPR after "${at} + ${length}")
            string(SUBSTRING "${rest}" 0 ${after} head)
            string(SUBSTRING "${rest}" ${after} -1 rest)
            # The function the call stands in: the last line that starts
            # with a letter and names a function before its `(`.
            if(head MATCHES "[^${KEEP}]*\n[A-Za-z_][^\n(]*[ *]([A-Za-z_0-9]+)\\(")
                set(enclosing "${CMAKE_MATCH_1}")
            endif()
            # Nearly every call is short, and the usage text is not.
            arguments_of(count "${rest}" 3000)
            if(count LESS 0)
                arguments_of(count "${rest}" 60000)
            endif()
            set(at_text ${text_at_${function}})
            if(count LESS 0)
                string(APPEND faults "  ${shown}: a call of ${function} in "
                       "${enclosing} does not end where the runner reads it\n")
                continue()
            endif()
            if(NOT count GREATER at_text)
                continue()
            endif()
            # A declaration names the type of its parameter.
            if(argument_${at_text} MATCHES "^[ \t\n]*const char")
                continue()
            endif()
            set(at_stream "${stream_at_${function}}")
            if(NOT at_stream STREQUAL "")
                string(STRIP "${argument_${at_stream}}" stream)
                if(NOT stream STREQUAL "stderr")
                    continue()
                endif()
            endif()
            texts_of(texts unreadable "${argument_${at_text}}")
            set(expression "${argument_${at_text}}")
            math(EXPR next "${at_text} + 1")
            if(NOT unreadable AND texts STREQUAL "%s" AND count GREATER next)
                # The format hands its one argument on as the text.
                texts_of(texts unreadable "${argument_${next}}")
                set(expression "${argument_${next}}")
            endif()
            if(unreadable)
                string(REGEX REPLACE "[ \t\n]+" " " expression "${expression}")
                string(STRIP "${expression}" expression)
                set(found OFF)
                foreach(row IN LISTS TEXTS_INDIRECT)
                    encode(row "${row}")
                    if(NOT row MATCHES "^([^|]*)[|]([^|]*)[|](.*)$")
                        message(FATAL_ERROR "a row of TEXTS_INDIRECT has "
                                            "another form")
                    endif()
                    set(literal "${CMAKE_MATCH_3}")
                    if("${CMAKE_MATCH_1}" STREQUAL "${name}" AND
                       "${CMAKE_MATCH_2}" STREQUAL "${expression}")
                        read_c(whole "${file}")
                        string(FIND "${whole}" "\"${literal}\"" there)
                        if(there EQUAL -1)
                            decode(literal "${literal}")
                            string(APPEND faults "  ${shown}: the file holds no "
                                   "\"${literal}\", which TEXTS_INDIRECT "
                                   "names\n")
                        else()
                            add_message("${prefix_of_${function}}${literal}"
                                        "${shown}" ${tool})
                        endif()
                        set(found ON)
                    endif()
                endforeach()
                if(NOT found AND NOT enclosing IN_LIST known)
                    decode(expression "${expression}")
                    string(APPEND faults "  ${shown}: the text of ${function} in "
                           "${enclosing} is `${expression}`, which the runner "
                           "cannot read. Name ${enclosing} in TEXTS\n")
                endif()
            endif()
            foreach(text IN LISTS texts)
                add_message("${prefix_of_${function}}${text}" "${shown}"
                            ${tool})
            endforeach()
        endwhile()
    endforeach()
    set(faults "${faults}" PARENT_SCOPE)
    set(messages "${messages}" PARENT_SCOPE)
    set(message_files "${message_files}" PARENT_SCOPE)
    set(message_tools "${message_tools}" PARENT_SCOPE)
endfunction()

file(GLOB headers "${ROOT}/src/antic/*.h" "${ROOT}/src/antic/*.c"
     "${ROOT}/src/anti/*.h" "${ROOT}/src/anti/*.c" "${ROOT}/src/rt/*.h")
read_defines(${headers})
read_messages(antic src/antic ${TEXTS_ANTIC})
read_messages(anti src/anti ${TEXTS_ANTI})

# The expected outputs, as one text.
set(corpus "\n")
file(GLOB_RECURSE outputs "${ROOT}/tests/*.err" "${ROOT}/tests/*.expected"
     "${ROOT}/tests/*.pattern" "${ROOT}/tests/*.warnings")
foreach(file IN LISTS outputs)
    read_text(text "${file}")
    string(APPEND corpus "${text}\n")
endforeach()
# A CMake file writes a text inside a regular expression, with `\\` before
# a byte the expression would read.
file(GLOB_RECURSE scripts "${ROOT}/tests/*.cmake" "${ROOT}/tests/CMakeLists.txt")
foreach(file IN LISTS scripts)
    read_text(text "${file}")
    string(REGEX REPLACE "${BS}+([^${BS}])" "\\1" text "${text}")
    string(APPEND corpus "${text}\n")
endforeach()
# A unit test writes a text as literals, which the next line may continue.
file(GLOB units "${ROOT}/tests/unit/*.c")
foreach(file IN LISTS units)
    read_text(text "${file}")
    string(REPLACE "${BS}${BS}" "${KEEP}" text "${text}")
    string(REPLACE "${BS}\"" "${KEEP2}" text "${text}")
    string(REGEX REPLACE "\"[ \t\n]*\"" "" text "${text}")
    string(REPLACE "${KEEP2}" "\"" text "${text}")
    string(REPLACE "${KEEP}" "${BS}" text "${text}")
    string(APPEND corpus "${text}\n")
endforeach()
set(framed "${corpus}")
string(REPLACE "\"" "\"${START}" framed "${framed}")
string(REPLACE "error: " "error: ${START}" framed "${framed}")
string(REPLACE "warning: " "warning: ${START}" framed "${framed}")

# Whether an expected output holds the message.
function(expected out text tool)
    # The bytes the C literal stands for.
    string(REPLACE "${BS}${BS}" "${KEEP}" text "${text}")
    string(REPLACE "${BS}n" "\n" text "${text}")
    string(REPLACE "${BS}t" "\t" text "${text}")
    string(REGEX REPLACE "${BS}(.)" "\\1" text "${text}")
    string(REPLACE "${KEEP}" "${BS}" text "${text}")
    # The conversions.
    string(REPLACE "%%" "${KEEP}" text "${text}")
    string(REGEX REPLACE
           "%[-+ #0]*[0-9*]*([.][0-9*]+)?(hh|h|ll|l|z|j|t)?[diouxXcsfgeEp]"
           "${ANY}" text "${text}")
    string(REPLACE "${KEEP}" "%" text "${text}")
    set(ends OFF)
    if(tool STREQUAL "anti")
        if(text MATCHES "\n$")
            set(ends ON)
            string(REGEX REPLACE "\n+$" "" text "${text}")
        endif()
        string(REGEX REPLACE "^${ANY}+" "" text "${text}")
    endif()
    string(REGEX REPLACE "([.^$*+?()|])" "\\\\\\1" text "${text}")
    string(REPLACE "${ANY}" "[^\n]*" text "${text}")
    if(tool STREQUAL "antic")
        string(REPLACE "\"" "\"${START}?" text "${text}")
        string(REPLACE "error: " "error: ${START}?" text "${text}")
        string(REPLACE "warning: " "warning: ${START}?" text "${text}")
        if(framed MATCHES "${START}${text}(\n|\"| ${LEFT}|${BS}n|${SEMI})")
            set(${out} ON PARENT_SCOPE)
        else()
            set(${out} OFF PARENT_SCOPE)
        endif()
        return()
    endif()
    if(ends)
        string(APPEND text "(\n|\"|${BS}n|${SEMI})")
    endif()
    if(corpus MATCHES "${text}")
        set(${out} ON PARENT_SCOPE)
    else()
        set(${out} OFF PARENT_SCOPE)
    endif()
endfunction()

# The lines of OPEN.
set(message_keys "")
foreach(text IN LISTS messages)
    string(REGEX REPLACE "[ \t]+$" "" text "${text}")
    list(APPEND message_keys "${text}")
endforeach()
get_filename_component(open_name "${OPEN}" NAME)
read_text(open_text "${OPEN}")
string(REPLACE "\n" ";" open_lines "${open_text}")
set(open "")
set(number 0)
foreach(line IN LISTS open_lines)
    math(EXPR number "${number} + 1")
    # A line ends where its last byte that is no blank stands, since an
    # editor removes the blanks after it. Two formats of anti end in one.
    string(REGEX REPLACE "[ \t]+$" "" line "${line}")
    if("${line}" STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    list(FIND open "${line}" at)
    decode(shown "${line}")
    if(NOT at EQUAL -1)
        string(APPEND faults "  ${open_name}:${number}: `${shown}` stands twice\n")
        continue()
    endif()
    list(FIND message_keys "${line}" at)
    if(at EQUAL -1)
        string(APPEND faults "  ${open_name}:${number}: `${shown}` is no message "
               "of the sources\n")
        continue()
    endif()
    list(APPEND open "${line}")
    set(open_line_${at} ${number})
endforeach()

set(total 0)
set(covered 0)
set(listed 0)
foreach(tool antic anti)
    set(total_${tool} 0)
    set(listed_${tool} 0)
endforeach()
set(index -1)
foreach(text IN LISTS messages)
    math(EXPR index "${index} + 1")
    list(GET message_files ${index} file)
    list(GET message_tools ${index} tool)
    math(EXPR total "${total} + 1")
    math(EXPR total_${tool} "${total_${tool}} + 1")
    expected(held "${text}" ${tool})
    decode(shown "${text}")
    if(held)
        math(EXPR covered "${covered} + 1")
        if(DEFINED open_line_${index})
            string(APPEND faults "  ${open_name}:${open_line_${index}}: a test "
                   "expects `${shown}` of ${file}, so the line goes\n")
        endif()
    elseif(DEFINED open_line_${index})
        math(EXPR listed "${listed} + 1")
        math(EXPR listed_${tool} "${listed_${tool}} + 1")
    else()
        string(APPEND faults "  ${file}: no test expects `${shown}`, and "
               "${open_name} does not list it\n")
    endif()
endforeach()

if(NOT faults STREQUAL "")
    message(FATAL_ERROR "the messages and the tests that expect them "
                        "disagree:\n${faults}")
endif()
message(STATUS "${total} messages: ${covered} expected by a test, "
               "${listed} open. antic gives ${total_antic} with "
               "${listed_antic} open, anti ${total_anti} with "
               "${listed_anti} open")
