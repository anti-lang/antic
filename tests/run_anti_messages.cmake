# Run one case of a table of the messages of `anti`. Run with cmake -P and
# these values:
#   ANTI     the anti executable
#   TABLE    the table, a CMake file of calls of anti_message
#   CASE     the name of the case to run
#   WORK     a directory this run writes into
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   CLANG_VERSION, CLANG_TAG and CLANG_PAGE
#            the version of the pinned clang, the tag of its release and
#            the page of that release, which a refusal of `anti bind` names
#   HOST     the target name of this host
#   EXE      the suffix of a program of this host
#   VERSION  the version of anti, which tools/version holds
#
# A table holds one call per message:
#   anti_message(<name>
#       [STATUS <n>]              the exit status, 1 without it
#       [FILES <path> <text>...]  the files of the directory anti runs in
#       [DIRECTORIES <path>...]   the empty directories beside them
#       [SETUP <function>]        a function of the table, called after the
#                                 files are written
#       [ENV <name>=<value>...]   variables of the environment of anti
#       [ANY_OUT]                 the standard output is not compared
#       [COPY]                    run a copy of anti in WORK/bin, which has
#                                 no pinned clang beside its tree and takes
#                                 the first clang on PATH
#       [OUT <line>...]           the whole standard output, empty without it
#       ARGS <argument>...        the arguments of anti
#       [ERR <line>...]           the whole standard error, empty without it
#       [ERR_MATCH <pattern>])    instead of ERR, a pattern of the whole
#                                 standard error, for a case in which
#                                 another program writes a line of its own
#
# DESIGN: a case holds the whole standard error and not a pattern of it, so
# it fails for a second message as it fails for another one. Each line of
# ERR is the message as anti writes it, which is the form the test
# diagnostics_covered looks for in this file. anti runs in WORK/project,
# so a message that names a file of the project names it without the
# directory of the build tree. A line names another path through
# ${WORK}, ${PROJECT}, ${USER_CACHE} or ${RUNTIME}.
#
# DESIGN: the name of a case stands in its table alone. With LIST set
# and TABLE, the script runs no case and prints the name of each, one per
# line, and tests/CMakeLists.txt registers one test per name from that.
# Both runs read the table with the functions and the values below, so a
# table is written once.

cmake_minimum_required(VERSION 3.21)

if(LIST)
    # No case runs, so no value of a run is read.
    set(required TABLE)
    set(WORK "work")
else()
    set(required ANTI TABLE CASE WORK)
endif()
foreach(value IN LISTS required)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

set(PROJECT "${WORK}/project")
# The cache of the user, which the environment of every case puts under
# WORK: XDG_CACHE_HOME names it, and LOCALAPPDATA on Windows.
if(CMAKE_HOST_WIN32)
    set(USER_CACHE "${WORK}/cache\\anti\\cache")
else()
    set(USER_CACHE "${WORK}/cache/anti")
endif()

# The file:// URL of a directory: file:///tmp/repo, and file:///C:/repo
# for a Windows path, whose drive follows the third slash.
function(file_url path out)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()

# The lines of a list as one text, each with its line break.
function(lines_of out)
    set(text "")
    foreach(line IN LISTS ARGN)
        string(APPEND text "${line}\n")
    endforeach()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set_property(GLOBAL PROPERTY anti_message_ran OFF)

set_property(GLOBAL PROPERTY anti_message_names "")

function(anti_message name)
    if(LIST)
        get_property(names GLOBAL PROPERTY anti_message_names)
        if(name IN_LIST names)
            message(FATAL_ERROR "${TABLE} holds the case ${name} twice")
        endif()
        if(NOT name MATCHES "^[a-z0-9_]+$")
            message(FATAL_ERROR "${TABLE}: the name `${name}` holds more "
                                "than small letters, digits and `_`")
        endif()
        set_property(GLOBAL APPEND PROPERTY anti_message_names "${name}")
        return()
    endif()
    if(NOT name STREQUAL CASE)
        return()
    endif()
    cmake_parse_arguments(PARSE_ARGV 1 arg "ANY_OUT;COPY"
                          "STATUS;SETUP;ERR_MATCH"
                          "FILES;DIRECTORIES;ENV;OUT;ARGS;ERR")
    if(DEFINED arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${name}: `${arg_UNPARSED_ARGUMENTS}` follows "
                            "no keyword")
    endif()
    if(NOT DEFINED arg_STATUS)
        set(arg_STATUS 1)
    endif()
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${PROJECT}")
    # DESIGN: list(GET) gives an element with its `;`, where list(POP_FRONT)
    # writes the list again and ends a text at its first one.
    list(LENGTH arg_FILES count)
    set(at 0)
    while(at LESS count)
        math(EXPR next "${at} + 1")
        list(GET arg_FILES ${at} path)
        list(GET arg_FILES ${next} text)
        file(WRITE "${PROJECT}/${path}" "${text}")
        math(EXPR at "${at} + 2")
    endwhile()
    foreach(path IN LISTS arg_DIRECTORIES)
        file(MAKE_DIRECTORY "${PROJECT}/${path}")
    endforeach()
    if(DEFINED arg_SETUP)
        cmake_language(CALL ${arg_SETUP})
    endif()
    set(anti "${ANTI}")
    if(arg_COPY)
        get_filename_component(anti "${ANTI}" NAME)
        set(anti "${WORK}/bin/${anti}")
        file(MAKE_DIRECTORY "${WORK}/bin")
        file(COPY_FILE "${ANTI}" "${anti}")
        file(CHMOD "${anti}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                ${arg_ENV}
                "${anti}" ${arg_ARGS}
        WORKING_DIRECTORY "${PROJECT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    # What anti wrote stays beside the project, for the reader of a failure.
    file(WRITE "${WORK}/stdout" "${out}")
    file(WRITE "${WORK}/stderr" "${err}")
    # DESIGN: a case that takes the programs of the system off PATH takes
    # the symbolizer from a sanitizer build of anti as well, and the
    # runtime of the sanitizer then starts with two lines of its own. They
    # are no output of anti and leave before the comparison. Every other
    # line of a sanitizer stays, so a report of one fails the case.
    string(REGEX REPLACE
           "==[0-9]+==WARN: No external symbolizers found[^\n]*\n==[0-9]+==HINT: Is PATH set[^\n]*\n"
           "" err "${err}")
    lines_of(expected_err ${arg_ERR})
    lines_of(expected_out ${arg_OUT})
    if(NOT status STREQUAL arg_STATUS)
        message(FATAL_ERROR "${name}: anti exited with ${status}, and the "
                            "case expects ${arg_STATUS}\n${out}${err}")
    endif()
    if(DEFINED arg_ERR_MATCH)
        if(NOT err MATCHES "^${arg_ERR_MATCH}$")
            message(FATAL_ERROR "${name}: the standard error is\n${err}"
                                "and the case expects a match of\n"
                                "${arg_ERR_MATCH}")
        endif()
    elseif(NOT err STREQUAL expected_err)
        message(FATAL_ERROR "${name}: the standard error is\n${err}"
                            "and the case expects\n${expected_err}")
    endif()
    if(NOT arg_ANY_OUT AND NOT out STREQUAL expected_out)
        message(FATAL_ERROR "${name}: the standard output is\n${out}"
                            "and the case expects\n${expected_out}")
    endif()
    set_property(GLOBAL PROPERTY anti_message_ran ON)
endfunction()

include("${TABLE}")

if(LIST)
    get_property(names GLOBAL PROPERTY anti_message_names)
    list(JOIN names "\n" names)
    # message() writes to the standard error, and the names are the output.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${names}")
    return()
endif()
get_property(ran GLOBAL PROPERTY anti_message_ran)
if(NOT ran)
    message(FATAL_ERROR "${TABLE} holds no case ${CASE}")
endif()
