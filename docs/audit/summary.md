# Summary of the third audit

This document merges the eleven reports of the third audit under
`docs/audit/` and orders the work they found. The reports are `tool-pass`,
`structure`, `front-end`, `middle`, `back-end`, `library-file`, `rt`, `std`,
`anti-tool`, `build-and-tools` and `tests`, each named here by the stem of
its file. They audited `f3af7693`, `7746a9cc` and `69fc7bee`. `git diff`
shows no change under `src/`, `tests/`, `tools/` or the two root build
files from `f3af7693` to this commit. Every path:line below so points at
the code the reports read. The code at every severe finding was read again
for this summary, and each stands at its line. The major and minor findings
are taken as their reports give them. Nothing was built and no suite ran.

A finding that two or more reports list stands here once, with every report
that lists it. Where the reports grade one finding differently, the summary
takes the higher grade and says so. Three standards apply. They are safety
and C by `docs/c-guidelines.md`, structure, and documentation. The first
audit wrote its ids with one digit and the second with two. The ids here
have three digits, S301 to S335 and M301 to M371, so no number names two
findings. `docs/decisions.md:2126` and `2292` and
`tests/runners/run_anti_build_inputs.cmake:12` cite `docs/audit/summary.md` and
mean the summaries of the earlier audits, which stand under their dates.

## Counts

The eleven reports list 297 findings: 35 severe, 84 major and 178 minor.
Merged, they are 262.

| Report | Severe | Major | Minor |
|---|---|---|---|
| tool-pass | 0 | 4 | 10 |
| structure | 0 | 1 | 13 |
| front-end | 5 | 8 | 26 |
| middle | 0 | 5 | 14 |
| back-end | 2 | 4 | 15 |
| library-file | 3 | 5 | 15 |
| rt | 14 | 15 | 20 |
| std | 7 | 10 | 19 |
| anti-tool | 2 | 9 | 15 |
| build-and-tools | 2 | 17 | 20 |
| tests | 0 | 6 | 11 |
| Listed | 35 | 84 | 178 |
| Merged | 35 | 71 | 156 |

| Standard, merged | Severe | Major | Minor | Total |
|---|---|---|---|---|
| Safety and C | 19 | 20 | 67 | 106 |
| Structure | 0 | 25 | 52 | 77 |
| Documentation | 16 | 26 | 37 | 79 |
| Total | 35 | 71 | 156 | 262 |

| Status against the earlier audits, merged | Severe | Major | Minor |
|---|---|---|---|
| New | 32 | 68 | 146 |
| Returned | 0 | 2 | 1 |
| Still open, in whole or in part | 3 | 1 | 9 |

The merged findings of safety and C by rule. A finding under two rules
stands at the first its report names. "Bar of tests" is the sentence of
"Scope and bar" that a test must hide no failure.

| Rule | Severe | Major | Minor |
|---|---|---|---|
| 1 | 0 | 0 | 2 |
| 3 | 2 | 0 | 3 |
| 4 | 1 | 1 | 1 |
| 5 | 1 | 0 | 3 |
| 6 | 4 | 0 | 3 |
| 8 | 0 | 0 | 1 |
| 9 | 0 | 0 | 3 |
| 10 | 0 | 0 | 2 |
| 11 | 2 | 3 | 2 |
| 12 | 1 | 1 | 4 |
| 13 | 0 | 0 | 1 |
| 14 | 5 | 7 | 11 |
| 15 | 0 | 3 | 1 |
| 16 | 0 | 0 | 1 |
| 17 | 0 | 0 | 1 |
| 18 | 0 | 0 | 1, covering 142 functions |
| 19 | 0 | 0 | 1, covering 2 files |
| 20 | 0 | 0 | 1 |
| 21 | 0 | 0 | 1 |
| 22 | 0 | 2 | 1 |
| 23 | 3 | 1 | 2 |
| 24 | 0 | 0 | 2 |
| 25 | 0 | 0 | 1 |
| 26 | 0 | 0 | 15 |
| The warnings rule of `CLAUDE.md` | 0 | 0 | 1 |
| Bar of tests | 0 | 2 | 2 |
| Total | 19 | 20 | 67 |

The merged findings of structure by question.

| Question | Severe | Major | Minor |
|---|---|---|---|
| One purpose per module and file | 0 | 3 | 1 |
| One-way dependencies, boundaries, a part doing the work of another | 0 | 4 | 4 |
| Each concern in one place | 0 | 5 | 20 |
| Copies that drifted | 0 | 9 | 4 |
| Left of a replaced feature | 0 | 2 | 4 |
| Placement and names | 0 | 0 | 15 |
| State without an owner | 0 | 0 | 2 |
| Tests: goldens, expected texts and reach | 0 | 2 | 2 |
| Total | 0 | 25 | 52 |

The merged findings of documentation by question.

| Question | Severe | Major | Minor |
|---|---|---|---|
| A header, file or comment whose documented behaviour differs from the code | 16 | 8 | 12 |
| A file without a header comment, or one that says less than the file | 0 | 2 | 3 |
| A public function or item without a comment | 0 | 3 | 0 |
| The path from `CLAUDE.md` to a file, and the "Files of" sections | 0 | 2 | 1 |
| A note or a document that describes a design the code no longer has | 0 | 11 | 10 |
| Comments that name what is gone | 0 | 0 | 4 |
| A DESIGN comment without a reason, or with one that is gone | 0 | 0 | 4 |
| Rule 27, a comment above the wrong item or a banned phrase | 0 | 0 | 3 |
| Total | 16 | 26 | 37 |

Three reports list defects that no rule names and count none of them.
`anti-tool` lists eleven, among them the cache key of `anti build` at
`src/anti/build.c:120`. `rt` lists four under "Outside the rules". `std`
lists five under "Not confirmed". They stand in their reports, and fix steps
21 and 22 take those of `anti`. Step 22 fixed defects 1, 2, 4, 5, 8, 9, 10
and 11, and found defect 3 fixed by `bfe03ca0`.

### Merges and grades the reports disagree on

- The decisions on the native back end are one finding, S323. `back-end`
  grades lines 1219 to 1231 of `docs/decisions.md` severe. `structure`
  grades about twenty entries of the same kind major. Severe here, since
  two entries of the first authority disagree and the code follows one.
- The text a builder gives when memory runs out is one finding, S312.
  `rt` grades `src/rt/text.c:155` severe and `std` grades
  `src/std/anti/text.anti:200` major. Severe here.
- The files of `src/` without a header comment are one finding, M346.
  Seven reports list them. Together they name 64 of the 299 files, and six
  more headers that say less than their file.
- The public functions of a header without a comment are one finding,
  M348, from `tool-pass`, `front-end`, `rt` and `anti-tool`.
- The directories no document lists are one finding, M351. `tool-pass`
  grades `src/` major, `tests` grades `tests/` major and `structure` grades
  `tests/` and `tools/` minor. Major here.
- The layouts written on both sides of the runtime contract are one
  finding, M332. `rt` grades it major, and `middle` lists the four enums
  of antic under a minor finding of documentation.
- The builds of the runners that accept any output are one finding, M343.
  `build-and-tools` and `tests` each grade it major, and `tests` lists the
  copied helpers again as minor.
- Eleven minor findings absorb 21 more. They are the option `--llvm-mc`,
  the comments that name llvm-mc, the records in `antic.h`, rule 18 and
  rule 19. They are also the moved function names of the notes, the
  chapters of the book, the flat runners, the registrations and the header
  of `tests/runners/run_anti_build.cmake`.
- `tool-pass` counts its rule 26 findings under structure. They stand under
  safety and C here, as every numbered rule does. Its finding on two
  options without a test stands under the bar of tests.

## Severe

Every severe finding. "Reports" names each report that lists it. Paths
without a directory are in `src/antic/`.

### Safety and C

| ID | Place | Rule | Finding | Status | Reports |
|---|---|---|---|---|---|
| S301 | `types.c:1987`, `sema_const.c:101` | 14 | A class whose base was refused keeps a NULL base field, set at `sema.c:2516`. `pointer_free` and `class_value` read through it after the error. | fixed `184e5a39` | front-end |
| S302 | `sema_copies.c:197` | 3 | `xf` compares a field pointer with the bounds of arrays it does not lie in, which C11 6.5.8 leaves undefined. No target misreads it. | fixed `115e35d9` | front-end |
| S303 | `pattern.c:1126` | 3, 16 | The least match length adds in `long` before its cap. On Windows the sum wraps, and a `with` that does not fit compiles there and is refused on the Mac. | fixed `ffeda0a3` | front-end |
| S304 | `antl_verify.c:282` | 14 | The verifier holds no field name of a generic tree to its struct. `lower.c:902` then subtracts from NULL, and `layout.c:451` indexes outside the offsets. | fixed `9555cceb` | library-file |
| S305 | `antl_read.c:271` | 14 | Pointers, slices, arrays, function types and tuples of the type table nest without a limit. A file of 2 MB overflows the stack in `print_type` and `antl_visit_type`. | fixed `04dbb192` | library-file |
| S306 | `src/rt/trace.c:128` | 11 | A frame keeps the build id as a pointer into the image of its module. After `unload` of a plugin, `anti_rt_trace_text` reads memory that is gone. `trace.c:323` is the second place. | fixed `06e06339` | rt |
| S307 | `src/rt/registry.c:94` | 23 | `reflect.new` and `Object.deserialize` give the lock back before `build` calls into the library, so `unload` on another thread closes it first. | fixed `b80a1e46` | rt |
| S308 | `src/rt/hooks.c:81` | 23 | The `destroyed` hook counts an object out before its `destruct` and field teardown run. `unload` then closes the library under the thread still in it. | fixed `cfa95068` | rt |
| S309 | `src/rt/plugin.c:383` | 6 | The loader replaces the table of an object with a stub table sized by the interface. A class that inherits the interface at offset 0 then calls past it. Read, not run. | fixed `45aed114` | rt |
| S310 | `src/rt/plugin.c:589` | 14, 15 | `damage_of` tests the counts of a plugin table for a sign alone, and `fields_sound` accepts an offset without the width of the field. The test tries -1 and NULL alone. | fixed `32400589` | rt |
| S311 | `src/rt/snapshot.c:30` | 6, 10 | A `str` inside a heap snapshot has no NUL after it, since `lower_function.c:499` sizes the block without one. `s.ptr` as a C string reads past the block. | fixed `95b0a747` | rt |
| S312 | `src/rt/text.c:155`, `src/std/anti/text.anti:200` | 11, 4 | A builder that gets no memory gives the literal `""`, which the caller is told to free. `log.anti:197` puts it into an `own` field. | fixed `07caeed4` | rt, std |
| S313 | `src/std/anti/log.anti:404` | 14 | A logger file that names an unknown kind of sink calls `Log.get()` inside `construct` of `Log` and recurses until the stack is gone. Read, not run. | fixed `d385f56c` | std |
| S314 | `src/std/anti/trace.anti:230` | 23, 6 | `Profiler`, `ErrorMonitor` and `ChangeJournal` write plain tables from every thread. Two threads take `count` past the array in a release build. | fixed `bd5e69c6` | std |
| S315 | `src/std/anti/collection.anti:761` | 6 | The five place functions of `Collection<T>` are public and index a raw pointer with the `at` they are given. `take_place` on a free slot of a `Pool` links it twice. | fixed `b81ca20c` | std |
| S316 | `src/std/anti/collection.anti:714` | 5 | The size of a room is an unchecked product in `room_from`, `grid.anti:46`, `map.anti:297`, `sorted.anti:719`, `pool.anti:451` and `concurrent.anti:924`. A release build wraps. | fixed `f393ed52` | std |
| S317 | `src/std/anti/args.anti:273` | 4 | `declare` writes an option after `reserve` at line 317 gave up without a word, past the slice or through `none`. | fixed `ba344114` | std |
| S318 | `src/std/anti/collection/concurrent.anti:878` | 12 | `dup` of a `Snapshot` leaves two walks that free one room. `ArenaAllocator`, `FileSink`, `NotOne` and `Changed` have the same shape. Read, not run. | fixed `5518e342`, `0d126d62`, `f223467d`, `a0de8b0c` | std |
| S319 | `src/anti/bindexpr.c:238` | 6, 14 | A string that ends in a backslash moves the evaluator of `anti bind` one byte past its text, and `space` reads on from there. | fixed `e1a931d8` | anti-tool |

### Documentation

Each is a header or a file whose documented behaviour differs from the
code. Three of them are defects of the code as well: S321, S324 and S330.

| ID | Place | Finding | Status | Reports |
|---|---|---|---|---|
| S320 | `lexer.h:171` | Seven header sentences state what the code does not do: the error cap at `lexer.h:171` and `178`, `types_object` at `types.h:429`, `types_has_fields` at `types.h:889`, the pointer of two words at `types.h:218`, the copies at `sema_generic.c:16` and `sync` at `sema_thread.c:1`. | fixed, the five outside `lexer.h` `5ec24a80`, the two of `lexer.h` `dd7cce0d` | front-end |
| S321 | `sema_operator.c:470` | The DESIGN says two pointers compare whether or not either may hold `none`. `comparable_pointers` compares the addresses of two parameter arrays, so `fn(int) == ?fn(int)` is refused. | fixed `67f75fb8`, as T111 | front-end |
| S322 | `driver.h:26` | The comments of `opt`, `llc` and `llvm_ar` at line 114 promise a fallback to PATH. `driver_archive_tool` refuses a tool that `bin/` of the archive lacks. | fixed `d6ec7b4f` | back-end |
| S323 | `docs/decisions.md:1227` | "Build ids" and "Debug information" describe `.file` and `.loc` directives, llvm-mc and `src/antic/debug.c`. The entry on C99 at line 1229 stands against `DW_LANG_C11` in `llvm_debug.c:115`. About twenty more entries from line 916 state the native back end as current. | fixed `bed691d8` | back-end, structure, tool-pass |
| S324 | `src/rt/json.h:66` | `anti_rt_json_value` and `anti_rt_json_member` are said to take one value of the grammar. `anti_rt_json_skip` takes `+-.e` as a number, and `member` of `anti.json` gives it out. | fixed `7c16ab47` | library-file |
| S325 | `src/rt/trace.c:5` | The DESIGN says the capture reads nothing from a file. On Linux `elf_id` reads the whole file of every module but the runtime's own. | fixed `01610cdb` | rt |
| S326 | `src/rt/rt.h:6` | `anti_rt_init` is said to run on the first use of the runtime in a static library for C. No code runs it there. Line 21 names `start.c` as the writer of an option that `conf.c:274` writes. | fixed `e82f89dd` | rt |
| S327 | `src/rt/cpu.c:1` | The header says antic writes the level into the object of `main` and `start.c` passes it. The level is a macro of the runtime's own compile. | fixed `07a7cb67` | rt |
| S328 | `src/rt/plugin.h:80` | Three comments say a program that loads nothing links no loader. `conf.c:813` calls the loader in every program. `plugin.h:49` and `loaded.c:4` are the other two. | fixed `e2136f49` | rt |
| S329 | `src/rt/threads.h:34` | `join` of a null job is said to write nothing. `threads.c:350` writes `result_size` zero bytes. | fixed `733f1014` | rt |
| S330 | `src/rt/patterns.c:695` | `patch` is said to find a misfit before any byte changes. The second pass searches the data it writes, so a lookahead group returns `MISFIT` with the data changed. Read, not run. | fixed `e4688507` | rt |
| S331 | `src/rt/object.c:13` | Two DESIGN comments describe what was replaced: default bodies that walk the field list, and an array field serialized as null. `object.c:710` is the second. | fixed `510d96a7` | rt |
| S332 | `src/std/anti/log.anti:15` | The header describes a file of `level`, `format` and `[[sink]]` entries with a level each. `first_sink` reads `kind` and `path`, makes one sink and stops at the eighth entry. | fixed `fd716910` | std |
| S333 | `src/anti/bindmodel.h:168` | `bind_is_keyword` is said to answer for a word the lexer reserves. It answers true for every text that is no identifier, and the reader of a header holds a name to no rule. | fixed `d62c120a` | anti-tool |
| S334 | `tools/compile.sh:13` | The header says the first build installs the sysroots and raylib. The configure step installs clang and the LLVM tools alone, and a first `./c` stops at `src/native/raylib.cmake:10`. | fixed `ae5f7fe9` | build-and-tools |
| S335 | `tests/runners/run_codeview_empty.cmake:24` | The test searches the assembly for `DEBUG_S_SYMBOLS` and `S_LPROC32`, spellings of the native back end that antic no longer writes. Both tests pass whatever antic writes. | fixed `06d0f6b6` | build-and-tools |

## Major

Grouped by standard, then by rule or question. Each entry gives its place,
its status and its reports.

### Safety and C

Rule 4, a result that is not looked at:

- M301. `src/rt/object.c:1047`. `dup` and `anti_rt_copy_buffer` at line
  1145 give NULL when memory runs out. Generated code hands it on as a
  `*T`. New. rt. Fixed `43a52d7c`.

Rule 11, who owns memory:

- M302. `src/std/anti/error.anti:29`. A `SystemError` keeps a message in
  storage the runtime overwrites on the next call. Windows shows it. New.
  rt. Fixed `b06fe3ad`.
- M303. `src/rt/plugin.c:446`. A second load of a library gives one slot
  to two owners. The first `unload` closes it for both. New. rt. Fixed
  `3d33cbfa`.
- M304. `src/std/anti/collection/list.anti:189`. `reserve` and `shrink`
  move the room and count no change. A walk then reads freed room with no
  trap. Eleven more places in `list`, `deque`, `map`, `set` and `sorted`.
  New. std. Fixed `2004bca3`.

Rule 12, release:

- M305. `src/std/anti/trace.anti:873`. `install` reads no result of
  `Composite.add`. The ninth handler is built, owned by nothing and never
  deleted. New. std. Fixed `d62175c5`.

Rule 14, untrusted input:

- M306. `src/antic/lexer.c:1321`. The errors inside an `{expr}` never
  count toward `LEX_ERRORS_MAX`. The flood of diagnostics the cap closed
  is back on that path. Returned. front-end. Fixed `dd7cce0d`.
- M307. `src/antic/parser_stmt.c:823`. At the depth limit before a
  statement keyword the parser consumes nothing and loops until memory is
  gone. New. front-end. Fixed `68fd6aa2` as T103, and the one message of
  a block at the limit `1d0804e0`.
- M308. `src/antic/parser_expr.c:135`. The tree built inside an `{expr}`
  is not counted by the chain around it. The tree grows past
  `PARSE_DEPTH_MAX`. New. front-end. Fixed `b1e47d47`.
- M309. `src/rt/plugin.c:832`. An empty directory of the search path
  gives a bare name. The digest reads one file and `dlopen` may load
  another. New. rt. Fixed `06109ee3`.
- M310. `src/anti/fmt.c:248`. `anti fmt` sorts the names of a body that
  follows an import without its semicolon. The file is written back
  changed. New. anti-tool. Fixed `e42dda91`.
- M311. `src/anti/bindexpr.c:497`. A float product past the largest
  float is written as `inf.0`, which no lexer of Anti reads. New.
  anti-tool. Fixed `2937296e`.
- M312. `src/anti/bindclang.c:437`. A typedef is resolved again at every
  use. A chain of forty lines asks for 3^40 parses. New. anti-tool.
  Fixed `678ec24a`, in the reader of a description as well.

Rule 15, tests with malformed input:

- M313. `src/rt/regex.c:436`. The rewriter of a byte pattern has no test
  of a pattern cut short. S304 and S305 lack a test as well. New.
  library-file. Fixed `3ed05df4` for the rewriter. S304 and S305 take
  their tests in step 12, `9555cceb` and `04dbb192`.
- M314. `tests/unit/test_symbols.c:392`. The ELF and DWARF readers have
  no test of a file cut short, and none of a line table of DWARF 2 or 3.
  Still open in part, M43. rt. Fixed `b1a3820e`.
- M315. `tests/std/deserialize_input.anti:1`. The malformed input of
  `Object.deserialize` runs in a program that links no sanitizer. 23 of
  the 42 files of `src/rt/` run under none. New. tests. Fixed `f6e270fc`
  for `registry.c` and `object.c`, the two files step 20 names. The test
  showed T143, fixed `ca450252`.

Rule 22, one platform layer and one result on every target:

- M316. `src/rt/platform_posix.c:348`. macOS and Linux give file names as
  raw bytes where `platform.h:10` promises UTF-8. `anti_rt_getenv` does
  the same. New. rt. Fixed `c8ee2a81`.
- M317. `src/rt/plugin.c:995`. The search path of plugins splits at `:`,
  so it cannot hold a Windows directory. New. rt. Fixed `4b568e74`.

Rule 23, shared state:

- M318. `src/std/anti/random.anti:89`. State that threads reach is
  guarded in part: `SharedRandom.reseed`, `FileConfig.read` at
  `config.anti:74` and the fields of `Logger` at `log.anti:124`. New. std.
  Fixed `a7315143` and `58987147`. The fields of `Logger` stay unguarded,
  and `82850caa` says when a program writes them.

The bar of tests:

- M319. `tests/CMakeLists.txt:691`. Tests pass without running or do not
  exist, and the run says nothing: `llvm_coff_plugin`, `package_keys`,
  `debug_info`, `macos_sdk` and the dry run of `./r`. Returned.
  build-and-tools. Fixed `718b75d2`, `8ddb9cda`, `d5cb6dce` and
  `e3e89ef2`. The test `skips_registered` refuses a runner that prints
  `SKIP:` without the property.
- M320. `tests/runners/run_manifest_signature.cmake:155`. Five checks cannot
  tell a failure from a pass. The wrong-signature case passes for an
  installer that accepts every signature. New. build-and-tools. Fixed
  `2936609e`, `3362bafc`, `00fc93e9`, `54ab226f` and `a99dedcc`.

### Structure

One purpose per module and file:

- M321. `src/rt/std.h:1`. `std.h` declares the functions of nine files
  and both platform files, and `rt.h` holds what has no header. The
  platform layer's interface stands in three headers. New. rt. Fixed
  `a8a16e64`.
- M322. `src/anti/symmap.c:146`. The file of the symbols map also holds
  the readers of the licence notice and of `sources.txt`. New. anti-tool.
  Fixed `5b14cfd6`, which moves them into `licensing.c`.
- M323. `tests/runners/run_release_dry_run.cmake:1`. The two largest runners
  hold many tests under one name. `run_package.cmake` runs thirteen
  checks as `package_keys`. New. build-and-tools. Fixed `1ba2cce5`, which
  makes each check a test of its own over the fixture `package_pack` and
  the copy of `tests/release_copy.cmake`.

One-way dependencies, and a part doing the work of another:

- M324. `src/antic/driver_library.c:444`. A Linux shared library with a
  soname runs `ln` of the host, outside the platform layer. A Windows host
  fails there without a message. New. back-end. Fixed `80e29ef5` as T131,
  with its message and the test `antic_option_soname_linux`.
- M325. `src/rt/conf.c:781`. The configuration fills the injections and
  loads each provider. That call puts the loader in every program. New.
  rt. Fixed `66b3bd9f`, which moves the injections to `src/rt/inject.c`.
- M326. `tests/CMakeLists.txt:1050`. The target `llvm_tools` of the test
  directory is the only producer of `runtime/bin`, a part of the product.
  It is keyed on the path of llvm-mc. New. build-and-tools. Fixed
  `c470bafc`, which moves the target to the root file.
- M327. `tests/CMakeLists.txt:1297`. The raylib source is named by the
  test directory and fetched by no one. The recipe works by the order of
  two `add_subdirectory` lines. New. build-and-tools. Fixed `ab89b335`.

Each concern in one place:

- M328. `src/antic/sema_stmt.c:361`. `e[i] = v` without a hook checks its
  base twice. An error is reported twice and an `own` argument moves
  twice. The pattern of M35 on a new path. New. front-end. Fixed
  `0b794956`.
- M329. `src/antic/antl_read.c:1060`. Four limits bound the nesting where
  `antl_io.h:59` names one. antic writes a constant 40 levels deep and
  then calls its own file damaged. New. library-file. Fixed `3b2be74e`.
- M330. `src/std/anti/fs.anti:170`. Memory that runs out is answered five
  ways: stop, fail, drop the bytes, drop the option, or give a literal.
  New. std. Fixed `ba344114`, with the builder in `07caeed4`. T139 holds
  the places of the runtime outside step 3.
- M331. `tools/release.sh:666`. The release script spells `raylib-6.0`
  beside `tools/raylib-pin`, and no test holds the two together. New.
  build-and-tools. Fixed `17e2d89d`.
- M332. `src/rt/registry.h:12`. Layouts and numbers stand on both sides
  of the runtime contract with no test between them. Seven pairs stand
  outside `rt_abi.h`, and four enums of antic mirror the runtime. New.
  rt, middle. Fixed `e66be876`, `81d4b0b1`, `97ae741c`, `cc2a4857` and
  `dec36ffd`.

Tests, a golden written again without a reading:

- M333. `tests/runners/run_emit_identity.cmake:52`. The manifest of 1332 digests
  is written again with no way to read what changed. The old texts are
  removed first. New. tests. Fixed `f287bf9b`, which keeps the texts of
  the manifest and prints the first line that moved in each.
- M334. `tests/runners/run_dump.cmake:32`. No runner shows where two texts
  differ, and eleven scripts each compare with an expected file
  themselves. New. tests. Fixed `3dcbd6ae`, one comparison in
  `tests/program_output.cmake` that names the first line that differs.

Copies that drifted:

- M335. `src/antic/driver.c:1174`. The last separator of a path is found
  four ways. Ten places split at `/` alone, so a Windows path splits one
  way in the link and another in the library. The pattern of M26. New.
  back-end. Fixed `050b722d`. T140 holds the same split in
  `src/anti/bindclang.c` for step 14, fixed `89b9938a`.
- M336. `src/antic/antl_write.c:36`. The types the compiler declares are
  listed three times. The reader lacks `Job`, so a generic with a
  `dispatch` gives a file every importer refuses. New. library-file.
  Fixed `d28e47d5`, the reader's `Job` in `ba14f572` as T061.
- M337. `src/rt/platform_windows.c:455`. Windows lists a directory in two
  functions. They differ on `C:` and on a name that does not fit. Line
  1474 is the second. New. rt. Fixed `4b4a9930`.
- M338. `src/std/anti/collection/concurrent.anti:352`. `ConcurrentMap`
  stamps the key after the change and `SyncMap` before it. A later
  `set_if_version` succeeds on a changed entry. New. std. Fixed `9c1100dd`.
- M339. `src/anti/bindwrite.c:633`. Four pairs of copies have drifted.
  They are the test for an unsigned type, a struct constant, the sources
  of a project and the module path of a library file. New. anti-tool.
  The first two pairs are fixed, `f5887976` and `7be83886`. The last two
  are fixed in step 21, `5b136b75` and `9f70dc2d`.
- M340. `src/native/get-pcre2.cmake:14`. Six fetch scripts are one
  script, and `tools/get-raylib.cmake` has diverged. Two more downloads
  stand under other names. New. build-and-tools. Fixed `ab89b335`.
- M341. `tools/check-llvm.cmake:16`. The LLVM tools are listed and
  checked in two places that disagree. A missing llvm-objdump leaves a
  `runtime/bin` without it and no error. New. build-and-tools. Fixed
  `29873529`, one list, one check and `tools/copy-llvm.cmake`.
- M342. `src/native/CMakeLists.txt:31`. The triple and the flags of a
  target are written in eight places. The native libraries lack three
  flags the runtime takes, against their own comment. New.
  build-and-tools. Fixed `22884b98`, the table of
  `tools/target-compile.cmake` and a provisional entry on the flags.
- M343. `tests/runners/run_dev.cmake:26`. The helpers of the runners exist in
  many copies with different contracts. 49 builds in 31 runners accept
  whatever antic and the linker print. The pattern of M61. New.
  build-and-tools, tests. Fixed `5e3dde37`, one helper of each kind in
  `tests/program_output.cmake` and the test `antic_calls`.

Left of a replaced feature, with an effect:

- M344. `tools/publish.cmake:91`. The upload half of the publish script
  is reached by no caller. It would send the signature beside the
  binaries, against `tools/site-base`. New. build-and-tools. Fixed
  `2473f1d3`, which makes it `tools/check-release.cmake` and drops the
  upload.
- M345. `tests/CMakeLists.txt:554`. The LLVM program tests are a second
  runner from the two-back-end period. `llvm_division`, `llvm_wrap` and
  `llvm_flags` run on a Mac and on neither VM. New. build-and-tools.
  Fixed `044687b5`, which puts them in the glob of every host.

### Documentation

A file without a header comment that says its one purpose:

- M346. `src/antic/alloc.c:1`. 64 files of `src/` open with no sentence
  of their purpose: 44 of `src/antic/`, 15 of `src/rt/` and 5 of
  `src/anti/`. Six more headers say less than their file. New. tool-pass,
  middle, front-end, back-end, library-file, rt, anti-tool. The front
  end fixed `5ee03303`, which the test `file_headers` holds.
- M347. `tests/unit/test_target.c:1`. 24 of the 56 unit files open
  without a header comment. Three test more than one part and no line
  says so. New. tests.

A public function or item without a comment:

- M348. `src/antic/optimize.h:6`. 586 of 1346 declared functions share a
  comment or have none. 198 of them stand in public headers. The reports
  list the worst: `types.h`, `reflect.h`, `utf.h` and fifteen of
  `src/anti/`. New. tool-pass, front-end, rt, anti-tool. The public
  headers of the front end fixed `5ee03303`, which the test
  `file_headers` holds.
- M349. `src/std/anti/log.anti:40`. Public items of the library stand
  without a `///` comment. The public fields of `SlotsWalk` let a program
  read outside the blocks. New. std.
- M350. `src/std/anti/os.anti:138`. Comments of the library leave out the
  owner, the failure or the lifetime of what a function gives. Nine
  groups of places. New. std.

The path from `CLAUDE.md` to a file, and the "Files of" sections:

- M351. `docs/decisions.md:2161`. No document lists the files of a
  directory with their purposes. 41 of the 96 `.c` files of `src/antic/`
  stand in no "Files of" section. `src/anti/`, `src/rt/`, `tests/` and
  `tools/` have no list at all. New. tool-pass, tests, structure. Fixed
  `6a513f47`: `docs/notes/source-tree.md` and the test `source_tree`.
- M352. `docs/decisions.md:2176`. The sections "Files of the parser" and
  "Files of the checker" hold seven sentences that no longer describe the
  files. New. front-end.

A note or a document that describes a design the code no longer has:

- M353. `docs/notes/generics.md:31`. A call of a function with type
  parameters of its own is said to be refused. `own_params_copy` makes
  the copy. New. front-end.
- M354. `docs/notes/lowering.md:7`. Lowering is said to stop at scalars.
  The note names a test that does not exist. New. middle.
- M355. `docs/notes/whole-program.md:26`. Stale in five places. They are
  the flags of a class record, the readers of the registry and the slot
  table. Two sections cover devirtualisation, and five passes have none.
  New. middle.
- M356. `docs/notes/aggregates.md:16`. The note describes the emitter of
  the native back end and a binding that "stays planned". New. middle.
- M357. `docs/notes/linker.md:22`. The note gives `/vctoolsdir` and
  `/winsdkdir` for a Windows link that writes neither. It says a check
  runs before the compile. New. back-end.
- M358. `docs/notes/modules.md:7`. The instruction record is said to be
  49 bytes. Three messages of the reader are missing, and no note
  describes the verifier. New. library-file.
- M359. `docs/notes/runtime-conf.md:30`. Two notes describe plugins as
  not built. `docs/notes/injection.md:104` is the second. New. rt.
- M360. `docs/notes/setup.md:7`. The note places the runtime where the
  build no longer puts it, and says it is compiled with hidden
  visibility. New. rt.
- M361. `docs/notes/trace-handlers.md:22`. Two notes describe a handler
  design the code does not have. A third gives format version 57 for 81.
  New. std. Fixed `5f15d339`.
- M362. `docs/notes/warnings.md:40`. The note says the two safety checks
  are not built. Both are. New. anti-tool.
- M363. `docs/site/build-tool/index.md:30`. The site page of the build
  tool describes assembly files, llvm-mc, eight commands that do not
  exist and none of `anti symbols`. New. anti-tool.

A header or a comment that differs from the code:

- M364. `docs/c-guidelines.md:46`. Rule 10 names a test that fails the
  suite on a banned function. No such test exists. New. tool-pass.
- M365. `src/antic/memcheck.h:15`. The DESIGN gives a reason of the
  register allocator. `loop_depth` of `ir.h:312` is written and never
  read. New. middle.
- M366. `src/std/anti/collection/map.anti:47`. The growth of a `Map` is
  not what its DESIGN and `docs/decisions.md:2460` say. A full map that
  removes one entry rebuilds its room on every `add`. New. std. Fixed
  `d4a0c9dc`.
- M367. `CMakeLists.txt:303`. A DESIGN comment says the packer leaves the
  bitcode out of the package. `tools/pack-anti.cmake:388` copies it. New.
  build-and-tools. Fixed `199c8885`.
- M368. `tools/uninstall.sh:5`. The uninstallers name an address that no
  release publishes. New. build-and-tools. Fixed `cfec4ad3`, which has
  step 9 publish them by decision 9.
- M369. `tests/runners/run_script_policies.cmake:11`. The policy test says
  "every script" and reads two list files. Four runners that `src/native/`
  registers carry no `cmake_minimum_required`. New. build-and-tools.
  Fixed `c3e7267b`, with the minor finding on CMake 3.20.
- M370. `tests/CMakeLists.txt:126`. The allocator test is said to cover
  the programs that link `antic_core`. `tests/doc/antl_rename.c:29` calls
  `malloc` and is not listed. New. build-and-tools. Fixed `b0b90de3`.
- M371. `tests/runners/run_linux_libc.cmake:12`. The runner says it links with
  the recipe of the packer and holds a hand copy of it. New.
  build-and-tools. Fixed `1dac8065`, one function of
  `tools/host-compile.cmake` for both.

## Minor

Counted by rule or question, with the place each report opens the finding
at. Every finding is new unless the row says otherwise. Paths without a
directory are in `src/antic/`.

### Safety and C

| Rule | Count | Findings and reports |
|---|---|---|
| 1 | 2 | The format attribute on six runtime functions, `src/rt/assert.c:10`, still open (tool-pass). A text kept in `char bytes[1]`, `src/rt/trace.c:50` (rt). |
| 3 | 3 | Conversions the implementation defines, `types.c:373` (front-end). Lengths narrowed to `int`, with rule 16, `src/rt/assert.c:66`, and `serialize` called through another type, `src/rt/object.c:757` (rt). |
| 4 | 1 | The walk order of a `HashMap` falls back to one order, `src/std/anti/collection/map.anti:475` (std). |
| 5 | 3 | Products before an allocation, `sema.c:1292` (front-end), `lower_expr.c:1916` (middle) and `src/rt/toml.c:104` (library-file), fixed `6ec49dc0`. |
| 6 | 3 | A loop without its bound, `pattern.c:1019` (front-end). Arrays filled to a hand-counted bound, `llvm_run.c:224` (back-end). An index of the token lists, `tests/unit/test_modules.c:54` (tests). |
| 8 | 1 | A field cleared through a pointer that may be unaligned, `src/rt/registry.c:237` (rt). |
| 9 | 3 | `snprintf` results not read, `sema_const.c:191` (front-end), `whole_tables.c:1481`, still open from the first audit (middle), and `abi.c:12` (back-end). |
| 10 | 2 | A signature without its length, `src/rt/call.c:37` (rt). A `str` read as a C string, `src/std/anti/os.anti:53` (std). |
| 11 | 2 | Returned memory without its owner, `pattern.c:9` (front-end) and `src/rt/registry.h:47` (rt). |
| 12 | 4 | Releases before each return, `parser_item.c:15` (front-end), `src/rt/regex.c:491` (library-file), fixed `0d2a7566`, `src/rt/platform_posix.c:92` (rt) and `src/anti/main.c:198` (anti-tool). |
| 13 | 1 | The start of a Windows program aborts without a line, `src/rt/platform_windows.c:1363` (rt). |
| 14 | 11 | Smaller gaps of the front-end readers, `modpath.c:93` (front-end). Names with a NUL, flag bytes above 1, a narrowed address and a field of type `none`, `antl_tree.c:1459`, `antl_read.c:515`, `antl_read_ir.c:417` and `antl_read.c:917`, and a quadratic scan, `src/rt/toml.c:139` (library-file). The NUL of a group name in `src/rt/regex.c:578` is fixed `9bc09550`, and the scan `b71e491e`. Two readers that take what they should refuse, `src/rt/registry.c:734` (rt). Two forms of `args`, `src/std/anti/args.anti:251` (std). Paths read without the test of a regular file, `src/anti/syms.c:343`, names not held against the words of Anti, `src/anti/bindwrite.c:263`, fixed `8b920c34`, and zero added to NULL, `src/anti/zip.c:673` (anti-tool). |
| 15 | 1 | Refusals and paths without a test, `sema_copies.c:90` (front-end). |
| 16 | 1 | Counters of `int`, `parser.c:349` (front-end). |
| 17 | 1 | The fields of the zip format in `unsigned long`, `src/anti/zip.c:43` (anti-tool). |
| 18 | 1 | 142 functions past a threshold, 32 of them new, against 163. Still open. Judged in tool-pass, front-end, middle, back-end, library-file and rt. |
| 19 | 1 | Two files past 3000 lines. `sema_stmt.c` with 3089 returned, and `llvm_emit.c` with 4102 is new (tool-pass, front-end, back-end). |
| 20 | 1 | Two functions used in one file are exported, `sema_checker.h:392` (front-end). |
| 21 | 1 | Five includes that contribute nothing, `linker.c:9` (structure). |
| 22 | 1 | The walk of Windows stops at 64 frames, `src/rt/platform_windows.c:591` (rt), fixed `556f9c07`. |
| 23 | 2 | A rotating buffer without its comment, `sema_value.c:203` (front-end). Two globals without a guard, with rule 24, `src/rt/assert.c:49` (rt). |
| 24 | 2 | Casts that remove `const`, `sema_copies.c:152` (front-end) and `memcheck.c:248` (middle). |
| 25 | 1 | Names outside the prefix of their file, `platform.c:216` (back-end). |
| 26 | 15 | A type walk and an operand comparison twice, `sema_value.c:310`, and eleven lines of `list.anti:189` and `deque.anti:95`, still open (tool-pass). Three tests of the program's main, `reach.c:154`, a file's existence, `driver_search.c:50`, and two functions nothing calls, `target.c:151` (structure). Repeated blocks, `sema_generic.c:1838`, and dead code, `sema_copies.c:450` (front-end). Helpers written twice, `optimize.c:24` (middle). Blocks of the back end, `llvm_emit.c:242` (back-end). The comparison of two names, `antl_tree.c:1377` (library-file). Unused functions, `src/rt/object.c:1109`, and blocks twice, `src/rt/patterns.c:35` (rt). Dead code, `src/std/anti/collection/map.anti:71`, and duplicated code, `src/std/anti/args.anti:284` (std). Two copies that allocate on their own, `src/anti/syms.c:837` (anti-tool). |
| Warnings | 1 | `(void)is_union;` without a reason, `src/anti/bindapi.c:153` (anti-tool). |
| Bar of tests | 2 | `--soname` and `--strip-docs` have no test, `main.c:295` (tool-pass). Three guards have holes, `tests/runners/run_warning_set.cmake:14` (build-and-tools). |

### Structure

| Question | Count | Findings and reports |
|---|---|---|
| One purpose | 1 | `anti.regex` holds two functions that use no pattern, `src/std/anti/regex.anti:982` (std). |
| Dependencies and boundaries | 4 | The doc page of the front end builds an IR module, `docpage.c:13` (tool-pass). The inside header of the library file is included outside it, `antl_io.h:12` (structure). The records every part shares stand in the interface of the tool, `antic.h:4` and `antic.h:111` (structure, back-end). The status 3 of `driver_run` is a number at four places, `src/anti/build.c:333` (anti-tool). |
| Each concern in one place | 20 | The prefix of a message at every site, `src/anti/bindclang.c:101` (structure). Tables and names defined twice, `sema_class.c:1020` (front-end). The checks of a dev build in four files, `lower_lowerer.h:9` (middle). Errors in three forms, `driver.c:597`, fixed `418a71e8`, and a platform layer that names antic, `platform.h:9`, fixed `482136fd` (back-end). No `ptr_set_free`, `ptrset.h:8` (library-file). A walk built three ways, `queue.anti:75`, a lent key handled three ways, `set.anti:144`, `destruct` declared two ways, `lang.anti:192`, functions without `-> never`, `io.anti:36`, and numbers spelled again, `lang.anti:126` (std). Names of files and directories, `src/anti/main.c:488`, and a JSON file read by `strstr`, `src/anti/sdk.c:41` (anti-tool). A dozen readers of a pin, `tools/upstream-sources.cmake:21`, the six targets in twenty places, `tools/pack-anti.cmake:56`, the host worked out in many ways, `tools/pinned-compiler.cmake:37`, versions outside their pins, `tools/sysroot-pins:4`, CMake 3.20 beside a 3.21 command, `tools/get-sysroot.cmake:30`, fixed `c3e7267b`, three ways to move the downloads, `CMakeLists.txt:42`, and the nm of the host, `tests/runners/run_clib.cmake:391` (build-and-tools). |
| Copies and small duplication | 4 | Two readings of one grammar that drifted, `parser_stmt.c:165` (front-end). Small copies inside the tools, `tools/release.sh:975` (build-and-tools). Registrations written out, `src/native/pcre2.cmake:97` and `tests/CMakeLists.txt:479`, still open (build-and-tools, tests). Two unit files that run the stages themselves, `tests/unit/test_whole.c:49` (tests). |
| Left of a replaced feature | 4 | The option `--llvm-mc` through antic, anti, the build files and 57 runners, with `target_block_label` and the attributes column, `driver.h:23` (structure, back-end, build-and-tools, tests), fixed `c0f1e8e1`. The `Interlocked` branch for Microsoft's compiler, `src/rt/atomic.c:98` (rt), fixed `1a6f9729`. Checks and words of the Microsoft toolchain, `tools/release.sh:688` (build-and-tools). Its call of `vcvarsall.bat` waits for decision 16, since the call also gives the configure the `rc.exe` that CMake asks for, `docs/reports/2026-10-17-solid-step24.md`. `src/std/.gitkeep` (std). |
| Placement and names | 15 | A tracked `.pyc` under `tools/docs-style/__pycache__/` (structure). Functions outside the file their sentence names, `sema.c:1925`, and small things, `parser_stmt.c:12` (front-end). The channel design in the file of the workers, `lower_worker.c:349` (middle). Two headers that carry more than their sentence, `src/rt/regex.h:98` (library-file). Names that differ inside one module, `src/rt/object.h:211` (rt). Helpers that are `pub`, `collection.anti:575`, and three left by the closure: `Changed` and `Snapshot`, `synchronized.anti:29`, size and presence, `text.anti:174`, and `""` for what is not there, `args.anti:104`, all still open (std). Two records named `unit`, `src/anti/syms.c:409`, and three helpers in another file, `src/anti/repo.h:20` (anti-tool). Helpers in the file of another purpose, `tools/fetch-release.cmake:25` (build-and-tools). The runners flat in `tests/` and a build file in the order of its history, `tests/CMakeLists.txt:1`, still open (build-and-tools, tests). Test names that do not start with their kind, `tests/CMakeLists.txt:309` (tests). |
| State without an owner | 2 | Two context fields saved by hand, `sema.c:973` (front-end). The writer changes the tree it writes, `antl_tree.c:845` (library-file). |
| Tests | 2 | Expected texts that hold more than their subject, `tests/unit/test_lower.c:1277`, and 25 files of antic and anti no unit test reaches, `driver.c:1` (tests). |

### Documentation

| Question | Count | Findings and reports |
|---|---|---|
| A comment that differs from the code | 12 | `antic.h:11` against the test `anti_interface` (tool-pass). Stale sentences, `parser_item.c:345` (front-end). Two comments of the IR, `ir_verify.c:679` (middle). `antl_verify.c:13` and `src/rt/toml.c:1` (library-file). `src/rt/symbols.h:65` and comments that name what moved, `src/rt/platform.h:4` (rt). `src/std/anti/random.anti:5` (std). Nine comments, `src/anti/build.c:487` (anti-tool). `CMakeLists.txt:136`, the recipes that name modules, `src/native/sqlite.cmake:1`, and two presets, `CMakePresets.json:23` (build-and-tools). |
| A header that says less | 3 | `src/rt/platform_windows.c:1` (structure). Eight headers, `src/anti/zip.c:1` (anti-tool). Headers of runners, `tests/program_output.cmake:1` and `tests/runners/run_anti_build.cmake:4` (build-and-tools, tests). |
| Stale names in notes and documents | 10 | File names in the decisions and `CLAUDE.md`, `docs/decisions.md:1220` (tool-pass), fixed `e5efa818`. Function names that moved, `docs/notes/hashing.md:8`, `check.md:76` and `versions.md:41` (structure, front-end, library-file). `ir.md:7`, `optimizer.md:7`, `hashing.md:11` and `dev-checks.md:44` (middle). `llvm.md:17` (back-end). `threads.md:31` (rt). The site page of the library, `docs/site/standard-library/index.md:27` (std). `doc.md:53` (anti-tool). |
| Comments that name what is gone | 4 | llvm-mc as the assembler, `cpu.c:7`, `target.c:16` and `tests/runners/run_bench.cmake:14` (structure, back-end, tool-pass), fixed `c0f1e8e1` and `452739de`. Chapters of the book, `layout.h:12` and `sema.h:196` (middle, front-end). The steps of the work order, `llvm_emit.c:24` (back-end). Finding numbers that two audits use, `tests/runners/run_antl_verify.cmake:1` (tests). |
| DESIGN without a reason | 4 | `sema_expr.c:1984` (front-end). A reason that is gone, `driver_index.c:14` (back-end). Two bounds of 32, `src/rt/trace.c:142` (rt). `tests/runners/run_asserts.cmake:47` (build-and-tools). |
| Rule 27 | 3 | Comments above the wrong item, `parser.c:177` (front-end) and `driver.c:135` (back-end). Two banned phrases, `src/std/anti/collection/queue.anti:17` (std). |
| The path from `CLAUDE.md` | 1 | The docs-style rules for a comment stand in no file of the repository, `tools/docs-style/banned-terms.md:6` (build-and-tools). |

## Compared with the second audit

The second audit merged 187 findings: 34 severe, 62 major and 91 minor.
`docs/audit/2026-09-30/closure.md` accounts for them in 34, 62 and 90 rows,
and for 12 defects no rule names. It marks every severe and every major
finding fixed. Of the minor rows it marks 80 fixed, 6 no defect and 4 left.

The machine evidence of the two tool passes, at `70e8860` then and at
`f3af7693` now:

| Source | Then | Now |
|---|---|---|
| Analyzer warnings, unique | 43 | 34 |
| Compiler warnings, unique | 74 | 48 |
| Banned calls | 0 | 0 |
| Functions past a threshold | 163 | 142 |
| Files past 3000 lines | 9 | 2 |
| Host `#if` outside the platform layer | 110 | 0 |
| Duplicated blocks, exact and near | 28 and 40 | 2 and 43 |
| Includes against the direction | 3 | 2 |

### Fixed

No report found a fix of the closure undone at the place the closure
names. The reports read these again and found them holding:

- The library reader: S01, S03, S13 and S23, with M20 and M33.
  `antl_verify_tree` runs after every read. S304 is a relation of a tree
  that S01 did not name.
- The front end: S02, S04 to S12 and S14 to S16, with M34 to M36.
  `sema_enter` and `sema_leave` are the only writers of `c->ctx`.
- The middle: S17, S18, S19, S20 and S22, with M05, M13, M14, M16, M18
  and M49. One teardown, one reach and one folder stand.
- The runtime: S25, S26 and S27, S28 for `instance`, and M22, M32, M38 and
  M47.
- The tool: S30 to S32 and S34, and every fix the closure records for
  `src/anti/`. M02 holds, and the test `anti_interface` pins it.
- The structure: M03, M04 and M06 hold in `alloc.c` and `rt_abi.h`. M30
  holds, with no host `#if` outside the platform layers.
- The tests: M54 holds, with no `PASS_REGULAR_EXPRESSION` in any
  registration. The scripts that M60 to M62 named hold their fix.
- S21, M15, M50 and M53 stood in the native back end, which is gone.

### Returned

Three findings were reported, fixed and are open again:

- M306. The second audit's minor finding on a diagnostic per bad byte was
  closed by `LEX_ERRORS_MAX`. The errors inside an `{expr}` escape the cap.
- M319. "Tests drop out or pass without running on some hosts" was fixed
  in `07249add`. A runner added after that fix has the pattern again.
- Rule 19. The closure records every file of `src/` under 3000 lines.
  `sema_stmt.c` grew from 2962 to 3089.

Six more findings are a pattern the second audit fixed at the places it
named, found now at other places. The reports count them new:

- M328 is M35, a subtree checked twice, on the path of `e[i] = v`.
- M335 is M26, a path split at `/` alone, now in the driver and the linker.
- M343 is M61 and M62, runners that disagree on what antic may print.
- M340 is the five pin checks of the tests, now in the six fetch scripts.
- S308 is S28 on the other end of an object's life.
- M316 is M38, a `str` that is no UTF-8, now from a directory listing.

### Still open

Thirteen findings were reported before and never fixed in full:

- S307. S28 closed the window in `plugin.c`. The same window stands in
  `registry.c` for `reflect.new` and `Object.deserialize`.
- S310. S33 closed the lists of a descriptor. The counts of the table and
  the width of a field are still unchecked.
- S303. M44 moved the group number of `patch` to `int64_t`. The reader of
  `pattern.c` still counts in `long`.
- M314. M43 closed the Mach-O readers. The ELF and DWARF readers still
  have no test of a file cut short.
- The eleven lines `list.anti` and `deque.anti` share, the remainder of
  M27.
- The format attribute on six functions of the runtime, of nine at the
  second audit.
- Rule 18, with 142 functions.
- The registrations of `tests/CMakeLists.txt`, which the closure marks
  fixed and which still spell the arguments of antic at 103 sites.
- The unchecked `snprintf` results in the names of globals, from the first
  audit.
- The four rows the closure left for Eddie: `Changed` and `Snapshot`, the
  names of size and presence, the `""` of `args`, and the directories of
  `tests/`.

The two questions the closure left beside fixed rows are findings now. The
builder that runs out of memory is S312 and M330. The stub of a slot that
`reflect.call` alone reaches is S309.

### New since

246 of the 262 merged findings are new. They come from four sources:

- The documentation standard. The second audit had none. All 79 findings
  under it are new, 16 of them severe.
- Areas no audit read before. `build-and-tools` read the root build file,
  `tools/` and `src/native/` for the first time and lists 39 findings.
  `std` read all 35 modules line by line, and all seven of its severe
  findings are new.
- Code written since. The LLVM back end and the split of the driver came
  after the second audit, and all 21 findings of `back-end` are new. The
  distribution work of 2026-10-08 to 2026-10-10 gave `symmap.c` its second
  purpose, M322.
- The native back end left, and its words stayed. The decisions, the
  option `--llvm-mc` and the second runner of the program tests remain of
  it. So do the note `aggregates.md`, the site page of the build tool and
  S335.

## Verdict

### Safety and C

The compiler is sound in its middle. No line of the IR, lowering, the
optimizer or the layout reads outside an object, and that area has no
severe finding. No banned call stands in the tree, and no analyzer warning
points at a defect. antic allocates through one checked owner, `alloc.c`.
The readers the earlier audits hardened hold their checks. The three
places where the work is most needed:

1. The life of a loaded library in the runtime. Five of the seven severe
   safety findings of `rt` concern a plugin: S306 to S310, with M303, M309
   and M317. An object, a frame and a table each outlive or outgrow the
   library they point into. M315 adds that 23 of the 42 runtime files run
   under no sanitizer.
2. The standard library in a release build. A release build checks no
   index and no overflow, and the library relies on both: S314 to S318,
   with M304. `anti.log` and `anti.args` fail on input a user writes, S313
   and S317. The library has no one rule for memory that runs out, M330.
3. The edges where antic and `anti` read input. A library file reaches
   lowering with a field name and a nesting that source cannot have, S304
   and S305. The front end reads a NULL after an error it reported, S301,
   and its caps miss the `{expr}` path, M306 to M308. `anti bind` and
   `anti fmt` misread three inputs, S319 and M310 to M312.

### Structure

The direction of the build holds. The runtime includes nothing of antic or
`anti`, the IR nothing of the back end, and no call runs against the
direction. `anti` reaches antic through `antic.h`, `driver.h` and the
shared helpers alone. Each split of the compiler is one part with one
inside header, and the test `header_sections` keeps the headers true. One
teardown, one reach, one folder, one platform layer per part and one
contract in `rt_abi.h` stand. No structure finding is severe. The three
places where the work is most needed:

1. The build files and the test scripts. They hold 12 of the 25 major
   findings. `tests/CMakeLists.txt` has 4961 lines, builds a part of the
   product and names the raylib source, M326 and M327. A fetch, a tool
   list and a triple each stand in many copies that disagree, M340 to
   M342. 49 builds accept any output, and no runner shows a difference,
   M343 and M334.
2. The seam between the runtime and what calls it. Seven layouts and sets
   of numbers stand on both sides with no test, M332. `std.h` and `rt.h`
   are headers of everything, and the configuration loads plugins, M321
   and M325. The types the compiler declares are listed three times in
   the library file, and one list lacks `Job`, M336.
3. The host in the driver and the tool. A path is split four ways, and a
   link runs `ln` of the host, M335 and M324. A message has three forms
   and names `antic` when `anti` runs. Four pairs of copies in `anti` have
   drifted, M339. The option `--llvm-mc` still runs through both tools,
   the build files and 57 runners.

### Documentation

The prose passes its checker: `structure` ran it over all 408 `.md` files
and got no error. Every file of `src/anti/` but five opens with its
sentence, and so do 52 of the 66 runtime files `rt` read. Every module of
the library has its `//!` header. Every script of `tools/` that runs with
`-P` says how it is run. The notes on collections, direct imports, symbols and strings
agree with the code sentence by sentence. The three places where the work
is most needed:

1. Headers and files that state what the code does not do. 16 findings
   are severe, seven of them in `src/rt/`: S325 to S331. A reader of
   `rt.h`, `plugin.h`, `threads.h`, `driver.h` or `json.h` plans for
   behaviour that does not exist. Three of the 16 are defects of the code,
   found because the comment was read against it.
2. The path from `CLAUDE.md` to a function. No document lists the files of
   `src/anti/`, `src/rt/`, `tests/` or `tools/`, and the "Files of"
   sections reach 55 of the 96 files of `src/antic/`, M351. 64 files of
   `src/` and 24 unit files open without their sentence, M346 and M347.
   198 public functions share a comment or have none, M348.
3. The documents that describe an earlier state. `docs/decisions.md`, the
   first authority, states the native back end as current in about twenty
   entries, S323. Thirteen notes and the site page of the build tool
   describe designs the code left, M353 to M363. Ten claims of where a
   function stands moved with the splits.

## The sentence of each file

The sections "The sentence of each file" of the reports, in one table per
directory: all 428 tracked files of `src/`, `tests/unit/` and `tools/`. Each
row gives the purpose as a report read it from the code, and what the
file's own header gives. "Says it" means the first comment of the file
says the same. "None" means no comment describes the file. "One item"
means the first comment describes one function, one type or one table. A
`.c` file and its header share a row where the report reads them
together. Five files have a sentence in no closing section, and their rows
say so. The table is the draft of `docs/notes/source-tree.md`.
`build-and-tools` and `tests` hold the same table for the 157 `.cmake`
scripts of `tests/`, which are outside the three directories named here.

### The front end of antic

From `front-end`.

| File | Purpose | Header |
|---|---|---|
| `src/antic/lexer.c` | Splits a validated UTF-8 source into tokens under caps on size, nesting and errors, decodes literals and doc comments, and lexes each `{expr}` with a nested lexer. | None. The first comment describes the `kinds` table. |
| `src/antic/lexer.h` | The token kinds, the token and format-piece records, the five entry points, the three caps of the lexer and the depth limit of the parser. | None. Two of its comments differ from the code, S320. Fixed `dd7cce0d`. |
| `src/antic/parser.c` | The helpers every parser file shares, and `parser_parse`, which reads the lines at module level. | Says it. |
| `src/antic/parser.h` | The one entry point of the parser. | The comment of `parser_parse` alone. |
| `src/antic/parser_parser.h` | The state of the parser and the functions its seven files share, grouped by file. | Says it. |
| `src/antic/parser_clause.c` | Reads `allow` and `unchecked` and records the source each covers. | Says it. |
| `src/antic/parser_decl.c` | The parts of a signature and of a body: marks, parameters, result, `may fail`, type parameters, members, nested types, and the guard and clauses of a field. | Says "fields of a body", which `parser_item.c` reads. |
| `src/antic/parser_expr.c` | Expressions, from the primary forms through postfix, unary, casts and precedence climbing, with `f"..."` and anonymous functions. | Says it. |
| `src/antic/parser_item.c` | The items of a module, a class and a variant with their bodies and header clauses. | Says it. |
| `src/antic/parser_stmt.c` | Statements and blocks, the handlers of `catch` included. | Says it. |
| `src/antic/parser_type.c` | Types, and the scan that tells type arguments after `<` from a comparison. | Says it. |
| `src/antic/ast.h` | The syntax tree of one module, the fields the checker adds to it, and the entry points of the dump and the walk. | Says the first. |
| `src/antic/ast_dump.c` | Writes the tree, untyped or with the checked types, one node per line for `--dump-ast` and `--dump-types`. | None. The first comment says how node names are chosen. |
| `src/antic/ast_walk.c` | The one walk over a body that every analysis of the checked tree uses. | Says it. |
| `src/antic/types.c` | Builds and interns the checked types of a compilation, answers the predicates on them, declares the types of `anti.lang` the compiler knows, names types and measures how deep values nest. | None. `types.h` has it. |
| `src/antic/types.h` | The same, with the names of five library modules the compiler knows. | Says "The checked types of a compilation" and nothing of the names. Three comments differ from the code, S320. Fixed `5ec24a80`. |
| `src/antic/sema.c` | The shared helpers of the checker, every declaration pass over the items of a module, and `sema_check`, which runs the passes in order. | None. The first comment is the DESIGN of the one-pass checker. |
| `src/antic/sema.h` | The interface of the checker to the driver, lowering and the header: symbols, constants, the interface of a module, `sema_check` and the queries of the later passes. | None. |
| `src/antic/sema_checker.h` | The state of the checker and the functions its files share, grouped by defining file. | Says it. The `sema.c` line leaves out `provides`, `link` and the simd declaration. |
| `src/antic/sema_chain.c` | Takes a chain of declarations that name each other apart and resolves them leaves first. | Says it, as a reason. |
| `src/antic/sema_class.c` | Checks the class model of every type with fields once the module is declared, and gives lowering and the C header the primary table. | Says it without the table. |
| `src/antic/sema_const.c` | Evaluates constant expressions and gives each constant symbol its value once, in dependency order. | Says it. It also holds `sema_holds_class`. |
| `src/antic/sema_expr.c` | Checks expressions and the conversions a value makes where it stands, and dispatches over the kinds of expression. | Says less: the conversions and the forms of function values are not named. |
| `src/antic/sema_call.c` | Checks calls and members: fields and their visibility, functions of a class and their table, `construct`, the handlers of failing calls and the field inits of literals. | Says it, less the literals. |
| `src/antic/sema_operator.c` | Checks the binary operators, `in` and `??` through the operator table, and finds the `operator fn` functions and language hooks of a type. | Says it. One DESIGN differs from the code, S321. Fixed `67f75fb8`. |
| `src/antic/sema_stmt.c` | Checks statements and function bodies, and walks the functions a worker reaches. | Says it, less the views by `as` and `sema_fills`. |
| `src/antic/sema_value.c` | The value rules: what owns something, what moves, and the copies that `=`, `let`, a literal and a call refuse. | Says it. |
| `src/antic/sema_safety.c` | The checks of the thread-safe classes: the kind of a class, its base, the hidden lock, the guards and the reach of a field. | Says it. |
| `src/antic/sema_simd.c` | The checks of simd structs: operators lane by lane, `as` with the C layout it compares, and the built-ins. | Says it. |
| `src/antic/sema_thread.c` | The atomic operations, `Mutex.new` and `destroy`, the channel operations, and the workers of `parallel` and `dispatch` with `join`. | Says it plus `sync`, which it does not hold. Fixed `5ec24a80`. |
| `src/antic/sema_generic.c` | The generics of the checker: type parameters, constraints, aliases, the copies of generic types, substitution and inference, and the strip of generics before the later passes. | A DESIGN that ends in two stale sentences. Fixed `5ec24a80`. |
| `src/antic/sema_copies.c` | The compiled copies: clones the checked tree of a generic per use, rechecks the open nodes, and rewrites the calls that name the copies. | Says it. |
| `src/antic/sema_hash.c` | The checker's part of hashing and of the default `==`, and the default `equals` and `hash` of each class. | Says it. |
| `src/antic/sema_pattern.c` | The methods of `str`, `[]byte` and a match that become calls of `anti.regex` and `anti.text`, the groups of a literal as fields, the pattern literal with its checks, and `Regex.compile`. | Says it. It also holds `sema_if_let_none`. |
| `src/antic/sema_export.c` | The checks of what crosses to C, export items and the pointers of an `extern fn`, and the interface of a checked module. | Says it. |
| `src/antic/sema_doc.c` | The doc warnings of `anti check`, reading the module alone. | Says it. |
| `src/antic/pattern.c` | Checks a pattern literal: compiles it through the PCRE2 wrapper of the runtime, numbers its groups, and reads it into a tree for `exponential-pattern` and the least match length. | None. |
| `src/antic/pattern.h` | The interface of the same. | Partial: the compile and the check, not the groups or the length. |
| `src/antic/diagnostic.c` | Appends a formatted error, warning, safety check or doc warning to a growable list. | None. |
| `src/antic/diagnostic.h` | The record of one diagnostic with its stable name, and the list of one compilation. | None. The first comment is the DESIGN of the names. |
| `src/antic/warnings.c` | The one table of warning and safety-check names, the application of `allow` and `unchecked`, and the promotion of warnings to errors. | None. |
| `src/antic/warnings.h` | The interface of the same. | None. |
| `src/antic/modpath.c` | Derives a module path and a root-relative file name from a source path, and reads the segments of a module path. | None. `modpath.h` has it. |
| `src/antic/modpath.h` | The same. | Says it. |

### The middle of antic

From `middle`.

| File | Purpose | Header |
|---|---|---|
| `src/antic/ir.h` | The typed three-address IR: its types, instructions, functions, globals and class records, and the builders that append to it. | Says it. |
| `src/antic/ir.c` | The construction of a module of the IR, its tables, and the appenders of instructions. | None. The first comment says where the memory of the module lives. |
| `src/antic/ir_fold.h` | The one rule that folds an integer operation on constants, and the three cases that have no value. | Says it. |
| `src/antic/ir_fold.c` | The same. | None. The first comment describes `defined`. |
| `src/antic/ir_print.c` | The text form of a module of the IR, which `--dump-ir`, `--dump-opt` and the tests read. | None. |
| `src/antic/ir_verify.c` | The check of the structural and type rules of a module, with definite assignment, the facts and the class records. | Says it. |
| `src/antic/lower.h` | The entry of lowering, a checked module into a module of the IR, and its options. | None. The first comment is the DESIGN of `--no-hooks`. |
| `src/antic/lower_lowerer.h` | The state of lowering and the functions its seventeen files share, each under the file that defines it. | Says it. |
| `src/antic/lower.c` | The helpers every file of lowering uses, the declarations of C and runtime functions, and `lower_module`. | None. The first comment describes `lower_ir_type_of`. |
| `src/antic/lower_check.c` | The text of a failed check, the checks of the integer operators and the check of an index against its length. | Says it. |
| `src/antic/lower_class.c` | The functions written for a class: its init, the `construct` for C, the `get` of a singleton, the thunks and the tables of a new object. | Says it. |
| `src/antic/lower_desc.c` | The descriptors, field and function records, tables and class records of classes, structs, variants, tuples, arrays and `?T`. | None. The first comment describes `table_of`. |
| `src/antic/lower_eq.c` | The default `==` of a value and the `equals` of a class. | Says it. |
| `src/antic/lower_error.c` | The error channel: a handled call, its handler, the guards of a `let` and `fail`. | Says it. |
| `src/antic/lower_expr.c` | Expressions: a literal built in place, the operators, the conversions with their checks, and calls with their targets. | None. The first comment describes `wraps`. |
| `src/antic/lower_function.c` | A function body with its parameters, hooks and lock, an anonymous function, and a closure with its context or snapshot. | Says it. |
| `src/antic/lower_hash.c` | The default hash of a value and the `hash` of a class. | Says it. |
| `src/antic/lower_hook.c` | The sites of the hooks, and which classes are traced. | Says it. |
| `src/antic/lower_owning.c` | The one teardown, copy and clear of a value, the destroy and copy of a class, and the preparation of a new object. | Says it. |
| `src/antic/lower_pattern.c` | Pattern literals and the constructor that compiles them. | Says it. |
| `src/antic/lower_place.c` | Places and addresses, the type an access reads memory as, constant data and `here`. | Says it. |
| `src/antic/lower_simd.c` | The operations of simd structs, as one instruction, as one operation per lane or as a loop over the lanes. | None. The first comment is the DESIGN of that choice. |
| `src/antic/lower_stmt.c` | Statements, loops, `switch`, the exit actions of a block and the slots of a function. | None. The first comment describes `lower_has_defers`. |
| `src/antic/lower_sync.c` | Locks, `sync`, `select` and the operations of a channel and a Mutex. | Says it. |
| `src/antic/lower_worker.c` | `parallel` and `dispatch` with their thunks and contexts, and `join`. | Says it, apart from a channel section that belongs in `lower_sync.c`. |
| `src/antic/optimize.h` | The optimizer of the IR over a program, a module or one function, and the drop of the failure arms. | None. |
| `src/antic/optimize.c` | The passes over one function, and the removal of what the entry cannot reach. | None. The first comment is the DESIGN of the three invariants. |
| `src/antic/layout.h` | The layout of the aggregates of the IR for one target, and the folding of its symbolic values. | Says it. |
| `src/antic/layout.c` | The same, with the units of bitfields, the resolution of a function and the bytes of a constant global. | None. |
| `src/antic/memcheck.h` | The checks of AddressSanitizer that `--memory-checks` puts before every load and store. | Says what, and gives a reason of the native back end, M365. |
| `src/antic/memcheck.c` | The declarations and the insertion of those checks. | None. |
| `src/antic/whole.h` | The passes over the IR of the whole program and the class model they share. | Says it. |
| `src/antic/whole_parts.h` | What the two files of the pass share. | Says it. |
| `src/antic/whole.c` | The class model, devirtualisation, the merge of copies, the singleton check, the table facts and `whole_program`, which runs the passes. | None. The first comment is the DESIGN of class identity. |
| `src/antic/whole_tables.c` | The tables the runtime reads: the registry, the default of the backtraces, the trampolines, the slots, the injections and the provides table. | Says it. |
| `src/antic/reach.h` | What the entries of a program reach, by one rule. | Says it. |
| `src/antic/reach.c` | The walk that computes it. | None. The first comment describes `struct walk`. |
| `src/antic/rt_abi.h` | The contract with the runtime: the functions generated code calls and the records it writes. | Says it, and claims one place where five hold the contract. |
| `src/antic/rt_abi.c` | The tables behind that header. | None. The first comment describes a macro. |
| `src/antic/arith.h` | The arithmetic on constants of n bits that the checker, the optimizer and the layout share. | Says it. |
| `src/antic/arith.c` | The same. | None. The first comment describes `extend`. |

### The back end and the drivers of antic

From `back-end`. The rows of `driver.h`, `driver_parts.h`, `antic.h` and
`attributes.h` come from the body of the reports, since no closing section
names them.

| File | Purpose | Header |
|---|---|---|
| `src/antic/llvm_emit.c`, `llvm_emit.h` | The translation of a laid-out module of the IR into LLVM IR text, with the small texts of a package header, a bundle marker, a notice and the names of a `.def` file. | Says it in the header and in the DESIGN of the `.c`, which also lists the steps it was built in. |
| `src/antic/llvm_debug.c`, `llvm_debug.h` | The debug metadata of the LLVM text: the compile unit, the files, the subprograms and the locations, and the spans the build id leaves out. | Says it in the header. The `.c` opens with the comment of `llvm_metadata_string`. |
| `src/antic/llvm_run.c`, `llvm_run.h` | The runs of opt and llc on the text, with the options of each mode. | Says it in the header. The `.c` opens with the comment of `file_name`. |
| `src/antic/llvm_target.c`, `llvm_target.h` | The triple, the data layout, the relocation model and the processor attributes the text carries per target and level. | Says it in the header. The `.c` opens with the comment of the layout table. |
| `src/antic/abi.c`, `abi.h` | The classification of the parameters and the result of a function for the C convention of a target. | Says it in the header. The `.c` opens with the comment of the register counts. |
| `src/antic/driver.c` | The files of a compilation and the run of it through the front end, lowering, the passes and the back end, ending with the text, the library file or the header. | Says it, with "the assembler" that is gone. |
| `src/antic/driver.h` | The options and the entry points of the driver, the header `anti` compiles through. No closing section names it. | Two comments promise a fallback to PATH, S322. One keeps the dead `llvm_mc`. |
| `src/antic/driver_parts.h` | What the five files of the driver share. No closing section names it. | Says "the assembler", as `driver.c` does. |
| `src/antic/driver_index.c` | The plugin index beside a plugin, written with the entries of the other libraries kept. | Says it. Its DESIGN gives a reason that is gone. |
| `src/antic/driver_library.c` | A library for C or a plugin: the archive with its package header and bundled runtime, or the shared library with its exports. | Says it. |
| `src/antic/driver_link.c` | The link of a program: its facts, inputs, Windows paths, host exports and native libraries. | Says it. |
| `src/antic/driver_search.c` | The library files of a compilation, found under the roots and loaded in dependency order, with the link lines they carry. | Says it. |
| `src/antic/linker.c`, `linker.h` | The command lines of lld, the platform linkers and llvm-ar for a program, a shared library, an archive and a relocatable object of each target. | None. `linker.h` describes `struct link_inputs`, and the `.c` opens with the comment of `add_inputs`. |
| `src/antic/coff.c`, `coff.h` | The join of COFF objects into one relocatable object, and the names the linked members of a COFF archive define. | Says it in the DESIGN of the `.c`. `coff.h` describes the input struct. |
| `src/antic/applesdk.c`, `applesdk.h` | The newest SDK of the Command Line Tools that ld64.lld reads. | One item, `version_number`. The header says why Anti fetches no SDK. |
| `src/antic/target.c`, `target.h` | The table of the six targets, their names and the symbols of functions and of C names per object format. | One item, the names table. `target.h` opens with the macOS version. |
| `src/antic/cpu.c`, `cpu.h` | The table of the processor levels, their features, their clang arch and the default of each target. | One item, the table. Both name llvm-mc. |
| `src/antic/header.c`, `header.h` | The C header of a library for C, written from the interfaces of the module and its libraries. | Says it in the header. The `.c` opens with the comment of `scalar_name`. |
| `src/antic/notice.c`, `notice.h` | The licence notice of a set of packages, and the identifier of a component of the runtime archive. | None. |
| `src/antic/docpage.c` | The records of the page of `anti doc`, built from the interface or the syntax tree of a module. | Says it. |
| `src/antic/main.c` | The command line of antic and the run it asks the driver for, on a thread with a stack of its own. | None. |
| `src/antic/antic.c` | The walks over a source and a library file that `anti` asks for through `antic.h`. | Says it. |
| `src/antic/antic.h` | What the `anti` tool may use of antic. It also holds records every part shares. No closing section names it. | Its DESIGN says the first. Its list of shared helpers lacks `attributes.h`. |
| `src/antic/platform.c`, `platform.h` | The platform layer of antic and `anti`: files, environment, arguments, paths, processes and the work thread, once per host. | Says it. |
| `src/antic/userdirs.c`, `userdirs.h` | The directories of a user-local install on each platform, and the runtime archive of the machine. | One item, the DESIGN of `absolute`. The header says why the install is local to the user. |
| `src/antic/attributes.h` | No report gives a sentence. `tool-pass` names it as the header of the format attribute of antic. | Not read by a report. |

### The library file and the leaf code of antic

From `library-file`.

| File | Purpose | Header |
|---|---|---|
| `src/antic/antl.h` | The public face of the library file: its version, the depth limit of a tree and the four entry points. | Says it. |
| `src/antic/antl.c` | The magic, the checks of the enums the tables store, two shared helpers and the top of the reader, `antl_header` and `antl_read`. | Says it. |
| `src/antic/antl_io.h` | The inside of the library file: the writer, the reader and every function the seven files share. | Says it, and says no other file includes it, which `antic.c` does. |
| `src/antic/antl_io.c` | The primitives that write and read an integer, a count, a name and a C string, each with its check. | Says it. |
| `src/antic/antl_write.c` | The writer of the header, the type table, the constants, the items and the IR. | Says it. |
| `src/antic/antl_read.c` | The reader of the type table, the constants and the items, with the checks of each record. | Says it. |
| `src/antic/antl_read_ir.c` | The reader of the IR, which moves each record to the indices of the program and merges the copies it has. | Says it. |
| `src/antic/antl_tree.c` | The section of the generics, written, collected and read by one walker per kind of record. | Says it. |
| `src/antic/antl_verify.c` | The rules of a tree the reader built, with the shape of a simd struct and the type of an IR constant. | Claims every rule of a library file. |
| `src/antic/alloc.c`, `alloc.h` | The checked allocation of antic, which ends the run on a failure or an overflow. | `alloc.c`: none. `alloc.h` says it. |
| `src/antic/arena.c`, `arena.h` | Memory handed out from blocks and freed all at once. | Says it. |
| `src/antic/text.c`, `text.h` | A byte buffer that grows and stays NUL-terminated, and the formatting of a message into a fixed buffer. | `text.c`: one item, `reserve`. `text.h` says it. |
| `src/antic/ptrset.c`, `ptrset.h` | A set of pointers, a map from a pointer to a pointer and one to an index, over one table. | Says it. |
| `src/antic/sha256.c`, `sha256.h` | The SHA-256 digest of a file, by the hash of the runtime. | `sha256.c`: none. `sha256.h` also hands on `digest.h`. |

### The anti tool

From `anti-tool`.

| File | Purpose | Header |
|---|---|---|
| `src/anti/main.c` | The command line of `anti`, its usage text and one function per command. | None. |
| `src/anti/build.c`, `build.h` | `anti build` and `anti run`: a manifest into the files of `build/` and the deliverable of `dist/`. | Says it. |
| `src/anti/check.c`, `check.h` | `anti check`, the four classes that write no artifact of a program. | Says it. |
| `src/anti/deps.c`, `deps.h` | The dependency graph of a project: the lock file, the index, the walk, the comparison of versions and the reader of a library header. | Says the first three. |
| `src/anti/doc.c`, `doc.h` | `anti doc`, the pages of HTML and Markdown from the records antic fills. | Says it. |
| `src/anti/fmt.c`, `fmt.h` | `anti fmt`, the canonical form of a source and the run over the files. | Says the form. |
| `src/anti/test.c`, `test.h` | `anti test`, the runner of the `tests` and `fixtures` blocks of each module. | Says it. |
| `src/anti/repo.c`, `repo.h` | The fetch from a repository into the cache of the user, and the grammar of a name, a version and a digest. | Says it. Two comments of `repo.h` differ from the code. |
| `src/anti/manifest.c`, `manifest.h` | The reader of `anti.toml`. | `manifest.c` says the tables a build passes to antic. |
| `src/anti/project.c`, `project.h` | `anti new`, a project of the default layout. | Says it. |
| `src/anti/syms.c`, `syms.h` | `anti symbols inventory`, `check` and `resolve`. | Says it. |
| `src/anti/symmap.c`, `symmap.h` | The map of a symbols archive, and the readers of the licence notice and of the record of upstream sources. | Says the map. |
| `src/anti/sdk.c`, `sdk.h` | `anti sdk export` and `import`: the stubs of Apple's SDK into a bundle and out of one. | `sdk.c`: one item, the array `macos_targets`. |
| `src/anti/zip.c`, `zip.h` | The writer and the reader of a zip archive, with a decoder of deflate. | `zip.c` says the writer. |
| `src/anti/units.c`, `units.h` | The modules of a run in the order of their imports, the options every compile starts from, and the links of the library files. | `units.c` says the first two. `units.h`: none. |
| `src/anti/memreport.c`, `memreport.h` | The run of a `--memory-checks` program whose report names its frames through Anti's symbolizer. | Says it. |
| `src/anti/files.c`, `files.h` | The allocation helpers and the file helpers of the tool. | None in either. |
| `src/anti/cursor.c`, `cursor.h` | The cursor over one line of text from another machine. | Says it. |
| `src/anti/platform.c`, `platform.h` | The calls that differ by host and that antic does not make. | Says it. |
| `src/anti/licensing.c`, `licensing.h` | `anti license` over the runtime archive, a project and a static archive, with the reader of an `ar` archive. | Says it. |
| `src/anti/jsontree.c`, `jsontree.h` | A tree of a JSON document in a memory pool, built over the scanner of the runtime. | Says it. |
| `src/anti/bind.c`, `bind.h` | `anti bind` and `anti bind --header`. | Says it. |
| `src/anti/bindapi.c` | The reader of an API description into the model. | Says it. |
| `src/anti/bindclang.c` | The run of clang on a header, and the reader of its AST and its preprocessed text into the model. | Says the reader. |
| `src/anti/bindtype.c` | The fixed C types and the parser of a C type spelling. | Says it. |
| `src/anti/bindexpr.c`, `bindexpr.h` | The evaluator of a constant expression of C, and the constant it adds to the model. | `bindexpr.c` says the evaluator. |
| `src/anti/bindwrite.c` | The choice of what a module holds, and the writer of the module, the shim and the two probes. | Says the writer. |
| `src/anti/bindmodel.c`, `bindmodel.h` | The model of a binding, with its memory, its warnings, the keyword test and the tables of what a bundled library links. | Says it, as three things. The comment of `bind_is_keyword` differs from the code, S333. |

### The runtime

From `rt`, and from `library-file` for `json.c`, `toml.c` and `regex.c`.

| File | Purpose | Header |
|---|---|---|
| `src/rt/LICENSE` | No report gives a sentence. `rt` records that the build copies it to `licenses/anti_rt.txt`. | A licence text. |
| `src/rt/assert.c` | The failure routine: one line to standard error, then exit, abort or return, and the failed assertion of a test. | Says nothing of the file. |
| `src/rt/atomic.c` | The atomic operations at a width that comes with the address, by the builtins of the compiler. | Says it in a DESIGN. |
| `src/rt/atomic.h` | The calls of `atomic.c`. | Says it. |
| `src/rt/backtrace.c` | Whether `fail` captures frames, alone so a program that never fails links no default. | Says it. |
| `src/rt/call.c` | `reflect.call` over the trampolines of a program. | Says it. |
| `src/rt/cast.c` | The traps of a failed checked cast and of an object whose table is not set. | None. |
| `src/rt/check.c` | The traps of a failed dev-mode check and of a collection changed during a walk. | None. |
| `src/rt/conf.c` | The keys, layers, options and file of the runtime configuration, and the start of the injections. | Says the first half. |
| `src/rt/conf.h` | The record of an injectable interface and the five calls of the configuration. | Says the second half. |
| `src/rt/conf_include.h` | The bounds and the messages of the include walk, shared with `anti symbols`. | Says it. |
| `src/rt/cpu.c` | The table of processor levels, and the check of the machine against the level of the runtime. | Says something else, S327. |
| `src/rt/cpu_level.h` | The processor levels, shared by antic and the runtime. | Says it. |
| `src/rt/digest.c` | SHA-256 for the runtime, antic and `anti`. | Says it. |
| `src/rt/digest.h` | The calls of `digest.c`, in pieces and over a stream. | Says it. |
| `src/rt/errno.c` | The value of `errno` for an `extern fn`. | Says it in a DESIGN. |
| `src/rt/f16.c` | The two `f16` conversions a program calls where the processor has no instruction. | Says it in a DESIGN. |
| `src/rt/f16.h` | The conversions between `f16` and `f32`, shared with antic. | Says it. |
| `src/rt/fs.c` | The C side of `anti.fs`: open, size, read whole, list, remove and rename. | Says it. |
| `src/rt/grow.h` | The one growth rule of a block of the C library. | Says it. |
| `src/rt/hash.c` | The hash of bytes and the seed of the hashing collections. | Says it. |
| `src/rt/hash.h` | The constants and the last step of a hash, shared with antic. | Says it. |
| `src/rt/hooks.c` | What a hook site does: the installed handler, then the object's own hook. | None. `hooks.h` says it. |
| `src/rt/hooks.h` | The calls of the nine hooks and of `Trace.install`. | Says it. |
| `src/rt/init.c` | The start flag of the runtime and the option of the backtrace. | None. |
| `src/rt/io.c` | The write and the exit of `anti.io` through the C streams. | Says nothing of the file. |
| `src/rt/json.c` | A scanner of JSON text at the level of tokens, which builds nothing. | Says it. |
| `src/rt/json.h` | A scanner of JSON text at the level of tokens and positions. | Says it. Two comments differ from the code, S324. |
| `src/rt/license.c` | The licence text and the runtime version, read from the notice of the program. | None. |
| `src/rt/license.h` | The markers of the notice and its build id line, shared with antic and `anti`. | Says it. |
| `src/rt/license_stub.c` | The empty licence text and version of a bundled static library. | Says it after the include. |
| `src/rt/loaded.c` | The slots of the open libraries, their lock and the count of live objects of each. | Says it, with a false claim, S328. |
| `src/rt/lock.c` | The Mutex, the hidden lock of a synchronized object and the lock orders of a dev build. | Says it. |
| `src/rt/mem.c` | Aligned memory for `anti.mem`, the out-of-memory end and `anti_rt_grow`. | Says it. |
| `src/rt/object.c` | The C behind `anti.lang.Object`: the functions of the root, the walk that writes JSON and text, and delete, destroy, dup and give. | None, and its first DESIGN is stale, S331. |
| `src/rt/object.h` | The layout of descriptors, field records and tables, and the calls of `object.c` and `snapshot.c`. | Says the first two. |
| `src/rt/patterns.c` | The run-time side of `anti.regex`: literals, messages, searches, groups, replace, patch and stops. | Says the first two. |
| `src/rt/platform.h` | What the runtime asks of the system, apart from the clocks, the error texts and the kept block. | Says all of it. |
| `src/rt/platform_entry.c` | The entry point of a Windows program, in an object only the link of a program pulls in. | Says it. |
| `src/rt/platform_posix.c` | The platform layer on macOS and Linux. | Says it. |
| `src/rt/platform_stdio.c` | The printf and scanf families and `atexit` of a Windows target, weak and never bitcode. | Says it. |
| `src/rt/platform_windows.c` | The platform layer on Windows, and the static part of the C runtime. | Says the first half. |
| `src/rt/plugin.c` | Loading a library that provides interfaces: the checks, the objects and discovery through the index. | Says it, and claims more checks than it makes. |
| `src/rt/plugin.h` | The table a plugin carries, the slots of the host and the calls of the loader. | Says it. Two comments differ from the code, S328. |
| `src/rt/plugin_index.h` | The file name of the index of plugins, shared with antic and `anti`. | Says it. |
| `src/rt/reflect.c` | The walks of a descriptor behind `anti.reflect`, with `get` and `set` of a field. | Says it. |
| `src/rt/reflect.h` | The layout of `Value`, the reasons, the calls of `reflect.c` and the trampoline table. | None. |
| `src/rt/regex.c` | The compile of a text and a byte pattern with PCRE2, the rewrite of a byte pattern, and the groups and template pieces. | None. The first comment is that of a define. |
| `src/rt/regex.h` | What `regex.c` and `patterns.c` share with antic: the compile, the groups, the template pieces and the layout of `Match`. | Says the compile alone. |
| `src/rt/registry.c` | `reflect.new` and `Object.deserialize` over the registry of classes. | Says it. |
| `src/rt/registry.h` | The records of the registry and its two calls. | Says it. |
| `src/rt/rt.h` | Declarations of the runtime that have no header of their own. | None. Two comments differ from the code, S326. |
| `src/rt/signal.c` | The functions a program registers for signals, and their delivery. | Says it. |
| `src/rt/signals.h` | The two console signals and the two calls of `signal.c`. | Says nothing of the file. |
| `src/rt/snapshot.c` | The heap snapshots of `snapshot fn` and their count. | Says it. |
| `src/rt/start.c` | The C `main`: the processor check, init, the arguments, the option pass, the program and its status. | Says the arguments, the call and the status. |
| `src/rt/std.h` | The C functions behind eight modules, `struct anti_text` and the failure routines. | Names six modules. |
| `src/rt/symbols.c` | Bounds-checked readers of ELF and Mach-O symbol tables, the debug map and DWARF line tables. | None. `symbols.h` says it. |
| `src/rt/symbols.h` | The calls of `symbols.c`. | Says it. |
| `src/rt/sync.c` | The channels of `chan T` and `select`. | Says it. |
| `src/rt/sync.h` | The calls of locking and channels that generated code makes. | Says it. |
| `src/rt/text.c` | The C behind `anti.text`: text helpers, the buffer of a Builder, floats to text and text to floats. | None. The first comment is the DESIGN of `to_bytes`. |
| `src/rt/threads.c` | The worker pool behind `parallel`, `dispatch` and `join`. | Says `parallel`. |
| `src/rt/threads.h` | The calls of `parallel`, `dispatch` and `join`. | Says it. One comment differs from the code, S329. |
| `src/rt/toml.c` | One pass over a subset of TOML that gives a flat list of dotted keys. | Names two of its seven callers. |
| `src/rt/toml.h` | A read of the TOML subset the library, the logger and the configuration share. | Says it. |
| `src/rt/trace.c` | The module and the build id of a frame, the text of a trace and the choice of a symbol reader. | Says it, with a false claim, S325. |
| `src/rt/trace.h` | The layouts and the calls behind `anti.lang.StackTrace`. | Says it. |
| `src/rt/utf.c` | UTF-8 and UTF-16 conversion and the split of a Windows command line. | None. `utf.h` says it. |
| `src/rt/utf.h` | The calls of `utf.c`. | Says it. |

### The standard library

From `std`. The header of a module is its `//!` comment.

| File | Purpose | Header |
|---|---|---|
| `src/std/LICENSE` | The 0BSD text of the library. | A licence text. |
| `src/std/.gitkeep` | Nothing. A leftover of the step before the directory held a module. | An empty file. |
| `src/std/anti/lang.anti` | The root that imports nothing: the types the compiler knows by name, `Error` with its text, the trace of a stack, the hook handler and the hash seed. | Names `Object` and `Job` alone. |
| `src/std/anti/io.anti` | Console output and the exit of a program. | Says it. |
| `src/std/anti/text.anti` | Functions on `str`, and `Builder`, which collects bytes and formats values. | Says it. |
| `src/std/anti/license.anti` | The licence notice of the program. | Says it. |
| `src/std/anti/error.anti` | `SystemError`, `on_fatal` and `check`, beside the `Error` of `anti.lang`. | Says it. |
| `src/std/anti/time.anti` | `Duration`, the two clocks, and the interface `Clock` with its default. | Says it. |
| `src/std/anti/os.anti` | Signals, and the three per-user directories of the platform. | Says it. Two subjects under one title. |
| `src/std/anti/fs.anti` | Files by handle and by whole file, and the interface `FileSystem` with its default. | Lists no `read_file` and no `write_file`. |
| `src/std/anti/reflect.anti` | Reading the descriptor of a class: fields, functions, `get`, `set`, `new`, `call` and `Value`. | Says it. |
| `src/std/anti/random.anti` | The generator `Random`, the interface `Source` and the shared default. | Gives eight words of state for four. |
| `src/std/anti/collection.anti` | The interfaces of walking, the bases `Collection<T>` and `Indexed<T>`, `Copies<T>`, the hashes, the writers of a map entry and the stable sort. | Names the first three alone. |
| `src/std/anti/toml.anti` | Reading the TOML subset into a flat `Document`. | Says it. |
| `src/std/anti/config.anti` | The settings of the program: the interface `Config` and `FileConfig`. | Says it. |
| `src/std/anti/args.anti` | A command line declared once, parsed and read by name. | Says it. Lists three forms of an option, and the code takes two more. Fixed `fc960111`. |
| `src/std/anti/json.anti` | Writing and reading the members of the JSON that `serialize` writes. | Says it. |
| `src/std/anti/log.anti` | Levelled logging: sinks, the interface `Logger`, the logger of the program and the file that configures it. | Describes a file the code does not read, S332, fixed `fd716910`. |
| `src/std/anti/debug.anti` | `backtrace`, the short form of `StackTrace.capture`. | Says it. |
| `src/std/anti/mem.anti` | The interface `Allocator`, its two implementations, and `Shared<T>`. | Says it. |
| `src/std/anti/runtime.anti` | `configure` and `get` of the runtime configuration. | Says it. |
| `src/std/anti/simd.anti` | A header alone: the three mask functions the compiler knows by name. | Says it. |
| `src/std/anti/trace.anti` | The seven stock handlers of the hooks, `install`, `start` and their tests. | Claims atomic counts for all seven and names no `install`. Fixed `bd5e69c6`. |
| `src/std/anti/plugin.anti` | Loading a shared library and asking it for the class it provides. | Says it. |
| `src/std/anti/regex.anti` | The run-time half of patterns: the errors, `compile`, the iterators, every function the pattern methods call, and `patch`. | The first line names the first two alone. |
| `src/std/anti/collection/list.anti` | `List<T>`, with `sort`, `sort_by` and `map`. | Says it. |
| `src/std/anti/collection/deque.anti` | `Deque<T>`, a ring that grows at both ends. | Says it. |
| `src/std/anti/collection/ring.anti` | `Ring<T, N>`, a ring of fixed room. | Says it. |
| `src/std/anti/collection/grid.anti` | `Grid<T>`, a rectangle of cells that never changes its size. | Says it. |
| `src/std/anti/collection/queue.anti` | `PriorityQueue<T>`, a binary heap, and its walk. | Opens with "Ordered retrieval" and no class. |
| `src/std/anti/collection/map.anti` | `Map` and `HashMap`, their two tables for any element, their walks and the sort of serialized texts. | Names no sort. |
| `src/std/anti/collection/set.anti` | `Set` and `HashSet` on the tables of the maps, and `BitSet`. | Says it. |
| `src/std/anti/collection/sorted.anti` | `SortedMap` and `SortedSet` over one B-tree in one block. | Says it. |
| `src/std/anti/collection/pool.anti` | `Slots<T>`, `Handle<T>`, `Pool<T>`, the failure `Changed` and the walk. | Names no `Changed`. |
| `src/std/anti/collection/tree.anti` | `Tree<T>`, a hierarchy on the slots of the pool. | Says it. |
| `src/std/anti/collection/synchronized.anti` | Four synchronized classes around a plain collection each, with a version per element. | Names no `sort`, `sort_by` and `map`. |
| `src/std/anti/collection/concurrent.anti` | `ConcurrentMap`, `SpscRing` and the walk `Snapshot<T>`. | Names no `Snapshot`. |

### The recipes of the native libraries

From `build-and-tools`.

| File | Purpose | Header |
|---|---|---|
| `src/native/CMakeLists.txt` | Holds the helpers of the recipes and includes them in order. | Says it. One comment on the flags is wrong, M342. |
| `src/native/get-pcre2.cmake` | Downloads and unpacks the pinned PCRE2 source. | Says it. |
| `src/native/get-sqlite.cmake` | The same for the SQLite amalgamation. | Says it. |
| `src/native/get-mbedtls.cmake` | The same for Mbed TLS. | Says it. |
| `src/native/get-miniaudio.cmake` | The same for miniaudio. | Says it. |
| `src/native/get-mimalloc.cmake` | The same for mimalloc. | Says it. |
| `src/native/warnings.cmake` | Holds the warning flags of each third-party library. | Says it. |
| `src/native/pcre2-files.cmake` | Lists the sources and the definitions of PCRE2 for the build and the packer. | Says it. |
| `src/native/pcre2-source.cmake` | Fetches PCRE2, writes its generated files and builds the copy antic links. | Says it. |
| `src/native/pcre2.cmake` | Builds PCRE2 and the regex code of the runtime for every target, with their tests. | Says it. |
| `src/native/sqlite.cmake` | Builds SQLite for every target, with its tests. | Names `anti.db`, which does not exist. |
| `src/native/mbedtls.cmake` | Builds Mbed TLS for every target, with its tests. | Names `anti.net` and a client `anti` does not have. |
| `src/native/media.cmake` | Holds the targets, the flags and the glibc link test of miniaudio and raylib. | Says it. Names two modules that do not exist. |
| `src/native/miniaudio.cmake` | Builds miniaudio for every media target, with its tests. | Names `anti.miniaudio`. |
| `src/native/raylib.cmake` | Builds raylib for every media target, with its tests and the audio link test. | Names `anti.raylib`. |
| `src/native/mimalloc.cmake` | Builds mimalloc for both musl targets at every level. | Says it. |

### The unit tests

From `tests`.

| File | Purpose | Header |
|---|---|---|
| `tests/unit/check.h` | The two assertion macros, which count a failure and go on. | Says it. |
| `tests/unit/path_rules.h` | The cases of the path rules that both platform layers are run over. | Says it. |
| `tests/unit/pipeline.h` | The stages the unit tests run a source through, and the IR print without the types of the runtime. | Says it. |
| `tests/unit/pipeline.c` | Defines what `pipeline.h` declares. | None. |
| `tests/unit/test_main.c` | The table of the 51 entry functions by file, and the `main` that runs the files one argument names. | None. |
| `tests/unit/plugin_table.c` | A shared library with an empty plugin table, which `test_plugin_race.c` and `test_platform.c` load. | Says it. |
| `tests/unit/plugin_bad.c` | A shared library with a writable plugin table, which `test_plugin_table.c` damages and loads. | Says it. |
| `tests/unit/symbols_probe.c` | A program that looks a function up in a binary of another target with the readers of `src/rt/symbols.c`. | Says it. |
| `tests/unit/test_abi.c` | `abi_classify`, one case per rule of the calling conventions. | Says it. |
| `tests/unit/test_alloc_oom.c` | A program that asks one allocation helper of antic for more memory than a host gives, for `run_alloc_oom.cmake`. | Says it. Cites `M03` without its audit. |
| `tests/unit/test_arith.c` | The upper half of a product and the saturating operations of `arith.c` at every width. | One item. |
| `tests/unit/test_bind.c` | The parser of C type spellings and the evaluator of constant expressions of `anti bind`. | Says it. |
| `tests/unit/test_coff.c` | The join of COFF objects and the exports of an archive. | Says it. |
| `tests/unit/test_conf_race.c` | `rt.configure` replacing a value while another thread reads it. | Says it. |
| `tests/unit/test_cpu.c` | The level table of antic, the names of `--cpu` and the start-up check of the runtime. | Says it. |
| `tests/unit/test_deps.c` | The manifest, the version constraints and the repository URLs of `anti build`. | Says it. |
| `tests/unit/test_f16.c` | The conversions between `f16` and `f32`. | Says it. |
| `tests/unit/test_files.c` | The file helpers of `anti` and the walk over a directory tree. | Says it. |
| `tests/unit/test_files_oom.c` | A program that asks one allocation helper of `anti` for more memory than a host gives, for `run_files_oom.cmake`. | Says it. |
| `tests/unit/test_float_read.c` | The reader and the writer of the text of a float in `src/rt/text.c`. | Says it. |
| `tests/unit/test_fmt.c` | `anti fmt` over two sources the parser would refuse. | Says it. |
| `tests/unit/test_fs.c` | `anti_rt_fs_read`. | Says it. |
| `tests/unit/test_header.c` | The C header antic writes for a library, for owning structs and simd lanes. | One item. |
| `tests/unit/test_interface.c` | The walks of `antic.h` that the commands of `anti` call. | Says it. |
| `tests/unit/test_ir.c` | The IR built by hand, its printed form and its verifier. | None. |
| `tests/unit/test_json.c` | The JSON scanner on malformed input. | Says it. |
| `tests/unit/test_layout.c` | The sizes, alignments, bitfields and refusals of `layout.c`. | None. |
| `tests/unit/test_lexer.c` | The tokens, the literals and the errors of the lexer. | None. |
| `tests/unit/test_link.c` | The command lines of the linker per target and mode, the names of an SDK and the markers of the notice. | None. Three parts. |
| `tests/unit/test_llvm_emit.c` | `llvm_emit_module`, by the sections of the work order of the LLVM back end. | Says it. |
| `tests/unit/test_llvm_target.c` | The triple, the relocation model and the function attributes per target and level. | Says it. |
| `tests/unit/test_lower.c` | The IR that lowering writes for a source, and the table of the runtime functions against the prototypes of `src/rt/`. | None. |
| `tests/unit/test_modules.c` | The library file written, read back and damaged, the imports across modules and the module paths. | None. |
| `tests/unit/test_nullable.c` | The rules of nullable pointers in the checker. | After the includes. |
| `tests/unit/test_optimize.c` | The passes of `optimize.c` over the IR of a source. | None. |
| `tests/unit/test_parser.c` | The tree the parser builds, its errors and its depth limit. | None. |
| `tests/unit/test_platform.c` | The platform layer of the runtime on the host. | Says it. |
| `tests/unit/test_plugin_race.c` | The table of open libraries under threads that load and unload at once. | Says it. |
| `tests/unit/test_plugin_table.c` | The loader of the runtime against a damaged plugin table and a damaged index. | Says it. |
| `tests/unit/test_rt_bounds.c` | The growth of a block, the size of a text builder and atomic subtraction at the minimum of each width. | Says it, as three parts. |
| `tests/unit/test_sema.c` | What the checker accepts, refuses and warns of. | One item. |
| `tests/unit/test_sha256.c` | The SHA-256 of `src/rt/digest.c` over inputs given in pieces. | One item. |
| `tests/unit/test_signal.c` | The signal table of the runtime under threads that register at once. | Says it. |
| `tests/unit/test_symbols.c` | The readers of `src/rt/symbols.c`: the names of COFF, the sections and line tables of ELF and the symbols of Mach-O. | One item. |
| `tests/unit/test_syms.c` | `anti symbols` over archives and programs that name nothing. | Says it. |
| `tests/unit/test_sync.c` | The rules of locking and channels in the checker. | After the includes. |
| `tests/unit/test_target.c` | The mangled names, the C names and the block labels of `target.c`, and the target of the host. | None. |
| `tests/unit/test_text.c` | The text buffer of antic and the cut of a long line of a message. | None. Two parts. |
| `tests/unit/test_toml.c` | The TOML subset of `src/rt/toml.c`. | Says it. |
| `tests/unit/test_tool_platform.c` | The platform layer of antic and `anti` on the host. | Says it. |
| `tests/unit/test_types.c` | The type table and the pointer sets of `ptrset.c`. | None. Two parts. |
| `tests/unit/test_userdirs.c` | The three user directories under an environment the test gives. | One item. |
| `tests/unit/test_utf.c` | The repair, the conversion and the split of UTF-8 in `src/rt/utf.c`. | One item. |
| `tests/unit/test_variant.c` | The rules of sum types in the checker. | After the includes. |
| `tests/unit/test_whole.c` | The pass over the whole program. | One item. |
| `tests/unit/test_zip.c` | The zip reader of `anti symbols` on damaged archives. | Says it. |

### The scripts of tools

From `build-and-tools`.

| File | Purpose | Header |
|---|---|---|
| `tools/check-cpu.cmake` | Refuses a package whose runtime levels are not those of `tools/cpu-levels`. | Says it. |
| `tools/check-libc.cmake` | Refuses a Linux program that needs a glibc above 2.35. | Says it. |
| `tools/check-llvm.cmake` | Checks the version of the LLVM tools in a directory and, with `DEST`, copies them into the runtime archive. | Says it. Two purposes, M341. |
| `tools/check-offline.cmake` | Builds a raylib program, a plugin host and five cross links from an install alone. | Says it. |
| `tools/check-pdb.cmake` | Checks the CodeView record of a Windows executable against the GUID of its PDB. | Says it. |
| `tools/clang-release.cmake` | Gives the version, the tag and the release page of the pinned clang to the build of `anti`. | Says it. |
| `tools/compile.sh` | Configures, builds and tests one of the four presets. | Says something else: the downloads of the first build, S334. |
| `tools/deps-dir.cmake` | Names the directory of the pinned downloads. | Says it. |
| `tools/docs-style/check_docs.py` | Checks `.md` files against the docs-style rules. | Says it. |
| `tools/docs-style/banned-terms.md` | Lists the banned terms for a reader. | Names `scripts/check_docs.py`. |
| `tools/docs-style/__pycache__/check_docs.cpython-314.pyc` | A compiled copy of the checker, which should not be tracked. | None. |
| `tools/downloads.html.in` | The template of the downloads page that step 9 of a release fills. | Says it. |
| `tools/fetch-release.cmake` | Downloads one archive of the LLVM release and checks it against the pin, the manifest and the signature. | Says it, and also holds `antic_machine_host`. |
| `tools/get-clang.cmake` | Installs the pinned clang of the host. | Says it. |
| `tools/get-llvm.cmake` | Installs the pinned LLVM tools of the host, or lays out those of another host for the packer. | Says it. Calls llvm-mc the assembler. |
| `tools/get-raylib.cmake` | Downloads and unpacks the pinned raylib source. | Says it, and not who runs it. |
| `tools/get-sysroot.cmake` | Installs the sysroot of each named target and the licences of its components. | Says it. |
| `tools/git-hooks.cmake` | Points `core.hooksPath` of the checkout at `tools/hooks`. | Says it. |
| `tools/hooks/pre-commit` | Refuses a commit whose staged `.md` files fail the checker. | Says it. |
| `tools/host-compile.cmake` | Gives the options of the pinned clang that compile antic or `anti` for one host. | Says it. |
| `tools/install.sh` | Installs the package of a macOS or Linux host after checking the signature and the digest. | Says it. |
| `tools/install.ps1` | The same for a Windows host. | Says it. One DESIGN comment is stale. |
| `tools/libc-versions.cmake` | Gives the versions of musl and mimalloc to the build of antic. | Says it. |
| `tools/macos-sdk.cmake` | Finds the pinned Apple SDK and checks the digest of its `libSystem.tbd`. | Says "a release", and every Mac build uses it. |
| `tools/pack-anti.cmake` | Compiles antic and `anti` for each named host, and writes its package and the manifest. | Says it. Names `build/clang`, and does not list the bitcode. |
| `tools/pinned-compiler.cmake` | Fetches the pinned tools and sets the compiler, the archiver and the linker of a build. | Says it. |
| `tools/pinned-tools.cmake` | Holds the list of the LLVM tools and the checks that refuse a tool of the host. | Says it. |
| `tools/publish.cmake` | Checks that a release directory holds what its signed manifest names and nothing else. | Says something else: an upload, M344. Now `tools/check-release.cmake`, which says it, fixed `2473f1d3`. |
| `tools/release.sh` | Makes a release in ten steps, from the preflight to the report. | Says it. |
| `tools/sources.cmake` | Lists the C sources of antic and `anti` for the build and the packer. | Says it. |
| `tools/std-modules.cmake` | Orders the collection modules of the standard library by their imports. | Says it. |
| `tools/uninstall.sh` | Removes an install that carries the marker, with its executables and its profile line. | Names an address that is not published, M368. Published since `cfec4ad3`. |
| `tools/uninstall.ps1` | The same for Windows. | Names an address that is not published, M368. Published since `cfec4ad3`. |
| `tools/upstream-sources.cmake` | Reads pins, builds the URL of a pinned source and writes `licenses/sources.txt`. | Says it. Three jobs under one name. |
| `tools/warnings.cmake` | Holds the warning set of our own C. | Says it. |
| `tools/windows-compile.cmake` | Holds the triples, the compile options and the link options of a Windows target. | Says it. |

### The pins and data of tools

From `build-and-tools`. A pin has no code, and its sentence is what it
holds.

| File | Holds | Header |
|---|---|---|
| `tools/clang-pin` | The release, the asset name and six digests of the pinned clang. | Says it. |
| `tools/llvm-pin` | The same for the LLVM tools. | Says it. |
| `tools/llvm-version` | The LLVM version, one line. | A data file with no comment. |
| `tools/version` | The version of Anti, one line. | A data file with no comment. |
| `tools/cpu-levels` | The processor levels and the default level of each target. | Says it. One column is of llvm-mc. |
| `tools/sysroot-pins` | The pins of musl, the glibc packages and mingw-w64. | Names `get-sysroot.sh`. |
| `tools/zig-stubs-pin` | The pin of the Zig release and of the APSL text. | Says it. |
| `tools/macos-sdk-pin` | The version, the name and the digest of the Apple SDK. | Says "a release", as the script does. |
| `tools/raylib-pin` | The pin of raylib and the miniaudio version it bundles. | Says it. |
| `tools/pcre2-pin` | The pin of PCRE2. | Says it. |
| `tools/sqlite-pin` | The pin of SQLite. | Says it. |
| `tools/mbedtls-pin` | The pin of Mbed TLS. | Says it. |
| `tools/miniaudio-pin` | The pin of miniaudio. | Says it. |
| `tools/mimalloc-pin` | The pin of mimalloc. | Says it. |
| `tools/release-base` | The addresses of the GitHub release. | Says it. |
| `tools/site-base` | The addresses of the site, the signature and the key. | Says it. |
| `tools/keys/release.pem` | The public key of the release. | A key file. |
| `tools/keys/public/release.pem` | A second copy of the public key, which no script of `tools/` reads. | A key file. |

## Decisions a fix step needs

The reports leave these to Eddie. A step that needs one names it.

- S323. Whether each entry of `docs/decisions.md` on the native back end
  becomes history in the past tense, moves to its work order or goes. The
  entry on C99 at line 1229 needs a word on `DW_LANG_C11`.
- The option `--llvm-mc`. The entry at `docs/decisions.md:19` keeps it
  until a separate decision. Step 27 waits for that decision. Decision 2
  says it goes and llvm-mc leaves the package, fixed `c0f1e8e1`.
- S334. Whether the configure step fetches the sysroots and raylib, or the
  header of `tools/compile.sh` says what `README.md` says. It is a
  question of the build configuration.
- S312 and M330. The one rule for memory that runs out in the library:
  stop the program, fail, or give an empty text the caller can tell apart.
  The closure left the builder as a question.
- S326. Whether a static library for C runs `anti_rt_init` on its first
  use, as the provisional entry at `docs/decisions.md:399` assumes, or the
  comment and the entry change.
- M317. The separator of the plugin search path on Windows. The entry at
  `docs/decisions.md:1483` fixes `:` and does not speak of Windows.
- M316. Whether a file name that is no UTF-8 is repaired on macOS and
  Linux. The other answer is a stated exception for a name that must open
  a file.
- M363. Whether the site page of the build tool is the design to build
  towards or the description of `anti`.
- M368. Whether step 9 of a release publishes the uninstallers, or their
  headers name the copy a user has. Decision 9 says step 9 does, fixed
  `cfec4ad3`.
- The four rows the closure of the second audit left: `Changed` and
  `Snapshot`, the names of size and presence, the `?str` of `args`, and a
  new directory under `tests/` for the runners. Decision 13 moves them to
  `tests/runners/`, fixed `b906e240`.
- The `Interlocked` branch of `src/rt/atomic.c:98`, which no host
  compiles. Whether a reader's build with the system compiler keeps it.
- The cache key of a library file in `anti build`, defect 10 of
  `anti-tool`. Whether it holds the digests of the imports. Decision 15
  says it does, fixed `1f7a28f3`.
- The Build Tools on the Windows VM, which `tools/release.sh:688` loads
  for Ninja alone. Decision 16 gives the VM a pinned Ninja. The call
  also gives the configure the `rc.exe` of the Windows SDK, which CMake
  asks for with clang on Windows. No pinned tool replaces it, so step 24
  left the call, `docs/reports/2026-10-17-solid-step24.md`.
- Whether `A => free(p)` is a statement of a `switch` arm. `front-end` did
  not read the specifications for it.
- The grade of zero added to a null pointer at `src/anti/zip.c:673`, which
  `anti-tool` grades minor and Eddie may raise.

## Fix steps

Each step is one session's work and stays within the files of one area. It
names the findings it closes. Each fix of code starts with the test that
shows the defect, which also closes the rule 15 part of that finding.
Steps 1 to 17 close every severe finding. Steps 3, 19 and 27 cross an area
by their nature, and each says so. A step that adds, moves or renames a
file after step 28 changes `docs/notes/source-tree.md` in the same commit.

1. Runtime, the life of a loaded library. S306, S307, S308 and M303. The
   build id is kept as the path is. One object is counted under the lock
   in `registry.c`. The count of a library falls after the teardown
   returns. Files: `trace.c`, `registry.c`, `hooks.c`, `loaded.c`,
   `plugin.c` and `tests/unit/test_plugin_race.c`. S308 may need the order
   of the hook in `src/antic/lower_owning.c:559`.
2. Runtime, the checks of the plugin loader. S309, S310 and M309.
   `list_sound` on the three lists, the width in `fields_sound`, a stub
   table sized by the class, and a refused empty directory. Files:
   `plugin.c`, `tests/unit/test_plugin_table.c` and `tests/plugin/`.
3. Texts and memory that runs out. S311, S312, M301, M302 and M330, after
   the decision on the one rule. The step crosses the runtime, lowering
   and the library by its nature. Files: `src/rt/snapshot.c`, `text.c`
   and `object.c`, `src/antic/lower_function.c`, and `text.anti`,
   `log.anti`, `error.anti` and `mem.anti` of the library.
4. Runtime, patterns and the readers of text. S324, S330 and M313, with
   the minor findings of `regex.c` and `toml.c` under rules 5, 12 and 14.
   A test over every prefix of one byte pattern. Files: `json.c`,
   `json.h`, `patterns.c`, `regex.c`, `toml.c` and their unit tests.
5. Runtime, comments that state what the code does not do. S325 to S329
   and S331, with M325, which gives the start of the injections a file of
   its own. S326 waits for its decision. Files: `trace.c`, `rt.h`,
   `init.c`, `cpu.c`, `plugin.h`, `loaded.c`, `threads.h`, `object.c`,
   `conf.c` and `conf.h`.
6. Standard library, indices and sizes. S315, S316 and M304. A check of
   `at` or a private table for the place functions, one checked product in
   `room_from`, and a counted change wherever the room moves. Files:
   `collection.anti` and the modules under `collection/`.
7. Standard library, copies and versions. S318, M338 and M366. A `copy`
   or a refusal for `Snapshot`, `ArenaAllocator`, `FileSink`, `NotOne` and
   `Changed`, one stamp rule for both maps, and the growth of a `Map`.
   Files: `concurrent.anti`, `synchronized.anti`, `map.anti`, `pool.anti`,
   `mem.anti` and `log.anti`.
8. Standard library, `anti.log` and `anti.args`. S313, S317 and S332, with
   the minor finding on the forms of an option. The kinds of a sink move
   out of the object `construct` fills. Files: `log.anti`, `args.anti` and
   `tests/std/`.
9. Standard library, `anti.trace` and guarded state. S314, M305, M318 and
   M361. Atomic counters and a `Mutex` around every table of more than one
   word. Files: `trace.anti`, `random.anti`, `config.anti`, `log.anti`,
   and the notes `trace-handlers.md` and `hooks.md`.
10. antic checker. S301, S302, S321 and M328, with the sentences of S320
    in `types.h`, `sema_generic.c` and `sema_thread.c`. Files: `sema.c`,
    `types.c`, `sema_const.c`, `sema_copies.c`, `sema_operator.c`,
    `sema_stmt.c`, `tests/errors/` and `tests/unit/test_sema.c`.
11. antic lexer, parser and pattern reader. S303, M306, M307 and M308,
    with the two sentences of S320 in `lexer.h`. Files: `lexer.c`,
    `lexer.h`, `parser_stmt.c`, `parser_expr.c`, `pattern.c`,
    `tests/unit/test_lexer.c` and `tests/unit/test_parser.c`.
12. antic library file. S304, S305, M329 and M336, with the five minor
    findings of the readers under rule 14. One list of the declared types
    in `types.c`, and one limit of nesting. Files: the `antl_*.c` files,
    `types.c` and `tests/unit/test_modules.c`.
13. antic driver and the host. S322, M324 and M335, with the minor
    findings on the forms of a message. One helper of the platform layer
    for the file name of a path, a symbolic link in that layer, and a test
    of `--soname`. Files: `driver.h`, the `driver*.c` files, `linker.c`,
    `llvm_run.c` and `platform.c`.
14. `anti bind`. S319, S333, M311 and M312, with the first two pairs of
    M339 and the minor finding on names. Files: `bindexpr.c`,
    `bindmodel.c`, `bindmodel.h`, `bindclang.c`, `bindapi.c`,
    `bindwrite.c` and `tests/unit/test_bind.c`.
15. Tests that cannot fail. S335, M319, M320, M369 and M370. Each check is
    first shown to pass on a broken input. Files:
    `tests/runners/run_codeview_empty.cmake`, `tests/CMakeLists.txt` and the
    runners those findings name.
16. The first build and the fetch scripts. S334, M327, M331 and M340,
    after the decision on the configure step. One function fetches a
    pinned source, and raylib gets a recipe like the other five. Files:
    `tools/compile.sh`, `tools/get-raylib.cmake`, `tools/release.sh`,
    `CMakeLists.txt`, `tests/CMakeLists.txt` and `src/native/`.
17. The decisions on the native back end. S323 and the minor finding on
    stale file names, after Eddie's answers. Files: `docs/decisions.md`
    and `CLAUDE.md`. A commit of documents alone.
18. Runtime, the headers and the platform layer. M321, M337, M316 and
    M317, with the minor finding under rule 22. `struct anti_text` and the
    failure routines get headers of their own, and one walk lists a
    Windows directory. Files: `std.h`, `rt.h`, `platform.h` and the
    platform files.
19. The contract outside `rt_abi.h`. M332. A row of `rt_abi.h` or a unit
    test per pair, as `match_layout` holds `Match`. The step crosses
    antic, the runtime and the library by its nature. Files: `rt_abi.h`,
    `whole_tables.c`, `lower_desc.c`, the headers of `src/rt/`,
    `reflect.anti`, `regex.anti`, `lang.anti` and `tests/unit/`.
20. Runtime readers under a sanitizer. M314 and M315. C test programs that
    compile `registry.c` and `object.c` with stand-ins, as
    `test_conf_race.c` does, and ELF files cut short. Files: `tests/unit/`
    and `tests/CMakeLists.txt`.
21. `anti`, the formatter, the sources of a project and the notice. M310
    and M322, with the last two pairs of M339 and defects 6 and 7 of
    `anti-tool`. Files: `fmt.c`, `main.c`, `check.c`, `build.c`, `deps.c`,
    `symmap.c`, `licensing.c`, `project.c` and `tests/unit/test_fmt.c`.
22. `anti build` and the resolver. Defects 1 to 5, 8, 9 and 11 of
    `anti-tool`, and defect 10 after its decision. Files: `build.c`,
    `deps.c`, `repo.c` and the runners of `anti build`.
23. Build files, the tool list and the triples. M326, M341 and M342. The
    target `llvm_tools` moves to the root file, one list names the tools,
    and one table gives the triple of a target. Files: `CMakeLists.txt`,
    `tests/CMakeLists.txt`, `tools/check-llvm.cmake`,
    `tools/pinned-tools.cmake`, `tools/host-compile.cmake` and
    `src/native/CMakeLists.txt`.
24. Release scripts. M344, M367, M368 and M371. Files:
    `tools/publish.cmake`, `tools/release.sh`, the two uninstallers,
    `CMakeLists.txt` and `tests/runners/run_linux_libc.cmake`.
25. Runners, one way to build and one way to compare. M343, M333 and M334.
    `antic_program` for every build, and one function that prints the
    first line that differs. `emit_identity` keeps the texts of the last
    manifest. Files: `tests/program_output.cmake` and the runners.
26. Runners, the LLVM program tests and the two largest. M345 and M323.
    The `llvm_*.anti` programs join the glob of every host. Files:
    `tests/CMakeLists.txt`, `tests/runners/run_llvm_program.cmake`,
    `tests/runners/run_program.cmake`, `tests/runners/run_release_dry_run.cmake` and
    `tests/runners/run_package.cmake`.
27. The option `--llvm-mc` and what the assembler left, after its
    decision and after step 23. The merged minor finding and the comments
    that name llvm-mc. The step crosses antic, `anti`, the build files and
    57 runners by its nature.
28. The document of the source tree, `docs/notes/source-tree.md`. M351.
    Its content is the table of "The sentence of each file" above, one row
    per file of `src/`, `tests/unit/` and `tools/`, with the tables of the
    runners from `build-and-tools`. The "Layout" table of `CLAUDE.md` points
    at it. A test that refuses a tracked file without a row keeps it true,
    as `header_sections` does for the shared headers.
29. The file headers and the comments of public headers. M346, M347, M348,
    M349, M350 and M365, with the minor findings on headers that say less.
    Each file without a sentence takes the one of step 28 as its header.
    Each public function the reports list gets a comment that says what it
    takes, gives and refuses. Six sessions, one per directory: the front
    end, the rest of `src/antic/`, `src/rt/`, `src/anti/`, `src/std/` and
    `tests/unit/`.
30. The notes. M353 to M360 and M362, with the minor findings on stale
    names and sentences. The script of `structure` that reads every
    "function of file" claim runs as a test. Files: `docs/notes/`.
31. The "Files of" sections and rule 10. M352 and M364. The sections give
    way to a pointer at the document of step 28, or name every file. The
    test rule 10 names is written. Files: `docs/decisions.md`,
    `docs/c-guidelines.md` and one runner of `tests/`.
32. The site page of the build tool. M363, after its decision. File:
    `docs/site/build-tool/index.md`.
33. The minor findings, one session per area. The areas are the front
    end, the middle, the back end, the library file and the runtime. They
    are also the standard library, `anti`, the build and the tools, and
    the tests. Each session takes the rows of the minor tables whose
    places lie in its area.
34. Rules 18 and 19, last and one file per session. `sema_stmt.c` loses
    the anonymous functions to a file of their own. `llvm_emit.c` splits
    along the four seams `back-end` names. Then the functions the reports
    judged clearer split.
