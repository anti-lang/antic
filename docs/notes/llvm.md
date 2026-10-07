# The LLVM back end

Choices made inside the back end, which translates the IR into LLVM IR text
and runs opt and llc of the pinned release on it. `src/antic/llvm_emit.c`
writes the text, `src/antic/llvm_debug.c` its debug information,
`src/antic/llvm_target.c` the facts of each target and level,
`src/antic/abi.c` the classification of each parameter and result, and
`src/antic/llvm_run.c` runs the two tools. They describe the inside of the
compiler. `docs/decisions.md` holds what a reader of the language or a user
of the tools can observe, and `docs/work-order-llvm-back-end.md` is the
design the back end was built from.

## Where it runs

- `back_end` of `src/antic/driver.c` lowers the program and runs the passes
  over the whole program. It drops the failures the mode leaves out, runs the
  optimizer and puts in the checks of `--memory-checks`. `llvm_back_end` then
  folds every symbolic size with `layout_init`, `layout_data` and
  `layout_resolve`, runs the optimizer again on each function that had a
  symbolic value, and translates the module. The translation never sees a
  symbolic size.
- Release mode translates the whole program into one text, dev mode one
  module, and a plugin its own module alone. `--dump-llvm` prints the text
  and stops. Otherwise `driver_compile_llvm` writes the text to
  `<output>.ll`, and `llvm_run` takes it from there.
- The build id digests the text before the notice goes in, since the notice
  holds the id. `llvm_debug_mark` records every span of the text that the
  debug information added. The digest leaves the spans out, so a `-g` build
  and a plain build of one program carry one id.

## The text

- The IR is not in SSA form. Every temporary gets an `alloca` in the entry
  block, a definition stores into it and each use loads from it. The `sroa`
  and `mem2reg` passes of opt build the SSA form. A slot of the IR is an
  `alloca` of its bytes in the entry block too, wherever the IR puts it.
- Each block of the IR is one LLVM block named `b<index>`, and the entry
  block of the IR is the entry of the function. llc writes the prologue, the
  spills and the callee saves.
- A `bool` is an `i8` in memory and in a temporary. A comparison widens its
  `i1` to the type of its instruction, and a branch truncates the byte back.
  The trampolines of `whole_tables.c` compare with the type `i64`, a word of 0
  or 1, and the branch after it reads the low byte.
- A branch into the failure arm of an assertion or a check carries branch
  weights that mark that side cold.
- A function takes the symbol of `target_c_symbol` or `target_mangle`, which
  the runtime, the C headers and the symbolizer read. The text drops the
  leading `_` of Mach-O, which llc writes again from the triple.
- A whole program keeps every function `internal`, unless it is an export fn
  or the program hosts plugins. An object of one module makes every function
  global and `hidden`, and a copy of a generic `weak_odr`, in a COMDAT on
  COFF, which has no hidden symbols. An export fn is `dso_local`.
- A global is a packed struct of the bytes `layout_data` computed, with each
  address a member of type `ptr`. A global the program never writes is a
  `constant`, which llc puts in the read-only section of its format. A global
  of another object is `external global [N x i8]` with the size and the
  alignment of its IR global.
- The runtime enters the program through `anti.rt.main`, an alias of the
  program's `main`. That `main` is `noinline` and stands in
  `llvm.compiler.used`, so under LTO it keeps its frame and its own name in a
  trace, a map and a PDB. The function of each module that compiles its pattern
  literals and, in a shared library, the constructor of the runtime stand in
  `llvm.global_ctors`. The copy of the package header and the notice are
  constants in sections of their own, which `llvm.used` keeps.
- A call names the function type that `abi_classify` gives for its callee.
  An argument past the parameters of a variadic C function passes in its
  promoted type outside that type. The result of a call takes the type of its
  instruction, converted from the type the callee declares where the two
  differ.
- A table call loads the pointer from the slot of the descriptor and calls it
  with the signature of the IR.
- Every function is `nounwind`. Every function on Linux and macOS keeps its
  frame record, `"frame-pointer"="non-leaf"`, since `anti_rt_trace_walk` and
  the report of `--memory-checks` follow the records. Windows walks its
  unwind data and keeps none. A function that counts its own frame in the
  skip of a trace is `noinline` and keeps its calls out of tail position.
- The CPU and its features stand in the function attributes
  `"target-cpu"` and `"target-features"` of each level. The text alone then
  decides the code, and the command lines of opt and llc carry neither.
  macos-arm64 adds `"tune-cpu"="apple-m1"`.
- An atomic operation is a call of `anti_rt_atomic_*` in the IR, and
  `atomic_call` writes it as `load atomic`, `store atomic`, `atomicrmw` or
  `cmpxchg` of its width, `seq_cst` as the runtime's bodies are. The level
  then picks the instructions: LSE from `armv8.2`, a load-store exclusive
  loop at `armv8.0`, the lock prefix on x86_64.

## Facts for the optimizer

`docs/work-order-llvm-optimization.md` gave LLVM every fact the language
guarantees, one step each. The entries of `docs/decisions.md` hold the
rules, and the tests named there pin each fact.

- `noreturn cold` on a function of type `never` and on the failure routines,
  and `unreachable` after a call of one, so the fatal path costs a hot loop
  nothing. A branch into a failure arm carries cold weights.
- The memory effects of each function of the runtime, the column of
  `src/antic/rt_abi.h`, as `memory(...)` with `nofree`, `nosync` and
  `willreturn` where they hold.
- The table facts: `!invariant.group` on the store and the loads of a table
  pointer, and `!invariant.load` on a slot. A call through the table of a
  known object then becomes a direct call or inlines.
- The parameter facts: `noundef`, `nonnull` and `dereferenceable(N)` of a
  `*T`, `noalias` of an `own` pointer and of an allocation, `signext` and
  `zeroext` of a narrow integer, and `range()` on a result of a `bool` or an
  enum.
- The facts of arithmetic and addresses: `nsw` and `nuw` where the value
  rules make them hold, `getelementptr inbounds` on a field, `!range` on a
  load of a `bool`, an enum or a tag, and `llvm.lifetime` around the slot of
  a `let`.
- `!tbaa` from the aliasing rules of views by `as` in
  `docs/anti-language-additions.md`.

## The link

- A release build that links a program with lld links through full LTO by
  default. opt runs `lto-pre-link<O3>` with the threshold 225 and writes
  bitcode as the object, and lld links it with the runtime as bitcode of
  `lib/<target>/<level>/bitcode/full/` at `--lto-O3` and the same threshold,
  `/opt:lldlto=3` on Windows. `--lto thin` takes `bitcode/thin/`, and
  `--lto none` the link of objects below. Dev mode, `-S`, `-c`, `--lib`,
  `--linker platform`, a profile, `--memory-checks` and a Windows program
  that hosts plugins keep the objects.
- The runtime as bitcode of a Linux target carries no unwind tables. The link
  with `-g` that a symbols archive holds then lays the program out as the
  release link does. The runtime of ARM64 Linux takes no outline atomics on
  any host.
- Every link drops what nothing reaches: `-dead_strip`, `--gc-sections` or
  `/OPT:REF`. What only a name reaches stands in `llvm.used`.
- lld-link of windows-x86_64 folds identical code in the safe form,
  `/OPT:SAFEICF` with the address-significance table of llc. Every other link
  folds nothing.
- `--profile-generate` instruments the run of opt and links the profile
  runtime of compiler-rt, and `--profile-use <file>` runs opt with the
  merged profile. Both take the link of objects.

## Defined results and wide operations

- A division by zero gives 0, and the least value divided by minus one gives
  itself with a remainder of 0. The signed forms divide by 1 where the divisor
  is 0 or minus one, since LLVM leaves both undefined, and selects then give
  the defined results. A shift takes its count modulo the width. A float out
  of the range of an integer type saturates through `llvm.fptosi.sat` and
  `llvm.fptoui.sat`. Dev mode keeps the checks in front of each.
- Signed `+ - *` are plain `add`, `sub` and `mul` without `nsw`, so they wrap.
  The checks of dev mode use the signed `with.overflow` intrinsics, and the
  branch after one reads its `i1`.
- `docs/notes/flags.md` holds the translation of the flag operations, the
  saturating ones and `mul_high`, `docs/notes/simd.md` the one of the simd
  operations and `docs/notes/floats.md` the one of `f16`.

## Debug information

- `struct ir_function` carries `at_line`, the cursor. `lower_stmt` moves it
  to the line of the statement before it emits anything of it, and every
  appender of `src/antic/ir.c` stamps the cursor on the instruction it adds.
  A statement that holds a block leaves the cursor on the last line of the
  block. A function that lowering wrote itself, an initialiser or a thunk,
  has the cursor 0 and carries no line.
- Under `-g` every instruction with a line carries a `!DILocation`, every
  function a `!DISubprogram`, every source file a `!DIFile` and the text one
  `!DICompileUnit`. The nodes describe lines alone: no variable, type or
  parameter. The compile unit is `FullDebug`, since under `LineTablesOnly` llc
  writes the subprogram of a function only when it inlines another.
- A subprogram names the function as a person reads it, `app.List<int>.push`,
  where the symbol holds `app.List$3cint$3e.push`.
- A COFF text writes the compile unit and a subprogram per function in every
  build. CodeView then writes the record that names a static function in the
  PDB. Without `-g` the subprogram names line 0, and the runtime of Windows
  reads an entry of line 0 as no position.
- The path of a source is the one a failed check names. That is the file
  under the first search root that holds it, or the file name alone outside
  every root. The directory of a file is empty, so no path of the build
  reaches a PDB. A debugger resolves the path against its own working
  directory.
- A library file holds the file table of its module, the file and the
  declaration line of each function and the line of each instruction. A
  breakpoint in a module that came from an `.antl` therefore resolves, which
  the test `debug_info` checks.
- gdb writes the last segment of a dotted symbol in brackets, so
  `com.example.step.step` prints as `com.example.step[step]`. A test that
  matches a function name leaves the character before the last segment open.

## The tool run

- The link of objects in release mode, `--lto none`, runs
  `opt -passes=default<O3> -inline-threshold=225` into `<output>.bc` and llc
  at `-O2` on the bitcode. `llvm_opt_options` of
  `src/antic/llvm_run.c` holds the two options, which the step `config` of
  `docs/work-order-llvm-optimization.md` measured. Dev mode runs llc at `-O1` on the text without opt,
  so it inlines nothing and every function stays a frame of its own.
- llc writes the object with `-filetype=obj`, or the assembly under `-S` with
  `-filetype=asm`. The relocation model is `pic` on every target, the one
  the pinned clang passes. On Linux and Windows llc also runs with
  `-function-sections -data-sections`, so the link drops each function and
  datum that nothing reaches. Mach-O splits per symbol without them. On
  windows-x86_64 llc adds `-addrsig`, the table that the safe folding of
  lld-link reads.
- llc runs in the directory of its output and writes it by its file name,
  with its input and its own path absolute. llc records the name of its
  output in the CodeView of a COFF object. lld-link carries that name into
  the PDB, which must hold no path of the build.
- `archive_tool` finds opt and llc in `bin/` of the runtime archive, then on
  the search path. `--opt` and `--llc` name others. A failed tool prints its
  own message, then `antic: opt failed` or `antic: llc failed`.
- `<output>.ll` and `<output>.bc` are deleted as soon as llc ends, unless
  `--keep-llvm` keeps them. `--llvm-mc` is accepted and runs nothing.

## Tests

- `llvm_verify_<target>` runs `opt -passes=verify` over the text of every
  program of `tests/dump` and `tests/programs`, in both modes.
- `tests/dump/*.ll` are the goldens of `--dump-llvm`, with the version of
  antic written as `VERSION`. Each program and target that had a golden of
  the machine code of the native back end has one.
- `emit_identity` holds the digests of the `--dump-llvm` text of every
  program of `tests/programs` on the six targets. `link_identity_macos-arm64`
  holds the digest of one linked program, and `unwind_<target>` the Windows
  unwind data of the assembly llc writes for `tests/dump/unwind.anti`.
  `docs/notes/value-rules.md` says how to write each again.
- The tests that read assembly read the one of llc, whose instruction lines
  start with a tab. `cpu_level_<level>` and `f16_<target>_<level>` read a dev
  build, and `simd_width_<target>_<level>` and `asm_copies_<target>` the
  assembly as it is. `trap_table` reads the body of `main` up to the comment
  llc writes after every function.
- `llvm_division` and `llvm_flags` check the defined results and the flags in
  release mode, `llvm_build_id_<target>` the one build id of a `-g` build and
  a plain build, and `llvm_inline_lines` the line of an inlined callee.
