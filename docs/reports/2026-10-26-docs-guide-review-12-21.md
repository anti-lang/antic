# Review of chapters 12 to 21 of the developer's guide

Date: 2026-10-26, from `e7aeb3d8`. The step read the parts "How antic builds it" and "What it costs" of chapters 12 to 21 of `docs/site/developers-guide/` against the code, the claims of the same kind in the chapters 18 to 20 that have no such parts, and every row of the cost table of chapter 21. It changed `.md` files under `docs/` alone, so no suite ran. The probes stand under `build/grev2/`, and the logs under `build/drive/logs/grev2-*`.

## Method

- `build/grev/wil.py` of the review of chapters 1 to 11 compared each file and name of the "Where it lives" lines with `src/`. Every file exists, and every name is defined in a file its line names. Log `build/drive/logs/grev2-wil.log`.
- Each claim was read in the code. Where a count or a size could be built, a probe built it with the host antic. Its code is that of `96a09525`.
- `build/grev2/page.sh` ran the example runner on one page after each change, with a fresh work directory.

## Changes by chapter

### Chapter 12

- `alloc T { }` of a type that `align(N)` raises above 16 calls `anti_rt_alloc_aligned`, `lower_new_memory` of `src/antic/lower.c:1156-1170`. The page named `lower_expr.c` for that function.
- `alloc(T, n)` passes the declared alignment above 16, `src/antic/lower_expr.c:2068-2093`. It zeroes a class and every type that `=` tears down. A Windows program gets the memory of `malloc` at 16 there, `src/rt/platform_windows.c:1199-1210`.
- A part `a[lo..hi]` carries two compares and two branches in dev mode, `lower_slice_bounds_check` of `src/antic/lower_check.c:350`. The cost table of slices said both modes are the same. Probe `build/grev2/c12/sl.anti`.
- A block of `ArenaAllocator` pads its link to the alignment of the request, `src/std/anti/mem.anti:174-197`. A request above the block size leaves the room of the head block before it unused. The page said such a request wastes nothing.

### Chapter 13

- A dev build tests a job before each `join` and writes the mark 1 after it, and `join_all` of a dev build is a loop, `src/antic/lower_worker.c:289-306` and `:350-398`. Two cost tables said dev and release cost the same. A new `ir` excerpt of `workers.start` shows the test and the mark.
- On x86-64 an atomic `and` or `or` whose value the program uses is a loop of `lock cmpxchgq`. A new `asm` excerpt of `mask` in the block `counters` shows it. LSE needs `neg` before `sub` and `mvn` before `and`. Probe `build/grev2/c13/at.anti`.
- `delete` of a channel of owning values takes each held value out with `anti_rt_chan_held` first, `src/antic/lower_sync.c:51-80`.
- A `parallel` over no elements allocates nothing, `src/rt/threads.c:248-263`.

### Chapter 14

- `find_one`, `update_one`, `remove_one` and the hash of a `ConcurrentMap` hold all 16 locks at once, and `==` holds the 32 of both maps, `whole` and `locked_parts` of `src/std/anti/collection/concurrent.anti:652-667` and `:805-820`. The page said every operation over the map holds one lock at a time.
- A dev build searches the stack of the thread for a `Mutex` that a `sync` takes, `held_already` of `src/rt/lock.c:226-244`. Two tables now name it.

### Chapter 15

- The `Profiler`, the `ErrorMonitor` and the `ChangeJournal` hold their tables under a `Mutex`, and `Composite` takes its own in `add` alone, `src/std/anti/trace.anti:143`, `:402`, `:595` and `:715`. The page said no lock guards them.
- The `Profiler` reads the clock at the outermost call of each name.
- The claim that a traced function inlines less freely was replaced by what the excerpt shows: the store stays before the call of `leave`.
- A `parallel` without `by` reads the key `threads` at each call, `worker_count` of `src/rt/threads.c:92-101`.
- The plugin sizes of the example, built again: 309,152, 151,504 and 59,264 bytes, unchanged. Probe `build/grev2/c15`.

### Chapter 16

- A walk of a thread-safe collection copies the elements into a `Snapshot<T>`, `src/std/anti/collection/synchronized.anti:307-311`. The page said no walk allocates.

### Chapter 17

- The program of `--linux-lib X11` is 29,104 bytes, not 29,112, and the column of the needed libraries is the third. Probe `build/grev2/c17/xd.anti`.

### Chapter 20

- `llvm-objdump -d` prints 10,530 lines for the program of the debugging session, with and without `-g`, not 10,528. The `-g` build is 112,496 bytes in the directory of the probe. The debug map of Mach-O names each object by its absolute path, so a directory name 32 characters longer gives 112,528. The page now says so.

### Chapter 21

Nine rows follow the corrected sections: pointers, slices, `anti.mem`, `parallel`, atomics, concurrent classes, thread-safe collections, tracing and walking. The paragraph on the stock handlers names the `Mutex` of the `Profiler`.

## Claims checked and kept

Built again and equal: the three library forms of chapter 17 with their 7 and 43 dynamic symbols, its plugin table and the output of the C program on macos-arm64. Equal as well: `units.antl` of 828, 676 and 825 bytes, its lock digest `02870f25...`, the `hello` sizes 137,008 and 107,008 and its 19 notice lines. So are `purse.antl` of 18,185 bytes with and without the blocks, the build ids and six release sizes of chapter 20, and the instrumented `objects` of 128,880 bytes. Read in the code and kept: the job of 56 bytes, the channel header of 56 bytes, the hidden lock of 24, the order table of `src/rt/lock.c`, the growth of `HashMap` and of `Indexed<T>`, the quadratic `clear` of a list and `RUN` of 20. Logs `build/drive/logs/grev2-units.log` and `grev2-plug.log`.

## Examples added

An `ir` excerpt of `workers` in chapter 13, and the function `mask` of `counters` with its `asm` excerpt. The guide holds 174 blocks and 286 excerpts.

## Older documents against the code

- `docs/notes/` was not the source of any claim changed here. The Mutex of the stock handlers, which the page denied, is stated by the header of `src/std/anti/trace.anti:31`.
- `CLAUDE.md` says the runtime calls `alloc` and `free` of an allocator as table entries 8 and 9. The code calls them as entries 17 and 18, `ANTI_ENTRY_ALLOC` of `src/rt/object.h:229`, as the guide says. The step left `CLAUDE.md` as it is.

## Findings

No defect was recorded. Two points for Eddie, neither against a rule. `clear` of a `List` removes from the front and moves `n (n - 1) / 2` elements, `src/std/anti/collection/list.anti:438-447`. `text.from_c(none)` calls `strlen` of a null pointer, `src/rt/text.c:18-25`.

## Questions answered from the code

- Does a dev build check a second `join`? Yes, by a mark in the variable. `--checks` does not bring it into a release build.
- Does `alloc(T, n)` honour `align(N)` above 16 on Windows? No. The memory is that of `malloc`.
- Does a walk of `ConcurrentMap` see one moment of the map? No. `find_one` and its two siblings do.

## Gates

- `guide_examples` and `overview_examples` passed, 2 of 2, in `build/drive/logs/grev2-examples-final.log`. The guide ran 174 blocks, 125 programs, 598 refusals and 286 excerpts. The overview ran 63 blocks, 7 programs and 16 refusals, with 9 skipped as not built. `build/drive/logs/grev2-examples-counts.log` holds both counts.
- The docs-style checker reports nothing on the ten pages, in `build/drive/logs/grev2-style-all.log`, and on this report.
- `git grep` finds no TODO, FIXME or placeholder text in the ten pages.
- No suite ran, since the step changed `.md` files under `docs/` alone.

## State after the push of the report

`git log --oneline -3`:

```text
f7140af3 Report the review of chapters 12 to 21 of the developer's guide
afe682e9 Bring the cost table of the guide in line with chapters 12 to 16
4265c914 Check chapter 20 of the guide against the code
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main` printed one hash twice:

```text
f7140af33fce61c4e7c7f5532d259d500272701e
f7140af33fce61c4e7c7f5532d259d500272701e
```
