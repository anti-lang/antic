# Batch 3ac of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
Batches 3z to 3ab found their defects by reading readers, so this batch
read the readers that no batch had read for an entry that is dropped, cut
short or taken from the wrong part of its input. It read the three
readers of a library file, `src/antic/antl_read.c`,
`src/antic/antl_read_ir.c` and `src/antic/antl_tree.c`, the reader of the
AST of clang in `src/anti/bindclang.c` and `src/anti/bindexpr.c`,
`src/anti/zip.c`, `src/anti/symmap.c`, `src/anti/sdk.c`,
`src/anti/memreport.c`, the inputs of `anti doc`, `anti test` and
`anti check`, the command lines of antic and anti, the reader of the
arguments and the environment of the runtime, `anti.args`, `anti.config`,
and the symbolizer and the traces of `src/rt/symbols.c`, `src/rt/trace.c`
and `src/rt/backtrace.c`. Eddie's direction is the Mac alone, so the VMs
were not touched. The batch found eight defects, T309 to T316, and fixed
seven. The next free id is T317.

## Commits

- `42a112e6` lists T309 to T314.
- `1d910b32` fixes T309, `03d27818` T310, `1b8fc5b5` T311, `78ed8325`
  T312, `d2ae956a` T313 and `dba91960` T314.
- `c1fd60e8` lists T315 and T316, and `b326871c` fixes T315.
- `7ababd6d` tests the default of a field of a generic copy, an export
  const of an unknown type and two forms of `#pragma pack`.
- `ab6db38d` tests a tab in `#pragma pack` and after a compound literal of
  a description.
- One commit records the times in `CLAUDE.md`, the commits of the fixes in
  `docs/audit/tests-findings.md`, the guards of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T309, minor, fixed. `license_command` of `src/anti/main.c` took
  `--project` and `--notice` out of the command line before it paired
  each option with its value, so `anti license --from --notice` and
  `--runtime --project` printed the usage, where the `[provisional]`
  entry of R155 makes the word after such an option its value.
- T310, minor, fixed. `syms_resolve` of `src/anti/syms.c` walked the trace
  as a C string, so a NUL byte ended the output. Every line after it was
  dropped, and the command exited with 0.
- T311, major, fixed. clang declares a builtin that a body of the header
  calls, such as `__builtin_expect`, as implicit at the top of its AST.
  `anti bind --clang` bound it as `extern fn` of the library.
- T312, major, fixed. An anonymous record took the name of the typedef
  after it whatever that typedef named, so `typedef struct { int a; } *P;`
  bound `struct P`, where `P` of C is a pointer.
- T313, major, fixed. `bind_struct_const` of `src/anti/bindexpr.c` read
  nothing after the `}` of a compound literal, so
  `(Color){ 230, 41, 55, 255 }.r` was bound as a constant of the struct.
  A description of `anti bind` goes through the same function.
- T314, severe, fixed. `pragma_pack` of `src/anti/bindclang.c` read the
  first digit anywhere between the parentheses. The label of
  `#pragma pack(push, id1, 4)` gave a pack of 1, so a struct of pack 4 was
  bound as `packed`, with another layout than C. `r2d2` gave 2 and
  refused a struct of pack 1, and `#pragma pack(show)` set the pack to 0.
- T315, minor, fixed. A lane index of `shuffle` that is no constant got
  the message of the constant and then the message of `shuffle`.
- T316, minor, open. The probe in C of `anti bind --probe` declares a
  variable of a typedef of a const-qualified struct and writes its
  fields, which C refuses. Its line stays in `tests/known-failures.txt`,
  since the fix is a choice of the form of the probe, below.

## Read without a find

- The readers of a library file refused every damage the reading named.
  The digest of a generic takes the default of a field and a field
  without `pub`, which the test of `Can` in the stale tree holds.
- `anti.args` reads the word after an option that takes a value as its
  value, as getopt does and as R155 has anti do.
- The runtime refuses `--anti.` without a name and an empty value of
  `--anti.backtrace=`, and an empty argument passes as an empty text.
- `src/anti/zip.c`, `src/anti/sdk.c`, `src/anti/memreport.c` and the
  inputs of `anti test` and `anti check` turned away every damage tried.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 43800.
`cmake -DFIRST=43801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 43801 to
44000 at the default size in 163 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.45%, 3693 missed of 56385 | 93.42%, 3714 missed of 56459 |
| Regions | 98.52%, 1260 missed | 98.52%, 1262 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1223 missed | 98.73%, 1224 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/anti/bindclang.c` | 18 | 34 |
| `src/anti/main.c` | 48 | 50 |
| `src/anti/bindexpr.c` | 5 | 7 |
| `src/antic/sema_export.c` | 32 | 31 |
| `src/antic/sema_simd.c` | 32 | 31 |

The fixes add 74 branches, and the suite reaches 53 more than before.
The reader of the arguments of `#pragma pack` and `names_record` hold
most of the rest, the guards and the unsorted sides of the batch in
`docs/notes/unreached-guards.md`, and the tabs that the last commit of
tests reaches. `src/rt/mem.c`, `src/rt/platform_posix.c` and
`src/rt/sync.c` moved by one each, since the threads of the suite take
other paths from run to run. The coverage run had 7885 tests, all
passed, and `./c coverage` took 1142 s. It ran before the commit of the
tabs, whose two sides a profile of their own shows reached.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7895. Every test passed.
- The host runs 7897 tests and each sanitizer tree 7895.
  `tests/known-failures.txt` holds the two lines of `host=windows` and
  the line of T316.

## Not reached

`anti doc` writes the page of a module whose path is `index` to
`index.html`, and the index written after it replaces it, so the page of
the module is lost and the index links to itself. Refusing the name, or
writing the page or the index under another name, needs a message or a
choice of a name, so the batch lists no finding for it. The sides left
unsorted stand at the end of the section of the batch in
`docs/notes/unreached-guards.md`.

## Questions for Eddie

- T316: should the probe declare a variable of a const-qualified type with
  `__typeof_unqual__`, which C23 names `typeof_unqual` and the pinned
  clang takes, or write each field into the bytes of an object at its
  `offsetof`? The probe takes `__typeof__` already for a field of a
  record without a name.
- Should `anti doc` refuse a module named `index`, which needs a new
  message, or write the index under a name no module path takes?

Fourteen questions stay open from before, and nothing here acts on them.
