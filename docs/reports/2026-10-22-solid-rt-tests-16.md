# Step rt-tests, sixteenth session

The session went on from the list of `2026-10-21-solid-rt-tests-15.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/anti/`, `src/rt/` and `src/antic/` that a program, a
command or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt16-*`. The list of the regions not reached is
`rt15-end-unreached.txt` at the start and `rt16-end-unreached.txt` at the
end. The probes and scripts are in `build/rt16/`.

## Coverage per directory

Regions reached. The start is `rt15-end-summary.txt`, measured at
`45a2ab21`, whose sources equal those of `7be81040`. The end is
`4f1b3be8`, `rt16-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt16-end-1.log` to `-4.log` and `rt16-end-5b.log`. Part 5 ran
twice: `rt16-end-5.log` failed `fmt_canonical` for three new sources,
which `ee31ca65` and `4f1b3be8` settle.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.81% of 9252 | 95.88% of 9252 | 388, 381 | 96.07% | 96.13% |
| `src/antic/` | 98.13% of 55587 | 98.19% of 55587 | 1038, 1004 | 98.19% | 98.26% |
| `src/anti/` | 98.54% of 11663 | 98.63% of 11663 | 170, 160 | 98.60% | 98.69% |

## The tests

- anti, two cases of `tests/anti-build/messages.cmake`: two failed
  `realpath` calls for the root of a build or for the dependency that
  closes a path cycle, `build_project` and `path_cycle` of `build.c`,
  `2b615797`.
- runtime, `plugin_versions_fault_stubs_memory`: the list of stub tables
  and a table of stubs of a load run out of memory, `claim` and
  `stubs_of` of `plugin.c`, `aaab8b49`.
- antic, a matrix refusal: a call in a class literal of a default
  parameter, through the base field and its own, `class_value` of
  `sema_const.c`, `6189591c`.
- antic, `antic_message_fault_frameworks_without_an_sdk`: the listing of
  the SDKs of the Command Line Tools fails, `apple_sdk` of
  `driver_link.c` and `apple_clt_sdk` of `applesdk.c`, `e3285357`.
- antic, the framework tests: a Mac links through `sdk/` of a stand-in
  sysroot and records the version of `sdk/sdk-version`, `apple_sdk`,
  `c3c0c43b`. Before it only Linux and Windows read `sdk/`.
- antic, two cases of `tests/doc/messages.cmake`: the library file of an
  import does not read in the search or in the load of a page,
  `interface_of` of `driver.c`, `f2a0346a`.
- antic, two cases of `tests/antic_messages.cmake`: `-S` when opt or llc
  does not start, `driver_run` of `driver.c` and `llvm_run` of
  `llvm_run.c`, `d9e3a233`.
- anti, `anti_build_index_unchanged` and `anti_build_index_absent`: an
  index the server answers as unchanged by its entity tag, and one it
  lacks, `fetch_http` and `fetch` of `repo.c`, `71d6b12a`. The fake curl
  of the redirect cases saves a tag and answers 304.
- antic, two matrix tests: `+%` on a type parameter, `param_binary` of
  `sema_operator.c`, `cf2cf5dd`, and a module operator of no parameters
  beside a struct, `takes_first`, `9b381c22`.

## Findings

The step closed none. It recorded three, each with its test in
`tests/known-failures.txt`:

- T176, `00dc41b9`: `a + 5` with `operator fn add<T: Number>(a: Key<T>,
  b: T)` is refused, since `right_literal_type` of `sema_operator.c`
  gives the literal the type of the left operand.
- T177, `1df046cc`: `==` between an `own fn` field and a plain function
  value is refused with two type names that leave out `own`.
- T178, `ee31ca65`: `anti fmt` takes the brace of a struct literal in a
  default parameter for the body of the function. `fmt_canonical` passes
  over its source and over the matrix program of `6189591c` while it is
  open.

`tests/known-failures.txt` held 125 lines and 106 entries, 104 expected
to fail on the Mac. It holds 130 lines and 111 entries, 109 expected to
fail on the Mac. No feature, option or command, and no `[provisional]`
entry.

## Regions nothing reaches

- `src/rt/object.c` 296: an array length is at least 1, so the walk of a
  fixed array never sees a level of length 0.
- `src/rt/text.c` 424: a rounding of the value lies at or above it. When
  a power of ten lies between the value and the high end, every rounding
  the loop tries reaches it. So the point of the rounded value is never
  below that of either end.
- `src/anti/fmt.c` 430: `angle_list` is entered at a `<`, and a pending
  half of `>>` stands at a `>>`, so no list starts half closed. 981: every
  caller of `space_before` passes a token, and a comment goes through
  `emit_comment`. 1470: the first line of a block comment holds `/*`, so
  `flush` writes it and clears the pending blank line before the second.
- `src/antic/llvm_emit_function.c` 147, 388, 393, 473, 477, 555, 559,
  613, 774, 902, 1051, 1197, 1230, 1252, 1290, 1358, 1399, 1553, 1679,
  1684 and 1740: each stops at an operand of no LLVM type or after
  `unexpected`. That function stands for a form that lowering never
  writes.
- `src/antic/header.c` 694: a simd struct of `char` lanes is refused.
- `src/antic/driver.c` 1558, 1563, 1586 and 1587: `anti doc` compiles
  each source with the same options before it asks for its page, so the
  page's source lexes, parses, names its module and checks.

## Not run on the target

- `plugin_versions_fault_stubs_memory` counts the allocations of the
  Mac. A Linux host may count others before the load.
- The fault counts of `fault_path_cycle_root_unresolved`,
  `fault_path_cycle_dependency_unresolved`, `fault_page_import_unfound`,
  `fault_page_import_unloaded` and `fault_assembly_llc_not_started` are
  those of the Mac.
- `anti_build_index_unchanged` and `anti_build_index_absent` run on
  Linux as well.

## Gates

- No warning of our code: `rt16-hb.log`, `rt16-asan-build.log` and
  `rt16-ubsan-build.log`.
- Host, 5744 tests, `-I 1,2000` and `-I 2001,5744`: 2000 and 3771 passed,
  in 377 s and 328 s. Part 2 runs 27 fixtures of part 1 again. 109
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt16-host-1.log` and `rt16-host-2.log`.
- ASan, 5742 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5742`: 1151, 1106, 1577, 906 and 1044 passed in 244 s, 190 s,
  191 s, 455 s and 364 s, `rt16-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1044 passed in 244 s,
  170 s, 142 s, 178 s and 314 s, `rt16-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report and on
  `docs/audit/tests-findings.md`.

## State at the push

```text
$ git log --oneline -3
4f1b3be8 Write two matrix programs in the canonical form
ee31ca65 Record T178, anti fmt and a struct literal in a default parameter
9b381c22 Test a module operator of no parameters beside a struct
$ git status --short
$ git rev-parse HEAD origin/main
4f1b3be80ede399d087d9f2a42a4305ce3cee594
4f1b3be80ede399d087d9f2a42a4305ce3cee594
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 160 places: `bindclang.c` and `bindwrite.c`, which a fake
  clang on `PATH` reaches with an AST of its own, as `clang_with_a_cut_ast`
  does. `repo.c` 349 and `deps.c` 373 need a library header that names a
  dependency, and `build.c` 198, 199, 440 and 687.
- `src/antic/`, 1004 places. `build/rt16/untriaged.py` lists those no
  report has given a reason for yet. `sema_generic.c`, `sema_copies.c`,
  `whole_tables.c`, `antl_tree.c` and `antl_read_ir.c` hold the most.
  `sema_const.c` 577 and 657 were not reached by a narrowing `as` or a
  unary `-` in a `const`, `build/rt16/t/k1.anti` and `k2.anti`, and
  `sema_thread.c` 391 not by a `join` inside `?.`, `oj.anti`.
  `coff.c` 782 needs a section of 65535 relocations.
- `src/rt/`, 381 places: `trace.c` of ELF and PE, `registry.c` 207, 769,
  790 and 927, `object.c` 80, 243 and 341, and `plugin.c` 642, 719, 864,
  1248 and 1279.

CONTINUE
