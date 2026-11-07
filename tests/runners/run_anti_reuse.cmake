# Drive `anti build` or `anti test` twice in a copy of
# tests/anti-test/project and check which files of the build directory of
# the mode the second run wrote again. By Q2 of "Answers of 2026-11-06,
# fourth set" in docs/decisions.md, a run reuses what an earlier run of
# the same mode compiled, whenever its input did not change. Run with
# cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   PROJECT   tests/anti-test/project
#   WORK      a directory this run writes into
#   TOOL      build or test
#   MODE      dev or release
#   CASE      one of the cases below
#   HOST      the target of the host, as macos-arm64
#
# Cases, each between the first run and the second:
#   unchanged  nothing changes, and the second run writes no file
#   source     the body of shapes.area changes. Its own files are written
#              again, and those of shapes.tuning and shapes.geometry stay
#   import     shapes.geometry gains a function. Its files and those of
#              shapes.area, which imports it, are written again, and
#              those of shapes.tuning stay
#   option     the manifest names a version, which every file carries
#   compiler   every key names another version of the compiler
#   native     a native library of the runtime archive gains a member. No
#              module is compiled again, and the program, or each runner
#              of `anti test`, is linked again, by Q2 of "Answers of
#              2026-11-07" in docs/decisions.md. Both runs take a stand-in
#              runtime archive of links into RUNTIME, whose lib/HOST/
#              holds a copy of libraylib.a, or raylib.lib on Windows
#   same_size  the native library of the case native holds a member that
#              the second run gives other bytes of the same length, so the
#              library keeps its size. The program, or each runner, is
#              linked again, by Q4 of "Answers of 2026-11-07, second set"
#   unreadable the native library of the case native cannot be read after
#              the first run. The second run writes no file, since the key
#              takes the digest the first run kept for a library whose
#              size, time and identity did not change. Not on Windows,
#              which keeps no permission bits
#
# A file counts as written when its time of change moved. The run waits
# more than a second before the second run, since CMake reads the time in
# seconds.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(here "${WORK}/project")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${PROJECT}/" DESTINATION "${here}")

# DESIGN: the case native changes a library of the runtime archive, so it
# runs against a stand-in of links, whose library of raylib is a copy of
# its own. The library of every other target stays a link.
set(runtime "${RUNTIME}")
set(native "")
if(CASE MATCHES "^(native|same_size|unreadable)$")
    set(runtime "${WORK}/runtime")
    file(MAKE_DIRECTORY "${runtime}/lib/${HOST}")
    file(GLOB parts RELATIVE "${RUNTIME}" "${RUNTIME}/*")
    foreach(part IN LISTS parts)
        if(NOT part STREQUAL "lib")
            file(CREATE_LINK "${RUNTIME}/${part}" "${runtime}/${part}"
                 SYMBOLIC)
        endif()
    endforeach()
    file(GLOB targets RELATIVE "${RUNTIME}/lib" "${RUNTIME}/lib/*")
    foreach(target IN LISTS targets)
        if(NOT target STREQUAL HOST)
            file(CREATE_LINK "${RUNTIME}/lib/${target}"
                 "${runtime}/lib/${target}" SYMBOLIC)
        endif()
    endforeach()
    set(name "libraylib.a")
    if(HOST MATCHES "^windows-")
        set(name "raylib.lib")
    endif()
    file(GLOB entries RELATIVE "${RUNTIME}/lib/${HOST}"
         "${RUNTIME}/lib/${HOST}/*")
    foreach(entry IN LISTS entries)
        if(entry STREQUAL name)
            file(COPY_FILE "${RUNTIME}/lib/${HOST}/${entry}"
                 "${runtime}/lib/${HOST}/${entry}")
        else()
            file(CREATE_LINK "${RUNTIME}/lib/${HOST}/${entry}"
                 "${runtime}/lib/${HOST}/${entry}" SYMBOLIC)
        endif()
    endforeach()
    set(native "${runtime}/lib/${HOST}/${name}")
    set(LLVM_AR "${RUNTIME}/bin/llvm-ar")
    if(CMAKE_HOST_WIN32)
        set(LLVM_AR "${RUNTIME}/bin/llvm-ar.exe")
    endif()
endif()

# Put the member note.txt with text into the native library.
function(note text)
    file(WRITE "${WORK}/note.txt" "${text}")
    execute_process(COMMAND "${LLVM_AR}" r "${native}" "${WORK}/note.txt"
                    RESULT_VARIABLE code ERROR_VARIABLE ar_err
                    ENCODING NONE)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "llvm-ar failed with ${code}\n${ar_err}")
    endif()
endfunction()

if(CASE STREQUAL "same_size")
    note("a member of no object\n")
    file(SIZE "${native}" size_before)
endif()

set(extra "")
if(MODE STREQUAL "release")
    set(extra --release)
endif()

function(run what)
    anti_run(status out err "${here}" ${TOOL} ${extra}
             --runtime "${runtime}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${out}${err}")
    endif()
endfunction()

# The files of build/<target>/<mode> as a list of `path=time` with the
# path below that directory. The work directory of `anti test`, build/tests,
# is no cache and stays out.
function(snapshot out)
    file(GLOB modes "${here}/build/*/${MODE}")
    list(LENGTH modes count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "build/<target>/${MODE} stands ${count} times")
    endif()
    file(GLOB_RECURSE files RELATIVE "${modes}" "${modes}/*")
    list(SORT files)
    set(list "")
    foreach(file IN LISTS files)
        file(TIMESTAMP "${modes}/${file}" time "%Y%m%d%H%M%S")
        list(APPEND list "${file}=${time}")
    endforeach()
    set(${out} "${list}" PARENT_SCOPE)
    set(mode_dir "${modes}" PARENT_SCOPE)
endfunction()

# The paths of before whose time moved, or that the second run removed,
# and the paths it added, in written.
function(compare before after out)
    set(result "")
    foreach(entry IN LISTS before)
        if(NOT entry IN_LIST after)
            string(REGEX REPLACE "=[0-9]+$" "" path "${entry}")
            list(APPEND result "${path}")
        endif()
    endforeach()
    set(paths "")
    foreach(entry IN LISTS before)
        string(REGEX REPLACE "=[0-9]+$" "" path "${entry}")
        list(APPEND paths "${path}")
    endforeach()
    foreach(entry IN LISTS after)
        string(REGEX REPLACE "=[0-9]+$" "" path "${entry}")
        if(NOT path IN_LIST paths)
            list(APPEND result "${path}")
        endif()
    endforeach()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

# The files of module m that the run caches: its library file and, where
# the run writes them, its object and the files of `anti test`. A dev run
# of `anti test` takes the object of `anti build` for shapes.tuning, which
# declares no test, and its own objects for the two modules it tests.
function(module_files m out)
    set(result "lib/shapes/${m}.antl")
    set(tested OFF)
    if(m STREQUAL "geometry" OR m STREQUAL "area")
        set(tested ON)
    endif()
    if(TOOL STREQUAL "build" AND MODE STREQUAL "dev" AND
       NOT m STREQUAL "main")
        list(APPEND result "obj/shapes/${m}.o")
    endif()
    if(TOOL STREQUAL "test" AND MODE STREQUAL "dev" AND
       m STREQUAL "tuning")
        list(APPEND result "obj/shapes/${m}.o")
    endif()
    if(TOOL STREQUAL "test" AND tested)
        list(APPEND result "obj/shapes/${m}.tests.antl"
             "obj/shapes/${m}.runner")
        if(MODE STREQUAL "dev")
            list(APPEND result "obj/shapes/${m}.tests.o")
        endif()
    endif()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

# Each file of the modules in ARGN stands, and was written again when
# expected is ON or kept when it is OFF.
function(expect_modules written expected)
    foreach(m IN LISTS ARGN)
        module_files(${m} files)
        foreach(file IN LISTS files)
            string(REGEX REPLACE "\\.o$" "${object_suffix}" file "${file}")
            if(NOT EXISTS "${mode_dir}/${file}")
                message(FATAL_ERROR "the run wrote no ${file}")
            endif()
            if(file IN_LIST written AND NOT expected)
                message(FATAL_ERROR "the second run wrote ${file} again")
            endif()
            if(NOT file IN_LIST written AND expected)
                message(FATAL_ERROR "the second run kept ${file}")
            endif()
        endforeach()
    endforeach()
endfunction()

set(object_suffix ".o")
if(CMAKE_HOST_WIN32)
    set(object_suffix ".obj")
endif()

run("the first run")
snapshot(before)
# The second run starts in a later second than the first ended.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.2)
if(CASE STREQUAL "source")
    file(READ "${here}/src/shapes/area.anti" source)
    string(REPLACE "+ tuning.padding();" "+ tuning.padding() + 0;"
           source "${source}")
    file(WRITE "${here}/src/shapes/area.anti" "${source}")
elseif(CASE STREQUAL "import")
    file(APPEND "${here}/src/shapes/geometry.anti"
         "\n/// Half of `n`.\npub fn halved(n: int) -> int\n{\n"
         "\treturn n / 2;\n}\n")
elseif(CASE STREQUAL "option")
    file(WRITE "${here}/anti.toml" "[package]\nversion = \"1.2.3\"\n")
elseif(CASE STREQUAL "compiler")
    file(GLOB_RECURSE keys "${mode_dir}/*.key")
    if(NOT keys)
        message(FATAL_ERROR "the first run wrote no key")
    endif()
    foreach(key IN LISTS keys)
        file(READ "${key}" text)
        string(REGEX REPLACE "^([0-9a-f]+) [^ ]+ " "\\1 0.0.0-older "
               text "${text}")
        file(WRITE "${key}" "${text}")
    endforeach()
    snapshot(before)
elseif(CASE STREQUAL "native")
    # A member the link never takes, so the library still links.
    note("a member of no object\n")
elseif(CASE STREQUAL "same_size")
    note("b member of no object\n")
    file(SIZE "${native}" size_after)
    if(NOT size_after EQUAL size_before)
        message(FATAL_ERROR "the library went from ${size_before} to "
                            "${size_after} bytes")
    endif()
elseif(CASE STREQUAL "unreadable")
    file(CHMOD "${native}" PERMISSIONS OWNER_WRITE)
elseif(NOT CASE STREQUAL "unchanged")
    message(FATAL_ERROR "no case ${CASE}")
endif()
run("the second run")
snapshot(after)
compare("${before}" "${after}" written)

if(CASE STREQUAL "unchanged" OR CASE STREQUAL "unreadable")
    if(written)
        message(FATAL_ERROR "the second run wrote ${written}")
    endif()
    expect_modules("${written}" OFF tuning geometry area main)
elseif(CASE STREQUAL "source")
    expect_modules("${written}" OFF tuning geometry)
    expect_modules("${written}" ON area)
elseif(CASE STREQUAL "import")
    expect_modules("${written}" OFF tuning)
    expect_modules("${written}" ON geometry area)
elseif(CASE STREQUAL "native" OR CASE STREQUAL "same_size")
    # The library files and the objects stay, and the link of each runner
    # of `anti test` is written again.
    foreach(m tuning geometry area main)
        module_files(${m} files)
        foreach(file IN LISTS files)
            string(REGEX REPLACE "\\.o$" "${object_suffix}" file "${file}")
            if(file MATCHES "\\.runner$")
                if(NOT file IN_LIST written)
                    message(FATAL_ERROR "the second run kept ${file}")
                endif()
            elseif(file IN_LIST written)
                message(FATAL_ERROR "the second run wrote ${file} again")
            endif()
        endforeach()
    endforeach()
else()
    expect_modules("${written}" ON tuning geometry area main)
endif()
# A program the build links is written again exactly when a module or
# the runtime archive changed.
if(TOOL STREQUAL "build")
    set(program "project")
    if(CMAKE_HOST_WIN32)
        set(program "project.exe")
    endif()
    if(NOT EXISTS "${mode_dir}/${program}")
        message(FATAL_ERROR "the build wrote no ${program}")
    endif()
    set(kept OFF)
    if(CASE STREQUAL "unchanged" OR CASE STREQUAL "unreadable")
        set(kept ON)
    endif()
    if(kept AND program IN_LIST written)
        message(FATAL_ERROR "the second build linked ${program} again")
    endif()
    if(NOT kept AND NOT program IN_LIST written)
        message(FATAL_ERROR "the second build kept ${program}")
    endif()
endif()
