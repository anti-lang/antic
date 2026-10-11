# Runtime

This step audited `src/rt/` against the three standards, at `69fc7bee`. It
covers every `.c` and `.h` of the directory except `json.c`, `toml.c` and
`regex.c`, which the library-file area reads. That is 39 C files and 27
headers, about 16,400 lines. The platform layer came first:
`platform.h`, `platform_posix.c`, `platform_windows.c`, `platform_entry.c`
and `platform_stdio.c` were read whole and against each other. The session
then read `threads.c`, `lock.c`, `sync.c`, `hooks.c`, `loaded.c`,
`signal.c`, `fs.c`, `errno.c`, `io.c`, `assert.c` and `atomic.c` with their
headers. Three foreground reviewers read the rest in full: the object
model, the loader with the configuration, and the traces with the text and
the patterns. Every finding a reviewer gave was read again in the code
before it entered this report, and what could not be confirmed was left
out. The notes `threads.md`, `locking.md`, `traces.md`,
`trace-handlers.md`, `hooks.md`, `injection.md`, `plugins.md`,
`runtime-conf.md`, `strings.md`, `fmt.md` and `setup.md` were read against
the files. Nothing was built and no suite ran. Three findings rest on
reading alone and say so. The machine evidence is `docs/audit/data/`.

The runtime needs nothing of antic. No file of `src/rt/` includes a header
of `src/antic/`, `src/anti/` or `tools/`. It names seven symbols that only
generated code defines: `anti.rt.main`, `anti_rt_registry`,
`anti_rt_injectable`, `anti_rt_slots`, `anti_rt_trampolines`,
`anti_rt_backtrace_default` and `anti_licenses`. In a plugin it reads
`anti_rt_provides` and `anti_rt_imports` as well. They are the contract of
`src/antic/rt_abi.h`, which the tests `runtime_functions` and
`runtime_records` pin. What the contract leaves out is the finding on
mirrored layouts below. No host `#if` stands outside the platform layer.

Four findings of `docs/audit/tool-pass.md` and `docs/audit/structure.md`
on these files are not repeated. They are the missing format attribute on
six functions and the two halves of `platform_windows.c`. They are also
the two out-of-memory messages without the prefix, and the directory that
no document lists.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Safety, rules 6, 11, 14, 15 and 23 | 7 | 5 | 0 | 2 |
| Severe | Documentation | 7 | 7 | 0 | 0 |
| Major | Safety, rules 4, 11, 14, 15 and 22 | 7 | 6 | 0 | 1 |
| Major | Structure | 4 | 4 | 0 | 0 |
| Major | Documentation | 4 | 4 | 0 | 0 |
| Minor | Safety | 14 | 13 | 0 | 1 |
| Minor | Structure | 2 | 2 | 0 | 0 |
| Minor | Documentation | 4 | 4 | 0 | 0 |

In all 49 findings: 14 severe, 15 major and 20 minor. Four are still open
in part from an earlier audit, and none returned. Five of the seven severe
safety findings concern a loaded library. The closed findings of the second
audit that were read again hold: S25, S26, S27, S28 for `instance`, M22,
M32, M38 and M47.

## Severe

### A frame points into the image of a library that may be unloaded

`src/rt/trace.c:128`, safety, a use after free, rule 11, severe, new.
`src/rt/trace.c:323` is the second place.

`notice_id` gives the build id as a pointer into the notice of the module:
`return text_part(notice + sizeof head - 1, ...)`. `anti_rt_trace_frame`
stores it in the frame at line 301. The path of the same frame is copied
with `keep_in` at line 299, "so a frame may point at them". The id is not.
A trace that holds a frame of a plugin outlives the plugin, since the trace
and its error are objects of the host. After `lib.unload()`,
`anti_rt_trace_text` reads `f->build_id.ptr` at line 514 from memory that
is no longer mapped. On macOS `symbolize_macho` passes `stable` as true at
line 323, so a function name without an escape points into `__LINKEDIT`
of the image as well. `known[]` at line 205 keeps the id by the address of
a header and never drops an entry. The fix keeps the id as the path is
kept, and passes `stable` as false for an image.

### `reflect.new` builds an object of a library after it gave the lock back

`src/rt/registry.c:94`, safety, rule 23, severe, still open in part. The
first audit named `registry.c` under "The table of open libraries has no
guard", and S28 of the second closed the same window in `plugin.c`.

`registry_find` walks the classes of the open libraries under the lock of
the slots and releases it at line 94. `build` then reads
`c->descriptor->size` at line 101 and calls `c->init` at line 104, both in
the image of the library. No object is counted against the slot in between.
An `unload` on another thread finds `live` at 0 and closes the library, and
the call lands in code that is gone. `read_object` of `Object.deserialize`
takes the same path. The DESIGN comment at line 70 says the class "stays
mapped while the program keeps its library open", which nothing holds
against a second thread. The fix counts one object under the lock, as
`reserve` of `plugin.c:715` does.

### The count of a library falls to zero before its teardown has run

`src/rt/hooks.c:81`, safety, rule 23, severe, new. It is the shape of S28
on the other end of an object's life.

`anti_rt_hook` subtracts the object from its library on the `destroyed`
hook. `lower_class_teardown` of `src/antic/lower_owning.c:559` writes that
hook first, before the `destruct` bodies and the teardown of the fields.
Those run in the code of the library with the count already at 0.
`anti_rt_plugin_unload` on another thread then passes its check at
`plugin.c:810` and closes the library under the thread that is still in
`destruct`. The object's own `destroyed` hook, which `anti_rt_hook` calls at
line 88 after the count, is the first code to run unmapped. The fix counts
the object out after its teardown returns, or holds the slot for its
length.

### The loader replaces the table of an object with one too short for it

`src/rt/plugin.c:383`, safety, rule 6, severe, new. Read, not run: the
closure of the second audit records that no test reaches a stub.

`build` writes the loader's table into `sub->table` when the host calls
through `reflect.call` and its interface has more slots than the library
was built with. `stubs_of` gives that table `function_count + 1` entries of
the host's interface, and line 377 copies `e->chain_length` of them. A
class that inherits the interface holds it at offset 0, as
`interface_offset` of `src/antic/whole_tables.c:878` writes. `sub` is then
the object, and the table replaced is the class's own. The functions the
class adds stand at slots past the copied ones. A call of one reads a stub
or reads past the block that `calloc` gave. `delete` then runs the
`destroyed` hook with a table on the heap. `anti_rt_library_image` names no
library for it, so `live` never falls and `unload` refuses for good. The
fix writes stubs for a sub-object alone, or sizes the table by the class.

### The table of a library is walked by counts nothing checked

`src/rt/plugin.c:589`, safety, rules 14 and 15, severe, still open in part
(S33 of the second audit, which closed the lists of a descriptor).

`damage_of` tests `table->count` and `table->class_count` for a sign and
their lists for NULL. It never passes them to `list_sound`, so a damaged
count walks `table->entries[i]` past the table. Line 613 tests
`e->chain_length` and `e->chain` the same way, and `checked` reads
`e->chain[at - 1]` at line 237. `fields_sound` accepts `offset <= size` at
line 522 without the width of the field, and `Object.deserialize` writes
there. The header of the file says "It checks every count, length, pointer
and offset of the table before it uses one". `damaged_counts` of
`tests/unit/test_plugin_table.c:258` tries -1 and NULL alone. The fix is
`list_sound` on the three lists, the width in `fields_sound` and a count
that is too large in the test.

### A `str` inside a heap snapshot has no NUL after it

`src/rt/snapshot.c:30`, safety, rules 6 and 10, severe, new. The other half
is `src/antic/lower_function.c:499`.

Lowering sizes a heap snapshot as its fixed part plus the `len` of each
captured `str`, with no byte more. `anti_rt_snapshot_text` copies exactly
`length` bytes. `docs/decisions.md:52` says a `str` is "always followed by
a NUL byte outside its length so `s.ptr` is a valid C string". The last
captured text ends where the block ends, so `printf("%s", name.ptr)` inside
an `own fn` reads past the allocation. `tests/programs/snapshots.anti`
prints every captured text with `%.*s` and does not reach it. The fix is
one byte more per text in the size, and the NUL written by
`anti_rt_snapshot_text`.

### A builder gives a text the program is told to free and cannot

`src/rt/text.c:155`, safety, rule 11, severe by the scale, new. It needs
`malloc(1)` to fail.

`anti_rt_builder_take` returns `text.ptr = (const unsigned char *)"";` when
the one byte of an empty text finds no memory. `std.h:120` says "Hand the
bytes to the caller, who frees them", and the DESIGN at line 144 says every
text "can be freed alike". The program then passes a string literal to
`free`. `anti_rt_text_copy` at line 39 ends the program in the same state.
The fix is that end, or a NULL pointer with length 0.

### The header of trace.c says the capture reads no file

`src/rt/trace.c:5`, documentation, a file whose documented behaviour
differs from the code, severe, new.

The DESIGN says of the capture "It reads nothing from a file" and that a
handled error "costs the walk alone". On Linux `anti_rt_trace_frame` calls
`module_id`, which calls `elf_id` for every module but the runtime's own.
`elf_id` calls `load(m->path, false)` at line 251, which reads the whole
file with `anti_rt_fs_read` and keeps it until exit. A program linked
against glibc has frames in `libc.so.6`, so its first trace reads that
file. `docs/notes/traces.md:62` describes the read as it is. The fix
corrects the comment, or moves the id to the text of a trace.

### rt.h describes an initialisation and a writer that do not exist

`src/rt/rt.h:6`, documentation, a public header whose documented behaviour
differs from the code, severe, new. `src/rt/rt.h:21` and
`src/rt/init.c:4` are the other places.

The comment of `anti_rt_init` says "A static library for C initialises it
on the first use of the runtime", and `init.c:4` repeats it. The callers
are `start.c:137` and the constructor that `src/antic/driver.c:810` names
for a shared library. No code runs it on a first use, so a static library
for C never calls `anti_rt_hash_seed_start`. The provisional entry at
`docs/decisions.md:399` rests on the same sentence. `anti_rt_ready` has no
caller in `src/`. Line 21 says of `anti_rt_option_backtrace` that
"src/rt/start.c writes it before main". `set_key` of `conf.c:274` writes
it, from the command line, the file and `rt.configure`.

### cpu.c describes a level that antic writes and start.c passes

`src/rt/cpu.c:1`, documentation, a file whose documented behaviour differs
from the code, severe, new.

The header says "antic writes the level a program was built for into the
object that defines main, and start.c passes it here before it calls
main". The level is `ANTI_CPU_LEVEL_ID`, a macro of the runtime's own
compile, read at line 184. `start.c` passes nothing. `cpu_level.h:3` and
the entry on processor levels in `docs/decisions.md` have it right.

### Three comments say a program that loads nothing links no loader

`src/rt/plugin.h:80`, documentation, a public header whose documented
behaviour differs from the code, severe, new. `src/rt/loaded.c:4` and
`src/rt/plugin.h:49` are the other places.

The DESIGN says "A program that loads nothing links no loader, no digest
and no reader of an index". `fill_injections` of `conf.c:813` calls
`anti_rt_plugin_provider`, and `anti_rt_conf_start` calls it in every
program from `start.c`. `src/antic/whole.c:881` says so itself: "the
runtime archive puts the loader in every program". `plugin.h:49` says the
pass writes `anti_rt_slots` "into a program that loads a library or injects
an interface", and `whole.c:885` writes it into every program.

### threads.h says a join of no job writes nothing

`src/rt/threads.h:34`, documentation, a public header whose documented
behaviour differs from the code, severe, new.

The comment of `anti_rt_join` reads "A null job carries no work, and `join`
of one writes nothing". `threads.c:350` writes `result_size` zero bytes
into `out`, and its own comment two lines above says "its result is zero".
The header comment is repeated at `threads.c:341`.

### `patch` says a misfit is found before any byte changes

`src/rt/patterns.c:695`, documentation, a file whose documented behaviour
differs from the code, severe, new. Read, not run.

The DESIGN says the walk runs twice, so "a with that does not fit" is
"found before any byte changes". The second pass searches `data` while it
writes into it at line 749. A group in a lookahead lies outside its match,
so a write changes what a later search finds. With `x(?=(y+))` over
`xyyyy`, `with` of `x` and offset 2, the first pass counts one place that
fits. The second writes `data[3]`, finds a new match whose group is one
byte, and returns `MISFIT` at line 745 with the data changed. The fix
records the spans in the first pass and writes from the record.

### Two DESIGN comments of object.c describe what was replaced

`src/rt/object.c:13`, documentation, a file whose documented behaviour
differs from the code, severe, new. `src/rt/object.c:710` is the second.

The first comment of the file says the default bodies "walk the field
list" and under `--no-reflect` "fall back to the class name and the
address". The DESIGN at line 303 says the runtime "keeps no walk of the
fields" for `equals` and `hash`, and no body uses an address. The DESIGN of
`anti_lang_Object_serialize` says "A union, an array, a bitfield" are
null. The `ANTI_TYPE_ARRAY` case at line 640 writes nested JSON arrays.

## Major

### macOS and Linux give file names as raw bytes where the layer promises UTF-8

`src/rt/platform_posix.c:348`, safety, rule 22 and one platform layer, a
result that differs between targets, major, new.

`platform.h:10` says "Every function below takes and gives text as UTF-8",
and line 233 says a listing gives each name "in UTF-8".
`anti_rt_directory_list` of Windows converts through `utf8_of`, which
writes U+FFFD for a unit that pairs with no other. The POSIX one passes
`entry->d_name` as the system gave it. `add_entry` of `fs.c:210` puts those
bytes into a `str` of the listing. A file name that is no UTF-8 on Linux
then breaks the rule of `docs/decisions.md:52`, the case M38 closed for
`Object.deserialize`. The arguments and the environment are repaired in
`utf8_list` of the same file. `anti_rt_getenv` at line 259 copies raw bytes
as well, and its two callers read a path and a level. The fix is the repair
of `utf8_list`, or a stated exception for a name that must open a file.

### An error keeps a message the runtime may overwrite

`src/std/anti/error.anti:29`, safety, rule 11 and a result that differs
between targets, major, new. The storage is `platform_windows.c:45` and
`platform_posix.c:54`.

`std.h:152` says the text of `anti_rt_errno_text` "stays valid on its
thread until the next call". `SystemError.from_errno` stores
`anti_rt_text_from_c` of it in `e.message`, and `text.c:15` wraps the
pointer without a copy. Windows writes every message into one
`static _Thread_local char text[128]`. The second `SystemError` of a thread
then changes the message of the first, which keeps its old length. POSIX
lets `strerror` overwrite its storage on a later call, and musl returns
constants, so Linux does not show it. `from_win32` at line 43 keeps the
buffer of `anti_rt_last_error_text` the same way. The fix copies the text
where the error is made.

### The search path of plugins cannot hold a Windows directory

`src/rt/plugin.c:995`, safety, a result that differs between targets,
major, new.

`discover` splits the `plugins` value with `strcspn(at, ":")`, and
`conf.c:609` joins the array of the file with `:`. `C:\plugins` becomes the
two directories `C` and `\plugins`. The entry at `docs/decisions.md:1483`
fixes the separator and does not speak of Windows, so the answer is
Eddie's. The separator of a list of paths belongs in the platform layer
beside `anti_rt_path_is_absolute`.

### An empty directory of the search path loads a file the digest never read

`src/rt/plugin.c:832`, safety, rule 14, major, new. The search rule of
`dlopen` is the system's and was not run.

`joined` writes no separator for an empty directory, so `--anti.plugins=:x`
gives the bare name of the library. `digest_file` reads that name in the
working directory. `dlopen` of a name without a slash searches the paths of
the system first. The bytes the digest covers are then not the bytes that
load. The fix refuses an empty directory or writes `./` before the name.

### A second load of a library gives a handle that two owners hold

`src/rt/plugin.c:446`, safety, rule 11, major, new.

`claim` returns the slot of a library that is open already, and
`anti_rt_plugin_load` returns it at line 692 without a count.
`plugin.h:126` says "The caller owns the handle". Two `Library` objects
then own one slot, and the first `unload` closes it for both. A handle is
the address of a slot, so the second names whatever library takes the slot
next. `claim` also tests for a free slot first, so the same load fails with
sixteen libraries open. The fix counts the loads of a slot or refuses the
second.

### `dup` gives NULL when memory runs out, and generated code does not look

`src/rt/object.c:1047`, safety, rule 4, major, new.
`src/rt/object.c:1145` is `anti_rt_copy_buffer`.

Both return NULL when `malloc` fails. `src/antic/lower_expr.c:2268` passes
the result of `dup` to the `copied` hook and gives it out as a `*T`.
`lower_owning.c:499` stores the buffer in the owning field, and a slice
keeps its length beside a NULL pointer. `alloc T { }` ends the program
through `anti_rt_out_of_memory`, and `anti_rt_snapshot_new` aborts.
`object.h:364` does not say that the copy may be NULL. The fix ends both
through `anti_rt_out_of_memory`.

### The ELF and DWARF readers have no test of a file cut short

`tests/unit/test_symbols.c:392`, safety, rule 15, major, still open in part
(M43 of the second audit, which the Mach-O tests closed).

The Mach-O readers are fed every prefix of a file at lines 766, 956 and
984. The ELF reader has `nobits_sections`, `line_overflow`,
`empty_entry_formats`, `long_leb` and `loaded_room`. No test cuts an ELF
file short, moves `e_shoff` or `e_shstrndx` outside it, or gives a symbol
table whose size is no multiple of a symbol. No test builds a line table of
DWARF 2 or 3, which `docs/notes/traces.md:92` says the reader takes. The
reviewer read the checks at each of these places and found them present.
The gap is the test.

### Windows lists a directory in two functions that already differ

`src/rt/platform_windows.c:455`, structure, duplicated logic that has
diverged, major, new. The second copy is `opendir` at line 1474.

`anti_rt_directory_list` and the `opendir` and `readdir` of the static C
runtime each build the pattern `path\*` and walk it with `_wfindfirst64`.
`opendir` takes a path that ends in `:` as it stands, and the other adds a
separator. `C:` is then the current directory of the drive in one and its
root in the other. `readdir` ends the walk with `ENAMETOOLONG` for a name
that does not fit, "since an empty name would stand for a file that is not
there". `anti_rt_directory_list` passes the empty name on. The fix is one
walk that both call.

### The configuration fills the injections, which puts the loader in every program

`src/rt/conf.c:781`, structure, a part doing work that belongs to another,
major, new.

The header of `conf.c` says "The runtime configuration". Lines 327 to 392
and 775 to 827 hold the lines of `[injections]`, the load of each provider
and the write of the program's slots. Most of `conf.h` is the record of an
injectable interface. This is the call that makes the three comments above
false. The fix is a file for the start of the injections, which `conf.c`
hands its lines to.

### std.h and rt.h hold what has no header, and the layer's interface stands in three

`src/rt/std.h:1`, structure, a file whose purpose cannot be said in one
sentence, major, new. `src/rt/rt.h:1` is the second.

`std.h` declares the functions of nine files and of both platform files,
and 33 files include it. `trace.h:9` and `regex.h:13` include all of it for `struct anti_text`,
so antic reads every declaration through `regex.h`. `anti_rt_fs_read` at
line 214 stands under "The C side of anti.fs" and serves `trace.c`,
`conf.c` and `plugin.c` alone. `rt.h` has no comment before its guard. It
holds the start of `init.c`, the option of the backtrace, a function of
the platform layer and four of `hash.c`, which has `hash.h`. The platform
files define `anti_rt_errno_text`, `anti_rt_last_error`,
`anti_rt_last_error_text`, `anti_rt_monotonic`, `anti_rt_wall` and
`anti_rt_sleep` from `std.h`, and `anti_rt_memory_kept` from `rt.h`.
`platform.h` says it is "what the rest of src/rt asks of the system" and
declares none of the seven. The fix is `struct anti_text` and the failure
routines in headers of their own, and the seven in `platform.h`.

### Layouts and numbers are written on both sides with no test between them

`src/rt/registry.h:12`, structure, a change in one part that silently
breaks another, major, new.

`rt_abi.h` and five unit tests pin most of the contract. These stand
outside it:

- `ANTI_CLASS_ARGS` and `ANTI_CLASS_REQUIRED` are the literals `1` and `2`
  at `src/antic/whole_tables.c:117`.
- `enum anti_value_kind` of `reflect.h:10` is `enum value_kind` at
  `whole_tables.c:191` and `ValueKind` of `src/std/anti/reflect.anti`.
- `enum anti_reflect_reason` of `reflect.h:38` is `BAD_INDEX` to
  `WRONG_COUNT` of `reflect.anti`.
- `struct object_lock` of `lock.c:18` is "a Mutex and two `int` fields"
  that the compiler lays out.
- `struct anti_cursor` and `struct growing` of `patterns.c:129` and `589`
  are `Cursor` and `Growing` of `regex.anti`.
- `EXPENSIVE`, `MISFIT` and `MISSING` of `patterns.c:148` and `684` are
  `-1`, `-2` and `-3` in `regex.anti`.
- `struct anti_raw_frame` and `struct anti_frame` of `trace.h:12` are
  `RawFrame` and `Frame` of `lang.anti`.

No file of `tests/unit/` names one of them. `Match` shows the remedy: the
test `match_layout` holds it. The fix is a row of `rt_abi.h` or a unit
test per pair.

### Fourteen files have no sentence of their own, and six say less than they hold

`src/rt/object.c:1`, documentation, a file without a header comment that
says its one purpose, major, new. The tool pass named `text.c` and `utf.c`.

No comment that says what the file is for stands in `object.c`, `text.c`,
`utf.c`, `symbols.c`, `hooks.c`, `cast.c`, `check.c`, `init.c`,
`license.c`, `io.c`, `assert.c`, `reflect.h`, `rt.h` and `signals.h`. Each opens with an include, a guard or the comment of one
function. The matching header carries the sentence for `utf.c`,
`symbols.c` and `hooks.c`. Six headers are narrower than their file:

- `patterns.c:1` names the literals and the messages, lines 1 to 117 of
  960.
- `regex.h:1` names the compile and also holds the template pieces and
  the layout of `Match`.
- `conf.c:1` and `conf.h:1` leave out the injections.
- `threads.c:1` names `parallel` and also holds `dispatch` and `join`.
- `start.c:1` leaves out the processor check and the option pass.

The closing section gives each sentence.

### Public functions without a comment of what they take, give and refuse

`src/rt/reflect.h:46`, documentation, a public function of a header
without a comment, major, new. The tool pass counted the pattern.

- `reflect.h:46` to `64`: eleven functions, none with a comment. What each
  gives for a NULL descriptor or an index out of range stands in
  `reflect.c` alone.
- `utf.h:14` to `17`: `anti_rt_utf8_repair`, `anti_rt_utf16_to_utf8` and
  `anti_rt_split_command_line`. The room `out` must hold stands in
  `utf.c`.
- `std.h:110` to `113`: `anti_rt_builder_append`, `anti_rt_builder_text`
  and `anti_rt_builder_clear`. `std.h:83`: `anti_rt_text_slice`.
- `digest.h:24`: `anti_rt_sha256_init` and `anti_rt_sha256_update`.
- `atomic.h:8` to `14` and `platform.h:169`, which the tool pass named.
- `conf.h:62`: `anti_rt_conf_start` leaves out that it loads the provider
  of every injection and ends the program when one fails.
- `plugin.h:131`: `anti_rt_plugin_load` leaves out what a second load
  gives.

### Two notes describe plugins as not built

`docs/notes/runtime-conf.md:30`, documentation, a notes file that describes
a design the code no longer has, major, new.
`docs/notes/injection.md:104` is the second.

- `runtime-conf.md:30`: "`inject` carries an interface and a path and ends
  the program, because no program has an injectable interface yet".
  `injection_named` keeps the line, and `fill_injections` of `conf.c:781`
  loads its library.
- `runtime-conf.md:69`: "Nothing reads `plugins` yet". `conf.c:783` reads
  it and hands it to `anti_rt_plugin_provider`.
- `runtime-conf.md:10` lists five keys. The table of `conf.c:39` has six,
  with `hash_seed`, which line 63 leaves out of the keys the configuration
  applies itself.
- `runtime-conf.md:34` leaves `fill_injections` out of
  `anti_rt_conf_start`, and line 57 names `anti_rt_fs_open` where
  `conf.c:733` calls `anti_rt_fs_read`.
- `injection.md:104`: a line that passes both checks "is a startup error
  while plugins are not built". It is kept and its provider loaded.
- `injection.md:108` gives the line of `--anti.inspect` without the
  version and the used slots that `conf.c:862` prints.

### setup.md places the runtime where the build no longer puts it

`docs/notes/setup.md:7`, documentation, a notes file that describes a
design the code no longer has, major, new.

The note says the build compiles `anti_rt` "for the host into
`build/host/runtime/lib/<target>/libanti_rt.a`" and "with hidden
visibility". `CMakeLists.txt:359` writes `lib/<target>/<level>/`, and
`anti_rt.lib` on Windows, for every target with a sysroot. No rule of the
runtime passes `-fvisibility`. `docs/notes/plugins.md:93` says the runtime
"is compiled without hidden visibility", so the two notes disagree. The
copy of `src/rt/LICENSE` to `licenses/anti_rt.txt` holds.

## Minor

### The walk of Windows stops at 64 frames and the other at the room it was given

`src/rt/platform_windows.c:591`, safety, rule 22 and one platform layer,
minor, new.

`anti_rt_trace_walk` of Windows lowers `room` to 64 and gives 0 for a
`skip` above 1000. The POSIX one has neither bound, and `platform.h:265`
names neither. The 64 repeats `TRACE_DEPTH` of
`src/std/anti/lang.anti:30`. The fix is the bound in `platform.h`, or in
neither file.

### Resources released before each return

Safety, rule 12, minor, new places of a pattern the first audit reported.

`anti_rt_monitor_new` at `platform_posix.c:92` frees and destroys at three
returns. `anti_rt_getenv` at `platform_windows.c:271` frees at five.
`discover_in` at `plugin.c:968`, `974` and `983` frees the document three
times.

### The start of a Windows program aborts without a line

`src/rt/platform_windows.c:1363`, safety, rule 13, minor, new.

`anti_rt_windows_start` calls `abort()` when an initialiser fails or the
C runtime gives no arguments. The state is reachable, and the failure
routine of `assert.c` is not called.

### Functions, a field and includes that nothing uses

Safety, rule 26, minor, new.

- `anti_rt_destroy_elements` and `anti_rt_copy_elements` at
  `object.c:1109` and `1120` have no caller in `src/`, `tests/` or
  `tools/`. They hold the cast-align warnings of the tool pass at lines
  1132 and 1133.
- `anti_rt_ready` at `init.c:18` has no caller in `src/`. One test program
  declares it.
- `destruct` of `struct anti_descriptor` at `object.h:177` is written by
  `src/antic/lower_desc.c:1192` and read by nothing.
- `#include <stdio.h>` at `threads.c:9`, `cpu.c:15` and `start.c:6` names
  nothing the file uses.
- `plugin.c:977` tests a length that two buffers of one size rule out.

### Blocks written twice

Safety, rule 26, with rule 5 for the third item, minor, new.

- `anti_rt_regex_literal` and `anti_rt_regex_literal_bytes` at
  `patterns.c:35` and `55` differ in one call and one word.
- `struct anti_str` at `start.c:16` repeats `struct anti_text` of `std.h`,
  which the file includes.
- `registry.c:173` and `195` double a list by hand with an unchecked
  product, where `grow.h` is "one rule for every block that grows".
- `hash_run` at `object.c:181` is a second FNV-1a, and line 210 writes
  `ANTI_HASH_START` of `hash.h` in decimal.
- "out of memory at program start" with its check stands eight times in
  `conf.c` and twice in `start.c`, beside `allocate` of `start.c:67`.

### Lengths and counts narrowed without a check

Safety, rules 3 and 16, minor, new.

`(int)length` feeds `%.*s` from an `int64_t` at `assert.c:66`,
`patterns.c:47` and `67`, `cast.c:10` and `22`, and `check.c:12`.
`worker_count` at `threads.c:92` and `98` narrows two `int64_t` values to
`int`. `license_stub.c:11` gives a NULL pointer with length 0, which
`plugin.c:673` passes to `%.*s`. `license.c` gives `""` there.
`object.c:136`, `141` and `146` convert a value out of range to a signed
type, as `hash.c:21` and `48` do.

### `serialize` is called through a type its function does not have

`src/rt/object.c:757`, safety, rule 3, minor, a new place of a pattern the
first audit reported.

`serialize_object` calls the entry as `void (*)(const void *, void *)`. The
entry holds `anti_lang_Object_serialize`, declared with
`struct anti_object *`. `object.h:265` says "The caller casts it to the
type the entry declares", and the other four casts of the file do.

### A text is kept in an array of one byte

`src/rt/trace.c:50`, safety, rule 1, minor, new.

`struct kept` ends in `char bytes[1]`, and line 69 copies `n` bytes into it
with a NUL at `bytes[n]`. C11 writes that as `char bytes[]`, and an index past a declared bound is
undefined.

### An owning field is cleared through a pointer that may be unaligned

`src/rt/registry.c:237`, safety, rule 8, minor, new.

`owned_by` keeps the address of a field as `void **` at line 703, and
`give_back` stores through it. Line 707 writes the same field with
`memcpy`, as the rest of the walk does, since a field may stand at any
offset. The fix keeps the address as bytes and clears it with `memcpy`.

### Two readers take what they should refuse

Safety, rule 14, minor, new.

`registry.c:734` skips any JSON value that is no object in a class value
field and keeps the default. The comment says the serializer wrote `null`
there. `conf.c:424` accepts `--anti.inspect=x` and `--anti.help=x` and
drops the value. `conf.h:55` says a value the runtime does not know ends
the program.

### Two globals without a guard or a reason

Safety, rules 23 and 24, minor, new.

`running_name` and `running_length` at `assert.c:49` are written by
`anti_rt_test_running` as two plain stores, and a failed assertion on a
worker reads them. `anti_rt_give_nothing` at `object.c:1054` is writable
data that nothing writes.

### Comments that leave out who frees

Safety, rule 11, minor, new places of a pattern both earlier audits
reported.

`registry.h:47` does not name `destroy(p, from)` or `delete(p, from)` for
`anti_lang_Object_deserialize`. `object.h:368` and `380` name no release
for `anti_rt_snapshot_new` and `anti_rt_snapshot_dup`. `plugin.h:165` does
not say who owns the object of `anti_rt_plugin_provider`. The static
`copy`, `resolve`, `join_elements` and `environment_path` of `conf.c` and
`stubs_of` of `plugin.c` return memory without a word.

### A signature travels without its length

`src/rt/call.c:37`, safety, rule 10, minor, new.

`anti_rt_reflect_call` compares two signatures with `strcmp`.
`struct anti_function` and `struct anti_trampoline` carry a length for the
name beside it and none for the signature.

### Functions past a threshold

Safety, rule 18, minor, still open.

`data/thresholds.txt` lists eleven functions of this area. `read_value` at
`registry.c:624` and `put_value` at `object.c:529` are each one `switch`
over the type ids, and the size follows the shape. `anti_rt_parallel` has
nine parameters, and the seven parameter lists of `patterns.c` each match
an `extern fn` of `src/std/anti/regex.anti`. `anti_rt_split_command_line`
nests five deep along the quoting rules. None would read better split.

### What is left of Microsoft's compiler and headers

`src/rt/atomic.c:98`, structure, a dead branch of what was replaced, minor,
new.

Lines 98 to 260 of `atomic.c` are the `Interlocked` form for a compiler
without the builtins. `docs/notes/hosts-and-harness.md:88` says "No host
here builds with `cl`", so the branch has never compiled.
`platform.h:32` names the architecture for MSVC as well. The DESIGN at
`platform_windows.c:1164` says the host build of a Windows machine
compiles the file "against the headers of Microsoft". `docs/vm-setup.md:201`
has that suite run "against the Windows sysroots of mingw-w64 and no Build
Tools". Whether a reader's build with the system compiler keeps the branch
is Eddie's decision.

### Names that differ inside one module

Structure, rule 25, minor, new.

Ten include guards read `ANTI_<NAME>_H` and the others
`ANTI_RT_<NAME>_H`: `ANTI_OBJECT_H` beside `ANTI_RT_REGISTRY_H`. `object.h:211` names the entry
`ANTI_ENTRY_DROP`, and lines 165 and 202 call it "the destruct entry".
`object.h:349` and `357` declare two `anti_rt_json_` functions beside
`json.h`, and lines 370 to 384 the five functions of `snapshot.c`.

### Two bounds of 32 that end a lookup without a word

`src/rt/trace.c:142`, documentation, a DESIGN comment that gives no reason,
minor, new. `src/rt/trace.c:203` is the second.

`LOADED_MAX` and `KNOWN_MAX` are 32, and the DESIGN at line 131 says "a few
of each". The file past the bound is never read, so its frames lose their
line on macOS and their function on Linux. A dev build on macOS has one
object per module.

### symbols.h does not say that a line may come without a file

`src/rt/symbols.h:65`, documentation, a public function whose comment
leaves out a result, minor, new.

`run_line_program` at `symbols.c:533` returns true with `out->file` NULL
when the index names no file. `anti_rt_elf_line` and
`anti_rt_macho_object_line` pass that on, and each caller in `trace.c`
tests `found.file != NULL`. `tests/unit/symbols_probe.c:70` does not.
`anti_rt_macho_object_line` needs the `_` of the symbol, which line 117
does not say.

### Comments that name what moved or is gone

Documentation, a stale name in a comment, minor, new.

- `platform.h:4`: a host `#if` "stands only in this header, in
  platform_posix.c and in platform_windows.c". `platform_entry.c` and
  `platform_stdio.c` each hold one.
- `platform_entry.c:9` says it holds "the one #if on a system outside the
  files named platform_<system>.c". `platform_stdio.c:12` holds another.
- `object.h:200`: "root_names in src/antic/lower_desc.c". It stands in
  `src/antic/sema_class.c`.
- `symbols.c:773`: "A function of Anti has no type in the symbol table,
  since its assembly writes none". antic writes no assembly, and a
  reviewer found `symbols.main` typed as a function in a test binary.
- `trace.c:361`: "A name of C++ is demangled". The code calls
  `anti_rt_coff_demangle` on the COFF form of an Anti name.
- `trace.h:27`: "unless the command line named --anti.backtrace". The key
  of the file and `rt.configure` set it as well.
- `plugin.c:831`: "with the separator of the host". Line 836 writes `/` on
  every host.
- `conf.c:342`: "rt.configure may add to them later". `read_key` refuses
  every line of `[injections]` there.

### Sentences of the notes that no longer hold

Documentation, a stale name or sentence in a note, minor, new.
`docs/audit/structure.md` reported `hooks.md:10`.

- `threads.md:31`: the results are memory "that nothing frees".
  `threads.c:215` and `fs.c:174` say the program frees them. The note
  describes `parallel` alone, and `threads.c` holds `dispatch` and `join`.
- `locking.md:15` names `same_mutex`. The function is `sema_same_place` of
  `src/antic/sema.c:107`.
- `plugins.md:49` names `write_plugin_index`, which is
  `driver_write_plugin_index` of `driver_index.c`. Line 87 names
  `emit_names`, which is `llvm_emit_names`.
- `plugins.md:114` says every interface descriptor "must lie in the host's
  image". `plugin.c:407` refuses one in the library's own image and passes
  one in any third.
- `plugins.md:150` says two libraries for one interface are an error. That
  holds inside one index. Across directories the first wins at
  `plugin.c:997`.
- `traces.md:29` gives the command line alone as what wins over the
  default. Line 57 says a frame "still names a library after it is
  unloaded", which holds for the path and not for the build id.
- `traces.md:61` says the answer is kept "per image", and line 79 that an
  object "is read whole once". Both stop at 32 entries.
- `fmt.md` describes `src/anti/fmt.c` and nothing of the runtime. Line 85
  says a tab counts what `anti html` and `anti tex` render, and neither
  command exists.

`trace-handlers.md`, `strings.md` and the rest of `hooks.md` and
`locking.md` agree with the files they name.

## Outside the rules

Four things no rule names, for Eddie to judge.

- `on_raise` of `platform_posix.c:627` calls `write` in a signal handler
  and does not put `errno` back. A write that fails changes the `errno` of
  the code it interrupted.
- `anti_rt_reflect_get` and `anti_rt_reflect_set` at `reflect.c:157` and
  `203` take the descriptor from the caller and never hold it against the
  object. `reflect.set(a, reflect.describe(b), ...)` writes by the offsets
  of another class.
- `inspect` of `conf.c:870` prints "loaded plugins:" and lists the lines of
  `[injections]`, not the libraries that are open. A provider the build
  named is loaded and not listed.
- `anti_rt_on_signal` of `signal.c:43` returns nothing, so a route that
  found no pipe or no thread leaves the signal uncaught without a word.

## The sentence of each file

One sentence per file as read from the code. The last column says what the
file's own header gives. `json.h`, `toml.h` and `regex.h` are here, and
their `.c` files belong to the library-file area.

| File | Purpose | Header |
|---|---|---|
| `assert.c` | The failure routine: one line to standard error, then exit, abort or return, and the failed assertion of a test. | Says nothing of the file. |
| `atomic.c` | The atomic operations at a width that comes with the address, by the builtins of the compiler. | Says it in a DESIGN. |
| `atomic.h` | The calls of `atomic.c`. | Says it. |
| `backtrace.c` | Whether `fail` captures frames, alone so a program that never fails links no default. | Says it. |
| `call.c` | `reflect.call` over the trampolines of a program. | Says it. |
| `cast.c` | The traps of a failed checked cast and of an object whose table is not set. | Says nothing. |
| `check.c` | The traps of a failed dev-mode check and of a collection changed during a walk. | Says nothing. |
| `conf.c` | The keys, layers, options and file of the runtime configuration, and the start of the injections. | Says the first half. |
| `conf.h` | The record of an injectable interface and the five calls of the configuration. | Says the second half. |
| `conf_include.h` | The bounds and the messages of the include walk, shared with `anti symbols`. | Says it. |
| `cpu.c` | The table of processor levels and the check of the machine against the level of this runtime. | Says something else. |
| `cpu_level.h` | The processor levels, shared by antic and the runtime. | Says it. |
| `digest.c` | SHA-256 for the runtime, antic and anti. | Says it. |
| `digest.h` | The calls of `digest.c`, in pieces and over a stream. | Says it. |
| `errno.c` | The value of `errno` for an `extern fn`. | Says it in a DESIGN. |
| `f16.c` | The two `f16` conversions a program calls where the processor has no instruction. | Says it in a DESIGN. |
| `f16.h` | The conversions between `f16` and `f32`, shared with antic. | Says it. |
| `fs.c` | The C side of `anti.fs`: open, size, read whole, list, remove and rename. | Says it. |
| `grow.h` | The one growth rule of a block of the C library. | Says it. |
| `hash.c` | The hash of bytes and the seed of the hashing collections. | Says it. |
| `hash.h` | The constants and the last step of a hash, shared with antic. | Says it. |
| `hooks.c` | What a hook site does: the installed handler, then the object's own hook. | Says nothing. `hooks.h` says it. |
| `hooks.h` | The calls of the nine hooks and of `Trace.install`. | Says it. |
| `init.c` | The start flag of the runtime and the option of the backtrace. | Says nothing. |
| `io.c` | The write and the exit of `anti.io` through the C streams. | Says nothing of the file. |
| `json.h` | A scanner of JSON text at the level of tokens and positions. | Says it. |
| `license.c` | The licence text and the runtime version, read from the notice of the program. | Says nothing. |
| `license.h` | The markers of the notice and its build id line, shared with antic and anti. | Says it. |
| `license_stub.c` | The empty licence text and version of a bundled static library. | Says it after the include. |
| `loaded.c` | The slots of the open libraries, their lock and the count of live objects of each. | Says it, with a false claim. |
| `lock.c` | The Mutex, the hidden lock of a synchronized object and the lock orders of a dev build. | Says it. |
| `mem.c` | Aligned memory for `anti.mem`, the out-of-memory end and `anti_rt_grow`. | Says it. |
| `object.c` | The C behind `anti.lang.Object`: the root's functions, the walk that writes JSON and text, and delete, destroy, dup and give. | Says nothing, and its first DESIGN is stale. |
| `object.h` | The layout of descriptors, field records and tables, and the calls of `object.c` and `snapshot.c`. | Says the first two. |
| `patterns.c` | The run-time side of `anti.regex`: literals, messages, searches, groups, replace, patch and stops. | Says the first two. |
| `platform.h` | What the runtime asks of the system, apart from the clocks, the error texts and the kept block. | Says all of it. |
| `platform_entry.c` | The entry point of a Windows program, in an object only the link of a program pulls in. | Says it. |
| `platform_posix.c` | The platform layer on macOS and Linux. | Says it. |
| `platform_stdio.c` | The printf and scanf families and `atexit` of a Windows target, weak and never bitcode. | Says it. |
| `platform_windows.c` | The platform layer on Windows, and the static part of the C runtime. | Says the first half. |
| `plugin.c` | Loading a library that provides interfaces: the checks, the objects and discovery through the index. | Says it, and claims more checks than it makes. |
| `plugin.h` | The table a plugin carries, the slots of the host and the calls of the loader. | Says it. |
| `plugin_index.h` | The file name of the index of plugins, shared with antic and anti. | Says it. |
| `reflect.c` | The walks of a descriptor behind `anti.reflect`, with `get` and `set` of a field. | Says it. |
| `reflect.h` | The layout of `Value`, the reasons, the calls of `reflect.c` and the trampoline table. | Says nothing. |
| `regex.h` | What `regex.c` and `patterns.c` share with antic: compile, groups, template pieces and the layout of `Match`. | Says the compile alone. |
| `registry.c` | `reflect.new` and `Object.deserialize` over the registry of classes. | Says it. |
| `registry.h` | The records of the registry and its two calls. | Says it. |
| `rt.h` | Declarations of the runtime that have no header of their own. | Says nothing. |
| `signal.c` | The functions a program registers for signals, and their delivery. | Says it. |
| `signals.h` | The two console signals and the two calls of `signal.c`. | Says nothing of the file. |
| `snapshot.c` | The heap snapshots of `snapshot fn` and their count. | Says it. |
| `start.c` | The C `main`: processor check, init, arguments, the option pass, the program and its status. | Says the arguments, the call and the status. |
| `std.h` | The C functions behind eight modules, `struct anti_text` and the failure routines. | Names six modules. |
| `symbols.c` | Bounds-checked readers of ELF and Mach-O symbol tables, the debug map and DWARF line tables. | Says nothing. `symbols.h` says it. |
| `symbols.h` | The calls of `symbols.c`. | Says it. |
| `sync.c` | The channels of `chan T` and `select`. | Says it. |
| `sync.h` | The calls of locking and channels that generated code makes. | Says it. |
| `text.c` | The C behind `anti.text`: text helpers, the buffer of a Builder, floats to text and text to floats. | Says nothing. |
| `threads.c` | The worker pool behind `parallel`, `dispatch` and `join`. | Says `parallel`. |
| `threads.h` | The calls of `parallel`, `dispatch` and `join`. | Says it. |
| `toml.h` | A read of the TOML subset the library, the logger and the configuration share. | Says it. |
| `trace.c` | The module and the build id of a frame, the text of a trace and the choice of a symbol reader. | Says it, with a false claim. |
| `trace.h` | The layouts and the calls behind `anti.lang.StackTrace`. | Says it. |
| `utf.c` | UTF-8 and UTF-16 conversion and the split of a Windows command line. | Says nothing. `utf.h` says it. |
| `utf.h` | The calls of `utf.c`. | Says it. |
