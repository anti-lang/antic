# Runtime audit

The second audit of `src/rt/` and of the C glue of `src/native/`, at
`238a4f1`. All 61 C files and headers of `src/rt/` were read in full.
`src/native/` holds CMake recipes and no C file. Its glue is
`src/rt/regex.c` and `src/rt/patterns.c`, which `src/native/pcre2.cmake`
compiles, and the recipes were read for how they build it. The runtime
is held to every rule of `docs/c-guidelines.md`. Structure comes first
and is judged by the questions of the task. The machine evidence of
`docs/audit/data/` served as a list of places to look, and each item was
checked against the code. Where a finding depends on a caller, the
caller was read in `src/antic/` or `src/std/`. Nothing was built. The
pinned docs-style checker reports nothing in the runtime sources.

The runtime depends on nothing of antic. No file of `src/rt/` includes
a header of `src/antic/`, `src/anti/` or `tools/`, as
`data/include-graph.txt` shows. Each file's purpose can be said in one
sentence, and the opening comment of most files says it. The platform
layer is where the runtime falls short: it holds the clocks, the loader
and the environment, and 13 other files still talk to the system
themselves.

## Counts

| Severity | Findings |
|---|---|
| Severe | 5 |
| Major | 4 |
| Minor | 13 |

| Severity | Rule or question | Findings | New | Still open |
|---|---|---|---|---|
| Severe | 5 | 1 | 1 | 0 |
| Severe | 6 | 1 | 1 | 0 |
| Severe | 14 | 1 | 1 | 0 |
| Severe | 23 | 1 | 0 | 1 |
| Severe | Boundaries between parts | 1 | 1 | 0 |
| Major | 22, one platform layer | 2 | 1 | 1 |
| Major | 14 | 1 | 1 | 0 |
| Major | 23 | 1 | 1 | 0 |
| Minor | 1 | 2 | 1 | 1 |
| Minor | 3 | 2 | 1 | 1 |
| Minor | 11 | 1 | 1 | 0 |
| Minor | 13 | 1 | 1 | 0 |
| Minor | 18 | 1, covering 16 functions | 0 | 1 |
| Minor | 20 | 1 | 1 | 0 |
| Minor | 25 | 1 | 1 | 0 |
| Minor | 26, duplicated code | 1 | 1 | 0 |
| Minor | One place per concern | 2 | 2 | 0 |

No finding returned. Of the first audit, S26 to S33 and S35 to S38 are
closed. So are M1 for `trace.c`, M10, M18, M19, M20, M27, M30 and M22
for the load. So are the minor findings under rules 1, 9, 10, 12, 21
and 23. Rules 11, 13, 25 and 26 are closed for the places the first
audit named.

## Severe

### `patch` adds the program's offset to a length without a bound

`src/rt/patterns.c:753`, rule 5, severe, new.
`src/rt/patterns.c:802` is the second place.

`if (at + with_length > end - start)` adds two `int64_t` values, and
`at` is the `offset` that the public `patch` of `anti.regex` passes
through unchanged. `at` is refused below 0 and never above. An offset
of `INT64_MAX` with a replacement of one byte overflows the sum, which is
undefined. Built as it is, the sum wraps below the span, and `memcpy(data + start + at, ...)` at line 758
writes far outside `data`. `anti_rt_bytes_patch` makes the same sum at
line 802 and writes at line 823. The fix is `at > span - with_length`,
after a check that `with_length` fits the span.

### A group is read by a count the program may change

`src/rt/patterns.c:547`, rule 6, severe, new.

`anti_rt_regex_group` bounds `n` by `m->count` and then reads
`ov[2 * n]` at line 480. The ovector holds one pair per group of the
pattern. `count` is a `pub` field of `Match`, since `match_form` of
`src/antic/types.c:987` hides only the names in parentheses. A program
that assigns `m.count = 50` and calls `m.group(50)` reads past the match
data of PCRE2. The fix is to bound `n` by
`anti_rt_regex_group_count(m->pattern)`, as `expand` already bounds its
groups.

### Symbolize reads a Mach-O header at an address the text gives

`src/rt/trace.c:367`, rule 14, severe, new.

On macOS `anti_rt_trace_symbolize` takes `frame->base` as the header of
an image and walks its load commands at line 382. `StackTrace` holds its
frames as `pub own frames: []RawFrame`. `Object.deserialize` reads an
owned slice of structs, and it reads `base` as the `u64` the text
writes. A program that deserializes a `StackTrace` and calls `symbolize`
then reads memory at an address the text chose. This is the case S32
closed for pointers, reached through an integer. Linux reads the file
that `module` names at line 537, which the text chooses as well. The fix
is to take the image from the address with `dladdr`, as
`anti_rt_trace_frame` does, and never from the record.

### A library can unload while an object of it is being built

`src/rt/plugin.c:597`, rule 23, severe, still open in part (S34).
`src/rt/plugin.c:616` and `880` are the other places.

`anti_rt_plugin_instance` finds the slot and the entry under the lock
and gives the lock back. `build` then reads `p->stubbed` and calls
`e->init`, which lies in the library's image. `anti_rt_plugin_unload` on
another thread sees `live` at 0, since the new object's `created` hook
has not run. It clears the slot and closes the library, and `build`
calls into unmapped code. `anti_rt_plugin_supports` reads
`e->class_of->functions` after the lock the same way. The DESIGN comment
at line 639 promises that an object is counted before the check or finds
no library open, and this path keeps neither. The fix is to count the
object against the slot under the lock before the lock is given back.

### The guard of `unload` rests on hooks that `--no-hooks` removes

`src/rt/hooks.c:79`, boundaries between parts, severe, new.
`src/rt/loaded.c:106` and `src/antic/lower.c:936` are the other ends.

The runtime counts the live objects of a loaded library in
`anti_rt_hook` alone, on the `created` and `destroyed` hooks. `unload`
refuses while the count is above 0. `lower_hook_object` writes no site
under `--no-hooks`, as its DESIGN comment says. A program built with
the option counts nothing, so `unload` closes a library whose objects
are alive. The next call through such an object jumps into unmapped
code. A compiler option for tracing switches off a safety check of
the loader. The fix is a count that the compiler writes for every class
of a library, or a refusal of `--no-hooks` in a program that loads one.

## Major

### The platform layer holds a fraction of the host code

`src/rt/platform.h:20`, rule 22 and one platform layer, major, still
open (M29, and the tool pass at `src/rt/trace.c:11`).

`data/platform-conditionals.txt` lists 81 host `#if` lines in 13 runtime
files outside the layer. `fs.c`, `trace.c` and `threads.c` hold 38 of
them. The layer has diverged from its own design. `platform.h:20` says
that every lock at file scope is a name of `enum anti_rt_lock`, and one
name stands there. Six files keep locks of their own:

- `loaded.c:38` and `signal.c:48`, an `SRWLOCK` or a `pthread_mutex_t`;
- `trace.c:67`, `179` and `670`, three more;
- `sync.c:27` with its condition variables;
- `threads.c:60`, a `CRITICAL_SECTION` on Windows;
- `lock.c:32`, a futex, `os_unfair_lock` or `SRWLOCK`.

`sync.c:5` says a channel stands on the layer that `threads.c` uses. Its
Windows lock is an `SRWLOCK` and the one of `threads.c` a
`CRITICAL_SECTION`, so the two copies already differ. The fix is the
lock, the condition variable, the thread and the loader queries in the
two platform files, which `lock.c` could keep apart for the word of a
Mutex.

### Windows gives the text of a system error in the ANSI code page

`src/rt/errno.c:52`, rule 22 and one platform layer, major, new.

`anti_rt_last_error_text` calls `FormatMessageA`, which writes the
message in the code page of the user's language. `SystemError.from_win32`
makes a `str` of it. `docs/decisions.md` makes every `str` valid UTF-8,
and every other text of Windows comes through UTF-16 in
`platform_windows.c`. On a French or a German Windows the message holds
bytes that are no UTF-8. A search of `anti.regex` over it then stops the
program at `patterns.c:194`. The other targets give an empty message.
The fix is `FormatMessageW` and the conversion of `platform_windows.c`,
with the function moved there.

### `Object.deserialize` makes a `str` that breaks the rules of `str`

`src/rt/registry.c:409`, rule 14, major, new.

`read_text` stores the bytes that `anti_rt_json_string` decoded. The
scanner takes any byte from 0x20 up as it stands, and `\u0000` gives a
NUL at `json.c:119`. A text from outside the program therefore puts
invalid UTF-8 or a NUL byte into a `str` field. `docs/decisions.md`
forbids both. `anti.regex` aborts on such a text at `patterns.c:194`, and
`anti.fs` refuses the path with `EINVAL`. The configuration reader
already refuses a NUL at `conf.c:502`. The fix is to fail the text at
the member, with `anti_rt_text_invalid` and a check for 0.

### The loader asks the system for an image while it holds its lock

`src/rt/plugin.c:420`, rule 23, major, new.
`src/rt/plugin.c:565` is the second place.

`loaded.c:102` says the image is found before the lock, because the
loader of the platform holds a lock of its own while it answers.
`claim` runs under the lock of the slots and calls `anti_rt_plugin_image`
for every entry. `anti_rt_plugin_load` evaluates `anti_rt_plugin_image`
after `anti_rt_plugin_hold`. On Windows a load on another thread runs
the library's constructors under the loader lock. When such a
constructor makes an object while a library is open, its `created` hook
waits for the lock of the slots. The thread in `claim` holds that lock
and waits for the loader. The fix is to find every image before
`anti_rt_plugin_hold`, as `count` does.

## Minor

### Compiler extensions outside the files that hold them

`src/rt/lock.c:63`, rule 1, minor, still open in part.
The tool pass reported `init.c:31` and `trace.c:140`.

`atomic.c` exists to hold the atomic builtins and has a form for every
compiler. `lock.c:63` to `80`, `176`, `304`, `318`, `328` and `337`, and
`snapshot.c:25`, `42` and `62` call `__atomic_*` directly. `trace.c:249`
calls `__builtin_frame_address`, and `trace.c:137` writes the
`#pragma comment` of the linker. `cpu.c:124` and `141` and `start.c:49` write
`__asm__`. `__attribute__((weak))` stands in `init.c:31` and
`trace.c:140`. Each belongs in the platform layer or in `atomic.c`.

### antic compiles the PCRE2 glue without `-isystem`

`CMakeLists.txt:124`, rule 1, minor, new.

`docs/c-guidelines.md` has the glue include the headers of a
third-party library with `-isystem`. `src/native/pcre2.cmake:69` does so
for the runtime's copy of `src/rt/regex.c`. antic compiles the same file
into `antic_core`, and its `target_include_directories` without `SYSTEM`
gives `pcre2.h` with `-I`. A warning in `pcre2.h` would then fail
the build of antic under our flags. The fix is the `SYSTEM` keyword.

### The Windows atomics rely on the signedness of `char`

`src/rt/atomic.c:60`, rule 3, minor, still open in part (M22).

The comment at line 20 gives the load signed types by name, so a byte
loads alike whatever `char` is. Swap, add, subtract, and, or and
compare-and-swap of width 1 still pass `(char)value` to the
`Interlocked` functions. They return the old byte as a `char`, which
widens to `int64_t` by the sign of `char`. The fix is `int8_t` casts, as
the load has.

### The status of `main` narrows without a check

`src/rt/start.c:213`, rule 3, minor, new.
`src/rt/start.c:252` is the POSIX entry.

`return (int)anti_main(args, env);` converts an `int64_t` to `int`. C11
leaves a value out of range to the implementation. The fix is to state
the rule of a status beyond 32 bits and apply it before the conversion.

### Functions that return a resource do not name its release

`src/rt/regex.h:29`, rule 11, minor, new.
Also `regex.h:39`, `src/rt/platform.h:50` and `src/rt/plugin.h:127`.

`anti_rt_regex_compile`, `anti_rt_regex_compile_bytes`,
`anti_rt_library_open` and `anti_rt_plugin_load` return a handle. Their
comments do not say that `anti_rt_regex_free`, `anti_rt_library_close`
and `anti_rt_plugin_unload` give it back.

### A builder drops bytes without a word when memory runs out

`src/rt/text.c:133`, rule 13, minor, new.

`anti_rt_builder_append` returns when `reserve` finds no memory, and the
text of an `f"..."` or of `serialize` comes out short. `anti_rt_text_copy`
at line 38 stops the program through the failure routine instead. A
serialized object cut short then fails to read back with no report of
why. The fix is one rule for memory in the text functions, through
`anti_rt_fail_abort`.

### Functions past a threshold

Rule 18, minor, still open.

`data/thresholds.txt` lists 16 runtime functions. The judgements of the
first audit and the tool pass hold. `unit_line` at `src/rt/symbols.c:363`
grew from 163 lines to 172, and `read_value` at `src/rt/registry.c:634`
from 120 to 149. The header of a line table would still come out of the
first cleanly. The other 14 are `put_value`, `anti_rt_read_float`,
`walk_class`, `anti_rt_parallel`, `anti_rt_json_string`,
`anti_rt_split_command_line`, `read_level`, `put_level` and six parameter
lists of `src/rt/patterns.c`.

### A function used in one file is exported

`src/rt/object.c:218`, rule 20, minor, new.

`anti_rt_variant_case` is declared in `object.h:115`. Only `put_value`
of `object.c` calls it, and no generated code names it.

### The root class carries a prefix the rule does not allow

`src/rt/object.c:65`, rule 25, minor, new.

`anti_lang_Object_type_name` and the other functions of the root, and
`anti_lang_Object_deserialize` at `registry.c:883`, are exported under
`anti_lang_`. They are the mangling of `anti.lang.Object`, and
`tests/run_rt_names.cmake:10` allows the prefix. Rule 25 says no other
prefix. The exception belongs in the rule, which is Eddie's decision.

### Growing buffers written five times

`src/rt/patterns.c:606`, rule 26, minor, new.

`grow_append` of `patterns.c`, `add` of `trace.c:830`, `reserve` of
`text.c:95`, `put` of `regex.c:85` and `add_name` of `fs.c:218` each
double a block. They already differ on a failure. The first stops the
program, the builder drops the bytes, `trace.c` returns NULL and the
other two set a flag. The DESIGN comment at `patterns.c:830` explains why
`anti.regex` has no `Builder` of `anti.text`. It does not cover the C
side, where `text.c` is part of every runtime.

### The glue's build lists its headers by hand and has missed one

`src/native/pcre2.cmake:71`, one place per concern, minor, new.

The `DEPENDS` of the glue objects names `regex.h`, `std.h`, `atomic.h`
and `cpu_level.h`. `patterns.c:6` also includes `rt.h`. A change of
`rt.h` then leaves the glue of `anti_rt_regex` built against the old
declaration of `anti_rt_memory_kept`. The fix is the dependency file of
the compiler rather than a list.

### The layout of `Match` stands where antic says it does not

`src/rt/patterns.c:126`, one place per concern, minor, new.

`struct anti_match` mirrors the fields that antic gives `anti.lang.Match`.
`src/antic/types.c:956` names it as the struct of `src/rt/regex.h`,
where it is not. A change of the fields on either side is found through
the wrong file. The fix is to name `patterns.c`, or to move the
struct into `regex.h`.

## Outside the rules

`rt.configure` reads the `[injections]` table and applies none of it.
`read_key` checks every line and records it at `src/rt/conf.c:626`.
`fill_injections` runs only from `anti_rt_conf_start` at line 890, which
runs before `main`. A file that `rt.configure` names then takes no
provider from a library and says nothing. `docs/decisions.md` loads such
a provider before `main`, which a call from `main` cannot reach. The
runtime should refuse the table there or apply it, and which one is
Eddie's decision.
