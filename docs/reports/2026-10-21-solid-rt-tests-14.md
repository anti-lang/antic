# Step rt-tests, fourteenth session

The session went on from the list of `2026-10-21-solid-rt-tests-13.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/anti/`, `src/rt/` and `src/antic/` that a program, a
command or an injected fault reaches, and recorded one new finding. Logs
are under `build/drive/logs/`, named `rt14-*`. The list of the regions
not reached is `rt13-end-unreached.txt` at the start and
`rt14-end-unreached.txt` at the end. The probes are in `build/rt14/`.

## Coverage per directory

Regions reached. The start is `rt13-end-summary.txt`, measured at
`ed0df4cc`, whose sources equal those of `f71a0f25`. The end is
`03e6b831`, `rt14-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt14-end-1.log` to `-5.log`, every part passed.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.64% of 9252 | 95.70% of 9252 | 403, 398 | 96.00% | 96.04% |
| `src/antic/` | 98.11% of 55587 | 98.11% of 55587 | 1053, 1051 | 98.15% | 98.16% |
| `src/anti/` | 98.35% of 11663 | 98.42% of 11663 | 193, 184 | 98.39% | 98.45% |

## The tests

- anti, `anti_build_resolve`: f of the diamond needs e and g, and its
  index names g three times, once without a version. The lock file names
  each need of f once. A need added, a module added and a package named
  twice by hand are undone by the next build. It reaches the loops of
  `graph_add`, `package_module`, `require` and `package_need` and the list
  of needs of `lock_write` in `deps.c`, `26aaccea`.
- runtime, `tests/programs/faults_collect.anti` with the case
  `room_memory`: `to_slice` of a countdown, whose room from
  `anti_rt_grow` runs out and stops the program with its size,
  `8677baf5`. `emit_identity` gains the six lines of the program, and no
  other digest moves.
- antic, `unit_pattern`: `[[:x\d]` is a class of PCRE2 whose `\d` the scan
  of `posix_end` reads a byte at a time, `b6f99aca`.
- antic, `std_collection_text_values_no_reflect`: T175, below. Its run
  under `--no-reflect` reaches the early return of `give_fields` in
  `lower_desc.c`, `0d664d13`.
- runtime, `trap_pattern_group_name_empty`: a group name that a Builder
  left empty stops the program as a name the pattern lacks, the first
  refusal of `named_group` in `patterns.c`, `03e6b831`.

## Findings

The step closed none. One is new, with its test.

- T175, severe: a program built with `--no-reflect` writes `[{}]` and
  `[null]` for `to_text` of a `List<Point>` and a `List<?int>`, where the
  same program without the option writes `[{"x": 1, "y": 2}]` and `[4]`.
  The element text of `src/rt/object.c` walks the field lists the option
  drops, and antic neither keeps them nor refuses the call, as it refuses
  `serialize`, `deserialize` and `reflect`. Left for the step of the
  runtime's text.

`tests/known-failures.txt` held 122 lines and 103 entries, 101 expected
to fail on the Mac. It holds 123 lines and 104 entries, 102 expected to
fail on the Mac. No feature, option or command, and no `[provisional]`
entry.

## Regions nothing reaches

- `src/antic/sema_const.c` 227 to 229: the shortest digits of a float
  never end in 0, since a 17th digit 0 leaves 16 digits that read back.
  236 to 242: a float below 1 in magnitude converts to 0, which every
  integer type holds, so no conversion reports it.
- `src/antic/llvm_emit_function.c` 946 to 948: `simd_operation` checks
  every shift of integer lanes and writes it lane by lane, so `vbinary`
  never meets a shift, `build/rt14/t/sh.anti`.
- `src/antic/llvm_emit.c` 27 to 36 and 52 to 62: the lexer refuses a
  byte past ASCII outside a literal, and a module path is a lowercase
  identifier. So no symbol needs quotes, `build/rt14/t/u.anti` and
  `build/rt14/t/sp`.
- `src/antic/pattern.c` 432, 504, 514, 548 and 556 to 560: a class that
  PCRE2 takes ends with `]`, which the POSIX scan meets first. `\c` at the
  end, `\Q` without `\E` in a class and a range that ends in a set,
  `[a-\d]`, are malformed for PCRE2 before the reader runs.
- `src/antic/sema_copies.c` 217, the `NULL` arm, 223 and 226: the field
  of `xf` lies in a class of the chain of its type. A field of a struct
  is the struct's own.
- `src/antic/sema_call.c` 1345: `HANDLE_ENCLOSING` stands only where a
  `try` block is open, and the test before the `switch` returns for it.
- `src/antic/parser_stmt.c` 330, 358 and 422: the lookahead checked a
  name before `parser_expect_name` runs, and no caller passes `required`
  false without a `catch`.
- `src/antic/whole.c` 572 to 577: an element of an array field and a part
  of a tuple field are address steps of their own. Each follows the
  offset of the field, `build/rt14/t/wr`.
- `src/rt/registry.c` 507 to 509 and 539 to 541: the count of elements is
  bounded by the length of the text, and a class element always has a
  descriptor. Allocation faults 7 to 11 of a deserialize of an `own`
  slice and an `own` pointer reach neither.
- `src/rt/mem.c` 97: no caller passes a size below zero to
  `anti_rt_grow`.

## Not run on the target

None. Every change runs on the Mac, and the suites of new VMs run
`faults_collect`, `trap_pattern_group_name_empty` and
`std_collection_text_values_no_reflect` as well.

## Gates

- No warning of our code: `rt14-hb6.log`, `rt14-asan-build.log` and
  `rt14-ubsan-build.log`.
- Host, 5704 tests, `-I 1,2000` and `-I 2001,5704`: 2000 and 3730 passed,
  in 349 s and 313 s. Part 2 runs 26 fixtures of part 1 again. 102
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt14-host-1.log` and `rt14-host-2.log`.
- ASan, 5702 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5702`: 1151, 1105, 1576, 906 and 1004 passed in 236 s, 181 s,
  186 s, 455 s and 351 s, `rt14-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1105, 1576, 906 and 1004 passed in 236 s,
  163 s, 137 s, 172 s and 294 s, `rt14-ubsan-1.log` to `-5.log`.
- `emit_identity` passes with the six new lines of `faults_collect`, and
  `link_identity_macos-arm64` passes unchanged.
- The docs-style checker reports nothing on this report and on
  `docs/audit/tests-findings.md`.

## State at the push

```text
$ git log --oneline -3
03e6b831 Test an empty group name computed at run time
0d664d13 Test the text of a collection of values without the field lists
b6f99aca Test an escape inside a class that opens like a POSIX class
$ git status --short
$ git rev-parse HEAD origin/main
03e6b831cdc6776b13b9fd001fe0fd1017494b59
03e6b831cdc6776b13b9fd001fe0fd1017494b59
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 184 places: `bindclang.c` and `bindwrite.c`. `repo.c` 293
  to 295: an empty index of a `file://` repository beside a cached one is
  read as the answer of no change, and no document says how an empty
  index reads.
- `src/antic/`, 1051 places. `build/rt14/untriaged.txt` lists those no
  report has given a reason for yet, `driver_link.c`, `sema_generic.c`,
  `antl_tree.c` and `whole_tables.c` among the most.
- `src/rt/`, 398 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `text.c` 424 and 700.
