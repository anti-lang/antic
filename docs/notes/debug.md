# Debug information

The choices inside `src/antic/debug.c`. The rules are in "Debug information" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md` under the same heading.

## What the assembly carries

Three kinds of line. A `.file` per source file of the program, written once
after the text section. A `.loc` before the first instruction of every
statement. And the compile unit, the bytes of `.debug_abbrev` and
`.debug_info`, after the last function.

llvm-mc turns the `.file` and `.loc` directives into `.debug_line`. On COFF
the three become `.cv_file`, `.cv_loc` and, per function, a `.cv_linetable`
in `.debug$S`.

## Why antic writes the compile unit

`.file` and `.loc` alone give a line table and nothing else. A debugger that
finds no compile unit finds no source file: `breakpoint set --file --line`
resolves to no location, and the program runs to its end. llvm-mc writes a
unit of its own under `-g`, but only for an assembly file that carries no
`.file` directive, and that unit names the assembly file and takes the lines
of the assembly text rather than the Anti source.

So the unit is antic's. It holds one entry, and a child entry per function:

| Attribute | Form | Value |
|---|---|---|
| `DW_AT_stmt_list` | `sec_offset` | the start of `.debug_line` |
| `DW_AT_low_pc` | `addr` | the label before the first function |
| `DW_AT_high_pc` | `data4`, `addr` on Mach-O | the end of the last function |
| `DW_AT_name` | `string` | the source of the program's own module |
| `DW_AT_producer` | `string` | `antic` and its version |
| `DW_AT_language` | `data2` | 0x0c, C99 |

The entry of a function, `DW_TAG_subprogram`, holds its name, its first byte
and its length. The name is the one a person reads, `app.List<int>.push`,
where the symbol table holds `app.List$3cint$3e.push`. lldb names a frame
after the entry, and makes a function of one only when the unit names a
language whose type system it has. The code of an assembler, 0x8001, gave
it none, so the unit names C99.

On ELF and COFF the end of the unit is its length, a difference of two
labels. On Mach-O a difference of two labels in two atoms is a pair of
relocations, which lldb does not apply when it reads an object. The unit
covered the first function alone, and lldb found no line past it. The end
is therefore an address on Mach-O, one relocation that the debug map moves.
Each function's own length is a difference inside one atom and stays a
number.

The version is 4, which is what llvm-mc writes into the line table of an
assembly file. The two agree.

## The offsets into another section

`DW_AT_stmt_list` and the offset of the abbreviations are relocations on ELF
and COFF. There the linker joins the debug sections of every object and moves
each one. Both name a label that antic defines at the start of its own
section. `.debug_line` gets that label from a section directive with nothing
after it, and llvm-mc appends the line table there.

Mach-O keeps the debug information in the object files and names them from
the executable, so nothing moves. Both offsets are then 0, the offset of the
one unit of the object. `doesDwarfUseRelocationsAcrossSections` is false for
Mach-O in LLVM, and clang writes 0 there for the same reason.

## Where a line comes from

`struct ir_function` carries `at_line`, the cursor. `lower_stmt` moves it to
the line of the statement before it emits anything of it, and every appender
of `src/antic/ir.c` stamps the cursor on the instruction it adds. A statement that
holds a block leaves the cursor on the last line of the block, which is where
the code after the block comes from.

The cursor is 0 in a function that lowering wrote itself, an initialiser or a
thunk, so those carry no line.

Selection copies the line of the IR instruction onto every machine
instruction of its pattern. A pattern reaches its target's own helpers for a
move, an immediate or an addition of a frame offset. Those append to the block
without the selector, so the loop after `p->emit` gives the line to each
instruction that has none. Register allocation does the same around one
instruction. That covers the load of a spilled operand, the store of a spilled
result, a wide frame offset and the epilogue before a return.

An instruction that still carries no line writes no `.loc` and keeps the
position of the instruction before it. The prologue therefore belongs to the
line of the declaration, which `debug_open` writes after the symbol.

## The paths

The path of a source is the one a failed check names. That is the file under
the first search root that holds it, or the file name alone outside every root.
It comes from `module_file_of_source`, so the library file, the check and the
debug information never disagree. The bytes are then the same on every host.

The unit carries no `DW_AT_comp_dir`, so a debugger resolves a relative path
against its own working directory. A reader who debugs from the root of the
project finds every source.

## What the two debuggers make of it

gdb gives a position to every frame of a backtrace. Apple's lldb gives one to
the frame that stops and names the function alone below it. A compile unit with
no `DW_TAG_subprogram` is the reason, and the step that describes variables
describes the functions as well, because a variable lives inside one.

Neither debugger reaches `main` by its Anti name. `emit_entry` gives the
runtime entry a second global name at the same address with `.set`, so the two
symbols share it, and a debugger prints whichever it picked: lldb `app.main`
and gdb `anti.rt[main]`.

gdb writes the last segment of a dotted symbol in brackets, so
`com.example.step.step` prints as `com.example.step[step]`. A test that matches
a function name leaves the character before the last segment open.

## The library file

A library file holds the file table of its module, the file and the
declaration line of each function, and the line of each instruction. The
reader maps the file index of the file onto the program's table. A breakpoint
in a module that came from an `.antl` therefore resolves. The test
`debug_info` checks it by stopping inside an imported function.
