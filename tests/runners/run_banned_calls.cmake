# No file of antic, of the runtime or of anti calls a function that rule
# 9 of docs/c-guidelines.md bans: strcpy, strcat, sprintf, vsprintf, gets
# and strncpy, and a function of the scanf family with a bare %s in the
# text of its call. Rule 10 names this test. Run with cmake -P and ROOT,
# the root of the repository.
#
# The check reads the text of each file, comments included, so a comment
# names such a function without the parenthesis of a call. A name counts
# where no letter, digit or `_` stands before it, so snprintf, fgets and
# a name of our own that ends in one pass. Before it reads the tree, the
# runner checks itself on lines that must be refused and lines that must
# pass, so a pattern that finds nothing fails the test.

cmake_minimum_required(VERSION 3.21)

set(banned "strcpy|strcat|sprintf|vsprintf|gets|strncpy")
set(call_pattern "(^|[^A-Za-z0-9_])(${banned})[ \t\r\n]*\\(")
# A call of the scanf family up to the `;` that ends its statement, with
# %s in it. %5s and %[ name a width or a set, and pass.
set(scanf_pattern "(^|[^A-Za-z0-9_])(v?f?s?scanf)[ \t\r\n]*\\([^;]*%s")

# banned_names(<out> <text>): the banned functions <text> calls, one per
# call, in the order they stand. A match that starts at a `;` splits into
# an empty item and the call, and the empty item names nothing.
function(banned_names out text)
    set(names "")
    string(REGEX MATCHALL "${call_pattern}" calls "${text}")
    foreach(call IN LISTS calls)
        string(REGEX MATCH "(${banned})" name "${call}")
        if(NOT name STREQUAL "")
            list(APPEND names "${name}")
        endif()
    endforeach()
    string(REGEX MATCHALL "${scanf_pattern}" calls "${text}")
    foreach(call IN LISTS calls)
        string(REGEX MATCH "v?f?s?scanf" name "${call}")
        list(APPEND names "${name} with %s")
    endforeach()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()

set(failures "")

# The check of the runner itself: each text and the names it must give.
function(self_case text expected)
    banned_names(found "${text}")
    if(NOT "${found}" STREQUAL "${expected}")
        set(failures "${failures}\nthe runner gives [${found}] for [${text}], not [${expected}]"
            PARENT_SCOPE)
    endif()
endfunction()
self_case("    strcpy(to, from);\n" "strcpy")
self_case("    n = sprintf (buffer, \"%d\", x);\n" "sprintf")
self_case("    strcat(\n        a, b);\n" "strcat")
self_case("{gets(line);}" "gets")
self_case("    x = 0;strcpy(a, b);\n" "strcpy")
self_case("    vsprintf(b, f, a); strncpy(a, b, n);\n" "vsprintf;strncpy")
self_case("    sscanf(text, \"%s %d\", name, &n);\n" "sscanf with %s")
self_case("    snprintf(b, n, \"%s\", s);\n" "")
self_case("    vsnprintf(b, n, f, a); fgets(line, n, f);\n" "")
self_case("    my_strcpy(a, b); strcpy_safe(a, b);\n" "")
self_case("    sscanf(text, \"%15s\", name);\n" "")
self_case("    /* strcpy is banned; a copy is memcpy. */\n" "")

# Every C file of the three parts, apart from the one file that defines
# banned functions. platform_stdio.c defines sprintf, vsprintf and the
# scanf family for a Windows target, whose headers declare them and whose
# ucrtbase.dll does not export them, and sprintf calls vsprintf.
set(parts src/antic src/rt src/anti)
set(left_out src/rt/platform_stdio.c)

set(files "")
foreach(part IN LISTS parts)
    file(GLOB_RECURSE part_files RELATIVE "${ROOT}"
        "${ROOT}/${part}/*.c" "${ROOT}/${part}/*.h")
    if(part_files STREQUAL "")
        message(FATAL_ERROR "no file of ${part} under ${ROOT}/${part}")
    endif()
    list(APPEND files ${part_files})
endforeach()
foreach(path IN LISTS left_out)
    if(NOT EXISTS "${ROOT}/${path}")
        string(APPEND failures "\n${path}: left out, and no such file")
    endif()
endforeach()
list(REMOVE_ITEM files ${left_out})
list(SORT files)

foreach(path IN LISTS files)
    file(READ "${ROOT}/${path}" text)
    banned_names(found "${text}")
    foreach(name IN LISTS found)
        string(APPEND failures "\n${path}: calls ${name}")
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "banned calls:${failures}")
endif()
list(LENGTH files count)
message(STATUS "${count} files call no banned function")
