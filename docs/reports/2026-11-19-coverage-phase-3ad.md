# Batch 3ad of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it found their defects by reading readers, so this batch
read the readers of text that no batch had read: `anti.text`, the format
specifications of `f"..."` and the Builder of the runtime, `anti.json`,
`anti.time`, the glue of `anti.regex`, `src/rt/text.c` and `src/rt/utf.c`,
the input of `anti fmt` and the `///` comments and fences of `anti doc`
and `anti check`, the lexer with the escapes and the numbers of its
literals, and the arguments of `link`, `allow` and `unchecked`. It read
the reader of a pattern literal in `src/antic/pattern.c` and the reader of
`#pragma pack` of `anti bind --clang`, and sorted the sides of
`src/anti/bindclang.c` that no test reaches. Eddie's direction is the Mac
alone, so the VMs were not touched. The batch found sixteen defects, T317
to T332, and fixed them all. The next free id is T333.

## Commits

- `26bcd565` lists T317 to T332.
- `39ddbc5c` fixes T317, `dc69f248` T318, `5f347a8c` T319 and `aa15e297`
  T320.
- `dd933c7c` fixes T321, `3a0c3afc` T322, `53e6ecc6` T323, `f1da2b23`
  T324, `fcb07a71` T325 and `8a949866` T326.
- `1d55257e` fixes T327 and `11b95a95` T328.
- `62d8f9f5` fixes T329, `41080ca8` T330, `0b2567c4` T331 and `cb4d4fac`
  T332.
- `7430d45a` records the times in `CLAUDE.md`, the commits of the fixes in
  `docs/audit/tests-findings.md` and the guards of the batch in
  `docs/notes/unreached-guards.md`.

## Findings

- T317, severe, fixed. `pragma_pack` of `src/anti/bindclang.c` dropped the
  value of `#pragma pack(pop, 2)`, popped one level for a label and set
  the pack to 0 for a pop that found nothing and for an unknown action. A
  struct of pack 2 after a pop was bound as `packed`, and a struct of
  pack 1 after the other forms was refused.
- T318, major, fixed. The reader of a pattern literal read the name of a
  verb, `(*MARK:abc)`, as text of the match. A `patch` whose `with` cannot
  fit then compiled and stopped the program with the message of a missing
  group.
- T319, major, fixed. The reader counted the text after `(*ACCEPT)` in the
  fewest bytes of a match, with the same result.
- T320, major, fixed. The reader cut a callout at the first `)` of its
  string, with the same result, and a nested repeat in the string failed
  `exponential-pattern`.
- T321, major, fixed. `anti fmt` wrote the brace of a block that follows
  a line comment into the comment, and antic refused the file.
- T322, major, fixed. `anti fmt` dropped the parentheses around a
  condition that holds a struct literal, which antic then refused.
- T323, minor, fixed. `anti fmt` indented the statement after an `if`
  whose condition holds a handler on one line one tab deeper.
- T324, major, fixed. The fill of `anti fmt` read no fence after a tab and
  joined the fence and its code into the paragraph.
- T325, major, fixed. The fill started a line with a `-` word or three
  backticks, which opened a list or a fence.
- T326, minor, fixed. `anti fmt` kept the indent under the gutter of a
  block doc comment, which a second run removed.
- T327, minor, fixed. The index of `anti doc` rendered a first paragraph
  line by line, so a code span cut over two lines stayed text.
- T328, minor, fixed. `anti check` ended a code span at the end of its
  line and lost the `doc-unresolved` warning of the name after it.
- T329, severe, fixed. `f"..."` wrote a `c_wchar` that holds -1 on macOS
  as 18446744073709551615.
- T330, minor, fixed. A float below a power of two took one digit more
  than the fewest, 3 values of `f32` and 46 of `f64`.
- T331, severe, fixed. The limit of `patch` counted a match whose group
  took no part, so `limit` 1 and -1 patched nothing.
- T332, major, fixed. A byte pattern read a class inside a `#` comment of
  `(?x)` and was refused.

## Read without a find

- `parse_int` of `anti.text` reads the lowest `int` and refuses every
  value outside the range. The parser of format specifications refuses a
  `0` with an alignment, a width of more than nine digits and a `.`
  without digits.
- The lexer refuses every malformed number, escape and character literal
  tried. `007.5` is a float, since the rule of a leading zero names an
  integer literal alone.
- `link framework ""` and a framework named twice are refused, and so are
  a clause without its reason and a name that is no warning.
- `anti.json`, `src/rt/utf.c`, the reading of floats and the templates of
  `replace` give the expected result for every case tried. `anti.time` reads no text.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 44000.
`cmake -DFIRST=44001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 44001 to
44200 at the default size in 161 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.42%, 3714 missed of 56459 | 93.38%, 3751 missed of 56677 |
| Regions | 98.52%, 1262 missed | 98.52%, 1269 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1224 missed | 98.72%, 1231 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/anti/fmt.c` | 80 | 95 |
| `src/rt/regex.c` | 4 | 12 |
| `src/antic/pattern.c` | 43 | 49 |
| `src/anti/doc.c` | 10 | 13 |
| `src/anti/bindclang.c` | 34 | 36 |

The fixes add 218 branches, and the suite reaches 181 of them. The sides
of the fixes it misses stand unsorted at the end of the section of the
batch in `docs/notes/unreached-guards.md`, and the section names the
guards of `src/anti/bindclang.c`. `src/rt/mem.c` and `src/rt/threads.c`
moved by one each, since the threads of the suite take other paths from
run to run. The coverage run had 7913 tests, all passed, and `./c
coverage` took 1058 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7923. Every test passed.
- The host runs 7925 tests and each sanitizer tree 7923.
  `tests/known-failures.txt` holds the two lines of `host=windows` and
  the line of T316.
- Three expectations of the list commit changed in the fixes. The test of
  T329 frees the texts it prints, whose leak `--memory-checks` showed once
  the output was right. The test of T331 expected `X` over the `a` of each
  match, where the span of `a(b)?` is its group. The test of T332 holds its
  pattern as a literal, since a pattern of `ByteRegex.compile` is reported
  as a leak, below.

## Not reached

- A pattern that `Regex.compile` or `ByteRegex.compile` gives is never
  given back. The runtime marks no block of it as kept, as it marks the
  blocks of a literal. A program of `--memory-checks` that no longer
  holds one gets a leak report it cannot fix. Marking them, or giving
  them back, is a choice of the design, so the batch lists no finding.
- The fill of `anti fmt` may still start a line with `#`, `|` or digits
  and a `.`, such as `0.0 to 1.0`, which `anti doc` renders as text and
  the doc warnings read as a heading, a table or a numbered list. The fix
  of T325 holds the markup `anti doc` renders, since the wider rule moved
  three lines of the checkout past 80 columns.
- The fill joins two spaces inside a code span into one.
- A comment after the `{` of a block moves to the next line, and the
  comment lines of a file with CRLF keep their CR.
- `anti_rt_builder_float` caps a precision at 100000000 without a message.

## Questions for Eddie

- Should a pattern of `Regex.compile` and `ByteRegex.compile` be marked as
  kept for `--memory-checks`, as a literal is, or be given back?
- The doc warnings read `#`, `|` or digits and a `.` at the start of a
  line as markup. Should the fill start no line with them, at the price
  of lines past 80 columns?
- Should the fill keep two spaces inside a code span?

The [provisional] entries of T321, T322 and T325 in `docs/decisions.md`
await review. Sixteen questions stay open from before, and nothing here
acts on them.
