# Batch 3o of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this is
the first batch chosen by the branches the suite misses, and the first
that runs the fuzzer. `./c coverage` at HEAD `ce8c5e91` gave the ranking.
The step took the four files that batch 3n named: `src/anti/bindclang.c`
with 101 missed branches, `src/rt/regex.c` with 48, `src/anti/bindexpr.c`
with 32 and `src/anti/doc.c` with 30. Each side ran first through the
anti or the unit tests of the coverage tree alone, with a profile of its
own, as `docs/reports/2026-11-16-coverage-phase-3n.md` did. A clang of
the shell that writes an AST by hand is an input, as `ast_malformed`
holds. Eddie's direction is the Mac alone, so the VMs were not touched.
The probes found eight defects, T268 to T275, all of `anti bind`. The
next free id is T276. The phase goes on, since this batch found defects.

## Commits

- `d5f8a8d6` lists T268 to T273, `a2ed47f1` lists T274 and T275.
- `9f6d0785`, `a8cd9228`, `06318d03`, `18033fc5`, `112fd4aa`, `962da622`,
  `645577ee` and `d8b1be7e` fix T268 to T275 in order.
- `d02e75f5` tests `regex.c`, `8f15de97` the reader and the evaluator,
  `369c157d` the text of an empty pattern without memory and `76967659`
  the links of a page of `anti doc`. `fb465928` tests the sides of the
  fixes that the measurement after them showed missed.
- `f45c4807` records the rules of the fixes and their commits, and
  `2a83ecba` names the guards. One commit records the times and the
  counts in `CLAUDE.md` with this report.

## Findings

- T268, minor. A first field aligned without a value got the message of
  a field other than the first. It now gets one of its own.
- T269, severe. A typedef could name an anonymous record or enum by the
  tag of another type. The reader joined the two into the record declared
  first, so `take(S v)` passed another struct than C, or wrote two items
  of one name. Such a header is refused, as a description that names one struct
  twice is: `api.h names two types S`. A `[provisional]` entry records it.
- T270, major. `enum E : unsigned char;` before the definition gave two
  enums. The declarations of one tag now name one enum, C11 6.7.2.3.
- T271, severe. A packed, aligned or mode enum was bound with the base
  `c_int`. The addendum refuses every other layout directive, so such an
  enum refuses the header. A `[provisional]` entry records the message.
  The case `clang_packed_enum` bound one to reach an attribute among the
  enumerators, and became `clang_enum_with_an_attribute` with `unused`.
- T272, severe. `-8 >> 1u` was 2147483644 of `c_uint`. A shift takes the
  promoted type of its left operand, C11 6.5.7.
- T273, major. A cast to `uint8_t` or a typedef of the header was no
  constant. The evaluator takes the names of the reader, in a header and
  in a description.
- T274, minor. With two headers, a struct, a function or a constant that
  `bind_settle` left out named the first header. Each names its own.
- T275, major. `(struct S){ 300 }` of an unsigned char field gave a
  binding antic refuses. An element converts to its field, C11 6.7.9. An
  element past a long long stays left out, as `anti_bind_api_literals`
  holds by M339.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 41000.
`cmake -DFIRST=41001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 41001 to
41200 at the default size in 147 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.08%, 4437 missed | 92.36%, 4288 missed |
| Regions | 98.49%, 1276 missed | 98.49%, 1280 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1232 missed | 98.70%, 1243 missed |

| File | Branches missed before | after | Lines missed before | after |
|---|---|---|---|---|
| `bindclang.c` | 101 | 24 | 1 | 7 |
| `regex.c` | 48 | 4 | 4 | 4 |
| `bindexpr.c` | 32 | 16 | 4 | 9 |
| `doc.c` | 30 | 17 | 5 | 5 |

The second run measured the tree before `fb465928`, whose tests reach
most of the lines and sides of the new code that it shows missed. The
coverage suite ran 7519 tests, all passed, and took 985 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`, up to test 7535 at the end. The
  times stand in `CLAUDE.md`. One run covered the three test commits
  `8f15de97`, `369c157d` and `76967659`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7533. Every test passed. The last part took 580 s
  under ASan and 599 s under UBSan.
- The host runs 7535 tests and each sanitizer tree 7533.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Next candidates

By branches, `sema_expr.c` misses 160, `sema_call.c` 160 and
`sema_generic.c` 153, sides that batch 3a did not sort. `sema_class.c`
misses 97, `antl_read_ir.c` 96 and `src/anti/fmt.c` 90. `doc.c` keeps
eight sides a test can reach: a path of an import in full, a DEL byte in
a link, a tab before a line, the Markdown of a text that ends in a line
end, a module without doc text in the index, a library file that does not
build, a source that is a library file and one of another suffix.

## Questions for Eddie

No new question. The question whether `take` should also move an
`own ?fn` field stays open, and nothing here acts on it.
