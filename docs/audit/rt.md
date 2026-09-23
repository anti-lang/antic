# Runtime audit

The audit of `src/rt/` and `src/native/` against `docs/c-guidelines.md`,
at `fddd434`. Every one of the 54 files of `src/rt/` was read in full.
`src/native/` holds `CMakeLists.txt` alone and no C file, so it has no C
glue to audit. The runtime is held to every rule. The machine evidence of
`docs/audit/data/` was used as a list of places to look, and each item was
checked against the code. The pinned docs-style checker ran over every
file for rule 27. Nothing was built. Where a finding depends on how the
compiler or the standard library calls the runtime, the caller was read
in `src/antic/` or `src/std/`.

## Counts

| Severity | Findings |
|---|---|
| Severe | 8 |
| Major | 9 |
| Minor | 13 |

| Severity | Rule | Findings |
|---|---|---|
| Severe | 3 | 1 |
| Severe | 5 | 2 |
| Severe | 14 | 3 |
| Severe | 23 | 2 |
| Major | 3 | 2 |
| Major | 12 | 2 |
| Major | 14 | 2 |
| Major | 16 | 1 |
| Major | 23 | 2 |
| Minor | 1 | 1 |
| Minor | 9 | 1 |
| Minor | 10 | 1 |
| Minor | 11 | 1 |
| Minor | 12 | 1 |
| Minor | 13 | 1 |
| Minor | 18 | 1, covering 7 functions |
| Minor | 21 | 1 |
| Minor | 22 | 1 |
| Minor | 23 | 1 |
| Minor | 25 | 1 |
| Minor | 26 | 1 |
| Minor | 27 | 1 |

## Severe

### A large pad width overflows the size of a builder

`src/rt/text.c:34`, rule 5, severe.

`reserve` computes `b->length + count + 1` and then doubles `size` until it
covers that sum, and neither step is checked. `anti_rt_builder_fill`
passes the `count` it was given. The public `append_text(value, width)` of
`anti.text.Builder` reaches it through `pad`, with `missing = width - ...`.
A width near `INT64_MAX` overflows the sum at line 34. `reserve` then
returns 1, and the `memmove` and `memset` at lines 76 and 77 write far past
the block. A width above 2^62 overflows `size *= 2` at line 39 instead.
The fix is to refuse a `count` above `INT64_MAX - b->length - 1` and to stop
the doubling before it passes `INT64_MAX / 2`.

### The results of `parallel` are allocated from an unchecked product

`src/rt/threads.c:300`, rule 5, severe.

`malloc((size_t)(chunks * result_size))` multiplies two `int64_t` values
without a check. `chunks` is capped by `count` alone, so a long slice split
into many chunks with a large result type overflows. The workers then
write `result_size` bytes per chunk at `j->results + index * j->result_size`
past the block. The values it needs are extreme, but nothing refuses them.
The fix is the check that `anti_rt_chan_new` already makes at line 185.

### An ELF section of type NOBITS is read to its stated size

`src/rt/symbols.c:517`, rule 14, severe.

`elf_section_at` checks `inside(b, out->offset, out->type == 8 ? 0 :
out->size)`, so a NOBITS section passes with any size. Its callers then
trust that size. `elf_find` and `elf_symbols` hand a NOBITS string table
to `string_in`, whose `memchr` at line 149 reads up to `size - offset`
bytes past the file. `anti_elf_line` at line 718 takes a NOBITS
`.debug_line` as a line table of that size, and `unit_line` reads past the
file. A malformed file therefore reads outside the buffer. The fix is to
refuse a NOBITS section wherever its bytes are read.

### Signed overflow on numbers read from files

`src/rt/toml.c:137`, rule 14, severe.
The same class stands in `src/rt/symbols.c:397` and `420`, and in
`src/rt/conf.c:140` with `src/rt/threads.c:120`.

- `repeats` in `toml.c` keeps `strtoll` of every key under a table name and
  returns `seen + 1`. The document `[t]`, `9223372036854775807 = 1`,
  `[[t]]` makes that `LLONG_MAX + 1`. A bare key may be all digits, so the
  input is valid in the subset up to that point.
- `unit_line` adds `sleb(&unit)` and the special opcode's step to the
  `int64_t` `r.line` without a bound. Two `DW_LNS_advance_line` of 2^62
  overflow it.
- `set_key` accepts `threads` when every byte is a digit and
  `atoi(value) > 0`, and `worker_count` calls `atoi` again. C11 leaves
  `atoi` undefined for a number out of range. glibc wraps, musl overflows
  inside its own loop and the Windows CRT clamps, so `--anti.threads` of
  4294967297 gives a different pool on each target.

The fix is `strtoll` with a range check in `conf.c`, and a bound before
each addition in the two readers.

### `Object.deserialize` takes addresses from the text

`src/rt/registry.c:626`, rule 14, severe.
`src/rt/registry.c:448` does the same for a slice.

A pointer or a function pointer that the object does not own is read as
the decimal address the text writes, `value = (void *)(uintptr_t)address;`.
A slice that it does not own is read as `{"address":A,"length":N}`. A
text from outside the program therefore plants any address in a field,
a function pointer included, and the next use of the field reads or
calls through it. This follows the entry on `Object.deserialize` under
"Object model" in `docs/decisions.md`, which says such a pointer comes back
as the address that was written. The rule and the decision disagree, and
settling that is Eddie's decision. The code does what the decision says.

### `rt.configure` frees values that other threads read

`src/rt/conf.c:148`, rule 23, severe.

`set_key` frees `k->value` and stores a new copy, with no lock.
`anti_rt_conf_get` at line 753 hands out `k->value` itself as the `str`.
`worker_count` in `threads.c:118` calls it on every `parallel` from any
thread and passes the pointer to `atoi`. `rt.configure` of anti.runtime
is public and may run after workers started. The same write sets
`anti_rt_option_backtrace` at line 153, which every `fail` reads. A
`parallel` that runs while `main` calls `rt.configure` then reads freed
memory. The fix is to settle the configuration before any thread can
start, or to guard the keys and hand out copies that are never freed.

### The table of open libraries has no guard

`src/rt/loaded.c:26`, rule 23, severe.
The writers stand in `src/rt/plugin.c:415`, `490` and `562`, the readers
in `loaded.c:80` and `src/rt/registry.c:72`.

`loaded[ANTI_PLUGIN_MAX]` is written by `anti_rt_plugin_load` and
`anti_rt_plugin_unload` without a lock or an atomic. The `created` and
`destroyed` hooks read `used` and `base` from every thread, and
`reflect.new` walks `classes` of each slot. Two loads at once can pick the
same free slot at line 417. An unload runs `close_library` and `memset`
while `registry_find` in another thread reads `classes.classes`, which
then points into an unmapped image. The failure text `message` at
`plugin.c:29` is one buffer that every thread writes. The comment at
`loaded.c:4` says why the state is global but not how threads share it.
The fix is one lock around load, unload and the walks, with the count of
open libraries kept as the fast path.

### Atomic subtraction negates its operand

`src/rt/atomic.c:172`, rule 3, severe.

`anti_rt_atomic_sub` returns `anti_rt_atomic_add(address, width, -value)`.
For `value == INT64_MIN` the negation overflows. An Anti program that
subtracts the minimum of `i64` atomically reaches it. The fix is a
subtraction per width, `__atomic_fetch_sub` and `_InterlockedExchangeAdd`
of the unsigned negation, as the other operations are written.

## Major

### The pool wakes one waiter where two kinds wait

`src/rt/threads.c:94`, rule 23, major.
`src/rt/threads.c:75` is the Windows twin.

`wake_caller` signals `work_done` once, with `pthread_cond_signal` or
`WakeConditionVariable`. Two kinds of thread wait on that condition: the
caller of `parallel` at line 340 and each caller of `join` at line 429.
The signal for a finished chunk can wake a joiner whose job is not done.
That joiner waits again, and the `parallel` caller sleeps with every chunk
finished. The next signal may be lost the same way. A program that
dispatches from one thread and runs `parallel` in another can hang. The
fix is `pthread_cond_broadcast` and `WakeAllConditionVariable` in
`wake_caller`.

### The signal table and its start flag have no guard

`src/rt/signal.c:37`, rule 23, major.

`handlers`, `started` and `pipe_ends` are plain statics.
`anti_rt_on_signal` writes `handlers[sig]` at line 128 while the reader
thread reads it at line 66, which is a data race. Two first calls at once
both see `started == 0` in `start_reader` and each creates a pipe and a
reader thread, and one pipe is lost. The comment of the file says what the
pipe is for, not how the state is shared. The fix is an atomic store and
load of the handler, and `pthread_once` for the reader.

### The Windows atomics are not what the design says on ARM64

`src/rt/atomic.c:17`, rule 3, major.

The `_MSC_VER` branch, which clang takes for every Windows target, loads
through `volatile`. On windows-arm64 that is a plain `ldr`, not the
sequentially consistent load the comment at line 3 promises. An `atomic`
load of an Anti program calls `anti_rt_atomic_load` today. On
windows-arm64 such a load may then pass an earlier atomic store to another
address, which no other target allows. An algorithm that relies on
sequential consistency, as a flag handshake between two threads does,
works everywhere but there. The branch also returns `*(const volatile char *)` for
width 1, which relies on `char` being signed. The fix is `__atomic_load_n`
there, as the other branch does, or an interlocked read.

### Windows takes UTF-8 text through the ANSI entry points

`src/rt/plugin.c:55`, rule 3, major.
`src/rt/conf.c:593` is the second place.

`open_library` passes the UTF-8 path of a `str` to `LoadLibraryA`, which
reads it in the ANSI code page. A library path with a byte above 127 then
fails to load on Windows alone, while `anti.fs` opens the same path
through UTF-16 and the digest check at line 704 reads it. `environment_path`
reads `ANTI_CONF` with `GetEnvironmentVariableA` into 1024 bytes and returns
NULL for a longer value. That program then runs without its configuration
and says nothing, while every other target reads any length. The fix is
`LoadLibraryW` and `GetEnvironmentVariableW` over the UTF-16 conversion
that `fs.c` already has.

### A long sleep wraps on Windows

`src/rt/time.c:61`, rule 16, major.

`Sleep((DWORD)(nanoseconds / 1000000))` narrows an `int64_t` count of
milliseconds to 32 bits. A wait of 2^32 milliseconds or more wraps to a
short one, and 4294967295 is `INFINITE`. A wait below one millisecond
becomes `Sleep(0)`. The other targets sleep the full time through
`nanosleep`. The fix is a loop of bounded `Sleep` calls, with the rounding
of a short wait stated.

### A failed copy in the TOML reader leaks the other half

`src/rt/toml.c:109`, rule 12, major.

`add` stores `keep(key)` and `keep(value)` in the pair at `doc->count` and
counts it only when both succeed. When one fails, the other is never
freed, because `anti_rt_toml_free` frees the first `doc->count` pairs. The
fix is to free both before `r->failed = 1`, in one cleanup block.

### A failed start of the signal reader leaks the pipe

`src/rt/signal.c:84`, rule 12, major.

When `pthread_create` fails after `pipe` succeeded, `start_reader` returns
0 with both ends open and `started` still 0. Each later
`anti_rt_on_signal` opens another pair. The fix is to close both ends on
that path, in one cleanup block.

### A repeated member in `Object.deserialize` leaks what it built

`src/rt/registry.c:684`, rule 14, major.

`fill` reads every member that names a field, and nothing refuses a name
that came before. The text `{"type":"C","p":{...},"p":{...}}` for an
`own` field builds two objects and stores the second. The first ran its
`construct`, is never destroyed and never goes back to the allocator,
because `made.objects` is freed on success. A repeated `str` member leaks
its bytes the same way. The fix is to refuse a member whose field was
already read.

### A read error is taken for the end of a file

`src/rt/trace.c:140`, rule 14, major.

`load` loops on `fread` until it gives 0 and never calls `ferror`. A
read error then leaves a short image, which `anti_macho_relocate` and the
symbol readers take as the whole file. The tool pass reports the same loop
in `src/anti/`. The fix is `ferror(f)` after the loop and no entry on an
error.

## Minor

### Table entries become functions through object pointers

`src/rt/object.c:612`, rule 1, minor.
The same crossing stands in `object.c:641` and `650`, `src/rt/registry.c:159`
and `224`, `src/rt/hooks.c:35` and `src/rt/plugin.c:73` and `336`.

A table is an array of `const struct anti_descriptor *`, and its entries
are read as functions. ISO C does not define that conversion. Every target
gives it, as POSIX `dlsym` needs, so it is an extension. It is also spelt
three ways: a cast through `void *` in `object.c` and `registry.c`, and a
union in `hooks.c` and `plugin.c`. The fix is one helper in one file,
which the other files call.

### Formatted text is not checked for truncation

`src/rt/conf.c:489`, rule 9, minor.
Also `conf.c:188`, `255`, `283` and `531`, `src/rt/plugin.c:214` and `219`,
and `src/rt/trace.c:869` to `893`.

Each `snprintf` result is ignored or added to a length without a check. A
configuration path above 270 bytes cuts the position at line 489 short.
`list_injectable` adds the would-be length to `at` at line 188, and only
the loop test keeps it from being used. No case writes out of bounds today.

### The configuration reads texts of the TOML reader through their NUL

`src/rt/conf.c:416`, rule 10, minor.
Also lines 420, 452, 481 to 533, and `threads.c:120`.

`read_includes`, `read_keys` and `join_elements` take `key.ptr` and
`value.ptr` of an `anti_text` as C strings with `strcmp`, `strchr` and
`strlen`. `anti_rt_conf_get` returns `strlen(k->value)` as the length.
They rely on the NUL that `keep` in `toml.c` adds. A quoted value may hold
a NUL byte, and the value is then cut short without a message.

### Functions that return memory do not say who frees it

`src/rt/object.h:213`, rule 11, minor.
Also `object.h:256`, `src/rt/registry.h:47`, `src/rt/toml.h:22`,
`src/rt/sync.c:88` and `174`, `src/rt/threads.c:350`, `src/rt/plugin.h:124`
and `src/rt/std.h:139`.

`anti_rt_dup`, `anti_rt_copy_buffer`, `anti_rt_reflect_new`,
`anti_rt_toml_read`, `anti_rt_mutex_new`, `anti_rt_chan_new`,
`anti_rt_dispatch`, `anti_rt_plugin_instance` and `anti_rt_fs_open` return
memory or a stream. Their comments say where it comes from, not which call
releases it. `anti_rt_builder_take` and `anti_rt_trace_text` show the form.

### Resources are released by hand before each return

`src/rt/plugin.c:433`, rule 12, minor.
Also `plugin.c:439`, `450`, `462`, `468` and `484`, `src/rt/fs.c:354` and
`362`, and `fs.c:316` with the Windows branch above it.

`anti_rt_plugin_load` calls `close_library(handle)` on six paths, and
`anti_rt_fs_rename` releases `old_name` on two. Every path releases today,
so nothing leaks, but the rule asks for one cleanup block. Moving the
stub loop of `anti_rt_plugin_load` into a function would also serve the
threshold finding below.

### The runtime has no one failure routine

`src/rt/sync.c:79`, rule 13, minor.
Also `src/rt/threads.c:304`, `358` and `366`, `src/rt/plugin.c:287` and
`695`, `src/rt/conf.c:262`, `501` and `523`, `src/rt/start.c:90` and
`src/rt/cpu.c:319`.

`sync.c` has its own `fatal`, `conf.c` has `startup_error` and then prints
by hand twice beside it, and `start.c` and `cpu.c` print and exit on their
own. `threads.c` calls `abort()` with no message when memory runs out,
which a program can reach. `discover_in` prints a passed-over library to
standard error during discovery. `assert.c`, `check.c` and `cast.c` are the
failure routines the decisions name. The fix is one routine that prints a
message and ends the program, with an `exit` and an `abort` form.

### Functions past a threshold

Rule 18, minor.

- `unit_line` at `src/rt/symbols.c:290`, 163 lines. It reads the header of a
  line table and then runs its program. The two parts split cleanly, and
  the split would read better.
- `anti_rt_read_float` at `src/rt/text.c:627`, 134 lines. Reading the text
  into a decimal, lines 646 to 702, is one step and could stand alone. The
  scaling and the rounding follow the algorithm.
- `read_value` at `src/rt/registry.c:540`, 120 lines. One `switch` over the
  type ids. The size follows the shape of the problem.
- `anti_rt_plugin_load` at `src/rt/plugin.c:399`, 101 lines. A sequence of
  checks, and the stub loop at lines 473 to 489 would read better as a
  function.
- `anti_rt_json_string` at `src/rt/json.c:73` and `anti_split_command_line`
  at `src/rt/utf.c:142` nest 5 deep. Each nest follows the escape or quote
  rules it implements.
- `anti_rt_parallel` at `src/rt/threads.c:276` takes 9 parameters. They are
  the ABI that generated code calls, and they stay.

### A header declares half its functions outside its guard

`src/rt/std.h:107`, rule 21, minor.

The `#endif` of `ANTI_STD_H` stands at line 107, and the declarations of
`anti_rt_errno` to `anti_rt_fs_rename` follow it. Its first comment names
anti.io, anti.text and anti.license, though the header serves anti.time,
anti.mem and anti.fs as well. `src/rt/hooks.h:9` includes `std.h` and uses
nothing of it.

### Platform branches outside a platform file

`src/rt/conf.c:357`, rule 22, minor.
Also `conf.c:376` and `591`, `src/rt/plugin.c:52` to `95` and
`src/rt/loaded.c:59`.

No document or comment names the files of the platform layer, so the rule
cannot be checked as written. Files whose whole work is the system hold
branches, which reads as intended: `mem.c`, `time.c`, `errno.c`, `fs.c`,
`signal.c`, `threads.c`, `sync.c`, `cpu.c`, `start.c`, `trace.c` and
`platform.c`. `conf.c`, `plugin.c` and `loaded.c` hold `#if defined(_WIN32)` for
path rules, the environment, the loader and the image of an address. Those
could move into one platform file, and a comment could name the layer.

### A global without a reason

`src/rt/init.c:3`, rule 23, minor.

`static int ready;` carries no comment. `rt.h` says a static library for C
initialises the runtime on its first use. The flag is then written and
read without a guard from whichever thread calls first.

### Names outside the prefix of the runtime

`src/rt/signal.h:8`, rule 25, minor.
Also `src/rt/cpu_level.h:32` to `59`, `src/rt/utf.h:9` to `20`,
`src/rt/symbols.h:27` to `88` and `src/rt/f16.h:17` and `42`.

`SIGINT_SIGNAL` and `SIGBREAK_SIGNAL` carry no prefix at all, beside the C
`SIGINT` and `SIGBREAK`. The shared headers use `anti_cpu_`, `anti_utf8_`,
`anti_elf_`, `anti_macho_`, `anti_coff_` and `anti_f16_`. They are shared
with antic and `anti`, which may be the reason, but no comment documents
those prefixes as the rule asks.

### Dead code and duplicates

`src/rt/cpu.c:307`, rule 26, minor.

- `anti_cpu_built` is declared in `cpu_level.h` and called nowhere in
  `src/`, `tests/` or `tools/`.
- `file_bytes` at `src/rt/conf.c:326` and `index_bytes` at
  `src/rt/plugin.c:581` are the same function. `load` in `trace.c` is a
  third reader, the one without the `ferror` check.
- `same_bytes` is written in `src/rt/registry.c:14` and `plugin.c:97`.
- `const wchar_t *p = block;` at `src/rt/start.c:159` is set again by the
  `for` at line 164.

### Two comments run past the sentence length

Rule 27, minor.

The checker reports `src/rt/assert.c:19`, a sentence of 31 words, and
`src/rt/signal.c:6`, a sentence of 31 words ending at line 11. Its other
hits in `src/native/CMakeLists.txt` read CMake `#` lines as Markdown
headings and a `";"` argument as prose, and are not findings.

## Tool items not reported

- The analyzer's `conf.c` warnings are false positives for the reason the
  tool pass gives: `startup_error` and `unknown_option` end in `exit(70)`.
- `start.c:113` reads each argument up to the 0 unit that
  `anti_split_command_line` writes after it, in a block of `2 * n + 2`
  units that its output never fills. `fs.c:185` needs
  `capacity` 0, where the loop above it grows the block first.
- `-Wcast-align` at `object.c:624`, `706` and `707` restores pointers to
  objects that were aligned, and `type->size` is a multiple of the class
  alignment.
- `-Wenum-enum-conversion` in `hooks.c`, `object.c` and `registry.c`, and
  `-Wsign-conversion` at `trace.c:228`, compute the values the code means.
- `-Wmissing-prototypes` and `-Wmissing-format-attribute` name no rule of
  the guidelines.
