# Audit of the anti tool

The second audit of `src/anti/`, at `238a4f1`. All 40 files, 17 180
lines, were read in full. Structure was judged first, against the
questions of the step, and then every rule of `docs/c-guidelines.md`.
Where a finding turns on antic or the runtime, the code there was read
too: `text.c`, `arena.c`, the header reader of `antl.c`, `selfpath.c`,
the depth bound of `parser.c`, and `toml.c` and `conf.c` of `src/rt/`.
The data of the tool pass in `docs/audit/data/` served as a list of
places, and each item was checked in the code. Nothing was built and
nothing was run, so no finding was reproduced. Each status is against
`docs/audit/2026-09-23/anti-tool.md`. No finding returned.

`anti` runs antic one way only, through `driver_run` in its own process,
and reads and writes files through `files_read` and `files_write`.

## Counts

| Severity | Rule or question | Findings | New | Still open |
|---|---|---|---|---|
| Severe | 4 | 1 | 1 | 0 |
| Severe | 14 | 1 | 1 | 0 |
| Severe | 16 | 1 | 1 | 0 |
| Major | One-way dependencies | 1 | 1 | 0 |
| Major | Duplicated code that drifted | 4 | 4 | 0 |
| Major | 12 | 1 | 1 | 0 |
| Major | 14 | 2 | 2 | 0 |
| Major | 22, one platform layer | 1 | 0 | 1 |
| Minor | One purpose per file | 1 | 1 | 0 |
| Minor | 1 | 1 | 0 | 1 |
| Minor | 5 | 1 | 0 | 1 |
| Minor | 12 | 1 | 1 | 0 |
| Minor | 14 | 1 | 1 | 0 |
| Minor | 18 | 1, covering 21 functions | 0 | 1 |
| Minor | 24 | 1 | 0 | 1 |
| Minor | 25 | 1 | 0 | 1 |
| Minor | 26 | 1 | 0 | 1 |
| Minor | 27 | 1 | 1 | 0 |

Three severe, nine major and ten minor findings, 15 of them new.

Closed since the first audit: all five severe findings, the paths and
the loopback check of the repositories, the deflate bomb, the lock file
written unescaped, the file name and the markup of `anti doc`, the read
loops, the directory walk, the readers that left memory behind, the
command blocks of `main`, the signed shift and the recursion of the bind
readers. So are the thirteen readers and nine writers, the second module
reader of `test.c`, the dead stores, the rule 20 and rule 21 findings,
the prefix of the file helpers, the unchecked `snprintf` calls, the
saturated numbers, the narrowing of the zip writer and the misplaced
comment of `test.c`. Malformed-input tests now exist for the zip reader,
the index and the lock file, the symbols readers and both bind readers.

## Severe

### The allocation helpers never check the result of calloc and realloc

`src/anti/files.c:38`, rule 4, severe, new.

`files.h:13` promises that a failed `calloc` exits through
`files_out_of_memory`. `files_array` returns the result of `calloc` as it
stands, and `files_resize` returns the result of `realloc` the same way.
`files_grow` then runs `memset(bytes + *room * size, ...)` at line 65,
a write through NULL after a failure, and the old block leaks. The 63
calls in 12 files check nothing, since the header says they need not.
`d20c08a`, the fix of the first audit's rule 5 finding, removed the
check of every caller when it moved the allocations into these helpers,
and the helpers never held one. The fix is the NULL test and the exit
the header describes.

### A line counter of long overflows on Windows

`src/anti/bindclang.c:1165`, rule 16, severe, new.

`read_preprocessed` counts the lines of `clang -E` in `long line`, and
`struct pack` and `struct reader` keep lines in `long` as well. A `long`
holds 32 bits on Windows. `#line 2147483647` is valid C, and the marker
clang writes for it sets `line` to `LONG_MAX` at line 1088. `line++` at
line 1165 then overflows on the next line of the header, which is
undefined. `strtol` at line 1088 saturates without a check, and `track`
narrows the `int64_t` of the AST into `long` at line 325. The fix is
`int64_t` for every line and a refusal of a marker past its range.

### An enumerator after INT64_MAX overflows a signed sum

`src/anti/bindclang.c:794`, rule 14, severe, new.

An enumerator without a value takes the one before it plus one, as
`e->values[e->value_count - 1].value + 1` in `int64_t`. The header
`enum E : unsigned long long { A = 0x7fffffffffffffff, B };` is valid C23,
and the pinned clang takes it. Its AST gives `B` no `ConstantExpr`, so
the sum is `INT64_MAX + 1`, which is undefined. The fix is the sum in
`uint64_t` through `bind_signed`, as `find_constant` at line 538 already
does for a value above `INT64_MAX`.

## Major

### The anti tool uses the internals of antic, and no header is its interface

`src/anti/doc.c:1440`, one-way dependencies, major, new.

The tool pass reported the class at `units.c:91`, and this is the whole
list for `src/anti/`. `driver.h` is the header meant for a caller, and it
includes `linker.h` of the back end itself. Past it, 11 files include
headers of the front end, the IR, the library reader or the back end:

- `lexer.h` in `bindtype.c:13`, `check.c:25`, `fmt.c:26` and
  `units.c:16`, with `diagnostic.h` in the same four files.
- `parser.h` and `ast.h` in `check.c` and `units.c`, and `ast.h` in
  `doc.c:22`.
- `sema.h` in `deps.c:23` and `doc.c:29`, and `types.h` in `doc.c:32`.
- `header.h` in `bind.c:12` and `build.c:22`, for `HEADER_SUFFIX`.
- `ir.h` in `doc.c:26`.
- `antl.h` in `build.c`, `check.c`, `deps.c`, `doc.c`, `repo.c` and
  `units.c`.
- `linker.h` in `build.c:23`, `main.c:12` and `sdk.c:11`.

The calls go as deep. `doc_run` calls `ir_module_init` and
`ir_module_free` at lines 1440 and 1501, and owns the arenas, type tables
and IR modules that `driver_interface` fills. `doc.c` then reads
`struct type`, `struct item` and `struct symbol` field by field.
`check.c:348` and `units.c:91` lex, parse and walk `struct module`.
`build.c:343` and `deps.c:318` call `antl_header`. `fmt.c:447` tests
`k >= TOKEN_BOOL_TYPE && k <= TOKEN_C_WCHAR`, an order of `lexer.h`
that no comment there promises. A change to any of these compiles in
`anti` and changes what it does. The fix is a header beside `driver.h`
that states what `anti` may call, with the walks behind it.

### The package name reaches one compile of anti build and none of anti test or anti doc

`src/anti/build.c:177`, duplicated code that drifted, major, new.

Each command fills `struct options` in code of its own: `base_options`
at `build.c:177`, `check.c:54` and `test.c:42`, and inline at
`doc.c:1359` and `1422`. `manifest.h:34` says every call of a check
carries the package name, since it decides which modules share an
`internal` item. `check.c:520` sets it on every call.

`build.c` sets it in `header_options` at line 210, which
`module_library` alone calls. The source compile of `build_dev` at line
462, `build_release`, `build_symbols` and `build_c_library` run
without it. So does every compile of `test.c` and `doc.c`, and
`main.c:388` reads the name for `anti doc` and drops it.
`sema_library_item` at `sema.c:256` hides every `internal` item from a
compile that names no package. By this reading, a module that uses an
`internal` item of another module of its package passes `anti check`
and fails `anti build`, `anti test` and `anti doc`. No test drives
`anti` over such a pair, and the failure was not reproduced. One
function that fills the options of a project compile would hold the
rule once.

### Two readers of the build id of a binary, and five spellings of its marker

`src/anti/symmap.c:146`, duplicated code that drifted, major, new.

`notice_of` at `syms.c:136` finds the id after
`ANTI_LICENSES_BEGIN\nbuild `, the head of the notice. `symmap_build_id`
searches for `build ` and 64 hex digits anywhere in the program and
takes the first match. A program whose data holds such a line before its
notice gets a wrong `# build` line in its map. `map_id` at `syms.c:535`
reads that line when an archive holds no debug twin. The marker is
`NOTICE_BEGIN` of `src/antic/notice.h:11`, and it is spelled again at
`src/rt/license.c:7` and `9`, `src/rt/trace.c:149` and `syms.c:45`.
Neither file of `anti` uses the definition. The fix is one reader in
`anti` over `NOTICE_BEGIN`.

### A second reader of the runtime configuration, one include short of the runtime's bound

`src/anti/syms.c:312`, duplicated code that drifted, major, new.

`read_configuration` repeats the file layer of `src/rt/conf.c`: the
includes first and relative to their file, the cycle check,
`runtime.plugins` as a text or an array, and `[injections]`. The
runtime refuses a file with more than 32 files above it, as
`depth > 32` at `conf.c:707`. `anti` refuses at 32, as
`depth >= INCLUDE_DEPTH`. A chain of 33 files starts its program, and
`anti symbols inventory` and `check` refuse it with the message that
the file includes itself. One reader of the file layer in `src/rt/`,
which both call, would keep the rules in one place.

### anti bind --header splits a path at the slash alone

`src/anti/bind.c:44`, duplicated code that drifted, major, new.

`base = strrchr(library, '/')` takes the file name by hand.
`default_module`, twenty lines below, calls `files_base_name`, which
takes both separators. On Windows `anti bind --header C:\app\geo.antl`
keeps the whole path as the name and writes `./C:\app\geo.h`, which
`fopen` refuses. The first audit's finding on small helpers written
twice moved `base_name` into `files.c`, and this copy stayed behind.
The result differs between targets.

### The angle lists of anti fmt recurse without a bound

`src/anti/fmt.c:457`, rule 14, major, new.

`angle_type` calls itself once per `?`, `*`, `?*`, `chan` and `[]`
before a type. `angle_list` and `angle_type` call each other once per
nested `<`. `fmt_source` runs the lexer alone, so the bound
`PARSE_DEPTH_MAX` of `parser.c:144` never sees the file. A line
`f<chan chan chan int>(x)` with a few hundred thousand `chan` exhausts
the stack of `anti fmt`. The bind readers gained bounds for this reason
after the first audit, and no test feeds the formatter a deep list.

### anti doc writes the URL of a link as the doc text gives it

`src/anti/doc.c:917`, rule 14, major, new.

`inline_html` writes `[text](url)` as an `<a href>` with the URL passed
through `escape_html` alone. A `///` comment of a library file from a
repository can write a `javascript:` URL, and the page runs it when a
reader follows the link. The fix of the first audit's finding on
`anti doc` escaped every name and checked the module path. The scheme
of a link stayed open.
The fix is to take `https:`, `http:`, a relative path and a `#` anchor,
and to write any other URL as text.

### anti fmt leaks the brackets of an anonymous function left open

`src/anti/fmt.c:1491`, rule 12, major, new.

`emit_anonymous_open` copies the open brackets into `f.bracket_type` and
pushes the frame. Only `emit_anonymous_close` frees the copy, when a `}`
pops that frame. A source that ends inside such a body, `f(fn() {` with
no `}`, still lexes. `fmt_source` then frees `e.frames` at line 1857
and not the copies the frames hold.

### Host conditionals stand in three files of anti, and none is the platform layer

`src/anti/files.c:15`, rule 22, major, still open.

Rule 22 names `src/anti/platform.c`, which does not exist.
`data/platform-conditionals.txt` lists 6 `#if` lines in `files.c`, 3 in
`sdk.c` and 2 in `syms.c`. `syms.c:1442` calls `getenv`, where
`platform_getenv` of `src/antic/platform.h` stands for it. `files.c:12`
includes that header as `../antic/platform.h`, while every other include
names the file alone. This is the part of `anti` in M29 of the first
audit, which the tool pass reports as still open.

## Minor

### Four files hold a second purpose, and two modules depend on each other

`src/anti/syms.c:1251`, one purpose per file, minor, new.

- `syms.c` holds the three symbols commands and, from line 1251, the
  run of a `--memory-checks` program that rewrites its report, which
  `build.c:1068` and `test.c:311` call.
- `bindtype.c` holds the parser of a C type. Beside it stand the memory
  of the binding model, `bind_warn`, the keyword test and the tables of
  frameworks and Linux libraries.
- `files.c` holds the out-of-memory exit and three allocation helpers,
  which every file calls, among the file and path helpers.
- `build.c` holds `anti new` beside `anti build` and `anti run`.
- `repo.c:19` includes `deps.h` for `deps_version_valid`, and
  `deps.c:22` includes `repo.h`. The grammar checks of versions, names
  and digests would stand in one of the two.
- `anti.toml` has three readers, `manifest_read`,
  `manifest_layout_read` and `manifest_inject_read`. `manifest_read`
  reads the file twice through the third, at `manifest.c:372`, and the
  defaults `src` and `test` stand at lines 126 and 301.

### Two raw attributes and a printf-like function without one

`src/anti/bindmodel.h:143`, rule 1, minor, still open.

`bind_warn` carries `__attribute__((format(printf, 2, 3)))` under
`#if defined(__GNUC__) || defined(__clang__)`, where
`src/antic/attributes.h` holds `ATTRIBUTE_PRINTF`. `refuse` at
`bindclang.c:287` passes its `format` parameter to `fprintf` and
carries no attribute, which `-Wmissing-format-attribute` names. The
tool pass reports both.

### Three growable lists double without a check of the product

`src/anti/jsontree.c:82`, rule 5, minor, still open.

`list_add` computes `room * sizeof *items` for `realloc`, and so do
`bind_list_add` at `bindtype.c:18` and `arg_add` at `bindclang.c:140`.
The last two also print their own out-of-memory message and exit, a
third and a fourth copy beside `files_out_of_memory`. The message of
`d20c08a` says the JSON reader checks the product, and its diff adds no
check. `bindtype.c:12` includes `files.h` and calls nothing of it.

### Functions release by hand before each return

`src/anti/check.c:474`, rule 12, minor, new.

`check_run` frees up to four resources by hand before each of three
early returns, at lines 474, 481 and 489, and again at `done`. The
same pattern stands at `syms.c:852` and `1187`, `test.c:148` and
`153`, and `fmt.c:1816`. No path leaks today. One cleanup block at the
end, reached by `goto`, is the form the rule asks for.

### The names of an API description reach the binding unchecked

`src/anti/bindwrite.c:263`, rule 14, minor, new.

`bindapi.c` takes the names of structs, fields, enums, values, defines
and parameters from `raylib_api.json` as they stand. `write_record`,
`write_enum`, `bind_write_module` at line 374 and the C probe write them
into Anti and C source. `name_of` at line 228 appends `_` to a name that
is not one identifier and writes it anyway. A struct named
`A { x: int } fn f()` writes items of its own into the module, and
`fmt_source` lexes the result and nothing more. A function survives
only because `bind_is_keyword` answers true for every string that is
not one identifier, and the warning then calls its name a word of Anti.
The fix is to refuse a name that is no C identifier where `bindapi.c`
reads it.

### Functions past a threshold

`src/anti/check.c:441`, rule 18, minor, still open.

`data/thresholds-compared.txt` lists 21 functions of `anti`, 4 new and
17 kept. `main` and `resolve_frame` left the list. The first audit's
judgement holds for 15 of the 17 kept. The four new ones and two that
grew:

- `emit_token` (`fmt.c:1599`, 151 lines, up from 83) is a dispatch over
  token kinds and follows the problem. The import braces and the angle
  brackets at its head would read as helpers.
- `resolve_from_index` (`deps.c:486`, 133 lines) runs three loops over
  the versions, the modules and the dependencies of one index, and
  splits along them.
- `angle_list` (`fmt.c:379`, nest 5) follows the grammar of a list.
- `cache_key` (`build.c:111`, 7 parameters) grew by two flags. A struct
  of the parts of a key would be clearer.
- `test_run` (`test.c:324`) now takes 11 parameters and `fn_signature`
  (`doc.c:252`) 10. A request struct, as `build_run` takes, would be
  clearer.

### Pointers to arrays the callee does not change are not const

`src/anti/test.h:21`, rule 24, minor, still open.

`struct options` at `src/antic/driver.h:29` takes `const char **` for
`roots`, `libraries`, `objects` and `inject`, which antic never changes.
`anti` follows it: `bind_header` at `bind.h:13`, the `inject` parameter
of `test_run`, `includes` of `struct bind_request` at `bind.h:23` and
`write_interfaces` at `doc.c:1330`. `check_run` and `doc_run` take
`const char *const *` and copy into a new array to meet it.

### The JSON tree exports json names

`src/anti/jsontree.h:48`, rule 25, minor, still open.

The module is `jsontree`, and it exports `json_read`, `json_free`,
`json_get`, `json_member_string`, `json_integer` and
`json_member_true`. The scanner below it exports `anti_rt_json_*`.

### Small helpers written twice

`src/anti/deps.c:457`, rule 26, minor, still open.

- `value_of` stands at `deps.c:457` and `manifest.c:106`. The copy in
  `manifest.c` gives NULL for an empty value, and the one in `deps.c`
  never does, since its NULL test at line 467 cannot hold.
- `library_module` at `build.c:330` and `library_header` at `deps.c:303`
  both read a library file and call `antl_header`.
- `link_frameworks` at `build.c:657` and `runner_frameworks` at
  `test.c:231` chain the same three driver calls.
- A suffix test stands at `files.c:278`, `syms.c:48`, `doc.c:1315`,
  `sdk.c:87`, `build.c:505` and `deps.c:355`.
- A name against a C string is compared at `units.c:23`, `doc.c:132` and
  `doc.c:775`.
- `check.c:54` and `test.c:42` exit with 2 on an unknown host, while
  `doc.c:1368`, `1429` and `bind.c:37` print the same message and
  return.
- The type words stand as a range at `fmt.c:447` and as a list at
  `fmt.c:739`.

### Comments that describe another function or another list

`src/anti/doc.c:780`, rule 27, minor, new.

- `doc.c:780` holds `The last segment of a module path` above the
  comment of `last_dot`, a comment left from a function that is gone.
- The DESIGN comment of the signature at `doc.c:175` stands above the
  comment of `c_function`, repeats `the name, the name` and breaks its
  line after `It`.
- `units.c:1` names `anti check` and `anti doc` as the users of the
  list, and `units.h:9` names those two with `anti build` and
  `anti test`.

## Noted outside the rules

- `manifest_inject_read` applies `[inject]` in its first loop and every
  key again in its second, at `manifest.c:92`. A document that writes
  `[inject]` after `[inject.test]` then gives `anti test` the provider
  of `[inject]`, where the comment at line 81 promises the test one.
- The resolver keeps every requirement it has read, which the first
  audit recorded for "Resolution" in `docs/tooling.md`. It stays open.
- `resolve_from_repo` at `deps.c:649` takes a repository whose index
  cannot be read or refused as one without the package. Two
  repositories that hold a package are an error only while both answer.
- `fmt_run` writes a source over itself through `files_write` at
  `fmt.c:1896`, which empties the file before it writes. A failed write
  leaves the source cut short.
