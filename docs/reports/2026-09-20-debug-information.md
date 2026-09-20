# Debug information

`antic -g` writes the source position of every statement, and the link keeps
the debug sections. lldb stops by file and line in an Anti program and prints a
backtrace of Anti function names. Variables were out of scope.

## What was built

`src/debug.c` and `src/debug.h` hold the new pass, and `docs/notes/debug.md`
its choices. The rest is plumbing for one position per instruction:

- The IR carries a file table per module, a file and a declaration line per
  function and a line per instruction. Lowering moves a cursor on the function
  to the line of each statement. Every appender of `src/ir.c` stamps it.
- Selection copies the line of an IR instruction onto every machine
  instruction of its pattern. That includes the ones a target's own helpers
  append without the selector. Register allocation does the same around one
  instruction, which covers a spill, a wide frame offset and the epilogue.
- The library file holds all three, so a breakpoint in a module that came from
  an `.antl` resolves. `ANTL_VERSION` is 28.
- `-g` on the command line, the emitter and the link. Without it nothing
  changes: no directive is written and every link strips the debug sections as
  before.

## The one thing the specification did not cover

"Debug information" of `docs/anti-language-additions.md` says antic writes
`.file` and `.loc` and llvm-mc turns them into DWARF. That produces a line
table and nothing else, and a debugger that finds no compile unit resolves no
file and line: lldb answered `Breakpoint 1: no locations (pending)` and the
program ran to its end. llvm-mc writes a unit of its own only for an assembly
file that carries no `.file` directive, and that unit names the assembly file.

So antic writes the unit, as the bytes of `.debug_abbrev` and `.debug_info`. It
holds one entry with the range of the code, the name of the line table, the
source and the producer, and no children, because function names come from the
symbol table. That is 60 lines of `src/debug.c` and it is the smallest thing
that reaches the behaviour the specification asks for. Three `[provisional]`
entries in `docs/decisions.md` record it, the language code 0x8001 and the
absence of `DW_AT_comp_dir`.

On COFF the same shape is `.cv_file`, `.cv_func_id`, `.cv_loc` and a
`.cv_linetable` per function in `.debug$S`, with no symbol record per function.

## Tested

- `debug_info` builds a program and a module it imports with `-g`, checks the
  `.file` and `.loc` directives, checks that a build without `-g` writes none,
  runs both, then has the debugger set a breakpoint by file and line inside the
  imported function and read a backtrace. It selects lldb on macOS and gdb on
  Linux.
- `asm_debug_<target>` assembles the `-g` output of all six targets with
  llvm-mc, which covers the DWARF of ELF and Mach-O and the CodeView of COFF.
- Unit tests: `positions` in `test_ir.c` for the file table and the cursor, and
  `strips_debug` in `test_link.c` now checks both ways for all six targets.
- 462 tests pass on the Mac, 461 under ASan and 461 under UBSan. The Linux VM
  ran `debug_info` with gdb and then the whole suite, and passed 403 of 404.

Checked by hand and not as tests: all 54 programs of `tests/programs/` build
with `-g` and print what they print without it. lldb stops in a dev-mode build
of two objects. And lldb resolves `step.anti:4` inside a cross-built
linux-arm64 executable, which is evidence for the ELF side without being gdb.

Two existing tests needed their sources aligned line for line, because a
library file now records the line of every statement: the line form and the
block form of the doc comments in `tests/modules/`, and `package_and_docs` in
`test_modules.c`. `tests/modules/scale.antl.hex` and the golden bytes in
`test_modules.c` were regenerated for version 28.

## What the Linux VM said

gdb stops by file and line and prints a backtrace of Anti function names. It
reads more than lldb does: a position for every frame. lldb gives one to the
frame that stops and names the function alone below it. The reason is that the
compile unit describes no function of its own, and the step that adds variables
adds those descriptions. `debug_info` therefore asks each debugger
for what it gives, and the gdb run checks the position of every frame.

Two spellings differ and the test leaves both open. gdb writes the last segment
of a dotted symbol in brackets, `com.example.step[step]`. And `main` shares its
address with the symbol of the runtime entry, so lldb names that frame
`app.main` and gdb `anti.rt[main]`.

## Two things to settle

1. **`std_toml` fails on Linux and did so before this work.** The program dies
   with a segmentation fault in `anti_rt_toml_free` of `rt/toml.c:350`, reached
   by `anti.toml.Document.destruct` from the error path of `Document.read`. The
   same build of `ae83536` fails the same way, so the debug information is not
   the cause. The Mac passes the test under both sanitizers. `-g` found the
   frame, which is the first thing it has been useful for. Nothing of it was
   touched here, because it is another task.
2. **`anti build` passes `-g` in dev mode** is still a rule without an
   implementation, as `docs/tooling.md` records it.

## Two things this session got wrong

- `docs/vm-setup.md` still named the export directory `book`, from the book
  repository, and the tools it clones came from there too. The session followed
  it. The names are now `antic-check` for the tree of a run, which
  `docs/notes/hosts-and-harness.md` already used, and `/tmp/antic` for the
  clone of the scripts.
- The address of the Linux VM is in `docs/notes/hosts-and-harness.md`, which the
  session did not read before reporting the machine unreachable. Reaching the
  test machines belongs in what a session reads when it needs a machine.
- Pre-existing docs-style findings in `src/ir.h` and `src/regalloc.c`, outside
  the lines this session touched, were left alone.
