# Every `anti` block of a document goes through the front end of antic,
# so an example that stops compiling fails the suite. A block that
# declares `main` is built and run, a block marked `refused` must fail
# with the error it names, and an `llvm` or `asm` excerpt must stand in
# what antic writes for the block it names. Run with cmake -P and these
# values:
#   ANTIC         the antic executable
#   RUNTIME       the runtime archive
#   DOCUMENTS     the documents: .md files, and directories whose .md
#                 files are read in the order of their names
#   WORK          a directory the run may write into
#   LLVM_OPT      opt of the pinned tools, for an `llvm` excerpt of a
#                 release build
#   LLVM_MC       llvm-mc of the pinned tools, for an `asm` excerpt
#   LLVM_OBJDUMP  llvm-objdump of the pinned tools, for an `asm` excerpt
#
# A block is a mix of items and statements. A line that starts with a
# word that opens an item, such as `fn`, `class` or `import`, begins an
# item, which runs until its braces close on a line that ends in `}` or
# `;`. Every other line is a statement. The items stand at module level
# and the statements go into the body of one function. That function may
# fail when a statement forwards with `try` or writes `fail`.
# `import anti.lang;` stands first unless the block writes it.
#
# The marks of a block:
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
#       checker off the `!` of the comment. `examples: context` opens the
#       same comment. A context that no `anti` block follows fails the run.
#   //! name: <id>
#       on the first line of an `anti` block names it. The name is a
#       lowercase identifier and becomes the module path of the block, so
#       its functions are `<id>.<function>` in the LLVM text. A name
#       stands once per document, and an excerpt reaches a block by it.
#   ```anti refused
#       opens a block the compiler must refuse. Its line
#       `//! error: <text>` holds a text of the first error, which the run
#       looks for in the first line of antic's standard error that says
#       `error:`. The front end refuses most of them. A block it accepts
#       goes through a release build, as a program when it declares `main`
#       and with -S otherwise, so that a refusal of the lowering or of the
#       whole-program pass counts as well. The `//!` lines of a block are
#       marks for the run and never reach the module.
#   ```text output
#       right after a program holds what it prints. A block whose items
#       declare `fn main` is a program: the run builds it in release
#       mode, runs it and compares what it printed with the `text output`
#       block, or with a bare `text` block, which the overview writes, or
#       with nothing when neither follows. The program exits with 0.
#   ```llvm excerpt=<id> target=<target> mode=<dev|release>
#   ```asm excerpt=<id> target=<target> mode=<dev|release>
#       holds up to 20 lines of what antic writes for the named block,
#       built for the target with `-S --keep-llvm`, and with `--dev` in
#       dev mode: for llvm the module llc receives, and for asm
#       llvm-objdump -d of the object. Each line of the excerpt must stand
#       in that output in the order shown: a line of the excerpt is found
#       in a line of the output, each line after it in the line after
#       that, and a line of `...` stands for any number of left-out lines.
#       Before the comparison, runs of blanks collapse to one space, blank
#       lines to one, and the numbered names of LLVM such as %12, the
#       labels of the assembler such as .LBB0_3 and a number that labels a
#       line are made equal. The named block stands before its first
#       excerpt.
#
# DESIGN: what --keep-llvm keeps, read from driver_compile_llvm of
# src/antic/driver.c and llvm_run of src/antic/llvm_run.c. It keeps
# <output>.ll, the LLVM text antic writes, with <output> the path of -o as
# given, so `-o hello.s` keeps `hello.s.ll`. That text is the module
# before opt in both modes. Dev mode runs no opt and llc reads the text,
# so the `.ll` is the module llc receives. Release mode runs opt over the
# text with `default<O3>` and keeps its result as <output>.bc, the module
# after opt, which llc receives. A release program under the default
# `--lto full` runs opt with `lto-pre-link<O3>` into the object itself,
# writes no `.bc` and runs no llc, since the LTO of lld takes the rest.
# -S links nothing and takes the path of `--lto none` in release mode. An
# `llvm` excerpt therefore reads <output>.ll in dev mode, and in release
# mode the text of <output>.bc, which `opt -S` of the pinned tools prints
# without running a pass. No other pinned tool prints bitcode.
#
# DESIGN: a release excerpt shows the module of the block alone, after
# default<O3> and before any link. A function that opt inlined into every
# caller is gone from it, and so is one that nothing reaches, since a
# whole program keeps every function internal. The LTO link of a release
# program may inline the runtime into what the excerpt shows as a call.
#
# DESIGN: an `asm` excerpt reads llvm-objdump -d of an object that
# llvm-mc assembles from the text llc wrote under -S, with the triple of
# the target from `antic --print-llvm-targets`. Dev mode writes the
# object of a module with `main` on its way to a link that needs the dev
# objects of the standard library, and release mode refuses a module
# without `main` unless -S or -c is given, and -c writes the library
# file alone. -S takes a block with or without `main` in both modes, and
# llc chooses the same instructions for -filetype=asm as for
# -filetype=obj, so the disassembly shows the code of the object.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# The lines become a list. A `;`, `[` or `]` of the text would split or
# join elements, so each stands as a control character until it is
# written out.
string(ASCII 1 semicolon)
string(ASCII 2 open_bracket)
string(ASCII 3 close_bracket)

# Set `out` to `text` with the three characters replaced, so that it can
# be split into a list.
function(escape out text)
    string(REPLACE ";" "${semicolon}" text "${text}")
    string(REPLACE "[" "${open_bracket}" text "${text}")
    string(REPLACE "]" "${close_bracket}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Give `text` back the characters the list form replaced.
function(restore out text)
    string(REPLACE "${semicolon}" ";" text "${text}")
    string(REPLACE "${open_bracket}" "[" text "${text}")
    string(REPLACE "${close_bracket}" "]" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(documents "")
foreach(entry IN LISTS DOCUMENTS)
    if(IS_DIRECTORY "${entry}")
        file(GLOB pages LIST_DIRECTORIES false "${entry}/*.md")
        list(SORT pages)
        list(APPEND documents ${pages})
    else()
        list(APPEND documents "${entry}")
    endif()
endforeach()
if(documents STREQUAL "")
    message(FATAL_ERROR "no document to read in `${DOCUMENTS}`")
endif()

set(item_start "^(import|pub|internal|fn|extern|export|struct|packed|union|enum|variant|class|abstract|final|singleton|simd|trace|worker|provides|link|tests|fixtures|constraint|type)( |$)")
set(checked 0)
set(skipped 0)
set(programs 0)
set(refusals 0)
set(excerpts 0)
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
    restore(source "${source}")
    file(WRITE "${path}" "${source}")
endfunction()

# Build the program `module` into `exe` and run it. It prints `wanted` and
# exits with 0. The program goes through program_expect, which reads what
# it wrote as bytes.
function(run_program module exe wanted)
    execute_process(
        COMMAND "${ANTIC}" --runtime "${RUNTIME}" -o "${exe}" "${module}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR
            "the program ${module} of ${document} does not build:\n${out}${err}")
    endif()
    get_filename_component(label "${exe}" NAME)
    if(wanted STREQUAL "")
        program_expect("${label}" COMMAND "${exe}")
    else()
        program_expect("${label}" COMMAND "${exe}" OUT "${wanted}")
    endif()
endfunction()

# Set `out` to the LLVM triple of `target`, from the table antic prints.
function(target_triple out target)
    get_property(table GLOBAL PROPERTY llvm_targets)
    if("${table}" STREQUAL "")
        execute_process(COMMAND "${ANTIC}" --print-llvm-targets
                        OUTPUT_VARIABLE table RESULT_VARIABLE status
                        ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "antic --print-llvm-targets failed")
        endif()
        set_property(GLOBAL PROPERTY llvm_targets "${table}")
    endif()
    if(NOT table MATCHES "(^|\n)target ${target} [^ ]+ ([^ ]+) ")
        message(FATAL_ERROR "antic compiles for no target `${target}`")
    endif()
    set(${out} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# Build the module `module` of the block named `id` for `target` in
# `mode`, unless this run built it already, and set `out` to the file a
# `kind` excerpt reads: the module llc receives for llvm, the disassembly
# for asm. The files stand in `dir` as <id>.<mode>.<target>.s and beside
# it.
function(excerpt_output out kind id module target mode dir)
    set(base "${dir}/${id}.${mode}.${target}")
    if(NOT EXISTS "${base}.s")
        set(dev "")
        if(mode STREQUAL "dev")
            set(dev --dev)
        endif()
        execute_process(
            COMMAND "${ANTIC}" ${dev} -S --keep-llvm --target "${target}"
                    --runtime "${RUNTIME}" -o "${base}.s" "${module}"
            RESULT_VARIABLE status
            OUTPUT_VARIABLE build_out
            ERROR_VARIABLE build_err
            ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR
                "the block `${id}` of ${document} does not build for ${target} in ${mode} mode:\n${build_out}${build_err}")
        endif()
    endif()
    if(kind STREQUAL "llvm" AND mode STREQUAL "dev")
        set(${out} "${base}.s.ll" PARENT_SCOPE)
        return()
    endif()
    if(kind STREQUAL "llvm")
        if(NOT EXISTS "${base}.opt.ll")
            if(NOT DEFINED LLVM_OPT)
                message(FATAL_ERROR
                    "an llvm excerpt of a release build needs LLVM_OPT")
            endif()
            execute_process(
                COMMAND "${LLVM_OPT}" -S -o "${base}.opt.ll" "${base}.s.bc"
                RESULT_VARIABLE status)
            if(NOT status EQUAL 0)
                message(FATAL_ERROR "opt refuses ${base}.s.bc")
            endif()
        endif()
        set(${out} "${base}.opt.ll" PARENT_SCOPE)
        return()
    endif()
    if(NOT EXISTS "${base}.dis")
        if(NOT DEFINED LLVM_MC OR NOT DEFINED LLVM_OBJDUMP)
            message(FATAL_ERROR
                "an asm excerpt needs LLVM_MC and LLVM_OBJDUMP")
        endif()
        target_triple(triple "${target}")
        execute_process(
            COMMAND "${LLVM_MC}" "-triple=${triple}" -filetype=obj
                    -o "${base}.o" "${base}.s"
            RESULT_VARIABLE status
            OUTPUT_VARIABLE mc_out
            ERROR_VARIABLE mc_err
            ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR
                "llvm-mc refuses ${base}.s:\n${mc_out}${mc_err}")
        endif()
        execute_process(
            COMMAND "${LLVM_OBJDUMP}" -d "${base}.o"
            RESULT_VARIABLE status
            OUTPUT_FILE "${base}.dis"
            ERROR_VARIABLE dump_err
            ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR
                "llvm-objdump refuses ${base}.o:\n${dump_err}")
        endif()
    endif()
    set(${out} "${base}.dis" PARENT_SCOPE)
endfunction()

# Set `out` to the lines of `text` as a list, with the differences an
# excerpt may ignore removed: a run of blanks is one space, a line has no
# blank at either end, a run of blank lines is one, and the numbered
# names of LLVM, the labels of the assembler and a number that labels a
# line are N. Every line starts with the marker L, so that a blank line
# stays an element of the list.
function(normalize_lines out text)
    escape(text "${text}")
    string(REGEX REPLACE "[ \t]+" " " text "${text}")
    string(REGEX REPLACE "(^|\n) " "\\1" text "${text}")
    string(REGEX REPLACE " (\n|$)" "\\1" text "${text}")
    string(REGEX REPLACE "\n\n+" "\n\n" text "${text}")
    string(REGEX REPLACE "%[0-9]+" "%N" text "${text}")
    string(REGEX REPLACE "\\.LBB[0-9]+_[0-9]+" ".LBBN_N" text "${text}")
    string(REGEX REPLACE "(^|\n)[0-9]+:" "\\1N:" text "${text}")
    string(REPLACE "\n" ";L" text "L${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Check that the lines of `excerpt`, a list, stand in the file `path` as
# the header says. Set `out` to "" when they do, and otherwise to the
# first line of the run of lines that does not.
function(excerpt_missing out excerpt path)
    file(READ "${path}" text)
    normalize_lines(output "${text}")
    list(JOIN excerpt "\n" wanted)
    normalize_lines(wanted "${wanted}")
    # The segments: the runs of lines between the `...` lines, each a
    # list of its own.
    set(count 0)
    set(segment_0 "")
    foreach(line IN LISTS wanted)
        if(line STREQUAL "L...")
            if(NOT segment_${count} STREQUAL "")
                math(EXPR count "${count} + 1")
                set(segment_${count} "")
            endif()
        else()
            list(APPEND segment_${count} "${line}")
        endif()
    endforeach()
    if(NOT segment_${count} STREQUAL "")
        math(EXPR count "${count} + 1")
    endif()
    # One pass over the output. The window holds the last lines, as many
    # as the segment has, and the segment is found where every line of
    # the window holds the line of the segment at its place. A segment
    # found earliest leaves the most room for the next.
    set(k 0)
    set(window "")
    foreach(line IN LISTS output)
        if(k EQUAL count)
            break()
        endif()
        list(LENGTH segment_${k} m)
        list(APPEND window "${line}")
        list(LENGTH window w)
        if(w GREATER m)
            list(REMOVE_AT window 0)
            set(w ${m})
        endif()
        if(w LESS m)
            continue()
        endif()
        set(ok TRUE)
        set(i 0)
        foreach(want IN LISTS segment_${k})
            list(GET window ${i} have)
            string(SUBSTRING "${want}" 1 -1 want)
            string(SUBSTRING "${have}" 1 -1 have)
            if(want STREQUAL "")
                if(NOT have STREQUAL "")
                    set(ok FALSE)
                    break()
                endif()
            else()
                string(FIND "${have}" "${want}" at)
                if(at EQUAL -1)
                    set(ok FALSE)
                    break()
                endif()
            endif()
            math(EXPR i "${i} + 1")
        endforeach()
        if(ok)
            math(EXPR k "${k} + 1")
            set(window "")
        endif()
    endforeach()
    if(k LESS count)
        list(GET segment_${k} 0 line)
        string(SUBSTRING "${line}" 1 -1 line)
        restore(line "${line}")
        set(${out} "${line}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

set(document_index 0)
foreach(document IN LISTS documents)
    math(EXPR document_index "${document_index} + 1")
    get_filename_component(stem "${document}" NAME_WE)
    set(dir "${WORK}/${stem}")
    file(MAKE_DIRECTORY "${dir}")
    file(READ "${document}" content)
    escape(content "${content}")
    string(REPLACE "\n" ";" lines "${content}")

    set(number 0)
    set(state outside)
    set(skip FALSE)
    set(refuse FALSE)
    set(context_line 0)
    set(context "")
    set(block "")
    set(block_line 0)
    # The program of the last block, which waits for the `text` block that
    # holds its output, and that output.
    set(pending_module "")
    set(pending_exe "")
    set(wanted "")
    # The header of the excerpt being read.
    set(excerpt_kind "")
    set(excerpt_id "")
    set(excerpt_target "")
    set(excerpt_mode "")
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(state STREQUAL "output")
            if(line STREQUAL "```")
                set(state outside)
                restore(wanted "${wanted}")
                run_program("${pending_module}" "${pending_exe}" "${wanted}")
                set(pending_module "")
                set(wanted "")
            else()
                string(APPEND wanted "${line}\n")
            endif()
            continue()
        endif()
        if(state STREQUAL "excerpt")
            if(NOT line STREQUAL "```")
                list(APPEND block "${line}")
                continue()
            endif()
            set(state outside)
            list(LENGTH block n_lines)
            if(n_lines EQUAL 0)
                message(FATAL_ERROR
                    "the excerpt at line ${block_line} of ${document} is empty")
            endif()
            if(n_lines GREATER 20)
                message(FATAL_ERROR
                    "the excerpt at line ${block_line} of ${document} has ${n_lines} lines, and an excerpt has at most 20")
            endif()
            if(failures)
                message(FATAL_ERROR "an example does not compile:${failures}")
            endif()
            if(NOT DEFINED module_${document_index}_${excerpt_id})
                message(FATAL_ERROR
                    "the excerpt at line ${block_line} of ${document} names the block `${excerpt_id}`, and no `anti` block before it carries that name")
            endif()
            excerpt_output(path "${excerpt_kind}" "${excerpt_id}"
                           "${module_${document_index}_${excerpt_id}}"
                           "${excerpt_target}" "${excerpt_mode}" "${dir}")
            excerpt_missing(missing "${block}" "${path}")
            if(NOT missing STREQUAL "")
                message(FATAL_ERROR
                    "the ${excerpt_kind} excerpt at line ${block_line} of ${document}: the line `${missing}` does not stand in ${path} where the excerpt puts it")
            endif()
            math(EXPR excerpts "${excerpts} + 1")
            set(block "")
            continue()
        endif()
        if(state STREQUAL "outside" AND NOT pending_module STREQUAL "")
            if(line STREQUAL "")
                continue()
            elseif(line MATCHES "^```text( output)?$")
                set(state output)
                continue()
            endif()
            run_program("${pending_module}" "${pending_exe}" "")
            set(pending_module "")
        endif()
        if(state STREQUAL "context")
            if(line STREQUAL "-->")
                set(state outside)
            elseif(NOT line MATCHES "^```")
                list(APPEND context "${line}")
            endif()
        elseif(state STREQUAL "block")
            if(NOT line STREQUAL "```")
                list(APPEND block "${line}")
                continue()
            endif()
            set(state outside)
            if(skip)
                math(EXPR skipped "${skipped} + 1")
                set(context_line 0)
                set(context "")
                set(block "")
                continue()
            endif()
            # The `//!` lines are marks of the run, read here and left
            # out of the module.
            set(name "")
            set(error_text "")
            set(source "")
            set(first TRUE)
            foreach(l IN LISTS block)
                if(l MATCHES "^//! name: (.*)$")
                    set(name "${CMAKE_MATCH_1}")
                    if(NOT first)
                        message(FATAL_ERROR
                            "the name of the block at line ${block_line} of ${document} stands on its first line")
                    endif()
                    if(NOT name MATCHES "^[a-z][a-z0-9_]*$")
                        message(FATAL_ERROR
                            "`${name}` names the block at line ${block_line} of ${document}, and a name is a lowercase identifier")
                    endif()
                elseif(l MATCHES "^//! error: (.*)$")
                    if(NOT refuse)
                        message(FATAL_ERROR
                            "the block at line ${block_line} of ${document} names an error and is not marked `refused`")
                    endif()
                    set(error_text "${CMAKE_MATCH_1}")
                else()
                    list(APPEND source "${l}")
                endif()
                set(first FALSE)
            endforeach()
            if(name STREQUAL "")
                set(module "${dir}/block_${block_line}.anti")
                set(exe "${dir}/block_${block_line}")
            else()
                if(DEFINED module_${document_index}_${name})
                    message(FATAL_ERROR
                        "the name `${name}` stands twice in ${document}, at line ${line_${document_index}_${name}} and at line ${block_line}")
                endif()
                set(module "${dir}/${name}.anti")
                set(exe "${dir}/${name}")
            endif()
            write_module("${context};${source}" "${module}")
            execute_process(
                COMMAND "${ANTIC}" --front-end --tests
                        --runtime "${RUNTIME}" "${module}"
                RESULT_VARIABLE status
                OUTPUT_VARIABLE out
                ERROR_VARIABLE err
                ENCODING NONE)
            if(refuse AND status EQUAL 0)
                # The front end accepts the block, so a later pass of a
                # release build is the one that refuses it.
                set(later -S -o "${exe}.s")
                if("${source}" MATCHES "(^|;)fn main\\(")
                    set(later -o "${exe}")
                endif()
                execute_process(
                    COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${later}
                            "${module}"
                    RESULT_VARIABLE status
                    OUTPUT_VARIABLE out
                    ERROR_VARIABLE err
                    ENCODING NONE)
            endif()
            if(refuse)
                if(status EQUAL 0)
                    message(FATAL_ERROR
                        "the block at line ${block_line} of ${document} is marked `refused` and compiles")
                endif()
                if(error_text STREQUAL "")
                    message(FATAL_ERROR
                        "the refused block at line ${block_line} of ${document} names no error: write `//! error: <text>`")
                endif()
                restore(error_text "${error_text}")
                string(REGEX MATCH "(^|\n)[^\n]*error:[^\n]*" first_error "${err}")
                if(first_error STREQUAL "")
                    set(first_error "${err}")
                endif()
                string(FIND "${first_error}" "${error_text}" at)
                if(at EQUAL -1)
                    message(FATAL_ERROR
                        "the refused block at line ${block_line} of ${document} fails with another error than `${error_text}`:\n${err}")
                endif()
                math(EXPR refusals "${refusals} + 1")
            elseif(NOT status EQUAL 0 OR err MATCHES "warning")
                string(APPEND failures
                    "\nthe block at line ${block_line} of ${document}, written as ${module}:\n${err}")
            else()
                math(EXPR checked "${checked} + 1")
                if(NOT name STREQUAL "")
                    set(module_${document_index}_${name} "${module}")
                    set(line_${document_index}_${name} ${block_line})
                endif()
                if("${source}" MATCHES "(^|;)fn main\\(")
                    math(EXPR programs "${programs} + 1")
                    set(pending_module "${module}")
                    set(pending_exe "${exe}")
                endif()
            endif()
            set(context_line 0)
            set(context "")
            set(block "")
        elseif(line MATCHES "^<!-- (overview|examples): context")
            set(context_line ${number})
            set(state context)
        elseif(line MATCHES "^```anti( not-built| refused)?$")
            set(skip FALSE)
            set(refuse FALSE)
            if(CMAKE_MATCH_1 STREQUAL " not-built")
                set(skip TRUE)
            elseif(CMAKE_MATCH_1 STREQUAL " refused")
                set(refuse TRUE)
            endif()
            set(state block)
            math(EXPR block_line "${number} + 1")
        elseif(line MATCHES "^```(llvm|asm) excerpt=([a-z][a-z0-9_]*) target=([a-z0-9_-]+) mode=(dev|release)$")
            set(excerpt_kind "${CMAKE_MATCH_1}")
            set(excerpt_id "${CMAKE_MATCH_2}")
            set(excerpt_target "${CMAKE_MATCH_3}")
            set(excerpt_mode "${CMAKE_MATCH_4}")
            set(state excerpt)
            set(block "")
            math(EXPR block_line "${number} + 1")
        elseif(line MATCHES "^```(llvm|asm) ")
            message(FATAL_ERROR
                "the excerpt at line ${number} of ${document} is not written as `llvm excerpt=<id> target=<target> mode=<dev|release>`")
        elseif(line MATCHES "^```" AND NOT context_line EQUAL 0)
            message(FATAL_ERROR
                "the context at line ${context_line} of ${document} stands before a block that is not `anti`")
        endif()
    endforeach()
    if(NOT pending_module STREQUAL "")
        run_program("${pending_module}" "${pending_exe}" "")
    endif()
    if(NOT context_line EQUAL 0)
        message(FATAL_ERROR
            "the context at line ${context_line} of ${document} has no block after it")
    endif()
    if(NOT state STREQUAL "outside")
        message(FATAL_ERROR
            "the block at line ${block_line} of ${document} does not close")
    endif()
endforeach()
if(checked EQUAL 0 AND refusals EQUAL 0)
    message(FATAL_ERROR "no `anti` block in ${DOCUMENTS}")
endif()
if(failures)
    message(FATAL_ERROR "an example does not compile:${failures}")
endif()
message(STATUS "${checked} blocks compiled, ${programs} programs run, ${refusals} refused as marked, ${excerpts} excerpts found, ${skipped} skipped as not built")
