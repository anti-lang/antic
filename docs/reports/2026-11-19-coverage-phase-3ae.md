# Batch 3ae of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read the readers of input, so this batch read the
writers whose output a reader of the project reads again: `anti fmt`, the
C header, `anti doc`, the index of `anti symbols`, `anti bind --clang`
and `serialize`. Eddie's direction is the Mac alone, so the VMs were not
touched. The batch found thirteen defects, T333 to T345, and fixed seven.
Six stay open, five of them for a choice of Eddie. The next free id is
T346.

## Commits

- `64f36381` lists T333 and T334, `59fd0f64` T335 to T345.
- `485203b5` fixes T334, `c1fec4f7` T333, `457e4119` T335, `aafef7c5`
  T336, `df0702bc` T337, `36e129ab` T339 and `16d9c570` T340.
- `9f408301` tests the callouts of a number and in braces of
  `skip_callout`, `72788e0f` checks the fixtures of T333 and T335 in
  `fmt_canonical`, and `3545bde0` tests an item after the first paragraph
  of the index of `anti doc`.
- `61891380` records the guards of the batch in
  `docs/notes/unreached-guards.md`, and `2d48ae04` the times in
  `CLAUDE.md` and the commits of the fixes in
  `docs/audit/tests-findings.md`.

## Findings

- T333, minor, fixed. `anti fmt` moved a comment after the `{` of a body,
  a block or an anonymous function that spans lines to the next line.
- T334, minor, fixed. The comment and string lines of a source with CRLF
  kept their CR, while every other line ended with LF.
- T335, minor, fixed. `anti fmt` read a comment where a rule means the
  token before it, after `=`, after `catch` and before the `while` of a
  `do` block, and indented the statements after it one tab deeper.
- T336, minor, fixed. The index of `anti symbols inventory` left out
  every `'` of the name of a program and of its archive.
- T337, minor, fixed. The index of `anti doc` joined a fence or an item
  after the first paragraph of a module into the paragraph.
- T338, severe, open. `serialize` writes `null` for an `own` pointer to
  an array and an `own` slice of arrays, whose field record carries
  neither the id of the element nor the count.
- T339, major, fixed. The header copied a `*/` of a doc text into its C
  comment, which ended the comment.
- T340, major, fixed. `anti bind --clang` copied a string macro of bytes
  that are no UTF-8 into a module that does not lex.
- T341, major, open. The macro of an export constant replaces a parameter
  and a field of the same name in the header.
- T342, major, open. The header names a tuple and a tuple of a struct
  whose name holds `_` alike.
- T343, major, open. `Object.deserialize` finds no class nested in a
  class by the name `serialize` writes.
- T344, minor, open. `serialize` writes a `?T` of a union that holds a
  value as `null`, which reads back as `none`.
- T345, major, open. `Object.deserialize` refuses a member name past 256
  bytes, the `NAME_ROOM` of `src/rt/registry.c`. Four readers keep a
  name there. A room on the heap per reader shifts the cases of
  `tests/std/faults_registry.faults`, and a reading in place with
  `anti_rt_json_key` still needs a room for the class name of `type_of`.
  The batch left the choice between the two open.

## Read without a find

- The writer of library files against its reader, the layouts of the
  header, `anti-plugins.toml`, `anti.lock` and `NOTICE.txt`.
- The escaping of `anti doc` in HTML and in Markdown.
- `to_text` of a float, over every finite `f16` and about 3 million
  patterns of `f32` and `f64`, each read back to the same bits.
- The formats of integers, and `serialize` of the kinds of field that
  T338 and T344 do not name.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 44200.
`cmake -DFIRST=44201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 44201 to
44400 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.38%, 3751 missed of 56677 | 93.38%, 3753 missed of 56729 |
| Regions | 98.52%, 1269 missed | 98.52%, 1266 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1231 missed | 98.73%, 1230 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/pattern.c` | 49 | 47 |
| `src/anti/bindclang.c` | 36 | 35 |
| `src/anti/doc.c` | 13 | 16 |
| `src/anti/fmt.c` | 95 | 97 |

The fixes add 52 branches. The sides of them the suite misses stand at
the end of the section of the batch in `docs/notes/unreached-guards.md`.
The fix of T340 reaches the side of `read_macros` that batch 3ad named,
so its row left that section. `src/rt/mem.c`, `src/rt/threads.c`,
`src/rt/platform_posix.c`, `src/rt/trace.c` and `src/anti/cache.c` moved
by one or two, as the threads and the clock of the suite take other
paths from run to run. The coverage run had 7928 tests, all passed, and
`./c coverage` took 1024 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7938. Every test passed.
- The host runs 7940 tests and each sanitizer tree 7938.
  `tests/known-failures.txt` holds the two lines of `host=windows`, the
  line of T316 and the six lines of T338 and T341 to T345.
- Two expectations of the list commits changed in the fixes. The first
  line of the fixture of T333 stood past 80 columns, which `anti fmt`
  fills, so the fixture holds it in two lines. The C++ check of three
  cases of `run_clib.cmake` read a `.c` file as C++, which clang
  deprecates and `-Werror` refuses, so it reads a `.cpp` file of the same
  text. The C check ran first and failed for the reason of each finding.
- The fixture of T174 stays in the list of `fmt_canonical`, although its
  finding is fixed.

## Questions for Eddie

- An export item named after a C or C++ keyword writes a header that does
  not compile. Should antic refuse it as R130 does? That needs a message.
- Two modules export the same struct or constant name, and the header
  defines it twice. Should antic refuse it? That needs a message, and it
  is related to T281.
- A header with `struct stat` and `int stat(...)` binds to a module antic
  refuses. Should `anti bind` refuse it or rename one of the two?
- `serialize` writes the bare class name, so a class name two modules
  declare does not read back. Should it write `module.Class` there?
- T338: should the field record of a pointer and a slice of arrays carry
  the id and the count of the element?
- T341: should a parameter or a field that carries the name of an export
  constant take a trailing `_`?
- T342: which form of name should a tuple take?
- T343: how should a type name with a dot read, given the DESIGN comment
  of `registry_find`?
- T344: which text should a `?T` take when `serialize` cannot write its
  value?
- T345: should the readers of `src/rt/registry.c` keep a name on the heap
  or compare it in place?
- After T336, `index.toml` still cannot hold a value with both quotes or a
  line break. Refusing such a name needs a new message. Should it?
- `anti fmt` writes `f( /* c */ x)`, and no rule says which form is right.

The [provisional] entries of T334, T337 and T339 in `docs/decisions.md`
await review. The questions of the batches before stay open, and nothing
here acts on them.
