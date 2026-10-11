# Back end and drivers of antic

This step audited the LLVM back end and the drivers of antic against the
three standards. It read every line of `llvm_emit.c`, `llvm_debug.c`,
`llvm_run.c`, `llvm_target.c`, `abi.c`, `driver.c`, `driver_index.c`,
`driver_library.c`, `driver_link.c`, `driver_search.c`, `linker.c`,
`coff.c`, `applesdk.c`, `target.c`, `cpu.c`, `header.c`, `notice.c`,
`docpage.c`, `main.c`, `antic.c`, `platform.c` and `userdirs.c`, with their
headers. That is 16,117 lines of C and 1,663 lines of headers. It read
"Files of the driver" in `docs/decisions.md` and the notes `llvm.md`,
`targets.md`, `linker.md`, `traces.md`, `flags.md` and `floats.md` against
the files. The task names `docs/notes/debug.md` as well, and no such file
exists. The debug information stands in a section of `llvm.md` and in two
sections of `docs/decisions.md`, which this report reads instead. Every
finding was checked in the code. No build ran, and no reviewer agent was
used. The machine evidence is `docs/audit/data/`. The findings of
`docs/audit/tool-pass.md` for these files are not repeated: the length of
`llvm_emit.c`, the comment of `antic.h` against the test `anti_interface`,
the include of `ir.h` in `docpage.c`, the two options without a test and
the functions past a threshold. Where this report judges one of them, it
says so. Each finding says whether `docs/audit/2026-09-23/` or
`docs/audit/2026-09-30/` reported it. Every finding is new. The LLVM back
end and the split of the driver came after the second audit, and no
finding of the two earlier reports on these files stands open.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Documentation | 2 | 2 | 0 | 0 |
| Major | Structure | 2 | 2 | 0 | 0 |
| Major | Documentation | 2 | 2 | 0 | 0 |
| Minor | Safety, rules 6, 9, 18, 19, 25 and 26 | 6 | 6 | 0 | 0 |
| Minor | Structure | 4 | 4 | 0 | 0 |
| Minor | Documentation | 5 | 5 | 0 | 0 |

In all 21 findings: 2 severe, 4 major and 15 minor. No finding names a
rule of safety above minor. The readers of untrusted input of this area,
`coff.c` and the plugin index of `driver_index.c`, hold the checks the
earlier audits asked for.

## Severe

### driver.h promises a fallback to PATH that the code refuses

`src/antic/driver.h:26`, documentation, a public header whose documented
behaviour differs from the code, severe, new.

The comment of `opt` and `llc` reads "NULL: opt and llc of the runtime
archive's bin/, or from PATH". The one of `llvm_ar` at line 114 reads
"NULL: llvm-ar from PATH". `driver_archive_tool` of
`src/antic/driver_link.c:65` takes a tool from `bin/` of the archive and
refuses a missing one by its path. Its DESIGN comment and step 28 under
"Binary distribution" in `docs/decisions.md` say so. No caller searches
PATH. `driver.h` is the header `anti` compiles through, so a reader of
`anti` plans for a fallback that does not exist. The fix is to say what
the code does: NULL names the tool of `bin/` of the runtime archive, and a
missing one is refused.

### The decisions on build ids and debug information describe the assembler

`docs/decisions.md:1227`, documentation, a file whose documented behaviour
differs from the code, severe, new.

The sections "Build ids" and "Debug information", lines 1219 to 1231,
describe `-g` as `.file` and `.loc` directives that llvm-mc turns into a
line table. They describe a compile unit that antic writes "as the bytes
of `.debug_abbrev` and `.debug_info`". They describe a digest of "the
assembly" around ranges that `src/antic/debug.c` records. The code writes
`!DIFile`, `!DISubprogram` and `!DILocation` nodes in
`src/antic/llvm_debug.c`, and one `!DICompileUnit` of `FullDebug` with
`directory: ""`. `build_id` of `src/antic/driver.c:704` digests the LLVM
text. Line 1229 records that Eddie chose the language 0x0c, C99, on
2026-09-25, with the reason. `src/antic/llvm_debug.c:115` writes
`DW_LANG_C11`, which `docs/work-order-llvm-back-end.md:602` gives and the
entry at line 1028 defers to. Two entries of the first authority disagree,
and the code follows the later one without an entry that closes the
earlier. The tool pass reported the file name `debug.c` of line 1220
alone. The fix is Eddie's: the two sections stand as history or are
written for the LLVM text, and the entry on C99 says whether C11 replaces
it.

## Major

### The last separator of a path is found four ways

`src/antic/driver.c:1174`, structure, duplicated logic that has diverged,
major, new.

`platform_last_separator` of `src/antic/platform.c` takes `\` on Windows
and `/` elsewhere. The driver calls it in `driver_windows_link_paths` and
`copy_memcheck_dll`. Ten other places split a path with
`strrchr(path, '/')`: `library_name` at line 1174 and `compile` at line
1364 of `driver.c`, `host_exports` at `driver_link.c:386`,
`driver_build_c_library` at `driver_library.c:276`, 443 and 450, and
`link_pdb_path`, `link_import_library_path`, `macos_shared` and
`linux_shared` at `linker.c:658`, 672, 955 and 1001. `file_name` of
`llvm_run.c:11` is a third rule, which takes both separators on every
host. On a Windows host an output path with the separator of Windows
splits one way in the link and another in the library. `dir` of
`driver_build_c_library` comes out empty, so the archive goes under the
directory and the header and the object into the working directory.
`link_pdb_path`, which `anti` calls too, cuts a path at a dot of a
directory name when the path holds no `/`. The second audit reported the
same drift in `src/anti/bind.c`, which `e0dbe4e1` fixed. Its structure
report named `userdirs.c:144`, which calls the platform layer now. These
places were not named. The fix is one helper of the platform layer for
the file name of a path, which every caller takes.

### A Linux shared library with a soname runs ln of the host

`src/antic/driver_library.c:444`, structure, a part doing work that
belongs to another, major, new.

`driver_build_c_library` makes `lib<name>.so` a link to
`lib<name>.so.<major>` with `ln -sf` through `process_run`. That is a
program of the host outside the platform layer, run for a Linux target
from any host. A Windows host has no `ln`, so the build fails there, and
it fails without a word: line 447 sets `ok` false and no message follows.
The rule under "Distribution" in `CLAUDE.md` refuses a dependence on a tool
of the host like a warning. The tool pass found that `--soname` has no
test, which is why nothing shows it. The fix is a symbolic link in the
platform layer, with a message on failure, and a test of `--soname`.

### linker.md describes a Windows link the code no longer makes

`docs/notes/linker.md:22`, documentation, a notes file that describes a
design the code no longer has, major, new.

The section "The tools and the sysroots of a link" ends: "every lld-link
command names `crt` and `sdk` of the sysroot as `/vctoolsdir` and
`/winsdkdir`, which ends the detection". `lld_link_start` of
`src/antic/linker.c:723` writes `/lldmingw`, `/lldignoreenv` and four
`/NODEFAULTLIB`, and `windows_libpath` one `/LIBPATH`. Neither of the two
options is written anywhere. The next section of the same note, and the
entry at `docs/decisions.md:315`, describe the current form, so the note
contradicts itself. Line 13 of the note says `link_facts` asks for the
flavour of lld "before the compile". `driver_run` calls `compile`, then
`driver_compile_llvm`, then `driver_link_program`, which reaches
`link_facts`. The check runs after the compile and after opt and llc. The
comment at `driver_link.c:179` reads the same way as the note. The fix is
to delete the sentence on the two options, and to say when the check runs
or to move it before the compile as the note promises.

### Twelve files open with the comment of one item and not of the file

`src/antic/abi.c:6`, documentation, a file without a header comment that
says its one purpose, major, new.

The first comment of `abi.c` describes the registers of System V. The one
of `applesdk.c:9` describes a function that reads digits, the one of
`cpu.c:5` the table of levels, and the one of `header.c:11` the C name of
a scalar. The one of `llvm_run.c:9` describes the file name of a path, the
one of `llvm_target.c:3` the data layout strings, the one of `target.c:5`
the names of the targets and the one of `userdirs.c:10` an XDG variable.
None says what the file is for. The tool pass lists `linker.c`,
`llvm_debug.c`, `main.c` and `notice.c` the same way, where the first
comment follows a declaration. Its heuristic passed over the eight above,
since their first comment stands before a declaration. Five of the twelve
have a header whose comment carries the sentence: `abi.h`, `header.h`,
`llvm_debug.h`, `llvm_run.h` and `llvm_target.h`. The other seven have it
nowhere. The closing section gives the sentence of each. The fix is one
comment at the top of each `.c` file.

## Minor

### llvm_emit.c is one responsibility with four seams

`src/antic/llvm_emit.c:1`, rule 19, minor, new.

The file has 4,102 lines, which the tool pass reports. Read whole, its
purpose is one sentence: translate a module of the IR, laid out for one
target, into LLVM IR text. Every function serves that sentence but
`llvm_emit_names` at line 3949, which writes the names of a `.def` file
for the linker and no LLVM text. So the file is one responsibility, and it
would split along four seams if it splits:

- The text of a function: `binary` at line 558 to `instruction` at 2911,
  the scalar, memory, wide and simd operations, and `definition` at 3263.
  About 1,600 lines.
- The C convention: `classify` at 2194 to `call` at 2798, with
  `signature`, `result_facts` and `linkage` at 3070 to 3129. The
  parameters, the prologue, the arguments and the results pair with
  `abi.c`. About 800 lines.
- The facts for opt: `access_fact`, `collect_ranges` and the `tbaa_*`
  functions at 788 to 1067, `branch_weights` at 2897 and `frames_kept` at
  3219. They read the module and add metadata, and translate nothing.
  About 500 lines.
- The module: the globals from `global_name` at 3335, the imports of a
  COFF plugin, `entry`, `declaration`, `attributes`, `metadata`, `roots`
  and `constructors`, the four texts from `llvm_emit_package` on, and
  `llvm_emit_module`. About 750 lines.

`struct emitter` would stay in a shared header, as `driver_parts.h` does
for the driver. The judgement of rule 19 is that a split would make the
file clearer. A reader of the convention or of the facts now scrolls past
the instructions. The size follows no shape of the problem that one file
must hold.

### The functions past a threshold

`src/antic/driver_library.c:271`, rule 18, minor, new.

The tool pass lists eleven functions of this area and judges four from
their counts alone. Read in full:

- A split would be clearer. `driver_build_c_library`, 199 lines, holds the
  static library in one branch and the shared library in another of 125
  lines, each a function's worth. `bundle` at line 145, 108 lines,
  extracts the members of the runtime and then joins them, two parts.
  `driver_interface` of `driver.c:1397`, 115 lines, takes a library file
  on one path and a source on another, and the first is a function of its
  own. `driver_load_libraries` of `driver_search.c:248`, nest 5, ends in a
  report of the first missing import or the cycle, which is a function's
  worth. `frames_kept` of `llvm_emit.c:3219`, nest 5, holds the test of
  one function inside the fixed point, and that test is a helper.
- The size follows the shape of the problem. `instruction` of
  `llvm_emit.c:2911`, 137 lines, and `run` of `main.c:205`, 329 lines,
  dispatch over the operations and the options. `compile` of
  `driver.c:1216`, 172 lines, and `llvm_emit_module`, 112 lines, run the
  passes and the parts of the text in order. `driver_run`, 154 lines, is
  the chain of refusals and the run. `collect_tbaa`, nest 5, walks the
  module as `collect_ranges` does. `write_sections` and `write_symbols` of
  `coff.c` follow the format, as the second audit judged.
  `llvm_debug_init` takes eight parameters, which are the fields of
  `struct llvm_debug` it fills, and `class_uses` of `header.c` seven.

### Fixed arrays filled to a hand-counted bound

`src/antic/llvm_run.c:224`, rule 6, minor, new.

`llvm_run` fills `options[LLC_OPTIONS]` with up to seven options and no
check against `LLC_OPTIONS`, and seven is the count today. `run_lto` at
line 136 fills `opt[10]` with ten entries at most and `run_opt` at line
200 `opt[9]` with nine, each exactly full. One option more in any of the
three writes past the array. `coff_join` at `coff.c:969` reads `inputs[0]`
without a check of `count`, which `join_coff` keeps above zero. `next` of
`linker.c:41` checks `LINK_MAX_STRINGS`, 16, and aborts past it, as rule
13 allows for a state that is not reached. The glibc link of a Linux
program takes 15 of the 16. Each is correct for today's callers. A check
at each, or an array that grows as `argv` of `link_command` does, would
keep it so.

### Formatting into fixed buffers without a check

`src/antic/abi.c:12`, rule 9, minor, new.

`set_word`, `vector`, `aapcs64_aggregate` and `windows_x64_aggregate`
write an LLVM type into `types[k][16]` with `snprintf` at lines 12, 72,
222, 236 and 284 and read no result. `<256 x i8>`, the widest lane list
the cap allows, fits. `intrinsic` of `llvm_emit.c:496` formats a
declaration into `char line[160]` with `text_format`, which cuts to three
dots, and a cut line would enter the text as it is. `coff_archive_exports`
at `coff.c:1207` and `dump_tokens` at `driver.c:518` format into 16 and 32
bytes the same way. None can truncate today. The second audit reported the
same class for messages.

### Blocks written more than once

`src/antic/llvm_emit.c:242`, rule 26, minor, new.

- The leading `_` of a Mach-O symbol is dropped in five places:
  `function_symbol` at line 242, `global_name` at 3347, `entry` at 3621,
  `notice_symbol` at 3764 and `constructors` at 3882. One helper that
  appends the symbol of a target without it would serve.
- `flag_count` at 3731 counts the module flags, and `metadata` at 3676
  writes them in the same order with ids of its own. A flag added to one
  and not the other gives the cold branches the id of a flag.
- `object_symbols` of `coff.c:1118` reads the name of a symbol from its
  field or the string table over 20 lines. `symbol_name` at line 211 does
  the same over a `struct object`. One reader over the raw table would
  serve both.
- `vector_typedef` of `header.c:677` writes `t->name` where every other
  writer of the header takes `types_c_name`. The two differ for a copy of
  a generic alone, which `sema_generic.c:597` names, so the typedef is
  right today and the one way is broken.
- `reader_name` of `llvm_debug.c:149` calls `target_c_symbol` with
  `TARGET_LINUX_X86_64` to get the name without the `_` of Mach-O. The
  call says the function depends on a target. `text_append` of the name
  says what it does.

### Names outside the prefix of their file

`src/antic/platform.c:216`, rule 25, minor, new.

`platform.c` exports `platform_open` and seven more under its prefix, and
beside them `path_is_absolute`, `path_relative`, `directory_exists`,
`self_directory`, `absolute_path`, `process_run`, `process_run_in`,
`process_capture` and `process_run_lines`. `userdirs.c` exports
`runtime_archive` beside `user_dir`. `llvm_debug.c` exports
`debug_spans_free` and `llvm_metadata_string` beside `llvm_debug_*`, and
`docpage.c` exports `doc_page_free` beside `antic_doc_page`. The entries
under "Files of the checker" in `docs/decisions.md` give the exported
functions of the lexer, the parser, the targets and the linker the prefix
of their file. These files have no such entry. The fix is one entry and
the renames, or an entry that keeps the names.

### Three functions of the linker are declared in the interface of anti

`src/antic/antic.h:111`, structure, a placement that makes the code harder
to find one's way in, minor, new.

`link_lto_name`, `link_lto_from_name` and `link_pdb_path` are declared at
`antic.h:111`, 114 and 119 and defined in `linker.c`. `enum linker` and
`enum lto` stand at lines 92 and 106 of the same header, and `linker.h`
includes `antic.h` for them. A reader of `linker.h` finds three of the
file's functions in the header of another part, and the interface of
`anti` carries declarations whose definitions stand in the back end. The
include runs one way, so no rule is broken, and `anti` may include
`antic.h` alone, which is why they stand there. The fix is a sentence in
`antic.h` that names the file of each, or `linker.h` as the home of the
five with `antic.h` including it.

### What the assembler left behind

`src/antic/driver.h:23`, structure, a dead option, a dead function, a dead
column and stale names, minor, new.

The native back end is gone and llvm-mc runs no more, as the entry at
`docs/decisions.md:19` says. These stay:

- `--llvm-mc`, which `driver.h:23` keeps as `llvm_mc`, `main.c:464` reads
  and `src/anti/main.c:293` and `src/anti/test.c:57` pass on. Nothing
  reads the field. The entry at `docs/decisions.md:19` keeps the option
  until a separate decision, so it is Eddie's to drop, and this item
  records where it lives.
- `target_block_label` of `target.c:151`, the label of a block of the
  assembly. `tests/unit/test_target.c:27` is its one caller, so a test
  keeps a dead function alive, against rule 26.
- The attributes column of the level table, `cpu_attributes` of
  `cpu.c:72`, the `-mattr=` of llvm-mc. `print_cpu_levels` of `main.c:131`
  prints it and `cpu_levels_pin` compares it with `tools/cpu-levels`, and
  no build reads it.
- The name `assembly` for the LLVM text: the parameter of
  `llvm_back_end`, `back_end`, `compile`, `compile_library_file` and
  `build_id` of `driver.c`, and the local of `driver_run`. The text is
  LLVM IR, which `driver_compile_llvm` writes to `<base>.ll`.

### Errors reach the user in three forms

`src/antic/driver.c:597`, structure, one way to report an error to the
user, minor, new.

`print_diagnostics` at line 403 writes `file:line:column: kind: message`
and the name of a warning. `whole_checked` at line 597 writes the errors
of the pass over the whole program as `file: error: message`, without a
position, and `back_end` at line 1006 writes the refusal of `--no-hooks`
the same way. `antic_outline` of `antic.c:187` is a second printer of the
diagnostics of the lexer and the parser, with `error` written out and no
name. Every other message of the driver starts with `antic:`. A user
reading `anti check` cannot click the second form, and the two printers
drift the day a diagnostic grows a field. The fix is one printer and one
form for a message with a position.

### The shared platform layer names antic in its messages

`src/antic/platform.h:9`, structure, one platform layer, minor, new.

The header says the layer serves antic and anti, and `src/anti/platform.h`
includes it. Every message of `platform.c` starts with `antic:`, as
"antic: cannot run %s" of `spawn` and "antic: waiting for %s" of
`wait_for` do. Run from `anti`, each names the wrong program. The fix is a
name the layer takes from its caller, or messages without a program name.

### Comments that still speak of llvm-mc and the assembler

`src/antic/target.c:16`, documentation, a comment that describes what is
gone, minor, new.

- `target.c:16` to 18 say the triples "select the object format in
  llvm-mc". They say the macOS version "comes from the .build_version
  directive in the assembly". `llvm_triple` of `llvm_target.c:64` appends
  the version to the triple, and the linker takes it as the platform
  version.
- `target.c:149`: "llvm-mc leaves a label that starts with L out of a
  Mach-O symbol table", above the dead `target_block_label`.
- `target.h:47`: `triple` is "the -triple option of llvm-mc". It is the
  triple of the LLVM text, which `llvm_triple` completes.
- `cpu.c:5` to 13 and `cpu.h:49`: the attributes "let llvm-mc assemble the
  instructions the level adds" and are "the -mattr= value of llvm-mc".
  `llvm_target.c:36` names "the llvm-mc attributes" as well.
- `driver.c:3`, `driver_parts.h:6` and "Files of the driver" at
  `docs/decisions.md:2171`: the driver runs "the back end and the
  assembler". llc writes the object, and no assembler runs. `driver.c:764`
  says the text "goes to assembly with the notice after the build id".

### Comments above the wrong item, and doubled comments

`src/antic/driver.c:135`, documentation, a comment that stands above
another item, minor, new.

The comment "The module path of the input, from its path under the search
roots", with its DESIGN, stands at `driver.c:135` above
`drop_test_blocks`. `module_name` at line 180 has none. `driver.c:58`
holds "Read the whole of a binary file" above the comment of `read_file`,
a leftover. `main.c:106`, "Take the value of an option such as -o", stands
above `print_targets`, and `value_of` at line 181 has none. The DESIGN of
a global as a packed struct at `llvm_emit.c:3425` stands above `imports`,
and `global` at line 3523 follows four functions later. The second audit
reported the class for `lower.c`.

### The steps of the work order as a changelog in comments

`src/antic/llvm_emit.c:24`, documentation, a DESIGN comment that records
history, minor, new.

The DESIGN at the top of `llvm_emit.c`, lines 24 to 30, lists what "the
step emit-core", "emit-arith", "emit-memory" and "emit-wide" each
translated. Line 1327 says "vsplat and vbinary come with the step
emit-memory". The comment of `unexpected` at line 147 speaks of "a form
that no step of the work order translates". The docs-style rules keep
changelog entries out of comments, and git holds the steps. The comment of
`llvm_emit_module` at `llvm_emit.h:48` to 50 says the error "names it and
the step of the work order that adds it". The message of `unexpected` names the
form and the function and no step. The fix is to say what the file does
and what the message holds.

### A DESIGN reason that is no longer true

`src/antic/driver_index.c:14`, documentation, a DESIGN comment whose
reason is gone, minor, new.

The DESIGN of the plugin index says "antic writes the file, because `anti
build` is not built". `anti build` is built, as "The build command" in
`docs/decisions.md` records. The rest of the comment holds. The fix is the
reason that stands now, or none: the entry under "Plugins" settles that
antic writes the index.

### Stale sentences in the notes

`docs/notes/llvm.md:17`, documentation, a stale description in a note,
minor, new.

Each note was read against the code. These sentences differ from it:

- `llvm.md:17` says `back_end` of `driver.c` "puts in the checks of
  `--memory-checks`". `back_end` declares them with `memcheck_declare`.
  `llvm_back_end` at `driver.c:768` puts them in with
  `memcheck_function`, as its own comment says.
- `llvm.md:63` to 64 says the package header and the notice "are
  constants in sections of their own". The package header is, in
  `.anti_package`. The notice goes into `.rodata`, `__TEXT,__const` or
  `.rdata` in `llvm_emit_licenses` at `llvm_emit.c:3968`, the read-only
  section of each format. The DESIGN at `llvm_emit.c:3914` says the same
  of both.
- `flags.md:16` says a saturating product "clamps the result with
  selects". `saturating` of `llvm_emit.c:1875` clamps with `llvm.smax`,
  `llvm.smin` and `llvm.umin`, as its DESIGN says.
- `linker.md:13` and 22 stand in a finding above.
- `targets.md:7` and `floats.md:7` point at "Chapter 11" and "Chapter 17"
  of the book, which `CLAUDE.md` puts outside this repository. A reader of
  the notes cannot follow them.

`traces.md` describes the runtime, and its three sentences on antic hold:
the frame records of `"frame-pointer"="non-leaf"`, the file table as the
path under the search root and the entry named `module.main`. The rest of
`targets.md`, `flags.md` and `floats.md` holds, and the tests they name
exist under the names `tests/CMakeLists.txt` builds.

## The sentence of each file

For each file of this step, the sentence of its purpose as read from the
code. "Header: says so" means the first comment of the `.c` file or of
its header says the same. "Header: one item" means the first comment
describes one function or one table. "Header: none" means neither file has
one.

- `src/antic/llvm_emit.c`: the translation of a laid-out module of the IR
  into LLVM IR text, with the small texts of a package header, a bundle
  marker, a notice and the names of a `.def` file. Header: says so, in
  `llvm_emit.h` and in the DESIGN of the `.c`, which also lists the steps
  it was built in.
- `src/antic/llvm_debug.c`: the debug metadata of the LLVM text, the
  compile unit, the files, the subprograms and the locations, and the
  spans the build id leaves out. Header: says so, in `llvm_debug.h`. The
  `.c` opens with the comment of `llvm_metadata_string`.
- `src/antic/llvm_run.c`: the runs of opt and llc on the text, with the
  options of each mode. Header: says so, in `llvm_run.h`. The `.c` opens
  with the comment of `file_name`.
- `src/antic/llvm_target.c`: the triple, the data layout, the relocation
  model and the processor attributes the text carries per target and
  level. Header: says so, in `llvm_target.h`. The `.c` opens with the
  comment of the layout table.
- `src/antic/abi.c`: the classification of the parameters and the result
  of a function for the C convention of a target. Header: says so, in
  `abi.h`. The `.c` opens with the comment of the register counts.
- `src/antic/driver.c`: the files of a compilation and the run of it
  through the front end, lowering, the passes and the back end. It ends
  with the text, the library file or the header. Header: says so, with
  "the assembler" that is gone.
- `src/antic/driver_index.c`: the plugin index beside a plugin, written
  with the entries of the other libraries kept. Header: says so.
- `src/antic/driver_library.c`: a library for C or a plugin, the archive
  with its package header and bundled runtime, or the shared library with
  its exports. Header: says so.
- `src/antic/driver_link.c`: the link of a program, its facts, inputs,
  Windows paths, host exports and native libraries. Header: says so.
- `src/antic/driver_search.c`: the library files of a compilation, found
  under the roots and loaded in dependency order, with the link lines they
  carry. Header: says so.
- `src/antic/linker.c`: the command lines of lld, the platform linkers and
  llvm-ar for a program, a shared library, an archive and a relocatable
  object of each target. Header: none. `linker.h` describes
  `struct link_inputs`, and the `.c` opens with the comment of
  `add_inputs`.
- `src/antic/coff.c`: the join of COFF objects into one relocatable
  object, and the names the linked members of a COFF archive define.
  Header: says so, in the DESIGN of the `.c`. `coff.h` describes the input
  struct.
- `src/antic/applesdk.c`: the newest SDK of the Command Line Tools that
  ld64.lld reads. Header: one item. The DESIGN of `applesdk.h` says why
  Anti fetches no SDK, and the `.c` opens with `version_number`.
- `src/antic/target.c`: the table of the six targets, their names and the
  symbols of functions and of C names per object format. Header: one
  item, the names table. `target.h` opens with the macOS version.
- `src/antic/cpu.c`: the table of the processor levels, their features,
  their clang arch and the default of each target. Header: one item, the
  table.
- `src/antic/header.c`: the C header of a library for C, written from the
  interfaces of the module and its libraries. Header: says so, in
  `header.h`. The `.c` opens with the comment of `scalar_name`.
- `src/antic/notice.c`: the licence notice of a set of packages and the
  identifier of a component of the runtime archive. Header: none, as the
  tool pass found.
- `src/antic/docpage.c`: the records of the page of `anti doc`, built from
  the interface or the syntax tree of a module. Header: says so.
- `src/antic/main.c`: the command line of antic and the run it asks the
  driver for, on a thread with a stack of its own. Header: none, as the
  tool pass found.
- `src/antic/antic.c`: the walks over a source and a library file that
  `anti` asks for through `antic.h`. Header: says so.
- `src/antic/platform.c`: the platform layer of antic and anti, files,
  environment, arguments, paths, processes and the work thread, once per
  host. Header: says so.
- `src/antic/userdirs.c`: the directories of a user-local install on each
  platform and the runtime archive of this machine. Header: one item, the
  DESIGN of `absolute`. `userdirs.h` says why the install is local to the
  user.
