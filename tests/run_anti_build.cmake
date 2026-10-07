# Drive `anti build`, `anti run` and `anti new` over the two-module
# project of tests/anti-build/app. Run with cmake -P and these values:
#   ANTI     the anti executable
#   OBJDUMP  llvm-objdump of the pinned release
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds app/ and units/
#   HOST     the target name of this host
#   OTHER    a target name that is not this host's
#   CPU      a processor level of this host's architecture
#   CHECK_PDB tools/check-pdb.cmake
#   READOBJ  llvm-readobj of the runtime archive
#   WORK     a directory this run writes into
#   OBJECT   the suffix of an object of the host, .o or .obj
#
# It covers the dev mode of docs/tooling-addendum.md with its cache,
# release mode, the `-g` rule of docs/tooling.md, --target, --cpu, the
# lock file, `anti run` and the project `anti new` writes.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(project "${WORK}/app")
# A Windows host writes app.exe and its object app.exe.obj, and its
# symbols lie in a PDB, so the map of its archive names no function.
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" DESTINATION "${WORK}")

# The cache of the repositories is the user's, so the run names its own
# and leaves the one of this machine alone.
function(run_anti out_status out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN}
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(build what)
    run_anti(status text ${ARGN} --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${text}")
    endif()
endfunction()

# The dev build of the two modules. The program prints the word of the
# second module and ends with its status.
build("the dev build" build)
set(program "${project}/dist/${HOST}/dev/app${exe}")
if(NOT EXISTS "${program}")
    message(FATAL_ERROR "the dev build wrote no ${program}")
endif()
program_expect("the dev program" COMMAND "${program}" STATUS 7 OUT "one\n")

# The lock file stands beside the manifest, even for a project with no
# dependency.
if(NOT EXISTS "${project}/anti.lock")
    message(FATAL_ERROR "the build wrote no anti.lock")
endif()

# The cache key of a module is the digest of its input, the compiler
# version and the target, so a build that changes nothing writes no
# object again.
set(object "${project}/build/${HOST}/dev/obj/com/example/greet${OBJECT}")
if(NOT EXISTS "${object}")
    message(FATAL_ERROR "the dev build wrote no ${object}")
endif()
file(TIMESTAMP "${object}" before "%Y%m%d%H%M%S" UTC)
file(SHA256 "${object}" digest_before)
build("the second dev build" build)
file(SHA256 "${object}" digest_after)
file(TIMESTAMP "${object}" after "%Y%m%d%H%M%S" UTC)
if(NOT before STREQUAL after OR NOT digest_before STREQUAL digest_after)
    message(FATAL_ERROR "the second build wrote ${object} again")
endif()

# A changed module is compiled again, and the program follows it.
file(READ "${project}/src/com/example/greet.anti" source)
string(REPLACE "return 7;" "return 9;" source "${source}")
file(WRITE "${project}/src/com/example/greet.anti" "${source}")
build("the build after a change" build)
file(SHA256 "${object}" digest_changed)
if(digest_before STREQUAL digest_changed)
    message(FATAL_ERROR "the changed module wrote the same ${object}")
endif()
program_expect("the changed program" COMMAND "${program}" STATUS 9
               OUT "one\n")

# `anti run` builds for the host and runs what it wrote.
run_anti(status text run --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
if(NOT status EQUAL 9 OR NOT text MATCHES "one")
    message(FATAL_ERROR "anti run ended with ${status} and wrote ${text}")
endif()

# Release mode compiles the whole program in one call.
build("the release build" build --release)
set(release "${project}/dist/${HOST}/release/app${exe}")
program_expect("the release program" COMMAND "${release}" STATUS 9
               OUT "one\n")

# A release binary carries no symbol data, so the build writes the
# archive that names it: the same link with the debug sections kept and
# the map of the program, which carries the build id of the binary.
set(archive "${project}/dist/${HOST}/release/app-symbols.zip")
if(NOT EXISTS "${archive}")
    message(FATAL_ERROR "the release build wrote no ${archive}")
endif()
file(REMOVE_RECURSE "${WORK}/symbols")
file(MAKE_DIRECTORY "${WORK}/symbols")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
    WORKING_DIRECTORY "${WORK}/symbols"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the symbols archive did not unpack\n${err}")
endif()
foreach(name app.debug app.map)
    if(NOT EXISTS "${WORK}/symbols/${name}")
        message(FATAL_ERROR "the symbols archive holds no ${name}")
    endif()
endforeach()
# The link with the debug sections is the same program at the same
# addresses, so the map answers for the binary beside it.
program_expect("app.debug" COMMAND "${WORK}/symbols/app.debug" STATUS 9
               OUT "one\n")
file(STRINGS "${release}" lines REGEX "^build [0-9a-f]+$")
string(REGEX MATCH "^build ([0-9a-f]+)$" line "${lines}")
set(id "${CMAKE_MATCH_1}")
file(READ "${WORK}/symbols/app.map" map)
if(NOT map MATCHES "# build ${id}")
    message(FATAL_ERROR "the map names no build id of the program:\n${map}")
endif()
# The debug link carries the id of the program, because the digest leaves
# what -g added out. A trace of the program then finds this archive.
file(STRINGS "${WORK}/symbols/app.debug" lines REGEX "^build [0-9a-f]+$")
string(REGEX MATCH "^build ([0-9a-f]+)$" line "${lines}")
if(NOT CMAKE_MATCH_1 STREQUAL id)
    message(FATAL_ERROR "app.debug carries ${CMAKE_MATCH_1} and the program "
                        "carries ${id}")
endif()
if(HOST MATCHES "^windows-")
    if(NOT EXISTS "${WORK}/symbols/app.pdb")
        message(FATAL_ERROR "the symbols archive holds no app.pdb")
    endif()
    # The PDB of the archive is the one of the shipped program: the GUID
    # of its CodeView record is the GUID the PDB carries.
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DBINARY=${release}"
                            "-DPDB=${WORK}/symbols/app.pdb"
                            "-DREADOBJ=${READOBJ}" -P "${CHECK_PDB}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the PDB of the archive is not the one of "
                            "${release}\n${out}${err}")
    endif()
else()
    # Release mode inlines word into main, which then has no symbol of
    # its own, as the entry on inlined frames in docs/decisions.md says.
    set(names com.example.app.main)
    set(lines "app.anti:[0-9]+")
    foreach(name IN LISTS names)
        if(NOT map MATCHES "${name}")
            message(FATAL_ERROR "the map names no ${name}")
        endif()
    endforeach()
    if(NOT map MATCHES "${lines}")
        message(FATAL_ERROR "the map carries no file and line:\n${map}")
    endif()
endif()

# The little-endian word of four bytes at byte offset at of hex, a string
# of two hex digits per byte.
function(word out hex at)
    math(EXPR start "${at} * 2")
    set(value "")
    foreach(i 6 4 2 0)
        math(EXPR from "${start} + ${i}")
        string(SUBSTRING "${hex}" ${from} 2 byte)
        string(APPEND value "${byte}")
    endforeach()
    math(EXPR value "0x${value}" OUTPUT_FORMAT DECIMAL)
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# The number of entries of a line other than 0 in the subsections of lines
# of the CodeView that `llvm-readobj --codeview --codeview-subsection-bytes`
# printed. A subsection is a header of 12 bytes and blocks of one file
# each: the file, the count of entries and the size of the block, then an
# offset and a word per entry whose low 24 bits are the line. The column
# of characters after the bytes goes first, and so do the brackets of the
# dump, which would hold lines of it together as one element of a list.
function(codeview_lines out dump)
    string(REGEX REPLACE "  \\|[^\n]*" "" dump "${dump}")
    string(REGEX REPLACE "[][;]" "" dump "${dump}")
    string(REPLACE "\n" ";" dump_lines "${dump}")
    set(count 0)
    set(in_lines FALSE)
    set(hex "")
    foreach(line IN LISTS dump_lines ITEMS "SubSectionType: end")
        if(line MATCHES "SubSectionType: ")
            string(LENGTH "${hex}" length)
            math(EXPR size "${length} / 2")
            set(at 12)
            while(in_lines AND at LESS size)
                math(EXPR entries_at "${at} + 4")
                word(entries "${hex}" ${entries_at})
                math(EXPR block_at "${at} + 8")
                word(block "${hex}" ${block_at})
                set(i 0)
                while(i LESS entries)
                    math(EXPR line_at "${at} + 12 + ${i} * 8 + 4")
                    word(value "${hex}" ${line_at})
                    math(EXPR value "${value} & 0xffffff")
                    if(NOT value EQUAL 0)
                        math(EXPR count "${count} + 1")
                    endif()
                    math(EXPR i "${i} + 1")
                endwhile()
                if(block LESS 12)
                    message(FATAL_ERROR "a block of lines of ${block} bytes")
                endif()
                math(EXPR at "${at} + ${block}")
            endwhile()
            set(hex "")
            set(in_lines FALSE)
            if(line MATCHES "Lines \\(0xF2\\)")
                set(in_lines TRUE)
            endif()
        elseif(in_lines AND line MATCHES "^ *[0-9A-F]+: ([0-9A-F ]+)$")
            string(REPLACE " " "" bytes "${CMAKE_MATCH_1}")
            string(APPEND hex "${bytes}")
        endif()
    endforeach()
    set(${out} ${count} PARENT_SCOPE)
endfunction()

# `anti build` passes -g in dev mode and never in release, so the object
# of a dev build holds a line table: a section `debug_line` of DWARF, or
# entries of lines other than 0 in the CodeView of `.debug$S` on Windows.
# A COFF object holds a subsection of lines for every function without -g
# as well, whose entries name line 0, since LLVM writes the record that
# names a function in the PDB only for a function with a location. The
# entry on -g of the LLVM back end in docs/decisions.md says so.
set(readobj_option --sections)
set(line_table "debug_line")
if(HOST MATCHES "^windows-")
    set(readobj_option --codeview --codeview-subsection-bytes)
endif()
foreach(mode dev release)
    execute_process(COMMAND "${READOBJ}" ${readobj_option}
                            "${project}/build/${HOST}/${mode}/app${exe}${OBJECT}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-readobj failed on the ${mode} object\n"
                            "${err}")
    endif()
    set(${mode}_object "${out}")
endforeach()
if(HOST MATCHES "^windows-")
    codeview_lines(dev_lines "${dev_object}")
    codeview_lines(release_lines "${release_object}")
    if(dev_lines EQUAL 0)
        message(FATAL_ERROR "the dev build passed no -g")
    endif()
    if(NOT release_lines EQUAL 0)
        message(FATAL_ERROR "the release build passed -g: "
                            "${release_lines} entries name a line")
    endif()
else()
    if(NOT dev_object MATCHES "${line_table}")
        message(FATAL_ERROR "the dev build passed no -g")
    endif()
    if(release_object MATCHES "${line_table}")
        message(FATAL_ERROR "the release build passed -g")
    endif()
endif()

# --target builds for another target, and --cpu takes a level of this
# host's architecture.
build("the build for ${OTHER}" build --target "${OTHER}")
if(NOT EXISTS "${project}/dist/${OTHER}/dev/app" AND
   NOT EXISTS "${project}/dist/${OTHER}/dev/app.exe")
    message(FATAL_ERROR "the build for ${OTHER} wrote no program")
endif()
build("the build at ${CPU}" build --cpu "${CPU}")

# A level of the other architecture is refused.
run_anti(status text build --cpu nonesuch --runtime "${RUNTIME}"
         --llvm-mc "${LLVM_MC}")
if(status EQUAL 0 OR NOT text MATCHES "no processor level")
    message(FATAL_ERROR "--cpu nonesuch was taken: ${text}")
endif()

# `anti new` writes a project of the default layout that builds and runs.
file(MAKE_DIRECTORY "${WORK}/new")
execute_process(
    COMMAND "${ANTI}" new com.example.demo
    WORKING_DIRECTORY "${WORK}/new"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti new failed with ${status}\n${out}${err}")
endif()
foreach(file anti.toml src/com/example/demo.anti)
    if(NOT EXISTS "${WORK}/new/demo/${file}")
        message(FATAL_ERROR "anti new wrote no ${file}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${WORK}/cache"
            "LOCALAPPDATA=${WORK}/cache"
            "${ANTI}" run --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
    WORKING_DIRECTORY "${WORK}/new/demo"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out MATCHES "hello")
    message(FATAL_ERROR "the project of anti new wrote ${out}${err}")
endif()

# A package name of one segment is no module path of a package.
execute_process(
    COMMAND "${ANTI}" new demo
    WORKING_DIRECTORY "${WORK}/new"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES "one segment")
    message(FATAL_ERROR "anti new took a name of one segment: ${out}${err}")
endif()

# --profile-generate and --profile-use reach the release build of a
# program and nothing else. The instrumented program writes its raw
# profile where LLVM_PROFILE_FILE names it, llvm-profdata merges it, and
# the build that uses it warns about no function.
set(LLVM_BIN "${RUNTIME}/bin")
foreach(refused "--profile-generate" "--release;--profile-generate;--lib;static"
        "--profile-use;${WORK}/app.profdata")
    run_anti(status text build ${refused} --runtime "${RUNTIME}"
             --llvm-mc "${LLVM_MC}")
    if(NOT status EQUAL 2 OR NOT text MATCHES
       "--profile-generate and --profile-use build a program with --release")
        message(FATAL_ERROR "anti build ${refused} gave ${status}: ${text}")
    endif()
endforeach()
build("the instrumented build" build --release --profile-generate)
set(ENV{LLVM_PROFILE_FILE} "${WORK}/app.profraw")
program_expect("the instrumented program" COMMAND "${release}" STATUS 9
               OUT "one\n")
unset(ENV{LLVM_PROFILE_FILE})
execute_process(COMMAND "${LLVM_BIN}/llvm-profdata${exe}" merge
                        -o "${WORK}/app.profdata" "${WORK}/app.profraw"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge gave ${status}\n${err}")
endif()
run_anti(status text build --release --profile-use "${WORK}/app.profdata"
         --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
if(NOT status EQUAL 0 OR text MATCHES "warning")
    message(FATAL_ERROR "the build with the profile gave ${status}: ${text}")
endif()
program_expect("the program of the profile" COMMAND "${release}" STATUS 9
               OUT "one\n")

# A release build links through full LTO, so its object is bitcode, and
# --lto none links the program and the runtime as objects. Bitcode starts
# with `BC` 0xC0DE, or on Darwin with the magic of its wrapper.
set(release_object "${project}/build/${HOST}/release/app${exe}.o")
if(HOST MATCHES "^windows-")
    set(release_object "${project}/build/${HOST}/release/app${exe}.obj")
endif()
foreach(mode full none)
    build("the release build of --lto ${mode}" build --release --lto ${mode})
    program_expect("the program of --lto ${mode}" COMMAND "${release}"
                   STATUS 9 OUT "one\n")
    file(READ "${release_object}" magic LIMIT 4 HEX)
    if(mode STREQUAL "full" AND NOT magic MATCHES "^(4243c0de|dec0170b)$")
        message(FATAL_ERROR "the object of --lto full starts with ${magic}")
    elseif(mode STREQUAL "none" AND magic MATCHES "^(4243c0de|dec0170b)$")
        message(FATAL_ERROR "the object of --lto none is bitcode")
    endif()
endforeach()
