# Audit of the anti tool

The audit of `src/anti/` against `docs/c-guidelines.md`, at `fddd434`. All
40 files, 15 565 lines, were read in full: the manifest and lock readers,
repository fetching and the resolver, the zip reader and writer, and the
build, run, new, test, check, fmt, doc, bind, sdk and symbols commands.
`src/anti/` follows every rule, and the thresholds of rule 18 are reported,
not enforced. The machine evidence of `docs/audit/data/` served as a list of
places to look, and each item was checked against the code. Where a finding
turns on a helper of `src/antic/` or `src/rt/`, such as `text.c`, `arena.c`,
the TOML reader or the library file reader, that helper was read too. The
pinned docs-style checker ran over every file for rule 27 and found nothing.
Nothing was built, and no finding was reproduced by running code.

## Counts

| Severity | Findings |
|---|---|
| Severe | 5 |
| Major | 11 |
| Minor | 17 |

| Severity | Rule | Findings |
|---|---|---|
| Severe | 6 | 1 |
| Severe | 7 | 1 |
| Severe | 14 | 3 |
| Major | 3 | 1 |
| Major | 12 | 2 |
| Major | 14 | 8 |
| Minor | 5 | 1 |
| Minor | 9 | 1 |
| Minor | 11 | 1 |
| Minor | 14 | 1 |
| Minor | 15 | 1 |
| Minor | 16 | 1 |
| Minor | 18 | 1, covering 19 functions |
| Minor | 20 | 1 |
| Minor | 21 | 1 |
| Minor | 22 | 1 |
| Minor | 24 | 1 |
| Minor | 25 | 1 |
| Minor | 26 | 4 |
| Minor | 27 | 1 |

## Severe

### Two structs of one name in an API description write past a field array

`src/anti/bindapi.c:227`, rule 6, severe.

The first loop of `read_structs` adds one record per entry of `structs`. The
second loop finds each entry's record by name with `record_of`, which returns
the first match. For a second entry of the same name it gets the first
record back, whose `field_count` still holds the fields of the first entry.
Line 222 then gives it a new array of `fields->count + 1` elements, and
`&r->fields[r->field_count]` writes from the old count upwards. With two or
more fields in the first entry, the writes run past the new block in the
memory pool. A `raylib_api.json` with a repeated struct name is enough. The
fix is to refuse a repeated name in the first loop, or to reset
`field_count` where the array is replaced.

### A version with many digits overflows a signed accumulator

`src/anti/deps.c:60`, rule 14, severe.

`version_part` computes `value = value * 10 + (**p - '0')` in a `long` with
no bound. It reads the versions of a downloaded `index.toml` through
`satisfies_all` and `deps_version_compare`, the versions of `anti.lock`, and
the `antic` key of the manifest. A version of twenty digits overflows, which
is undefined. `next_major` at line 90 adds 1 to the same value. The fix is
to refuse a part above a bound before multiplying.

### Numbers read with `sscanf` and `atoi` have no defined result out of range

`src/anti/syms.c:961`, rule 14, severe.

`sscanf(text, "%llx-%llx %511s %1023s", ...)` reads a line of the map of a
symbols archive, which comes from another machine. C11 7.21.6.2 leaves the
result undefined when a converted number does not fit its object, so a
range of seventeen hex digits is undefined behaviour. The same stands at
`syms.c:1148` and `1152`, which read the lines of a trace a user pastes with
`%lld` and `%llx`. `atoi(digit)` at `bindclang.c:1067` reads the value of a
`#pragma pack` in a third-party header, and `atoi` at `bindclang.c:70` reads
the version line of clang. The tool pass left these to this step. The fix is
`strtoull` and `strtol` with a check of `errno` and of the end pointer.

### A null pointer reaches a library function with a count of zero

`src/anti/syms.c:643`, rule 14, severe.

`memcmp(u->id.data, name, u->id.length)` runs when the length of the id
equals the length before the first `/` of an entry name. A unit of a
deployment index that names no `id` has `id.data == NULL`. An archive entry
whose name starts with `/` then calls `memcmp(NULL, name, 0)`, which C11
7.24.1 leaves undefined. The same holds for `qsort(m.items, m.count, ...)` at
`symmap.c:219` and `225` when the program yields no function and `m.items`
is still NULL. Both are cheap to guard with a check of the count.

### An array of `void *` is read as an array of `const char *`

`src/anti/bindclang.c:1252`, rule 7, severe.

`process_capture((const char *const *)args->items, out)` hands the
`void **items` of a `bind_list` to a function that reads each element as a
`const char *`. The objects were stored as `void *`, and the two types are
not compatible, so each read is undefined under C11 6.5p7. The rule names
this cast. `void *` and `char *` share their representation, and clang's
pointer aliasing treats `void *` as compatible with every pointer, so the
pinned build is not known to miscompile it. The fix is a `const char **`
list for the argument vectors.

## Major

### Strings of a downloaded index become paths in the cache

`src/anti/repo.c:300`, rule 14, major.

`repo_module` builds `<cache>/pkg/<name>/<version>/<module>.antl` from the
version and module path that `resolve_from_index` took from a downloaded
`index.toml`, or that `lock_read` took from `anti.lock`. Nothing refuses a
`/` or a `..` in them. `fetch` at line 315 writes the file before line 321
checks its digest, and a file that fails the check is not removed. An index
that names the version `../../../..` writes a file of its own choosing
outside the cache. The name of a transitive dependency comes from the index
too, at `deps.c:554`, and `index_dir` at line 246 turns it into the
directory the next index is written to. The severity table has no class for
a write outside the cache, and this is the most urgent finding of the list.
The fix is to refuse every name, version and module path that is not the
grammar of its kind before it reaches a path.

### A plain `http://` URL passes the loopback check with a user part

`src/anti/repo.c:86`, rule 14, major.

`is_loopback` accepts a host that starts with `localhost` or `127.0.0.1` and
is followed by `:`. `http://localhost:@repo.example.com/` passes, and curl
reads `localhost:` as the user part and fetches from `repo.example.com` over
plain HTTP. The URL may come from the `repo` field of a dependency in a
downloaded index, at `deps.c:549`, so a repository can move a build onto an
unencrypted host. The fix is to refuse an `@` in the authority, or to parse
the host as curl does.

### The deflate reader grows its output without a bound

`src/anti/zip.c:398`, rule 14, major.

`codes` appends every literal and every copy to the output and never
compares it with `item->size`. `zip_unpack` checks the length only after
`inflate` returns. A copy of 258 bytes costs a few bits, so an entry of a
few megabytes of crafted deflate data asks for gigabytes, and the process
exits through the allocator's failure path. The archives come from other
machines. The fix is to pass `item->size` to `inflate` and stop when the
output would pass it.

### The lock file writes strings of the index into TOML unescaped

`src/anti/deps.c:762`, rule 14, major.

`lock_write` writes the name, version, repository, path and each module path
and digest between double quotes with `%s`. Every one of them may come from
a downloaded index. A version holding `"` and a line end writes a key of its
own choosing into `anti.lock`, which `lock_read` reads back on the next build
and which is committed with an application. The fix is to refuse those
strings on read, or to write them as literal strings, as `toml_literal` of
`syms.c` does.

### `anti doc` takes the module path of a library file as a file name

`src/anti/doc.c:1270`, rule 14, major.

A page is written to `<out>/<module>.html`, where `module` is
`iface->module`, the string the header of the library file names. For a
`.antl` input nothing checks that string, in `antl_header` or in
`driver_interface`, so a library file downloaded from a repository writes
its page anywhere the user can write. The same string and every item name
go into the page unescaped at lines 971 to 973, 1009 and 1011, so a library
file puts markup of its own into the page. The fix is to check the module
path against the grammar of a module path before it becomes a path, and to
escape every name in HTML.

### Read loops take a read error for the end of the file

`src/anti/build.c:51`, rule 14, major. Confirmed from the tool pass.

Each `read_file` loops `while ((n = fread(...)) > 0)` and returns true
without `ferror`. The same loop stands in `bind.c:87`, `check.c:69`,
`deps.c:47`, `fmt.c:1265`, `manifest.c:32`, `repo.c:42`, `sdk.c:34`,
`symmap.c:49`, `syms.c:56`, `test.c:66`, `units.c:37` and `zip.c:79`. The
worst consequence is in `fmt_run`. A read error halfway through a source
gives the canonical form of the part that was read. That differs from the
file, so line 1318 writes it back over the original and the tail of the
source is lost.

### Nested input recurses without a bound

`src/anti/bindexpr.c:319`, rule 14, major.

`unary` calls itself once per leading `-`, `+` or `~`, and `primary` calls
`conditional` once per `(`. A macro of a header with a hundred thousand of
either exhausts the stack. The type parser does the same with one call per
`[` in `suffixes` at `bindtype.c:506`, per group in `declarator` at `569` and
per parameter list in `function_of` at `454`. The probe writer's `settable`
at `bindwrite.c:594` follows a record into its fields, and a struct of
`raylib_api.json` whose field names its own struct recurses forever under
`--probe`. The JSON tree bounds its depth with `JSON_TREE_DEPTH` for this
reason, and these readers lack such a bound.

### The directory walk treats an unreadable directory as an empty one

`src/anti/files.c:235`, rule 14, major.

`files.h` says `list_tree` returns false when a directory cannot be read,
and `walk` returns true for every failure of `opendir`: a directory without
permission and a process out of descriptors both add nothing. `walk` also
follows links with `stat`, so a link to a parent directory recurses until
`opendir` runs out of descriptors, and then that failure passes as well.
`anti check` and `anti fmt` then pass a tree they did not read. The
fix is to return true for `ENOENT` alone, and to use `lstat` or record the
devices and inodes already visited.

### Readers leave allocations behind on some failure paths

`src/anti/manifest.c:340`, rule 12, major.

`manifest_read` writes four default directories into `out` and then returns
false at lines 340 and 349 without freeing them, while every later failure
calls `manifest_free`. `path_dependency` at `build.c:851` trusts the second
form and leaks the four. `zip_read` at `zip.c:256` copies the name of an
entry and then fails at line 259 before it counts the entry, so
`zip_archive_free` never frees that name. Each reader should leave nothing
behind on failure, or each caller should free on failure. At present the
behaviour depends on which path failed.

### The command blocks of `main` free by hand before each return

`src/anti/main.c:296`, rule 12, major. Confirmed from the tool pass.

The tool pass names lines 296, 348, 487 and 539. Two more returns leak
further: after the second `malloc` of `sources` fails, line 398 returns
without freeing `roots`, `found`, `src`, `test`, `package` or `home`, and
line 460 does the same in the `fmt` block. One function per command with one
cleanup block settles all six.

### A signed right shift of a negative constant

`src/anti/bindexpr.c:438`, rule 3, major.

`x = is_unsigned ? x >> y : (uint64_t)(a.i >> y);` shifts a negative
`int64_t` right when a macro such as `-8 >> 1` is evaluated. C11 leaves the
result to the implementation, which rule 3 rules out. Every target of the
pinned clang shifts arithmetically, so the values are right today. The fix
is to shift the complement of a negative value and complement the result.

## Minor

### Functions past a threshold

`src/anti/main.c:215`, rule 18, minor.

- `main` (`main.c:215`, 422 lines) is one block per command and splits into
  one function per command, which also settles the `main` finding above.
- `check_run` (`check.c:495`, 158 lines, 8 parameters), `doc_run`
  (`doc.c:1173`, 136 lines, 10 parameters), `test_run` (`test.c:471`, 10
  parameters), `front_end_class` (`check.c:124`, 8) and `write_interfaces`
  (`doc.c:1125`, 8) take parsed flags one by one. A request struct, as
  `build_run` and `bind_run` take, would be clearer. The bodies of the three
  `*_run` functions are a sequence of classes and follow the problem.
- `bind_read_clang` (`bindclang.c:1285`, 139 lines): the argument vectors of
  lines 1314 to 1342 split out cleanly, and the two passes over the AST
  follow the problem.
- `build_project` (`build.c:988`, 105 lines): the `anti run` tail of lines
  1063 to 1087 is a function of its own. `build_target` (`build.c:698`, 120
  lines) could move the library-project branch of lines 747 to 766 out, and
  the rest follows the steps of a build.
- `read_preprocessed` (`bindclang.c:990`, nesting 5) reads better with the
  `#pragma pack` branch in a helper. `resolve_frame` (`syms.c:1027`, nesting
  5) splits its Mach-O branch the same way, and `syms_resolve`
  (`syms.c:1113`, nesting 5) its frame branch.
- Size that follows the shape of the problem: `usage` (`main.c:24`, 118
  lines, one text), `specifiers` (`bindtype.c:315`, 108 lines, one branch
  per C word), `bind_run` (`bind.c:155`, 104 lines, one write after
  another), `unit_order` (`units.c:142`, nesting 5, one fixed-point loop),
  `module_object` (`build.c:300`, 7 parameters), `repo_module`
  (`repo.c:288`, 7) and `fn_signature` (`doc.c:195`, 9), whose parameters are
  one operand each.

### Thirteen copies of one file reader and nine of one writer

`src/anti/build.c:51`, rule 26, minor. Confirmed from the tool pass.

The tool pass lists the thirteen readers. The writers repeat the same way:
`write_file` or `write_text` in `bind.c:15`, `build.c:67`, `check.c:76`,
`doc.c:81`, `fmt.c:1272`, `repo.c:49`, `symmap.c:56` and `test.c:73`, and
`lock_write` at `deps.c:778`. All but the one of `fmt.c` ignore the result
of the last `fclose`, which is where a full disk reports a buffered write,
so a truncated lock file or map is reported as written. One reader and one
writer in `files.c` would carry the `ferror` and `fclose` checks once.

### `anti test` keeps its own copy of the module reader

`src/anti/test.c:29`, rule 26, minor.

`test.c` declares a second `struct unit` and a `read_unit` that repeat
`unit_read` of `units.c`: the same read, module path, lex and parse, with a
diagnostic line of another form. `library_path` at line 93 repeats
`unit_file` of `units.c:44`, and the loop of `run_unit` that turns each dot into `_` at
line 416 repeats `flat_path` of `check.c:96`. `anti check` and `anti doc`
already share `units.c`, whose header says the list has one definition.

### Small helpers written twice

`src/anti/syms.c:109`, rule 26, minor.

`base_name` stands in `syms.c:109` and `bind.c:94`. The first takes either
separator on every host, and the second takes `\` on Windows alone.
`last_segment` of `doc.c:620` is `bind_last_segment` of `bindtype.c:126` and
`module_path_last` of `src/antic/modpath.h`. `die_out_of_memory` or
`out_of_memory` stands in ten files, and `list_add`, `strings_add`,
`texts_add` and `bind_list_add` are four growable arrays of pointers or texts.

### Dead stores and parameters

`src/anti/build.c:636`, rule 26, minor.

`struct text header` of `build_c_library` is freed and never written.
`lock_answers` takes `root` and discards it at `deps.c:831`, `joins_do`
discards `l` at `fmt.c:1011`, and `ends_value` discards `e` at `fmt.c:402`.

### Functions used in one file are not static

`src/anti/bindtype.c:89`, rule 20, minor.

`bind_known_name` is used in `bindtype.c` alone, `json_string` in
`jsontree.c` alone and `repo_cache_dir` in `repo.c` alone, and each is
declared in a header.

### Headers include what they do not use

`src/anti/build.h:5`, rule 21, minor.

`build.h` and `repo.h:5` include `<stddef.h>`, and neither names a type of
it.

### A platform test outside the platform file

`src/anti/bind.c:97`, rule 22, minor.

`base_name` of `bind.c` holds `#ifdef _WIN32`, while `files.c` is where the
tool keeps its platform branches. The copy in `syms.c` takes both separators
without a test, which removes the branch.

### Pointers to data the callee does not change are not const

`src/anti/doc.h:18`, rule 24, minor.

`doc_run` takes `const char **sources` and `const char **roots`, and casts
`roots` to `const char *const *` at `doc.c:1139`. `bind_header` at
`bind.h:12` and the `inject` parameter of `test_run` at `test.h:20` take the
same unqualified form, where `check_run` and `unit_read` take
`const char *const *`. `bind.c:176` and `bindclang.c:1322`, `1337` and
`1341` cast `const` away to store strings in a `bind_list`.

### The file helpers carry no prefix

`src/anti/files.h:10`, rule 25, minor.

`make_dirs`, `remove_tree`, `copy_file`, `copy_program`, `path_exists`,
`list_tree`, `list_dir` and `file_list_free` share no prefix, where every
other module of `src/anti/` has one. The module `jsontree` exports `json_*`,
close to the `anti_rt_json_*` of the scanner below it.

### Readers do not say who frees on failure

`src/anti/zip.h:48`, rule 11, minor.

`zip_read` leaves memory in `out` on failure, which the caller must free
with `zip_archive_free`, and `syms.c:671` does. `json_read` frees on
failure itself, and `manifest_read` does so on some paths only. Neither
header says which. `deps_resolve`, `manifest_read`, `unit_read` and
`repo_index` fill structures or texts the caller frees, and their comments
name no free function.

### `snprintf` results are not checked for truncation

`src/anti/bindclang.c:571`, rule 9, minor.

`snprintf(synth, sizeof synth, "%s_%s", rec->name, field)` cuts a long name
at 255 bytes, and two cut names can collide. `declare_record` then returns
the other record, and line 582 overwrites its `c_name`. The same stands at
line 573, at line 1112 for `paren[160]` and at `jsontree.c:33` for the error
message.

### Numbers that saturate are taken as read

`src/anti/bindexpr.c:115`, rule 14, minor.

`strtoull` of an integer literal above 2^64 gives `ULLONG_MAX`, and the
binding writes that value without a word. The same holds for `strtod` at
line 102, whose infinity is written as `inf`. `strtoll` at `bindapi.c:399`
and `bindtype.c:499` and at `repo.c:215` behaves alike. None checks `errno`,
so the input is taken rather than refused.

### The zip writer narrows counts and sizes without a check

`src/anti/zip.c:121`, rule 16, minor.

`put32(&out, (unsigned long)bytes.length)` and `put16(&out,
(unsigned)name_length)` write a size of more than 4 GiB and a name of more
than 65 535 bytes cut to the field. `(unsigned)count` at line 156 does the
same for more than 65 535 entries, which `syms_inventory` can reach. The
archive is then broken and `zip_write` reports success.

### Products that reach an allocation are not checked

`src/anti/test.c:157`, rule 5, minor.

`malloc(tree->item_count * sizeof *out->tests + 1)` multiplies without a
check, and its `+ 1` adds one byte where one element was meant. The same
unchecked form stands at `manifest.c:101`, `doc.c:451` and `530`, and in the
doubling of every growable list. The counts are bounded by the input, so no
overflow is in reach, but the rule asks for the check.

### The readers of untrusted input have no malformed-input tests

`src/anti/zip.c:212`, rule 15, minor.

No test feeds `zip_read` or `inflate` a truncated archive, an offset past
the end or a broken deflate stream. None feeds `anti symbols resolve` a
malformed trace or map, or the resolver a malformed `index.toml` or
`anti.lock`, and the one refusal `tests/run_anti_bind.cmake` checks for
`raylib_api.json` is a syntax error. A test with a repeated struct name
would have found the first severe finding.

### A comment stands above the wrong function

`src/anti/test.c:371`, rule 27, minor.

`/* Build the runner of one module and run it. */` stands above the comment
of `runner_frameworks` and describes `run_unit`, twenty lines below it.

## Noted outside the rules

The resolver keeps every requirement it has read. When `walk_round` picks a
new version of a package, the dependencies of the old pick still constrain
the graph. A search over more than one repository adds the dependencies of
every candidate, the dropped ones included. That is a question of
resolution rather than of these rules, and it is recorded here for the step
that owns "Resolution" in `docs/tooling.md`.
