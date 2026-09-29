# Cross-cutting audit

The C audit across `src/rt/`, `src/antic/` and `src/anti/`, at `fddd434`,
against `docs/c-guidelines.md`. `src/native/` holds only
`CMakeLists.txt`, so it has no C to audit. This step looks at what no
single area shows. It covers the include graph of all 70 headers,
every non-static function against its callers in `src/`, `tests/` and
`tools/`, the crossings between the runtime and the host programs, and
every platform `#if`. It also covers mutable state at file scope and in
function-level statics, the prefixes of exported names, and helpers
written out in more than one file. Scripts under `build/drive/cc/`
found the candidates, and every finding below was read in the code. The
names that generated code calls, found as strings in `lower.c` and
`test.c` and as `extern fn` in `src/std/`, count as callers. Findings
of `docs/audit/tool-pass.md` are not repeated. Where a finding here
extends one of them, it says so.

Two checks found nothing. No two headers include each other, and every
header has an include guard. `src/rt/` includes no header of
`src/antic/`, `src/anti/` or `tools/`. The crossings in the other
direction are the two shared headers `src/rt/f16.h` and
`src/rt/cpu_level.h`, and the three runtime sources that `anti`
compiles. `DESIGN` comments in those headers and in `CMakeLists.txt`
settle all of them.

## Counts

| Severity | Findings |
|---|---|
| Severe | 1 |
| Major | 1 |
| Minor | 11 |

| Rule | Severity | Findings |
|---|---|---|
| 23 | Severe | 1 |
| 22 | Major | 1 |
| 26 | Minor | 4 |
| 25 | Minor | 2 |
| 1 | Minor | 1 |
| 13 | Minor | 1 |
| 20 | Minor | 1 |
| 21 | Minor | 1 |
| 23 | Minor | 1 |

## Severe

### The table of loaded libraries is shared between threads without a guard

`src/rt/loaded.c:26`, rule 23, severe.

`static struct anti_plugin loaded[ANTI_PLUGIN_MAX];` is written by
`anti_rt_plugin_load` and `anti_rt_plugin_unload` in `plugin.c`. It is
read by `owner` at `loaded.c:91`, which `anti_rt_hook` calls on every
`created` and `destroyed` hook, on whatever thread made the object.
Only `open_count` and `live` are atomic. `used`, `base`, `table` and
the rest are plain fields, and no lock covers them.

Once one library is open, a worker that creates an object reads
`loaded[i].used` and `loaded[i].base`. Meanwhile the main thread may
fill a slot in `plugin.c:490` to `496`, or clear one with
`memset(p, 0, sizeof *p)` at `plugin.c:564`. That is a data race, and
so undefined behaviour. Two threads that call `plugin.load` at once can
also claim the same free slot at `plugin.c:417`. Unload checks `live`
at `plugin.c:556` and closes the library after it. A `created` hook on
another thread can raise `live` between the check and the close, so
the check does not keep live code mapped.

`static char message[512];` at `plugin.c:29`, and the static
`text[64]` of `open_message` at `plugin.c:88`, are one buffer for every
thread. One thread's `fail` overwrites what another is about to read
through `anti_rt_plugin_message`. No comment says why either exists or
which thread may use it. `docs/notes/plugins.md` says nothing of
threads.

The same pattern stands in `src/rt/signal.c:160` to `162`.
`handlers[]` and `started` are plain. `anti_rt_on_signal` writes
`handlers[sig]` while the reader thread at `signal.c:66`, or the
console handler thread of Windows at `signal.c:97`, reads it. Two
threads that register at once both see `started == 0` and each start
a reader. The window is narrow, because it needs a second
registration of the same signal while that signal arrives. It is still
a race a valid program can reach.

The fix is one lock over the slot table, held by load, unload and
`owner`, with the message kept per thread or returned with the result.
In `signal.c` it is an atomic store and load of each handler, and a
once-guard for the reader.

## Major

### The runtime names no platform layer, and 15 of its 35 files branch on the platform

`src/rt/conf.c:359`, rule 22, major.

Rule 22 allows a platform `#if` only in the files of the platform
layer. No document, comment or file list says which files those are.
`src/rt/platform.c` holds one function. The branches stand in 15 of
the 35 `.c` files of `src/rt/`: `trace.c` (12), `fs.c` (12),
`threads.c` (10), `cpu.c` (9), `time.c` (6), `signal.c` (5),
`plugin.c` (5), `mem.c` (5), `sync.c` (4), `start.c` (4),
`loaded.c` (4), `errno.c` (4), `conf.c` (4), `platform.c` and
`atomic.c`. For most of them the file is itself the wrapper of one
system service, and naming them as the layer would settle the rule.

`conf.c` is not such a file. It reads the runtime configuration and
carries four branches of its own. `#include <windows.h>` stands at
line 20. The path rules of Windows are at 359 and 376. The environment
is read at 591 with `GetEnvironmentVariableA` in place of `getenv`. The
file reader and the path resolver of a portable module then depend on
the target.

The host programs show the same mix in files that are otherwise
portable: `src/antic/driver.c:504` and `524`, `src/anti/bind.c:97` and
`src/antic/userdirs.c:134`. The host has its own platform files,
`selfpath.c`, `process.c`, `target.c` and `applesdk.c` of `src/antic/`,
and `files.c` and `sdk.c` of `src/anti/`. The branches above belong
there.

The fix starts with a list of the platform files, in
`docs/c-guidelines.md` or a `DESIGN` comment. The branches of `conf.c`
and of the four host files then move behind functions of that layer. The path helpers of
the minor finding on path rules would take most of them.

## Minor

### Code that the runtime and the host both need is written twice

`src/antic/sha256.c:1`, rule 26, minor.

- SHA-256. `src/antic/sha256.c` and `src/rt/digest.c` are the same 150
  lines. A diff after renaming `anti_rt_sha256` to `sha256` leaves the
  header comment, the return type of the file digest, `bool` against
  `int`, and the call that opens the file.
- UTF-8. `append_utf8` and `utf8_length` of `src/antic/lexer.c:310`
  and `332` encode and validate what `put_scalar` and
  `sequence_length` of `src/rt/utf.c:3` and `29` do. `src/rt/json.c:120`
  to `131` writes a third encoder inline. `utf.h` says its functions
  use nothing of the platform so unit tests run them on any host.
- The `__TEXT` walk of a Mach-O header. `image_slide` at
  `src/rt/trace.c:226` and `macho_text` at `src/anti/syms.c:997` walk
  the load commands for the same segment. `anti` already links
  `src/rt/symbols.c`, which reads Mach-O headers.
- The names of the processor levels. `src/rt/cpu.c:79` and
  `src/antic/cpu.c:21` spell each name, and `tests/unit/test_cpu.c:101`
  compares two of the six.

`src/rt/f16.h` and `src/rt/cpu_level.h` already hold one definition
that both sides read, and `anti` compiles three runtime files.
Either form would serve each item above.

### Path helpers written per file, with different rules

`src/anti/syms.c:135`, rule 26, minor.

"Is this path absolute" is written three times. `path_is_absolute` of
`src/antic/selfpath.c:119` and `163` follows the host. `is_absolute` of
`src/anti/syms.c:135` takes `/`, `\` and any `X:` as absolute on every
host, so a drive-relative `C:name` counts as absolute. `is_absolute` of
`src/rt/conf.c:357` takes `\` and `X:` on Windows only. "The name after
the last separator" is `base_name` at `src/anti/bind.c:94`, which honours
`\` on Windows only. The one at `src/anti/syms.c:109` honours it on
every host. "The directory of a path" is `directory_of` at
`syms.c:121`, the Unix `absolute_path` at `selfpath.c:179`, and
`resolve` at `conf.c:370`. `anti` links `selfpath.c`, so both files of
`src/anti/` can call it. The runtime copy needs a home of its own on
its side of the boundary.

### The host programs repeat their file writer and their out-of-memory exit

`src/anti/doc.c:81`, rule 26, minor. This extends the tool pass
finding on the twelve copies of `read_file`.

`static bool write_file` stands in seven files: `src/antic/driver.c:132`,
`src/anti/bind.c:15`, `check.c:76`, `doc.c:81`, `fmt.c:1272`,
`repo.c:49` and `test.c:73`. The copies do not agree. Those of
`driver.c` and `fmt.c` fail when `fclose` fails. The other five
ignore the result of `fclose`. A write that fails when the buffer
is flushed, a full disk for one, then reports success for a
truncated file. In `repo.c` that file is a library in the cache.

`anti` writes its out-of-memory exit ten times, as `die_out_of_memory`
in `deps.c:32`, `build.c:45`, `test.c:50`, `manifest.c:17`,
`symmap.c:33` and `syms.c:41`, and as `out_of_memory` in `check.c:53`,
`fmt.c:33`, `doc.c:75` and `units.c:21`. `antic` writes
`fputs("antic: out of memory\n", stderr);` and `exit(70)` 76 times in 25
files, the helpers below included. `allocate`, a checked `calloc`, is written five
times, in `coff.c:142`, `layout.c:13`, `optimize.c:22`, `whole.c:47`
and `regalloc.c:20`. `src/anti/files.c` and `src/antic/text.c` are
the places one copy of each would stand.

### Two functions that nothing calls

`src/antic/select.c:236`, rule 26, minor.

`select_fail` is declared in `select.h:238` and defined here, and no
file of `src/`, `tests/` or `tools/` calls it. `anti_cpu_built` at
`src/rt/cpu.c:307` is declared in `cpu_level.h:54` and is called
nowhere either. `anti_cpu_check` in the same file reads
`ANTI_CPU_LEVEL_ID` directly. Both can go.

### Exported names of the runtime outside `anti_rt_`

`src/rt/utf.h:11`, rule 25, minor.

The runtime links into every program, so its exported names share the
user's namespace. Five families do not start with `anti_rt_`, and no
layer documents a prefix of its own:

- `anti_cpu_level`, `anti_cpu_level_name`, `anti_cpu_level_message`,
  `anti_cpu_missing`, `anti_cpu_built` and `anti_cpu_check` of `cpu.c`.
- `anti_elf_*`, `anti_macho_*` and `anti_coff_demangle` of
  `symbols.c`.
- `anti_utf8_repair`, `anti_utf8_encode`, `anti_utf8_decode`,
  `anti_utf16_to_utf8` and `anti_split_command_line` of `utf.c`.

`src/rt/signal.h:8` and `9` define `SIGINT_SIGNAL` and
`SIGBREAK_SIGNAL` without the `ANTI_` that every other runtime macro
carries. The names `anti_lang_Object_*` follow the mangling of the
root class, which `CLAUDE.md` records, and are not part of this
finding. The fix is either the `anti_rt_` prefix, or one line per
layer in `docs/c-guidelines.md` that names its prefix.

### Host headers whose names do not share the module's prefix

`src/anti/files.h:10`, rule 25, minor.

- `src/anti/files.h`: `make_dirs`, `remove_tree`, `copy_file`,
  `copy_program`, `path_exists`, `list_tree`, `list_dir` and
  `file_list_free` share no prefix.
- `src/antic/selfpath.h:11` to `24`: `self_directory`,
  `directory_exists`, `path_is_absolute` and `absolute_path`.
- `src/antic/target.h:82` to `101`: `object_format_name`,
  `convention_name`, `mangle`, `c_symbol` and `block_label` beside
  `target_name` and `target_info`.
- `src/antic/linker.h:108` and `114`: `archive_command` and
  `relocatable_command` beside `link_*`. `src/antic/userdirs.h:47`:
  `runtime_archive` beside `user_dir_*`.
- Functions declared in the header of another module.
  `src/antic/select.h:195` to `197` declares `mach_vreg`, `mach_preg`
  and `mach_imm` of the `mach` prefix. `select.h:174` and `175` declare
  `target_desc_x86_64` and `target_desc_arm64`, which `x86_64.c` and
  `arm64.c` define. `src/antic/sema.h:197` declares `ast_dump_typed`,
  which `ast_dump.c` defines.

### Compiler extensions written out in eight files

`src/antic/text.h:17`, rule 1, minor.

`__attribute__((format(printf, n, m)))` behind
`#if defined(__GNUC__) || defined(__clang__)` is written ten times:
`src/antic/text.h:17`, `diagnostic.h:30`, `36` and `44`,
`layout.c:25`, `sema.c:85`, `select.c:218`, `ir_verify.c:19`,
`antl.c:923` and `src/anti/bindmodel.h:142`. Rule 1 keeps an extension
to a file that exists to hold one, and none of these does. The four
functions the tool pass names under `-Wmissing-format-attribute` carry
no attribute, so the use is also uneven. One macro in one header per
side would meet the rule and cover all fourteen.

### The runtime has no one failure routine

`src/rt/sync.c:78`, rule 13, minor.

Rule 13 and the entry on `assert` in `docs/decisions.md` speak of the
runtime's failure routine. The code has seven, each of which writes to
standard error and ends the process on its own:
`anti_rt_assert_failed` at `assert.c:23`, `anti_rt_check_failed` at
`check.c:10`, `anti_rt_cast_failed` and `anti_rt_table_unset` at
`cast.c:9` and `23`, `fatal` at `sync.c:78`, the stub at
`plugin.c:297`, and `anti_cpu_check` at `cpu.c:312`. `startup_error`
of `conf.c:76` and the pool of `threads.c:304`, `358` and `366` add
more. The prefixes differ. `sync.c` and `cpu.c` write `anti: `.
`cast.c` writes `cast failed: ` with no prefix. `cpu.c` alone does not flush
standard output first. One routine that takes the text would
give one form and one place to hook `on_fatal`.

### Non-static functions used in one file only

`src/rt/digest.c:79`, rule 20, minor.

Each of these is declared in a header and defined without `static`.
No other file of `src/`, `tests/` or `tools/`, and no generated code,
calls it:

- `src/rt/digest.c:79`, `86` and `108`: `anti_rt_sha256_init`,
  `anti_rt_sha256_update` and `anti_rt_sha256_hex`. Only
  `anti_rt_sha256_file` is used outside the file.
- `src/rt/registry.c:56`: `anti_rt_registry_find`.
- `src/antic/ir.c:619`: `ir_block_op`.
- `src/antic/ir_print.c:67` and `91`: `ir_vtype_print` and
  `ir_sym_print`.
- `src/antic/select.c:15`, `76`, `241` and `270`: `mach_vreg`,
  `select_part_register`, `select_refuse` and `select_is_overflow`.
- `src/anti/bindtype.c:89`: `bind_known_name`.
- `src/anti/jsontree.c:285`: `json_string`.
- `src/anti/repo.c:66`: `repo_cache_dir`.

`deps_satisfies`, `repo_url_allowed`, `layout_fold`, `user_dir_of`,
`whole_build`, `whole_free` and `whole_entries` are also called from
one file of `src/` alone, but the unit tests call them, so they stay.

### Headers that include too much or lean on what another includes

`src/antic/cpu.h:9`, rule 21, minor.

- `src/antic/cpu.h:9` includes `../rt/cpu_level.h`, and nothing in the
  header uses it. Only `cpu.c` reads the `ANTI_CPU_*` ids. Every file
  that includes `cpu.h`, in `src/antic/` and `src/anti/`, then sees
  six runtime prototypes that `antic` does not link. The comment above
  the include gives the reason for one definition, which an include in
  `cpu.c` keeps.
- `src/rt/hooks.h:9` includes `std.h`, and nothing in it uses a name of
  `std.h`.
- `src/anti/build.h:5` and `src/anti/repo.h:5` include `<stddef.h>` and
  use nothing of it. In `build.h` `NULL` stands in comments alone.
- `src/antic/driver.h`, `src/antic/sema.h` and `src/rt/object.h` use
  `size_t` without `<stddef.h>`. `src/antic/select.h` uses `uint64_t`
  without `<stdint.h>`. Each compiles because another header brings it.

### Mutable state without the comment the rule asks for

`src/rt/conf.c:69`, rule 23, minor.

- `src/rt/conf.c:35`, `69` to `72`, `209` and `210`. `keys`,
  `conf_path`, `inspect_asked`, `help_asked`, `file_read`,
  `injections` and `injection_count` are written during start and by
  `anti_rt_conf_configure` at line 745. `anti_rt_conf_get` reads them
  afterwards from any thread. The comments say what each holds. None
  says that nothing writes them once threads run, which is what makes
  them safe. `src/std/anti/runtime.anti` asks that `configure` stand
  first in `main`, but nothing in the runtime holds a call to that.
- `src/rt/loaded.c:26`. The comment below `loaded[]` describes
  `open_count` alone. The severe finding covers its guard.
- `src/antic/lexer.c:1545`. `token_kind_name` fills
  `static char names[TOKEN_KIND_COUNT][16]` on first use, with no
  comment. The host programs are single-threaded, so this is the
  comment alone.

The other mutable state of the runtime carries a `DESIGN` comment or a
comment on its lock: `assert.c`, `hooks.c`, `init.c` through `rt.h`,
`sync.c`, `threads.c` and `trace.c`.
