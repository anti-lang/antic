# Structure audit

The architecture of the whole repository at `7746a9cc`: the parts of
`antic`, `anti` and the runtime, their boundaries, the direction of every
dependency, the one place of each concern, and the path a reader takes
from `CLAUDE.md` to a function. The evidence is the include graph, the
call graph, the duplication list, the first comments and the named files
of `docs/audit/data/`, and the code read at every place named below.
Nothing was built or run, apart from `tools/docs-style/check_docs.py` over
every `.md` file of the tree. The findings of `docs/audit/tool-pass.md`
belong to the same audit and are not repeated here. The answers at the end
cite them by title. Each finding was compared with the reports of
`docs/audit/2026-09-23/` and `docs/audit/2026-09-30/` and with
`docs/audit/2026-09-30/closure.md`. Every finding below is new. The
closure records a fix for each finding of the second structure report, and
the places read again for this report, the teardown, the runtime contract,
the test of a copy and the readers of files, hold it.

## Counts

| Severity | Findings | New | Returned | Still open |
|---|---|---|---|---|
| Severe | 0 | 0 | 0 | 0 |
| Major | 1 | 1 | 0 | 0 |
| Minor | 13 | 13 | 0 | 0 |

| Severity | Standard | Findings |
|---|---|---|
| Major | Documentation, the decisions describe the files that exist | 1 |
| Minor | Structure, a dead option of a replaced feature | 1 |
| Minor | Structure, placement and boundaries | 3 |
| Minor | Structure, one way to report an error | 1 |
| Minor | Safety and C, rule 21 | 1 |
| Minor | Safety and C, rule 26 | 3 |
| Minor | Documentation, a stale name or a header that says less | 4 |

## Major

### The decisions describe the native back end as current

`docs/decisions.md:1026`, documentation, the decisions describe the files
that exist, major, new. The tool pass raised eight of these lines as a
question for this step and graded nothing.

The native back end left in the step `switch` of
`docs/work-order-llvm-back-end.md`, and `docs/audit/data/back-end-mentions.txt`
finds no word of it under `src/`. `docs/decisions.md` keeps about twenty
entries, most of them `[provisional]` and so binding until Eddie changes
them, that state a rule or a state of the code that is gone:

- 916 and 918, the spill weights of the register allocator and the
  borrowed scratch register, which "changes the allocation order of
  `docs/notes/regalloc.md`". No such note exists.
- 941 to 943, whose reasons cite `locate` of `src/antic/arm64.c` and of
  `src/antic/x86_64.c`, and 943 says "the two back ends differ on this one
  case until the step `switch` removes the native one".
- 944, 1020, 1024 and 1026, which give `--backend llvm`, `BACKEND=llvm`,
  `antic --backend llvm` and "The CMake option `ANTIC_BACKEND`, `native` or
  `llvm`, names the back end of a build of antic without `--backend`",
  with `--dump-select` and `--dump-alloc`. No build file, test or source
  holds any of the four.
- 945, 1012, 1014 and 1027, which make the linkage, the sections and the
  import entries follow "the symbols of `emit.c`".
- 1013 and 1025, which refuse a COFF plugin and a COFF host "until the
  step `flags`", and 1034, the test `mixed_backends`, which the suite no
  longer has.
- 1227 and 1228 under "Debug information", where `antic -g` writes `.file`
  and `.loc` directives that llvm-mc turns into a line table and "antic
  writes the compile unit itself", beside 1220, which the tool pass
  reported for `src/antic/debug.c`.

A reader of the first authority looks for a CMake option, a note and three
files that do not exist. Whether each entry becomes history in the past
tense, moves to the work order it came from, or goes is Eddie's decision.

## Minor

### A dead option of the native back end runs through antic, anti and the suite

`src/antic/driver.h:23`, structure, a dead option of a replaced feature,
minor, new.

`--llvm-mc` named the assembler of the native back end. `driver.h:23`
says antic "accepts it for the tools that pass it and no longer runs"
it, `main.c:464` stores it, and no file of `src/antic/` reads the field.
The tools that pass it are anti and the suite:

- `src/anti/main.c:292` and `710` read the option for `build`, `run` and
  `test`, `build.c:193` and `test.c:57` write it into the options of every
  compile, and `test.c:44` carries it as a parameter.
- `tests/program_output.cmake:33` passes it to every program build,
  `tests/CMakeLists.txt:183` to every dev object, and
  `docs/audit/data/options-unexercised.txt` counts the option in 46 test
  files. `tests/CMakeLists.txt:1048` derives the directory of the LLVM
  tools from the path of llvm-mc, and each recipe of `src/native/` hands
  `-DLLVM_MC` to `tests/run_program.cmake`, as `pcre2.cmake:100` does.
- `docs/decisions.md:929` lists it among the driver options.

`docs/distribution.md:131` records that no build runs llvm-mc and that the
tool leaves the package on a separate decision, so the tool itself is
decided. The option and its plumbing are the remnant. The fix drops the
option from both tools and the suite, and keys the tool directory of the
suite on a tool antic runs.

### The records every part shares stand in the interface of the tool

`src/antic/antic.h:4`, structure, placement, minor, new.

The DESIGN comment of `antic.h` says it is "what the anti tool may use of
antic", and the test `anti_interface` pins that reading. The same header
holds what every part of the compiler needs: `ANTL_SUFFIX` at 25, the
layout of the runtime archive from 41, `enum linker` at 92, `enum lto` at
106 and `struct package` at 134. So the front end, the library file and
the back end include the interface of their client: `sema.h:8` for the
package of an interface at 178, `linker.h:7` for the two enums at 23 and
56, `llvm_run.h:6` for `enum lto` at 30, `notice.h:10` for the package,
`userdirs.c:6` for `RUNTIME_LIB_DIR` and `llvm_emit.c:11` for
`RUNTIME_BUNDLE_MARKER`. The same holds for the module names
`REGEX_MODULE` and `MEM_MODULE` of `types.h:517` and `611`, which
`driver_link.c:16` and `whole.c:11` include the type table of the checker
for. A reader who follows an include of the checker lands in the page of
the tool. One header of names and records, which `antic.h` includes, would
make the graph say what the parts are.

### The inside header of the library file is included outside it

`src/antic/antl_io.h:12`, structure, a boundary, minor, new.

The header ends its comment with "No other file includes this one", and
`src/antic/antic.c:12` includes it, for `antl_magic` at `antl_io.h:126`,
which `antic_library_starts` at `antic.c:53` compares. The driver side of
the interface so reads the inside of the library part, and the comment
is wrong. One function `antl_starts` in `antl.h`, beside `antl_header`,
holds the magic where the format owns it.

### Three tests of the program's main, two rules

`src/antic/reach.c:154`, rule 26, minor, new.

Whether an IR function is the main of the program is written three times,
with `"main"` as a literal in each. `is_main_of` at `reach.c:154` takes a
function of the unit of the entry, by `ir_in_unit`. `is_main` at
`llvm_emit.c:3257` and `has_main` at `driver.c:660` take a function whose
module is the compiled module, by `strcmp`. The two rules differ in what
they test, a unit against a module, and nothing says whether the
difference is meant. One helper of `ir.h` would hold the rule, as
`ir_is_patterns_start` at `ir.c:395` holds the one of the pattern
constructor.

### A file's existence tested by opening it, beside the helper that does so

`src/antic/driver_search.c:50`, rule 26, minor, new.

`driver_file_exists` at `driver.c:393` opens a file and closes it to
answer, and `driver_parts.h` declares it for every file of the driver.
`search_roots` at `driver_search.c:50` and `find_crt_dir` at
`driver_link.c:43` open and close a file themselves to ask the same
question, in two more copies of the four lines.

### Two functions that nothing of the program calls

`src/antic/target.c:151`, rule 26, minor, new.

`target_block_label` writes the label of a block as "llvm-mc leaves a label
that starts with L out of a Mach-O symbol table", its DESIGN at 149 says.
Its only caller is `tests/unit/test_target.c:27`. The LLVM text names its
blocks itself, so the function and its rule are a remnant of the native
back end. `rt_record_items` at `rt_abi.c:58` is likewise called by the
unit tests alone. `docs/audit/data/unused-static.txt` cannot see either,
since both are exported.

### Five includes that contribute nothing

`src/antic/linker.c:9`, rule 21, minor, new.

A script read the names each header declares against its file, outside
comments. For each of these includes no name appears:

- `linker.c:9` and `coff.c:11` include `ir.h`. These two are what makes
  `data/include-graph.txt` say the linker and the COFF joiner depend on
  the IR.
- `ir_verify.c:10` includes `cpu.h`.
- `header.h:7` includes `antic.h`, and `header.c` uses nothing of it
  either.
- `antl.h:8` includes `antic.h`, while `antic.c:4`, the one file that
  needs both, includes `antic.h` itself.

Dropping the five makes the include graph show the boundaries as they
are.

### A compiled Python file is tracked under tools/

`tools/docs-style/__pycache__/check_docs.cpython-314.pyc`, structure,
placement, minor, new.

`git ls-files` holds the file, committed in `bbc83d1d`. CLAUDE.md says
"Scratch work, temporary files and generated output go under `build/`
only", `.gitignore` has no line for it, and the test `repo_layout` keeps
the directories of `tests/` and `docs/` and does not look under `tools/`.
The fix removes the file from the index and ignores `__pycache__/`.

### The prefix of a message is spelled at every site, and three sites differ

`src/anti/bindclang.c:101`, structure, one way to report an error to the
user, minor, new.

The compiler reports a problem in a program through `diagnostic.c`, and
the runtime through `assert.c`, as rule 13 asks. What a tool says about a
file, a tool or an option has no function of its own in either tool. Each
site writes the name of the program as a literal:

| Program | Sites of `fprintf(stderr, "anti...` | Files |
|---|---|---|
| antic | 94, and 5 `fputs` | 9 |
| anti | 123, and 5 `fputs` | 21 |

`bindclang.c:101` writes "bind --clang needs clang %d" without the
prefix. In the runtime, where every call of the failure routine spells
the prefix as well, `lock.c:104` and `snapshot.c:22` write "out of memory
for" and the count without it, while `mem.c:39` writes `anti: out of memory for %lld
bytes`, the form the entry under "Files of lowering" in `docs/decisions.md`
gives. One function per tool that takes the message and writes the
prefix, and one for the runtime's out-of-memory message, would hold the
form.

### Comments and documents still name llvm-mc as the assembler

`src/antic/cpu.c:7`, documentation, a comment that describes what is gone,
minor, new.

`docs/distribution.md:131` says "no build runs llvm-mc". These say the
opposite, in the present tense:

- `src/antic/cpu.c:7`, "attributes let llvm-mc assemble the instructions
  the level adds", and `cpu.h:49`, "The -mattr= value of llvm-mc".
  `cpu_attributes` is read by `main.c:131` alone, for `--print-cpu-levels`,
  and `llvm_target.c` carries the features llc reads.
- `src/antic/target.c:17`, "The triples select the object format in
  llvm-mc", `target.c:149` and `target.h:47`, "the -triple option of
  llvm-mc". `llvm_triple` at `llvm_target.c:64` is what reads the triple.
- `src/antic/llvm_target.c:36`, "the llvm-mc attributes".
- `CLAUDE.md:44` and `docs/decisions.md:35`, "clang compiles, lld links,
  llvm-mc assembles and llvm-ar archives". CLAUDE.md says the four tool
  documents agree with the decisions, and here two disagree with the
  third.
- `tools/get-llvm.cmake:3`, "llvm-mc assembles", and
  `tests/CMakeLists.txt:4` and `1045`, where llvm-mc stands first among
  the tools of the archive.

### Ten claims of where a function stands have moved

`docs/notes/hashing.md:8`, documentation, a stale name in a document,
minor, new.

A script read every "`function` of `file.c`" claim of `docs/notes/` and
`docs/decisions.md` against the code. Ten name a file the function has
left:

- `check_hook` of `sema.c`, at `hashing.md:8` and `iteration.md:9`, is
  in `sema_class.c`.
- `same_value` and `hash_value` of `src/rt/object.c`, at
  `hashing.md:29`. Neither exists in the runtime, and a `hash_value`
  stands in `src/antic/lower_hash.c`.
- `interface_item` of `doc.c`, at `hashing.md:41`, is in `docpage.c`.
- `compatible_line` of `parser_decl.c`, at `versions.md:41`, is
  `parser_compatible_line`.
- `sema_doc_warnings` of `sema_export.c`, at `check.md:76`, is in
  `sema_doc.c`.
- `each_case` of `lower.c`, at `variants.md:16`, is in `lower_owning.c`.
- `root_names` of `lower_desc.c`, at `hooks.md:10` and
  `docs/decisions.md:1494`, is in `sema_class.c`.
- `sema_needs_teardown` of `sema_stmt.c`, at `docs/decisions.md:2346`, is
  in `sema_value.c`.

The splits of the second audit moved them, and the documents were not
read against the new files. The script is four lines and would run as a
test.

### The path from CLAUDE.md ends at the directory for tests/ and tools/

`tests/CMakeLists.txt:1`, documentation, a directory whose contents no
document lists, minor, new. The tool pass reports the same for `src/` as
major, and this finding covers what it left out.

`CLAUDE.md` gives `tests/` and `tools/` one sentence each. Below that, the
first comment of `tests/CMakeLists.txt` describes `tests/programs` and
`tests/unit` alone, and `tests/run_repo_layout.cmake:24` lists the other
twenty-four directories by name without a purpose. Six of them appear in
no document at all: `anti-build`, `anti-test`, `check`, `framework`,
`opt` and `raw`. Of `tools/`, five files are named by no document:
`fetch-release.cmake`, `host-compile.cmake`, `libc-versions.cmake`,
`uninstall.sh` and `uninstall.ps1`, although each opens with a comment
that says what it is for. The closing section gives the sentence of each
directory as read from its contents.

### The Windows platform file is two parts under one sentence

`src/rt/platform_windows.c:1`, documentation, a header that says less than
the file does, minor, new.

The header says "The platform layer on Windows. See platform.h.", and
`platform.h:1` says the layer is "what the rest of src/rt asks of the
system it runs on". From line 1162 to the end, 413 lines, the file holds
"The static part of the C runtime, for the runtime archive alone":
`_tls_index`, `_fltused`, `__security_cookie`, `DllMainCRTStartup`,
`_assert`, `opendir` and `readdir`, which the compiler, the linker and the
loader look for and nothing of `src/rt` asks for. Rule 25 and the entry
under "Binary distribution" record the part, and `platform_entry.c` and
`platform_stdio.c` hold two more pieces of it in files of their own. The
sentence of the file should name both halves, or the half from 1162
should take a file of its own beside those two.

## The structure questions

### One purpose per module and file

The groups of `src/antic/`, judged from `data/call-graph.txt`, the
headers and the first comment of each file:

| Group | Files | Interface | Inside header | Judgement |
|---|---|---|---|---|
| `parser_*` | `parser.c` and six | `parser.h`, one function | `parser_parser.h` | One part. `parser.c` is the hub every file calls, the mutual calls follow the grammar, and each file's header names one purpose that the file keeps. |
| `sema_*` | `sema.c` and seventeen | `sema.h` | `sema_checker.h` | One part. `sema.c` holds the symbol table and the passes, which the other files call, apart from `sema_chain.c` and `sema_doc.c`, which read the module alone. `sema_expr.c`, `sema_call.c`, `sema_operator.c` and `sema_stmt.c` call each other as expressions hold blocks. Each file keeps its sentence. `sema_stmt.c` passed 3000 lines again, which the tool pass reports. |
| `lower_*` | `lower.c` and sixteen | `lower.h` | `lower_lowerer.h` | One part. `lower.c` holds the helpers and `lower_expr.c` the dispatch over expressions, which every feature file calls back to lower its operands. Each file keeps its sentence, and the owning values stand in `lower_owning.c` alone. |
| `antl_*` | `antl.c` and six | `antl.h` | `antl_io.h` | One part. `antl_io.c` is the floor nothing calls out of, and `antl_tree.c` reads and writes one section, which is one purpose. The inside header is included by `antic.c`, the fourth finding. |
| `driver*` | `driver.c` and four | `driver.h` | `driver_parts.h` | One part. `driver.c` is the orchestrator and the helper hub at once: the four other files call it 24 times for reading, writing and the existence of files and for the run of opt and llc, and it calls them 19 times. Each file keeps its sentence. |
| `llvm_*` | four | `llvm_emit.h`, `llvm_run.h`, `llvm_target.h` and `llvm_debug.h` | none | One part with one direction inside, `llvm_emit.c` calling `llvm_debug.c` and `llvm_target.c`. It has four headers rather than one: the driver includes two, `main.c` includes `llvm_target.h` to print the targets, and `linker.c` includes `llvm_run.h` for one threshold. |
| `whole*` | two | `whole.h` | `whole_parts.h` | One part, the analysis and the tables, calling each other 11 and 4 times through the shared walk. |
| the rest | 41 `.c` files | one header each | none | The front end, `lexer.c`, `types.c`, `header.c`, `docpage.c`, `diagnostic.c`, `warnings.c`, `modpath.c`, `pattern.c` and the two walks of the tree. The IR, `ir.c`, `ir_fold.c`, `ir_print.c`, `ir_verify.c`, `optimize.c`, `reach.c` and `rt_abi.c`. The back end, `layout.c`, `abi.c`, `coff.c`, `linker.c` and `memcheck.c`. The shared helpers, `antic.c` and `main.c`. Each file has one purpose, and no document groups them, which the tool pass reports. |

`src/anti/` is one file per command, `build.c`, `check.c`, `doc.c`,
`fmt.c`, `test.c`, `syms.c`, `bind.c`, `licensing.c`, `project.c` and
`sdk.c`, with `main.c` reading the options of each and dispatching, and
one file per helper: `files.c`, `platform.c`, `units.c`, `manifest.c`,
`deps.c`, `repo.c`, `zip.c`, `symmap.c`, `cursor.c`, `memreport.c`,
`jsontree.c` and the six files of the binding model. Every header opens
with one sentence that the file keeps, and the header of each helper
names the commands that use it. `units.c` holds the one definition of the
options every compile of a project starts from, and `build.c`, `check.c`,
`doc.c` and `test.c` build on it.

`src/rt/` is one file per concern, each with one sentence, and the
platform layer in five files. `symbols.c` holds the ELF and Mach-O readers
and the line table under one purpose, the symbols and lines of a binary.
`object.c` holds the root class with its default bodies and what
reflection reads of a descriptor. `platform_windows.c` is the one file
with two halves, the last finding. `std.h` declares the C side of six
modules and the failure routine under one comment, which is one purpose
by its own words.

### One-way dependencies

The runtime includes nothing of `antic` or `anti`: every file of
`src/rt/` in `data/include-graph.txt` lists no include across a part. The
headers the tools take from the runtime are `cpu_level.h`, `digest.h`,
`f16.h`, `grow.h`, `hash.h`, `json.h`, `license.h`, `plugin_index.h`,
`regex.h`, `toml.h` and `utf.h`, and each opens with a DESIGN comment that
says it is one definition read from both sides. The IR includes nothing of
the back end. Lowering includes `sema.h`, `types.h` and `ast.h`, and calls
the rules of the checker, which is the right way for the stage that
consumes the checked tree. `anti` reaches `antic` through `driver.h`,
`antic.h` and the shared helpers alone, 96 includes of 12 headers, and
calls `driver_run`, `driver_libraries`, `driver_frameworks`,
`driver_linux_libraries`, `driver_library_header`,
`driver_runtime_packages` and the `antic_*` walks. No file of `anti` calls
`driver_interface`.

Every include that runs against the direction:

- `docpage.c:13` into `ir.h`, the front end into the IR, which the tool
  pass reports under "The doc page of the front end builds an IR module".
- `src/anti/bindmodel.h:9` into `attributes.h`, past the list of
  `antic.h`, which the tool pass reports under "The comment of antic.h and
  the test anti_interface disagree".
- `sema.h:8`, `header.h:7`, `antl.h:8`, `linker.h:7`, `llvm_run.h:6` and
  `notice.h:10` into `antic.h`, the parts into the interface of their
  client, the third and the eighth findings.
- `antic.c:12` into `antl_io.h`, the driver side into the inside of the
  library file, the fourth finding.
- `linker.c:9` and `coff.c:11` into `ir.h` for nothing, the eighth
  finding. The link and the joiner need nothing of the IR.
- `whole.c:11` and `driver_link.c:16` into `types.h` for a module name,
  the third finding.

No call runs against the direction. The two-way calls inside the splits
are those of the table above.

### Each concern in one place

| Concern | The owner | Other places |
|---|---|---|
| Symbol table | `sema_lookup` and `sema_declare`, `sema.c:197` and `313`, over the scopes of `sema_checker.h` | The IR keeps no table. A function is found where it is needed, by its name against a constant, and the test for the program's main is written by two rules in three files, the fifth finding. `resolve_name` of `src/anti/doc.c:138` reads the tables a page carries, by the DESIGN above it. |
| Type rules | `types.c` for the types, `sema_require` of `sema_expr.c` for every conversion, called from ten files of the checker | `can_convert` and `check_cast` are static in `sema_expr.c` beside it. Lowering converts what the checker decided. |
| Layout | `layout.c`, for one target | `fixed_layout` of `sema_simd.c:219` measures a struct by the C rules for the cast of a simd struct, refuses every type whose layout differs by target, and was settled by S02 of the second audit. `header.c` leaves the C layout to the C compiler. |
| Teardown and copy | The rule is `sema_needs_teardown` at `sema_value.c:92`. The code is `lower_destroy_owned` at `lower_owning.c:194` and `lower_copy_owned` at `307`, and every teardown of a local, a parameter, a field and a dropped result goes through them. | `destroy_local` of `lower_stmt.c:858` is one call of the owner now. `lower_copy_parts` of `lower_expr.c:254` copies the lent parts of an iterator value, another operation. |
| Generic copies | `sema_copies.c` makes the items, `sema_generic.c` the types and the names, and `ir_is_copy_name` at `ir.c:133` is the one test of a copy | `antl_read_ir.c`, `llvm_emit.c` and `whole.c` call the one test. |
| File reading | antic: `read_file` at `driver.c:61` behind `driver_read_bytes`, `read_source` and `read_text`. anti: the `files_read` family at `files.c:192` to `215`. The runtime: `anti_rt_fs_read` of `fs.c`, which `conf.c`, `plugin.c` and `trace.c` call. | `sha256_file` streams a file through `platform_open`. Two existence tests beside the helper, the sixth finding. |
| Running a tool | antic: `driver_archive_tool` at `driver_link.c:65` finds a tool in `bin/` of the archive, and `process_run`, `process_run_in` and `process_capture` of `platform.c` run it: opt and llc from `llvm_run.c`, lld from `driver_link.c`, llvm-ar from `driver_library.c`. anti: the same three calls run clang, curl and tar, each by a decision, the built program and the runner. | None. |
| Reporting an error to the user | antic: `diagnostic.c` for a problem in a program. The runtime: `assert.c`. | The messages of both tools about files, tools and options, the tenth finding. |
| Platform layer of antic | `platform.c`, both halves under `#if`, and `target_host` with them | None. `data/platform-conditionals.txt` finds no host conditional outside the layers. |
| Platform layer of anti | `src/anti/platform.c`, over the layer of antic | None. |
| Platform layer of the runtime | `platform.h`, `platform_posix.c`, `platform_windows.c`, `platform_entry.c` and `platform_stdio.c` | The static C runtime inside `platform_windows.c`, the last finding. |
| The contract with the runtime | `rt_abi.h`, with the tests `runtime_functions` and `runtime_records` | None. `lower_rt_call` of `lower.c:1095` declares every call from its row. |
| Allocation | `alloc.c` of antic, "the one owner of the heap memory of antic". `files.c` of anti, by two provisional entries. `mem.c` of the runtime. | None. |

### Copies that drifted

Three copies have drifted: the prefix of a message at three sites, the
rule for the program's main, written by unit in one file and by module in
two, and the comments that name llvm-mc. The tool pass found two more,
the type walks `class_in` and `holds_mutex` and the operand comparison
`same_operand`. The other near blocks of `data/duplication.txt` were read:
the dispatch switches of `antl_read.c:876` and `lower_expr.c:2078`, of
`antl_tree.c:1254` and `llvm_emit.c:2953`, the `_Static_assert` tables of
`antl.c:17` and `antl_tree.c:31`, the free lists of `build.c:1086` and
`manifest.c:442`, and the `strcmp` chains of `bindtype.c:296` and
`main.c:247` agree once names are replaced and hold no logic twice.

### What is left of the native back end

The option `--llvm-mc` and its plumbing through anti and the suite, the
second finding. `target_block_label`, the seventh. The comments that name
llvm-mc as the assembler, the eleventh. The entries of `docs/decisions.md`,
the major finding. `tests/run_bench.cmake:14`, which the tool pass reports.
No dead branch of code was found: `data/back-end-mentions.txt` finds no
`regalloc`, `select.c`, `emit.c` or `--backend` under `src/`, and the two
exported functions nothing calls are the whole of the dead code the
script over `src/antic/` and `src/anti/` found.

### The path from CLAUDE.md to a function

`CLAUDE.md` gives every top-level directory and every directory of `src/`
one sentence in its "Layout" table, and "Repository layout" in
`docs/decisions.md` repeats the lists. Below that:

- `src/antic/`: the six "Files of" sections of `docs/decisions.md` give a
  sentence per file of six parts. They are the parser, the library file,
  the driver, the checker, lowering and the pass over the whole program.
  That is 55 of 96 files, and no document reaches the 41 others. The tool
  pass reports it.
  From the file, 25 files have no header comment and 198 public functions
  share a comment, which the tool pass reports as well.
- `src/anti/`: no document lists the directory. `docs/notes/build.md`,
  `bind.md`, `check.md`, `doc.md`, `fmt.md` and `symbols.md` name 24 of
  the 28 `.c` files inside their own topics, and `cursor.c`,
  `licensing.c`, `memreport.c` and `project.c` are named by none. Every
  file opens with its sentence.
- `src/rt/`: no document lists the directory. "Names and shared code of
  the runtime" in `docs/decisions.md` gives the rules, the entries of each
  feature name the files they touch, and `cast.c`, `errno.c`, `f16.c` and
  `license_stub.c` are named by none of `CLAUDE.md`, the decisions and the
  notes. Every file but `text.c` and `utf.c` opens with its sentence.
- `src/std/`: "State" in `CLAUDE.md` lists every module, and each file
  opens with a `//!` line.
- `src/native/`: the first comment of `src/native/CMakeLists.txt` lists
  the recipes in their order, and "Libraries and runtime" in
  `docs/decisions.md` holds the decisions of each.
- `tests/` and `tools/`: the path stops at the directory, the thirteenth
  finding.

The decisions and the notes describe the files by their current names
apart from `mach.c`, `debug.c`, `run_pcre2_pin.cmake` and
`get-media-sysroot.cmake`, which the tool pass reports, and the moved
functions of the twelfth finding. No notes file describes a design the
code no longer has: `docs/notes/llvm.md:219` speaks of the native back end
in the past tense, and `docs/notes/targets.md:8` and `llvm.md:4` to `8`
name the LLVM files. Of the DESIGN comments, `data/design-comments.txt`
marks 489 of 1243 as holding no reason word, and a sample of them reads as
giving the reason in another form, as the tool pass found. The docs-style
checker ran over every `.md` file of the tree, 408 files, and reported no
error and one warning, a filler risk at
`docs/work-order-llvm-optimization.md:10` for a word that is technical
there.

## The sentence of each file

The area of this step is the whole tree, so the table covers the
directories. For each, the one sentence of its purpose as read from its
contents, and whether the document that describes it, the "Layout" table
of `CLAUDE.md` for the top level and `src/`, the first comment of
`tests/CMakeLists.txt` for `tests/`, says the same. "Document: none" means
no document gives the directory a sentence.

| Directory | The sentence, as read | Document |
|---|---|---|
| `build/` | Every build tree, the downloads in `deps/`, the driver state in `drive/` and the scratch of a session, never committed. | Says it. |
| `docs/` | The specifications, the decisions, the notes, the reports, the audits and the site pages. | Says less: "The specifications, the decisions and the reports". |
| `docs/audit/` | The reports and data of the current audit, and the two earlier audits under their dates. | Says it. |
| `docs/audit/data/` | The machine evidence of the tool pass, one file per measurement. | `tool-pass.md` lists each file. |
| `docs/audit/2026-09-23/` and `2026-09-30/` | The reports of the first and the second audit with their data. | Says it. |
| `docs/notes/` | The choices inside one pass or one feature, one file per topic. | Says it. |
| `docs/reports/` | One report per session, dated and named after its subject. | Says it. |
| `docs/site/` | The design pages served from anti-lang.com, one directory per page: the build tool, the runtime archive and the standard library. | Says it. |
| `LICENSES/` | The licence texts of the six native libraries the runtime archive bundles. | Says it. |
| `src/` | The five source trees. | Says it. |
| `src/antic/` | The compiler, and the library of it that `anti` links. | Says "The compiler", and the "Files of" sections cover six of its parts. |
| `src/anti/` | The `anti` tool, one file per command and one per helper. | Says "The `anti` tool", and no document lists its files. |
| `src/rt/` | The runtime of every program, the headers it shares with the tools and the C side of the standard library. | Says "anti_rt sources, starting with `src/rt/start.c`, 0BSD", and no document lists its files. |
| `src/std/` | The standard library in Anti. | Says it. |
| `src/std/anti/` | The modules under the root `anti`, one file per module. | "State" in `CLAUDE.md` lists them. |
| `src/std/anti/collection/` | The collection modules, one file per family. | Says it. |
| `src/native/` | The recipes that fetch, check and build the six native libraries for every target. | Says it, and its own `CMakeLists.txt` lists the recipes. |
| `tests/` | The suite: the runner scripts flat in it, and one directory per kind of test. | Says it. |
| `tests/abi/` | Programs whose values cross to C and back, checked against C for struct passing, simd structs and two native libraries. | Named in documents, no sentence. |
| `tests/anti-build/` | The projects that the tests of `anti build` and `anti run` build. | Document: none. |
| `tests/anti-symbols/` | The project the tests of `anti symbols` read. | Named in documents, no sentence. |
| `tests/anti-test/` | The project the tests of `anti test` run. | Document: none. |
| `tests/bench/` | The benchmark programs with their C twins, and the ablation under `ablate/`. | Named in documents, no sentence. |
| `tests/bind/` | The headers and descriptions `anti bind` reads and the modules it must write. | Named in documents, no sentence. |
| `tests/check/` | One project per class of `anti check`, each with its outcome. | Document: none. |
| `tests/clib/` | Libraries for C built from Anti and the C programs that call them. | Named in documents, no sentence. |
| `tests/conf/` | Runtime configuration files and the patterns of what a program prints under each. | Named in documents, no sentence. |
| `tests/doc/` | The sources and the expected pages of `anti doc`. | Named in documents, no sentence. |
| `tests/dump/` | The goldens of every `--dump-*` stage per program and target, with `c_long`, `header` and `size_of` below. | Named in documents, no sentence. |
| `tests/emit-identity/` | The digests of the LLVM text of every program on the six targets. | Named in documents, no sentence. |
| `tests/errors/` | Programs that must fail to compile, each with the messages it must give. | Named in documents, no sentence. |
| `tests/fmt/` | A source and its canonical form for `anti fmt`. | Named in documents, no sentence. |
| `tests/framework/` | A program of `link framework`, which only Apple's SDK links. | Document: none. |
| `tests/inject/` | The projects and patterns of injection and of `--anti.inject`. | Named in documents, no sentence. |
| `tests/link-identity/` | Programs whose linked bytes are pinned by digest per target. | Named in documents, no sentence. |
| `tests/modules/` | Programs of more than one module and the tests of library files. | Named in documents, no sentence. |
| `tests/opt/` | The IR of a program before and after the optimizer. | Document: none. |
| `tests/plugin/` | Plugin hosts, the libraries they load and the versions they refuse. | Named in documents, no sentence. |
| `tests/programs/` | Programs that compile, run and must print their expected output. | `tests/CMakeLists.txt:1` says it. |
| `tests/raw/` | A program whose raw bytes the runner reads, so no host translates them. | Document: none. |
| `tests/std/` | The tests of the modules of the standard library. | Named in documents, no sentence. |
| `tests/trace/` | Programs of hooks and tracing with the patterns of their output. | Named in documents, no sentence. |
| `tests/traps/` | Programs that must trap under the checks of a dev build, each with its message. | Named in documents, no sentence. |
| `tests/unit/` | The unit tests in C, which link the core of antic and the files of anti. | `tests/CMakeLists.txt:1` says it. |
| `tools/` | The helper scripts, the pins, the installers and the docs-style checker. | Says it. |
| `tools/docs-style/` | The pinned docs-style checker and its list of banned terms. | Says it. |
| `tools/keys/` | The public key that checks the LLVM tools, and the ignored private key. | Says it. |
