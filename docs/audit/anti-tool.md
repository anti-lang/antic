# The anti tool

The third audit of `src/anti/`, at `69fc7bee`: 28 `.c` files and 23
headers, 18,623 lines, against the three standards. The session read 19
`.c` files line by line: `main.c`, `build.c`, `check.c`, `deps.c`,
`doc.c`, `test.c`, `repo.c`, `manifest.c`, `project.c`, `syms.c`,
`symmap.c`, `sdk.c`, `zip.c`, `units.c`, `memreport.c`, `files.c`,
`cursor.c`, `platform.c` and `licensing.c`, with the 19 headers beside
them. Two foreground reviewer agents read `fmt.c`, `jsontree.c` and the
seven files of the bind family with their headers, and returned
candidates. The session read each candidate of this report again in the
code, at the lines the finding cites. A candidate that did not hold, or
that the session did not read, is left out. It read
`docs/notes/build.md`, `bind.md`, `doc.md`, `symbols.md` and `warnings.md`
against the files and `docs/site/build-tool/index.md` against the
commands. Nothing was built and nothing was run, so no finding was
reproduced, and a finding that rests on a trace says "by this reading".
The machine evidence is `docs/audit/data/`.

The findings of `docs/audit/tool-pass.md` and `docs/audit/structure.md`
belong to the same audit and are not counted again: the header comment of
`files.c`, the comment of `antic.h` against the test `anti_interface`,
the dead option `--llvm-mc`, the prefix of a message spelled at 123
sites, the two options no test exercises, the functions past a threshold
and the directory `src/anti/` that no document lists. Where this report
adds to one of them, it says so. Each finding carries its state against
`docs/audit/2026-09-23/`, `docs/audit/2026-09-30/` and the closure of the
second audit. No finding returned. Every fix the closure records for
`src/anti/` was read again, by the session or by a reviewer, and holds at
the place it names.

The documentation grade "severe" is kept for a comment of a public header
that a caller acts on and gets another result. A header of a file that
says less than the file does is graded minor, as `docs/audit/structure.md`
grades it.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Safety, rules 6 and 14 | 1 | 1 | 0 | 0 |
| Severe | Documentation | 1 | 1 | 0 | 0 |
| Major | Safety, rule 14 | 3 | 3 | 0 | 0 |
| Major | Structure | 2 | 2 | 0 | 0 |
| Major | Documentation | 4 | 4 | 0 | 0 |
| Minor | Safety, rules 12, 14, 17, 26 and the warnings | 7 | 7 | 0 | 0 |
| Minor | Structure | 5 | 5 | 0 | 0 |
| Minor | Documentation | 3 | 3 | 0 | 0 |

In all 26 findings: 2 severe, 9 major and 15 minor. Eleven defects that no
rule names follow them and stand outside the table, as the closure of the
second audit lists its own.

## Severe

### A string that ends in a backslash sends the evaluator past its text

`src/anti/bindexpr.c:238`, safety, rules 6 and 14, severe, new.

`string` reads an escape as `char d = e->s[e->pos + 1]; e->pos += 2;` and
then switches on `d`. When the backslash is the last byte of the text, `d`
is the terminator, `pos` stands one byte past it, and the `default` branch
returns after `fail`. Nothing stops on the flag: `bind_eval` at line 601
calls `space`, which reads `e->s[e->pos]`, and `take` compares from there.
The read is outside the text, and outside its allocation when the text
fills the room `reserve` of `src/antic/text.c:12` gave it. `read_defines` at `bindapi.c:519` to `525`
hands the value of a define to the evaluator as it stands when its kind
is `INT`, `FLOAT`, `FLOAT_MATH` or `UNKNOWN`. A description that holds
`"type": "UNKNOWN", "value": "\"abc\\"` gives the text `"abc\`. The kind
`STRING` wraps the value in quotes and is safe, since the backslash then
escapes the closing quote and the terminator ends the loop at line 232.
`tests/unit/test_bind.c` feeds an open string and no string that ends in
a backslash, so rule 15 is not met for this path. The fix tests `d`
against the terminator before `pos` moves.

### bind_is_keyword answers for every text that is not one identifier

`src/anti/bindmodel.h:168`, documentation, a public header whose
documented behaviour differs from the code, severe, new. The second audit
described the behaviour inside its rule 14 finding at `bindwrite.c:263`,
which the closure records as fixed for the names of an API description.
The comment and the header path stayed.

The header says "Whether name is a word the lexer of Anti reserves". The
body at `bindmodel.c:52` is `return !antic_is_identifier(name);`, so it
answers true for the empty text, for `a$b` and for a name of UTF-8. The
reader of a description refuses such a name in `named` at
`bindapi.c:192`. The reader of a header holds a name to no rule:
`bindclang.c` calls the test once, for a macro at line 1394, and clang
takes `$` in an identifier. By this reading a field `a$b` of a header is
written as `a$b_` by `name_of` at `bindwrite.c:228`, and a function of
that name is left out with "its name is a word of Anti" at line 194. The
fix gives the function the name of what it tests, and one rule for a name
in both readers.

## Major

### anti fmt sorts the names of a body after an import without its semicolon

`src/anti/fmt.c:248`, safety, rule 14, major, new.

`sort_import_lists` walks from an `import` to the first `{` or `;` and
takes that brace as the list of a direct import. An import whose `;` is
missing reaches the brace of the next body. Lines 271 to 305 then trade
the text of every identifier between that brace and its match into the
order of their bytes, when the body holds no ordinary comment. By this
reading the source

```anti
import anti.text
fn main() -> int
{
	let b = 1;
	let a = 2;
	return b;
}
```

comes out with `let a = 1;` and `let b = 2;`. The formatter runs the lexer
alone, the source lexes, and `fmt_run` at line 1904 writes the result over
the file. A program that does not parse yet is changed in what it says.
The fix takes a list only where a module path alone stands between
`import` and `{` and a `;` follows the `}`. No test feeds the formatter
such a source.

### A float product past the largest float is written as inf.0

`src/anti/bindexpr.c:497`, safety, rule 14, major, new.

`apply` tests `isfinite(a.f)` on the double and then narrows with
`a.f = (double)(float)a.f;` at line 498. For `3e38f * 10` the double is finite and the
float is not. C11 6.3.1.5 leaves that conversion undefined where Annex F
does not hold, and with it the value is infinity. `bind_float_text` at
line 614 prints `inf`, finds no `.` and no `e`, and appends `.0`. The
binding then holds a constant no lexer of Anti reads. `number` at line
163 refuses a literal past `FLT_MAX`. `converted` at line 296 does not
for the cast `(float)1e300`. The fix is the test of `number` at both
places.

### A typedef is resolved again at every use, so forty lines ask for 3^40 parses

`src/anti/bindclang.c:437`, safety, rule 14, major, new.

`typedef_named` parses the spelling of a typedef each time a type names
it and keeps no result. The bound `TYPE_DEPTH` of `bindtype.c:81` counts
the depth, and the chain below is 40 deep and passes it:

```c
typedef int F0;
typedef F0 (*F1)(F0, F0);
typedef F1 (*F2)(F1, F1);
```

Each `F<n>` names `F<n-1>` three times, so the resolution of `F40` runs
3^40 parses, each taking types from the memory pool. By this reading
`anti bind --clang` on such a header does not end. The reader of a
description resolves an alias and a callback the same way at
`bindapi.c:110` to `123`, under its own bound of 32 at line 172. The fix
keeps the type of an entry once it is resolved.

### symmap.c holds the map of an archive and the readers of the licence notice

`src/anti/symmap.c:146`, structure, a file whose purpose cannot be said
in one sentence, major, new. The readers came with the commits of
2026-10-08 to 2026-10-10, after the second audit.

The header of the file and of `symmap.h:9` say "The text map of a symbols
archive". Lines 20 to 144 and 387 to 443 are that. Lines 146 to 252 read
the licence notice of a binary: `symmap_notice`, `symmap_license` and
`symmap_license_of`. Lines 254 to 367 read `licenses/sources.txt` of the
runtime archive: `symmap_source_version` and `symmap_license_sources`.
`main.c:472` and `licensing.c:76`, `149` and `200` call them for
`anti license`, which has a file of its own. A reader who looks for the
form of a notice finds it in the file of the map. The fix moves the five
functions into `licensing.c`, or into a file of the notice that
`syms.c`, `symmap.c` and `licensing.c` include.

### Four pairs of copies have drifted

`src/anti/bindwrite.c:633`, structure, duplicated logic that has already
diverged, major, new.

- Whether a type is unsigned. `is_unsigned_type` at `bindexpr.c:65`
  takes `u*`, `c_u*` and `c_size_t`. `settable` at `bindwrite.c:633`
  writes `t->name[0] != 'u' && strncmp(t->name, "c_u", 3) != 0` and so
  takes a bitfield of one bit of `c_size_t` for a signed one that the
  probe cannot set.
- A struct constant from `(Type){a, b}`. `color` at `bindapi.c:423` reads
  each field with `strtoll` in base 10 and writes `%lld`.
  `compound_literal` at `bindclang.c:1296` evaluates each field and
  writes a float through `bind_float_text`. `docs/notes/bind.md:72` says
  a define and a macro give the same constant, which holds for a struct
  of integers alone.
- The sources of a project. `project_sources` at `check.c:400` lists the
  source and the test directory and fails on none. `fmt_command` at
  `main.c:658` lists the same two and formats none in silence, and
  `doc_command` at `main.c:595` lists the source directory. The DESIGN
  comments at `main.c:586` and `655` say the three "read the same
  files", which a comment keeps and no function does.
- The module path of a library file. `library_module` at `build.c:343`
  takes it from the header of the file. `module_of_file` at `deps.c:393`
  takes it from the place of the file under its directory and drops the
  one `library_header` read at line 373. A file that stands under
  another name than its header gives two answers, and nothing compares
  them.

### The notes on warnings say the two safety checks are not built

`docs/notes/warnings.md:40`, documentation, a notes file that describes
a design the code no longer has, major, new.

"Neither check is built yet: each waits for the feature it guards,
concurrent classes and regular expressions. An `unchecked` of either
overrules nothing until then." Both are built: `sema_thread.c:103`
refuses a field without `unchecked(unguarded-field, "reason")`, and
`pattern.c:77` holds the check `exponential-pattern`. `CLAUDE.md` says so
under "State". The rest of the note agrees with `check.c:65` and `93` and
with `build.c:266` and `277`.

### The site page of the build tool describes another tool than the one built

`docs/site/build-tool/index.md:30`, documentation, a document that
describes a design the code no longer has, major, new. No earlier audit
read the page, whose front matter is dated 2026-09-15.

Gone with the native back end:

- 58, 199 and 207: `build/` holds "assembly text and object files", and a
  dev build writes `<module>.s`.
- 208: the tool "runs llvm-mc only on new assembly". Nine command
  lines from 218 to 619 pass `--llvm-mc`.
- 231 to 258: an assembly listing of `com.example.twice`.
- 288: "Linking and running need the target platform", and a link for
  Linux "stops there". lld links all six targets from any host.

Against the code and the decisions:

- 34, 57 and 284: `anti test` runs "the programs under `test/` against
  their expected outputs" and takes `--all-modes`. `test.c:334` refuses
  a run without files and runs the `tests` blocks of the files it is
  given.
- 176: the cache is `~/.anti/cache/`. `repo.c:99` takes the cache
  directory of the platform.
- 208: the cache key has three parts. `build.c:132` writes six.
- 272: "Release mode caches nothing". `module_library` at `build.c:268`
  takes a cached library file in release mode too.
- 532 and 534: the HTTPS client links Mbed TLS and reads
  `lib/cacert.pem`. `repo.c:234` runs curl, by the entry under "The build
  command", and the runtime archive carries no such file.
- 134: `http://` for two names of the loopback. `repo.c:114` takes three.
- 628: the notice listing has no `build <id>` line, which
  `src/rt/license.h:18` puts after the begin marker.
- 655: the `main` that `anti new` writes prints the licences for
  `--licenses`. `project.c:13` writes one that prints `hello`.
- 607: `license_text` may be an `https://` URL. `build.c:1171` joins it
  to the root as a path.
- 328: a library file "whose header disagrees with its index entry is
  refused". `repo.c:435` compares the digest and nothing of the header.
- 126 and 675: `anti.raylib`, `anti.miniaudio` and `anti.net` ship as
  bindings, and the table of obligations names neither musl nor mimalloc.

Described and not built: `add`, `fetch`, `clean`, `publish`, `html`,
`tex`, `completions` and `syntax` at 36 to 48, the `revision` of an index
at 180, the file `sha256` at 139, the probe that `anti test` compares at
484, and the code blocks of a page through `anti html` at 468. Built and
not on the page: `anti symbols` with its three commands, `anti sdk export`
and `import`, `--cpu`, `--memory-checks`, `--lto`, `--profile-generate`
and `--profile-use` of `anti build`, and `--targets all` of `anti check`.
Whether the page is the design to build towards or the description of
`anti` is Eddie's decision. Today it is neither.

### main.c and sdk.c open with no sentence of their purpose

`src/anti/main.c:26`, documentation, a file without a header comment that
says its one purpose, major, new. The tool pass reports `files.c:24` and
counted these two as opening with a comment.

The first comment of `main.c` stands before `usage` and says why its text
is three literals. The first comment of `sdk.c`, at line 14, describes the
array `macos_targets`. Neither says what the file is for. Of the headers,
`files.h` and `units.h` open with the comment of their first declaration
and give no sentence of the file either. The closing
section gives the sentence of each as read from the code.

### Fifteen functions of the headers share a comment or have none

`src/anti/bindmodel.h:136`, documentation, a public function of a header
without a comment that says what it takes, gives and refuses, major, new.
The tool pass reports the class for the tree, and this is its list for
`src/anti/`, from `data/header-function-comments.txt`.

- The seven that free: `deps_free` at `deps.h:64`, `files_list_free` at
  `files.h:114`, `json_free` at `jsontree.h:51`, `manifest_inject_free`
  and `manifest_free` at `manifest.h:37` and `97`, `unit_free` at
  `units.h:43` and `zip_archive_free` at `zip.h:62`. None says whether it
  takes a record that a failed read left, which every caller relies on.
- `bind_list_add` at `bindmodel.h:136`, with no word on who frees the
  list. `bind_run` frees four at `bind.c:201`.
- `bind_strndup` at 140, `bind_needs_shim`, `bind_write_shim`,
  `bind_write_probe_c` and `bind_write_probe_anti` at 186 to 189, each
  under the comment of the function above it.
- `bind_read_clang` at 206 and its request at 196, under the comment of
  `bind_read_api`. `target_options` at `bindclang.c:159` refuses a
  request without `runtime`, and the header does not say so.
- `repo_version_valid` at `repo.h:27` has its comment above the `enum`
  that stands between them, and is no defect.

## Minor

### Resources released by hand before each return

`src/anti/main.c:198`, safety, rule 12, minor, new places. The second
audit reported the pattern at `check.c:474` and five more places, which
the closure records as fixed and which hold.

- `symbols_command` frees `symbols` at `main.c:198` and `214`, and
  `license_command` frees `rest` and `runtime` at 441, 458 and 461.
- `path_dependency` frees `manifest_path` at `build.c:961`, `968` and
  `973` and again at 988.
- `resolve_from_path` frees up to three texts at `deps.c:443`, `457` and
  `466`.
- `compile_block` at `check.c:261` and `275`, `test_run` at
  `test.c:352`, `load_archive` at `syms.c:563` and `570`,
  `licensing_from_archive` at `licensing.c:397` and `fmt_run` at
  `fmt.c:1885` and `1894`.
- Three returns keep what a failed read may have taken. `files_read`
  restores the length of its text and not its room, and
  `add_binary` at `syms.c:171`, `read_configuration` at `syms.c:250`
  and `licensing_from_archive` at `licensing.c:392` return without
  freeing it.

### Paths that an input names are read without the test of a regular file

`src/anti/syms.c:343`, safety, rule 14, minor, new.

`files.h:55` says "A path that an input names reads through"
`files_read_file`, which refuses a device and a FIFO. Five reads of such
a path go through `files_read`: the libraries of a plugin index and of
`[injections]` at `syms.c:171`, an `include` of the configuration at 250,
the plugin index at 365, every entry of the directory of the
configuration at 343, and the module a line of a report names at
`memreport.c:120`. `platform_kind` at `platform.c:330` answers
`PLATFORM_FILE` for everything that is neither a directory nor a link, so
`files_list_dir` lists a FIFO as a file. By this reading
`anti symbols inventory` waits without end on a FIFO beside the
configuration, and `include = "/dev/zero"` reads until memory runs
out.

### Records, enums and defines are not held against the words of Anti

`src/anti/bindwrite.c:263`, safety, rule 14, minor, new. The second
audit's finding at this line concerned a name that is no identifier, and
its fix holds for a description.

`name_of` gives a field, an enumerator and a parameter a trailing `_`,
and `bind_settle` leaves a function out at line 194. `write_record` at
263 and `write_enum` at 294 write the name of a record and of an enum as
it stands, and `read_defines` at `bindapi.c:547` hands the name of a
define to `bind_eval_const` after the test of a C identifier alone. A
header with `struct in` or a define named `type` gives a module that the
formatter passes, since it lexes, and that does not parse.

### The fields of the zip format stand in unsigned long

`src/anti/zip.c:43`, safety, rule 17, minor, new.

`put32` takes and `get32` returns `unsigned long`, `crc32_of` computes in
it, and `struct zip_item` at `zip.h:45` holds `unsigned long crc` and
`unsigned method`. The width of each is part of the format, and
`unsigned long` holds 32 bits on Windows and 64 elsewhere. Every use
masks or shifts within 32 bits, so the bytes agree on both. The rule asks
for `uint32_t` and `uint16_t`.

### Zero added to a null pointer in two readers

`src/anti/zip.c:673`, safety, rule 14, minor, new.

`zip_unpack` computes `out->data + from` for the CRC. A deflate entry of
no bytes into a text that holds nothing leaves `out->data` NULL.
`json_read` at `jsontree.c:259` computes `bytes + length` and gets NULL
and 0 from `bindclang.c:1532` when clang wrote nothing. C11 6.5.6 leaves
both sums undefined, and the UBSan build of clang reports such a sum in
C. The severity table makes undefined behaviour severe. These two read
and write nothing, so they are graded minor here, which Eddie may raise.

### The escaped name of a symbol is read back by two copies that allocate on their own

`src/anti/syms.c:837`, safety, rule 26, minor, new.

`append_name` and `add_function` at `symmap.c:49` hold the same six
lines: `malloc` of the length and one, `anti_rt_symbol_unescape`, and the
raw name where either gave nothing. `docs/decisions.md:908` says "Every
allocation of `anti` goes through `files_array`, `files_resize` or
`files_grow`", with the JSON reader as the one exception. These two call
`malloc`, and a failure writes the escaped name where every other
allocation of the tool ends it with status 70.

### A parameter silenced with a cast and no reason

`src/anti/bindapi.c:153`, safety, the warnings rule of `CLAUDE.md`,
minor, new.

`record_named` holds `(void)is_union;` with no comment. The six casts of
`platform.c:294` to `310` carry the reason, the POSIX interface, in the
comment above each function.

### A status of driver_run is the number 3 at four places

`src/anti/build.c:333`, structure, a boundary, minor, new.

A module without `main` stops at its object, and antic says so with
`status = 3` at `src/antic/driver.c:1649`. `build.c:333`, `test.c:102`
and `test.c:174` test `status == 3`. `driver.h:165` says the function
returns "the process exit status for antic" and names no value. A change
of the number in the driver compiles in `anti` and fails every dev build.
The fix is a named value in `driver.h`.

### Two records named unit

`src/anti/syms.c:409`, structure, a name that makes the code harder to
find one's way in, minor, new.

`struct unit` of `units.h:14` is one module of a run over the sources.
`struct unit` and `struct units` of `syms.c:409` and `418` are the
symbols of one build. `syms.c` does not include `units.h`, so the two
never meet in one translation unit. A reader who searches for
`struct unit` finds two records under one name.

### Names of files and directories spelled at more than one place

`src/anti/main.c:488`, structure, each concern in one place, minor, new.

- The work directories `build/check`, `build/doc`, `build/doc-work` and
  `build/tests` at `main.c:488`, `550`, `551` and `686`. The build
  directory is `layout.build` of the manifest, whose default stands at
  `manifest.c:331`, so a project that names another keeps a `build/` of
  these three commands.
- The directory of a deliverable, `<root>/<dist>/<target>/<mode>`, at
  `build.c:840`, `983` and `1120`, with `release` and `dev` at 821, 984
  and 1122 and the name of the program at 888 and 1123.
- The entries of a symbols archive. `build.c:653`, `654` and `687` write
  `.debug`, `.map` and `.pdb`, and `syms.c:587`, `588`, `761`, `981` and
  `982` read the first two. `SYMS_ARCHIVE_SUFFIX` of `syms.h:12` shows
  the form: one definition for the writer and the reader.
- `anti/test` at `test.c:152` and `274`, and the targets `macos-arm64`
  and `macos-x86_64` at `sdk.c:15`, where `target_name` holds them.

### Three helpers stand in the file of another concern

`src/anti/repo.h:20`, structure, placement, minor, new.

- `repo_name_valid`, `repo_version_valid` and `repo_digest_valid` are the
  grammar of a name, a version and a digest. `manifest.c:368`,
  `doc.c:798` and `deps.c` call them and fetch nothing, and `doc.c:24`
  includes the header of the repositories for one test.
- `deps_library_header` at `deps.h:16` calls itself "the one reader of
  the header of a library file in anti". `bind.c:44`, `build.c:348` and
  `licensing.c:190` call it and resolve nothing.
- `default_runtime` at `main.c:237` returns `runtime_archive(out)` and
  nothing else. Five commands call the wrapper and `license_command`
  calls `runtime_archive` at 467.

### sdk export reads a JSON file by searching for a quoted word

`src/anti/sdk.c:41`, structure, one way of reading a file, minor, new.

`sdk_version` finds the version of an SDK with
`strstr(text_cstr(&json), "\"Version\"")` and takes the next quoted text.
`jsontree.c` is the JSON reader of the tool. A member `Version` of a
nested object that stands first answers for the SDK. `sdk_version_valid`
at line 58 then holds the text to digits and dots, so no path is at risk.

### Eight file headers say less than the file holds

`src/anti/zip.c:1`, documentation, a header that says less than the file
does, minor, new.

- `zip.c:1`, "The zip writer of the symbols archive". Lines 169 to 681
  are the reader and a decoder of deflate.
- `manifest.c:1`, "The tables of `anti.toml` that a build passes to
  antic". The file reads the whole manifest: the package, the layout,
  the targets, the repositories and the dependencies.
- `units.c:1` names the modules and the options. The file also holds the
  frameworks and the Linux libraries of a link from line 41.
- `deps.c:1` names the lock file, the index and the walk. The file also
  holds the comparison of versions and the reader of a library header.
- `bindclang.c:1`, "The reader of a C header through clang". Lines 29 to
  225 find clang, test its version and build its options.
- `bindwrite.c:1`, "The writer". `bind_settle` at line 153 decides what
  the module holds.
- `bindexpr.c:1`, "The evaluator". `bind_eval_const` at line 645 adds a
  constant to the model, and `bind_float_text` writes a float.
- `fmt.c:1` describes the canonical form. `fmt_run` at line 1875 reads,
  compares and replaces the files of the command.

### Nine comments differ from the code below them

`src/anti/build.c:487`, documentation, a comment that differs from the
code, minor, new. The second audit's finding of this kind named three
other places, which are fixed.

- `build.c:487`: "Without one the first module of the order stands".
  `root_module` returns `b->order[b->unit_count - 1]`, the last.
- `deps.c:638`: `found` is false "where the repository holds no index of
  the package or no version of it answers". For no version the function
  returns false, the fifth defect below.
- `bindclang.c:876`: "A typedef of a tagged enum takes the typedef name
  when the two differ". The branch returns and the enum keeps its tag.
- `bindclang.c:1105`: a comment on the reader of `clang -E -dD` stands
  above the comment of `struct pack_stack`, and `read_preprocessed` at
  line 1186 has none.
- `bindclang.c:1421`: `settle_enums` is described as the anonymous enums
  alone, and it adds every named enum to the module at line 1430.
- `bindmodel.h:156`: "whose Anti name goes to out". `enum_named` has no
  `out` and returns the name.
- `repo.h:13`: the comment names the index file and "the directory of
  the library files of one version". One `#define` follows it, the one
  of the index file.
- `repo.h:10` and the message at `repo.c:156` name `127.0.0.1` and
  `localhost`. `is_loopback` at line 114 takes `[::1]` as well, as the
  decision says.
- `check.c:29`: the blocks go "under docs" and "under dev". The two
  directories are `check/docs` and `check/dev` below the work directory.

### Three notes carry a statement the code has left

`docs/notes/doc.md:53`, documentation, a stale statement in a notes
file, minor, new.

- `doc.md:53`: "The format version was 51 then. It is 52". `ANTL_VERSION`
  at `src/antic/antl.h:19` is 81.
- `build.md:37`: the cache key is the digest, the version, the target and
  the level "with a mark for `-g`". `cache_key` at `build.c:132` adds
  `strict` and `memory-checks`, which `docs/decisions.md:1603` and `2411`
  record.
- `bind.md` has no word on `anti bind --header`, which `bind_header` at
  `bind.c:26` holds, and its list of files at line 6 gives `bindwrite.c`
  the module, the shim and the probes without `bind_settle`.

`docs/notes/symbols.md` agrees with `syms.c`, `zip.c` and
`src/rt/conf_include.h` at every sentence read.

## Defects no rule names

Each was found by reading and none was run.

1. `build.c:120`. The cache key of a module holds the digest of its input,
   the version of antic, the target, the level and three marks. It leaves
   out what else reaches the output. `--strip-docs` at line 278: a build
   after one without the option takes the cached library file with its
   doc text. The package version, the licence and the dependencies of
   `base_options` and `header_options` at 191 to 228: a new `version` in
   `anti.toml` leaves the library files and the objects of a dev build
   as they were. "Versions" in `docs/decisions.md` puts that version in
   every class descriptor.
2. `build.c:978`. `path_dependency` sets `sub.target = NULL`, and
   `build_targets` at 1021 then builds the list `default` of `[targets]`
   of the dependency. Line 983 reads `dist/<host>/<mode>` of it. A
   dependency whose manifest names another default target is built for
   that one and read where nothing was written. The comment at 946 and
   the decision say "built for the host first".
3. `build.c:905`. With `--lib shared --soname` antic writes
   `lib<name>.so.<major>` and a link `lib<name>.so` to it, at
   `src/antic/driver_library.c:373` and `440`. `copy_into` copies through
   the link, so `dist/` holds `lib<name>.so` alone, whose soname names a
   file that `dist/` lacks. No test passes `--soname` to `anti`, which
   the tool pass reports.
4. `build.c:905`. For a Windows target antic writes the import library
   `<name>.lib` beside the DLL, at `src/antic/linker.c:1073`. The build
   copies the DLL and the header into `dist/` and not the import library
   a C program links against.
5. `deps.c:675`. An index that holds the package and no version that
   answers leaves `resolve_from_index` with false. `resolve_from_repo` at
   732 then prints that the repository "gave no index" of the package
   "that anti reads". The dependency names no repository, and no version
   takes its constraint.
6. `fmt.c:1738`. `e->anonymous` is set for a `fn` after `,`, `(`, `[`,
   `=`, `return`, `yield`, `=>` and `??`, and cleared at line 1458 alone,
   when a `{` arrives at the same depth of brackets. A function type at
   such a place, the second type argument of `Map<str, fn(int) -> int>`
   or the tuple of `tests/clib/com/example/names.anti:520`, leaves it
   set. By this reading the next `{` at that depth is laid out as the
   body of an anonymous function, at 1511 and 1775. Fixed `e3d6d304`.
7. `project.c:32`. `anti new` counts the dots of its name and holds it to
   nothing else. `anti new com.My-App` writes a project that no build
   takes, and `anti new a./tmp/x` writes its project at `/tmp/x`.
   `repo_name_valid` is the test. Fixed `6d360b0e`.
8. `build.c:1156`. The `antic` of a manifest is compared without a test
   of its form. `antic = "v0.3"` reads as `0.0.0` in `version_part` at
   `deps.c:39` and every compiler passes, where the decision says a
   version of another form satisfies nothing.
9. `deps.c:1043`. `lock_answers` asks whether every dependency of the
   manifest stands in the lock file. A package of the lock file that the
   manifest no longer names stays in the graph and is fetched. It stands
   in the header of every library file and in `NOTICE.txt`.
10. `build.c:267`. The key of a library file is over its own source. A
    module whose import changed its interface is not checked again in a
    dev build, apart from the module that links. The specification gives
    the key so, and whether it should hold the digests of the imports is
    Eddie's decision.
11. `repo.c:235`. curl runs with `-L`, so an `https://` repository may
    redirect to plain `http://` on any host, which `repo_url_allowed`
    refuses in a URL of the manifest.

## What reaches past the interface

`anti` includes twelve headers of `src/antic/`: `antic.h` in 15 files,
`driver.h` in 7, and the shared helpers `text.h`, `arena.h`,
`target.h`, `cpu.h`, `modpath.h`, `userdirs.h`, `sha256.h`,
`applesdk.h`, `attributes.h` and `platform.h`. The last comes through
`src/anti/platform.h:9` as `../antic/platform.h`, the one relative
include of the tool. No file includes a header of a pass, and no file
calls `driver_interface`.

- Past the list of `antic.h`: `attributes.h` at `bindmodel.h:9`, which
  the tool pass reports.
- Through `antic.h` into the front end: `fmt.c` reads
  `struct token_list` and every `TOKEN_*` of `lexer.h`, and calls
  `lexer_token_is_builtin_type` at lines 447, 785 and 870 and
  `lexer_token_list_free` at 1870. The comment of `antic.h` and the test
  allow it. `fmt.c` also spells the contextual words `allow`,
  `unchecked`, `lent`, `guarded` and `snapshot` as texts at lines 751,
  796, 973 and 1425, where the parser has `parser_is_word`. A new word
  of the parser reaches the formatter through no list.
- The status 3 of `driver_run`, the finding above.
- Calls into antic: `driver_run` at 14 places, `driver_libraries` at 4,
  `driver_frameworks`, `driver_linux_libraries`, `driver_library_header`,
  `driver_runtime_packages`, `driver_memcheck_companion`, the walks
  `antic_outline`, `antic_tokens`, `antic_doc_page`,
  `antic_library_header`, `antic_library_starts`, `antic_is_identifier`,
  `antic_notice_lines` and `antic_component_license`, and
  `link_lto_from_name` and `link_pdb_path`, which `antic.h` declares.
- Six headers of the runtime: `toml.h`, `json.h`, `license.h`,
  `plugin_index.h`, `conf_include.h` and `symbols.h`. The list of shared
  headers in `docs/audit/structure.md` leaves out the last two.
  `conf_include.h` opens with the DESIGN of its two readers.
  `src/rt/symbols.h:1` says the readers serve `symbolize` and does not
  name `anti`, which reads a map and a twin with them.

## One responsibility per file

`main.c` reads the options of every command and calls one function per
command. `build.c`, `check.c`, `doc.c`, `fmt.c`, `test.c`, `project.c`,
`sdk.c`, `licensing.c` and `bind.c` each hold one command, and
`syms.c` the three of `anti symbols`. Their helpers are `units.c`,
`manifest.c`, `deps.c`, `repo.c`, `zip.c`, `symmap.c`, `cursor.c`,
`memreport.c`, `files.c`, `platform.c` and `jsontree.c`. `symmap.c` is
the one file with two purposes. `build.c` holds `anti build` and
`anti run`, which is one purpose, since a run is a build and one call.
`files.c` holds the allocation helpers beside the file helpers by the
provisional entry at `docs/decisions.md:908`.

The bind family is one part. `bindmodel.h` is its interface, the two
readers `bindapi.c` and `bindclang.c` fill the model, `bindtype.c` and
`bindexpr.c` parse and evaluate for both, `bindwrite.c` writes it and
`bind.c` runs it. Each file keeps one purpose apart from `bindmodel.c`,
whose own header names three things: the memory of the model, the
warnings and the keyword test, and the tables of frameworks and Linux
libraries. The second audit found those in `bindtype.c`, which now holds
the parser alone.

## The sentence of each file

Marked "header" where the file's own first comment says something else,
and "none" where it has no such comment.

- `main.c`: the command line of `anti`, its usage text and one function
  per command. None.
- `build.c`, `build.h`: `anti build` and `anti run`, a manifest into the
  files of `build/` and the deliverable of `dist/`.
- `check.c`, `check.h`: `anti check`, the four classes that write no
  artifact of a program.
- `deps.c`, `deps.h`: the dependency graph of a project, with the lock
  file, the index, the walk, the comparison of versions and the reader
  of a library header. Header: the first three.
- `doc.c`, `doc.h`: `anti doc`, the pages of HTML and Markdown from the
  records antic fills.
- `fmt.c`, `fmt.h`: `anti fmt`, the canonical form of a source and the
  run over the files. Header: the form.
- `test.c`, `test.h`: `anti test`, the runner of the `tests` and
  `fixtures` blocks of each module.
- `repo.c`, `repo.h`: the fetch from a repository into the cache of the
  user, and the grammar of a name, a version and a digest.
- `manifest.c`, `manifest.h`: the reader of `anti.toml`. Header of
  `manifest.c`: the tables a build passes to antic.
- `project.c`, `project.h`: `anti new`, a project of the default layout.
- `syms.c`, `syms.h`: `anti symbols inventory`, `check` and `resolve`.
- `symmap.c`, `symmap.h`: the map of a symbols archive, and the readers
  of the licence notice and of the record of upstream sources. Header:
  the map.
- `sdk.c`, `sdk.h`: `anti sdk export` and `import`, the stubs of Apple's
  SDK into a bundle and out of one. None in `sdk.c`.
- `zip.c`, `zip.h`: the writer and the reader of a zip archive, with a
  decoder of deflate. Header of `zip.c`: the writer.
- `units.c`, `units.h`: the modules of a run in the order of their
  imports, the options every compile starts from, and the links of the
  library files. Header of `units.c`: the first two. None in `units.h`.
- `memreport.c`, `memreport.h`: the run of a `--memory-checks` program
  whose report names its frames through Anti's symbolizer.
- `files.c`, `files.h`: the allocation helpers and the file helpers of
  the tool. None in either.
- `cursor.c`, `cursor.h`: the cursor over one line of text from another
  machine.
- `platform.c`, `platform.h`: the calls that differ by host and that
  antic does not make.
- `licensing.c`, `licensing.h`: `anti license` over the runtime archive,
  a project and a static archive, with the reader of an `ar` archive.
- `jsontree.c`, `jsontree.h`: a tree of a JSON document over the scanner
  of the runtime.
- `bind.c`, `bind.h`: `anti bind` and `anti bind --header`.
- `bindapi.c`: the reader of an API description into the model.
- `bindclang.c`: the run of clang on a header and the reader of its AST
  and its preprocessed text into the model. Header: the reader.
- `bindtype.c`: the fixed C types and the parser of a C type spelling.
- `bindexpr.c`, `bindexpr.h`: the evaluator of a constant expression of
  C, and the constant it adds to the model. Header of `bindexpr.c`: the
  evaluator.
- `bindwrite.c`: the choice of what a module holds, and the writer of
  the module, the shim and the two probes. Header: the writer.
- `bindmodel.c`, `bindmodel.h`: the model of a binding, with its memory,
  its warnings, the keyword test and the tables of what a bundled
  library links.
