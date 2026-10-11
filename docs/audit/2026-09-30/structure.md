# Structure audit

The architecture of the whole repository at `238a4f1`: the parts of
`antic`, `anti` and the runtime, their boundaries, the direction of every
dependency, and the one place of each concern. The evidence is the include
graph and the call graph of `docs/audit/data/`, and the code read at every
place named below. Nothing was built or run. The standard is the set of
structure questions of the audit, and `docs/c-guidelines.md` for the rules
it names. The findings of `docs/audit/tool-pass.md` belong to the same
audit and are not repeated here. The answers at the end cite them by title,
and a finding here that extends one says so. Each finding was compared with
the reports of the first audit in `docs/audit/2026-09-23/`.

## Counts

| Severity | Findings | New | Still open |
|---|---|---|---|
| Severe | 0 | 0 | 0 |
| Major | 3 | 3 | 0 |
| Minor | 5 | 4 | 1 |

| Severity | Question or rule | Findings |
|---|---|---|
| Major | One teardown rule | 1 |
| Major | One-way dependencies, the boundary with the runtime | 1 |
| Major | One platform layer, rule 22 | 1 |
| Minor | One rule for generic copies | 1 |
| Minor | One way of reading a file | 1 |
| Minor | Duplicated code, rule 26 | 1 |
| Minor | Names defined once | 1 |
| Minor | One platform layer, path rules | 1 |

No finding returned. No severe finding stands beyond the three of the tool
pass.

## Major

### Lowering tears a local down by a second rule that has drifted from the first

`src/antic/lower_stmt.c:1110`, one teardown rule, major, new. This extends
the tool pass findings on the splits and on the loops over array elements.

The checker holds the one rule of what owns something,
`sema_needs_teardown` at `src/antic/sema_stmt.c:556`. It is true for a
`?T` or an array of an owning type at any depth. Lowering emits a teardown
in two ways:

- `lower_destroy_owned` at `src/antic/lower.c:2477` handles every kind,
  and gates each part on `sema_needs_teardown`. Fields, `drop_result` and
  struct parts go through it.
- A local, an `own` parameter and a caught value go through
  `destroy_local` at `lower_stmt.c:1280`, with `destroy_value`,
  `destroy_array` and `destroy_optional` at `882` to `1167`. They are
  gated on `lower_needs_teardown`, which is
  `lower_type_needs_destruct(innermost(t)) || lower_optional_needs_destruct(t)`.

The second gate strips arrays first and the `?` of the top level only. So
it is false for `[2]?Counted` and for `?[2]Counted`, where `Counted` has
a `destruct`. The checker counts both as owning and refuses a copy of
them. The `let` at `lower_stmt.c:1721` and `1794` and the loop variable
at `631` then push no exit action, and the objects the local holds are
never torn down. `destroy_array` could not tear such a local down anyway:
it hands each `?Counted` element to `destroy_value`, which calls
`anti_rt_destroy` on it as a class value. No test declares such a local,
and this audit ran nothing, so the leak is read from the code alone.

`teardown_field` at `lower.c:2739` is a third copy of the `?T` case, and
`lower_copies_parts` at `lower.c:2653` is a separate rule for copies with
no comment on why it differs from `sema_type_owns`. The fix is one path:
every teardown goes through `lower_destroy_owned`, gated on
`sema_needs_teardown`, in one file of lowering.

### The ABI between generated code and the runtime has two definitions

`src/antic/lower_desc.c:205`, one-way dependencies, major, new.

The runtime includes nothing of `antic`, and that holds. The reverse
contract, what lowering emits and the runtime defines, is written twice
with nothing pinning the two together:

- Calls. Lowering declares each runtime function at its call, as an array
  of IR types, through `lower_rt_call` at `src/antic/lower.c:2347` and
  `lower_sync_call` at `src/antic/lower_expr.c:2772`. The C definitions
  in `src/rt/` are the second copy. Lowering spells 64 names of the
  runtime, and 25 of them have no prototype in any header of `src/rt/`.
  Two are `anti_rt_mutex_lock_at` at `src/rt/lock.c:288` and
  `anti_rt_chan_new` at `src/rt/sync.c:133`. No compiler then checks a
  call against a definition. The 25 are among the 58 functions the tool
  pass counted as false positives of `-Wmissing-prototypes`.
- Records. `lower_descriptor_agg` at `lower_desc.c:205` lists the 17
  fields of a class descriptor one by one, and `field_agg` at
  `lower_desc.c:171` the 6 of a field record. `struct anti_descriptor` at
  `src/rt/object.h:174` and `struct anti_field` at `object.h:134` declare
  them again. They agree today.

A parameter added on one side compiles and links. The suite finds it only
where a test passes values that expose the mismatch on the host it runs
on. The unit tests `records_hook_entries` and `records_type_ids` at
`tests/unit/test_lower.c:365` and `414` already pin two enums of the same
boundary. The fix is the same for calls and records: one table of each
runtime function and record, which a unit test compares with the
declarations of `src/rt/`.

### The platform layer of the tools treats a path as UTF-8 and as ANSI

`src/antic/selfpath.c:100`, one platform layer, rule 22, major, new.

`src/antic/process.c:54` states the rule, "antic holds its strings as
UTF-8", and `selfpath.c` and `process.c` follow it. `self_path` at
`selfpath.c:28` and `absolute_path` at `128` convert with `CP_UTF8`, and
`widen` at `process.c:55` converts back for `CreateProcessW`. The rest of
the layer passes the same bytes to the ANSI entry points of Windows, which
read them in the code page of the machine:

- `platform_open` at `src/antic/platform.c:32` calls `_fsopen`.
- `directory_exists` at `selfpath.c:100` calls `GetFileAttributesA`.
- `src/anti/files.c` calls `_mkdir` at `73`, `GetFileAttributesA`,
  `DeleteFileA` and `RemoveDirectoryA` at `104` to `137`,
  `FindFirstFileA` at `314` and `GetFileAttributesA` at `455`.
- `argv` of both `main` functions and `platform_getenv` arrive in that
  code page.

`runtime_archive` at `src/antic/userdirs.c:175` hands the UTF-8 path of
`self_directory` to `directory_exists`. An install under a profile whose
name holds a letter outside ASCII then finds no runtime archive on Windows
alone. The other direction fails as well: a path of `argv` outside ASCII
reaches `CreateProcessW` decoded as UTF-8. The first audit reported the
runtime half of this under "Windows takes UTF-8 text through the ANSI
entry points" in `rt.md`. The runtime now converts in `fs.c` and
`platform_windows.c`, and the tools do not. The fix is one encoding, UTF-8
inside and UTF-16 at every call of Windows, held in `platform.c` alone.

## Minor

### A copy of a generic is known by a `<` in its name, in three parts

`src/antic/whole.c:281`, one rule for generic copies, minor, new.

`sema_copies.c` makes the copies and names them `List<int>.push`. The IR
function carries no mark of a copy, so three later parts each test the
name with `strchr(name, '<') != NULL`. They are `link_once` at
`src/antic/emit.c:28`, `is_copy` at `whole.c:281` and `copy_name` at
`src/antic/antl.c:3197`, and they add conditions of their own. A change to
how a copy is named breaks the dev link loudly, while the release merge of
identical copies stops without a sign. A flag on `struct ir_function`, set
by lowering from the checked item, would carry the rule.

### antic refuses a source file that anti formats and checks

`src/antic/driver.c:114`, one way of reading a file, minor, new.

`read_source` of the driver refuses a file with a NUL byte, and one past
`LEX_SOURCE_MAX`. The lexer checks the size itself at
`src/antic/lexer.c:1483`, and it accepts a NUL inside a bytes literal at
`lexer.c:1112`. `anti` reads sources with `files_read` at
`src/anti/files.c:200` and lexes them itself, in `units.c:91`,
`check.c:348` and `fmt.c:1815`. So `anti fmt` formats a file with such a
NUL, while `antic` refuses the whole file. The rule on the bytes of a
source belongs in the lexer, where every reader reaches it. The two
readers, `read_bytes` and `read_source` of `driver.c:89` and `114` and
`files_read`, also stand in one link of `antic_core`.

### Allocations outside the one allocator, and one exit that drifted

`src/antic/lower.c:1542`, rule 26, minor, new. This extends the tool pass
finding on the checked allocator of antic.

Of the 46 places in `antic` that print its out-of-memory message, 43 end
with `exit(70)` and two return a failure. The growth of the pattern
literals at `lower.c:1537` to `1542` ends with `exit(1)`. Nothing reads
the status today. In `anti`, the
provisional entry at `docs/decisions.md:841` says every allocation goes
through `files.c`, with the JSON reader of `anti bind` as the one
exception. `bind_list_add` at `src/anti/bindtype.c:19` and `arg_add` at
`src/anti/bindclang.c:141` grow with their own `realloc` and exit.
`add_function` at `src/anti/symmap.c:49` and `append_name` at
`src/anti/syms.c:992` each call `malloc` for the same unescape of a
symbol name, written twice.

### Names that two parts spell for themselves

`src/anti/syms.c:32`, names defined once, minor, new.

- `PLUGIN_INDEX` is defined at `src/antic/target.h:43` and again at
  `src/anti/syms.c:32`, and `src/rt/plugin.c:763` spells
  `"anti-plugins.toml"` a third time. The runtime cannot include the
  header of `antic`, so its copy is a seam. `tests/run_plugin.cmake`
  covers that seam, and nothing covers the copy in `syms.c`.
- `-symbols.zip` is written by `src/anti/build.c:549` and read through
  `ARCHIVE_SUFFIX` at `syms.c:30`, two spellings in one program.
- `src/anti/bindclang.c:163` writes `"%s/sysroot/%s"` where
  `RUNTIME_SYSROOT_DIR` of `src/antic/linker.h:21` exists.

### The path rules of the tools and the runtime still differ

`src/antic/selfpath.c:119`, one platform layer, minor, still open.

The first audit reported this as "Path helpers written per file, with
different rules" in `cross-cutting.md`. `syms.c` and `bind.c` now call the
shared helpers, and two differences remain. On Windows `path_is_absolute`
of `selfpath.c:119` needs `X:/` or `X:\`, while
`anti_rt_path_is_absolute` at `src/rt/platform_windows.c:115` takes a bare
`X:`. `files_base_name` at `src/anti/files.c:441` and `parent_of` at
`src/antic/userdirs.c:144` take `\` as a separator on every host. The
runtime's `anti_rt_path_last_separator` follows the host.

## The structure questions

### One purpose per module and file

Most files have one. Four do not, and the tool pass names each. `driver.c`
compiles, plans and runs the link, finds the Apple SDK, joins COFF objects,
bundles libraries and writes the plugin index. Its finding under rule 19
names the planning of the link as the first part to move beside
`linker.c`. `sema_stmt.c` and `lower.c` hold the value rules and the
teardown beside their stated purpose. `antl.c` writes, reads and merges
the copies into the program. The splits of `sema.c` and `lower.c` are
answered below.

### One-way dependencies

The runtime includes no header of `antic` or `anti`. Its crossings the
other way are the shared headers `f16.h`, `hash.h`, `regex.h`, `utf.h`,
`digest.h` and `cpu_level.h`, and the six runtime sources the tools
compile, each settled by a `DESIGN` comment. The back end includes nothing
of the front end. The front end includes the IR at one place,
`header.c:8`, for the allocator. That is "The checked allocator of antic
lives in the IR". Lowering includes `sema.h` and calls four rules of the
checker, `sema_needs_teardown` among them, which is the right direction. It writes one field of
the checker's symbol, `ir`, which `lower.h` documents. `anti` bypasses
`driver.h` in 11 files, and `driver.h` itself includes `linker.h`. That is
"The anti tool uses the internals of antic". The contract with the
runtime is the second major finding above. `notice.h` and `userdirs.c`
are "Two small crossings of shared helpers".

### Each concern in one place

| Concern | The owner | Other places |
|---|---|---|
| Symbol table | `sema_declare` and `sema_lookup`, `sema.c:288` and `178`, over `struct scope` of `sema_checker.h:39` | `ir` of `struct symbol`, written by lowering. `resolve_name` of `src/anti/doc.c:813` resolves doc links over the tables of the interface, a separate use |
| Type rules | `types.c` for types, `sema_require` at `sema_expr.c:818` for conversions | `holds_union` and `lower_unreadable`, tool pass |
| Layout | `src/antic/layout.c` | `fixed_layout` at `sema_expr.c:2258`, tool pass, severe. The 8 bytes of a Regex at `lower.c:1533`, with its comment |
| Teardown and copy | The rule in `sema_needs_teardown`. The code in `lower_destroy_owned` and `lower_copy_owned` of `lower.c` | `destroy_local` and `lower_needs_teardown` of `lower_stmt.c`, first finding. The runtime calls the compiled teardown through the table, `object.c:984` |
| Generic copies | `sema_copies.c` makes the items, `sema_generic.c` the types of the copies | `antl_tree.c` stores the trees. `antl.c`, `emit.c` and `whole.c` each find a copy by name |
| File reading | `read_bytes` and `read_source` of `driver.c` in antic, `files_read` of `files.c` in anti, `anti_rt_fs_read` of `fs.c` in the runtime | `sha256_file` of `sha256.c` hashes a file in one call |
| Platform layer of antic | `src/antic/platform.c`, with 2 functions | 18 host branches in 7 files, tool pass. Two encodings, third finding |
| Platform layer of anti | None, although rule 22 names `src/anti/platform.c` | `files.c` holds 6 branches, `sdk.c` and `syms.c` 5 more |
| Platform layer of the runtime | `platform.h`, `platform_posix.c` and `platform_windows.c` | 81 host branches in 13 files, tool pass |

The allocator of `antic` has no owner of its own. `ir_alloc` stands in
the IR, and 46 places elsewhere print the message of a failed allocation.
That is "The checked allocator of antic lives in the IR".

### The splits of `sema.c` and `lower.c`

The split files each have a stated purpose in `sema_checker.h:4` and
`lower_lowerer.h:4`, and `sema_copies.c`, `sema_generic.c`,
`sema_pattern.c`, `sema_safety.c`, `lower_desc.c`, `lower_eq.c`,
`lower_hash.c` and `lower_simd.c` keep theirs. The files of expressions,
calls and statements still call into each other both ways, and the value
rules and the teardown are spread over three files each. The tool pass
reports this as major, with the minor findings on the header sections and
the wrappers of `antl.c`. The teardown finding above shows that the spread
has already produced two rules.

### Copies that drifted

Four copies have drifted. The first is the teardown gate of lowering, and
the second the exit status of one allocation. The third is the path rules
of the tools and the runtime, and the fourth the acceptance of a NUL byte
in a source. The tool pass found a fifth, the layout of the unit break
`_` in the checker.
