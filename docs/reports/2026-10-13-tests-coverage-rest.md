# The regions of the rest of antic and of anti that no test reached

The first session of a step of the test run, on 2026-10-13, on `main`. The
step does for the rest of `src/antic/` and for `src/anti/` what the step of
`docs/reports/2026-10-13-tests-coverage-front-end.md` did for the front
end. It ran `./c coverage` at the start and at the end, read the regions
the suite did not reach and wrote tests for them. No file under `src/`
changed. The step is not finished: this session read the lowering files,
`ir.c`, `ir_fold.c`, `ir_print.c` and `ast_dump.c`, and the last section
lists the rest.

## What the session added

- 18 programs in `tests/matrix/`, 54 tests. 14 are of the first commit, for
  regions of `lower*.c`: a bound function that takes a closure, called
  twice, `as c_wchar`, the operators of a simd struct with `f16` lanes, the
  loops of one above the vector cap, a default of a parameter that holds a
  class, a constant aggregate of `size_of`, `none == v` of a `?int`, a
  constant of `size_of` shifted and widened, a walk over an array of a
  `size_of` length, a range of `c_long`, the flags form into names in
  memory, array fields of enums in a descriptor and two programs with a
  `defer` that never returns.
- The other four: `sync` on a synchronized value in place, `==` and hash of
  a struct with a Mutex, the folds of the back end over `size_of`, and
  `classes_changed_hook_of_the_second_field`.
- `dump_ast_nodes` and `dump_types_checked_nodes` in `tests/dump/`. The
  first holds the nodes of the parsed tree that no other dump held, in a
  source the checker would refuse. The second holds the nodes the checker
  writes. The two reach 96 of the 105 unreached regions of `ast_dump.c`.
- `program_hook_trace_module` passes the module of `hook_trace.anti` as the
  value of `--trace`. `matrix_threads_jobs_no_hooks` joins jobs under
  `--no-hooks`.
- `matrix_classes_changed_hook_of_the_second_field_trace_writes` runs the
  matrix program under `--trace writes`. Its expected file is
  `tests/programs/hook_changed_fields.writes.expected`.

## The coverage of each file

The table holds every file of `src/antic/` outside the front end and every
file of `src/anti/`, with the regions that no test reached and the share of
regions and of lines the suite reaches, before and after the session.
`build/drive/logs/c4-before-summary.txt` and `c4-after-summary.txt` hold the
rows, and `c4-before-missed/` and `c4-after-missed/` one list of unreached
regions per file.

| File | Regions | Missed before | Missed after | Regions before | Regions after | Lines before | Lines after |
|---|---|---|---|---|---|---|---|
| `anti/bind.c` | 91 | 12 | 12 | 86.81% | 86.81% | 88.68% | 88.68% |
| `anti/bindapi.c` | 527 | 46 | 46 | 91.27% | 91.27% | 89.29% | 89.29% |
| `anti/bindclang.c` | 1250 | 100 | 100 | 92.00% | 92.00% | 93.29% | 93.29% |
| `anti/bindexpr.c` | 507 | 51 | 51 | 89.94% | 89.94% | 89.21% | 89.21% |
| `anti/bindmodel.c` | 32 | 2 | 2 | 93.75% | 93.75% | 88.68% | 88.68% |
| `anti/bindtype.c` | 462 | 24 | 24 | 94.81% | 94.81% | 93.44% | 93.44% |
| `anti/bindwrite.c` | 501 | 48 | 48 | 90.42% | 90.42% | 92.56% | 92.56% |
| `anti/build.c` | 563 | 39 | 39 | 93.07% | 93.07% | 94.82% | 94.82% |
| `anti/check.c` | 271 | 22 | 22 | 91.88% | 91.88% | 90.31% | 90.31% |
| `anti/cursor.c` | 63 | 2 | 2 | 96.83% | 96.83% | 96.61% | 96.61% |
| `anti/deps.c` | 775 | 45 | 45 | 94.19% | 94.19% | 93.40% | 93.40% |
| `anti/doc.c` | 550 | 75 | 75 | 86.36% | 86.36% | 85.74% | 85.74% |
| `anti/files.c` | 259 | 21 | 21 | 91.89% | 91.89% | 91.53% | 91.53% |
| `anti/fmt.c` | 1335 | 36 | 36 | 97.30% | 97.30% | 97.02% | 97.02% |
| `anti/jsontree.c` | 223 | 13 | 13 | 94.17% | 94.17% | 91.63% | 91.63% |
| `anti/licensing.c` | 251 | 6 | 6 | 97.61% | 97.61% | 98.00% | 98.00% |
| `anti/main.c` | 759 | 19 | 19 | 97.50% | 97.50% | 96.43% | 96.43% |
| `anti/manifest.c` | 237 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `anti/memreport.c` | 151 | 12 | 12 | 92.05% | 92.05% | 92.72% | 92.72% |
| `anti/platform.c` | 73 | 1 | 1 | 98.63% | 98.63% | 96.43% | 96.43% |
| `anti/project.c` | 26 | 3 | 3 | 88.46% | 88.46% | 89.09% | 89.09% |
| `anti/repo.c` | 287 | 20 | 20 | 93.03% | 93.03% | 89.73% | 89.73% |
| `anti/sdk.c` | 154 | 15 | 15 | 90.26% | 90.26% | 92.24% | 92.24% |
| `anti/symmap.c` | 271 | 26 | 26 | 90.41% | 90.41% | 94.93% | 94.93% |
| `anti/syms.c` | 662 | 68 | 68 | 89.73% | 89.73% | 91.35% | 91.35% |
| `anti/test.c` | 148 | 20 | 20 | 86.49% | 86.49% | 87.17% | 87.17% |
| `anti/units.c` | 137 | 14 | 14 | 89.78% | 89.78% | 91.19% | 91.19% |
| `anti/zip.c` | 391 | 35 | 35 | 91.05% | 91.05% | 93.44% | 93.44% |
| `antic/abi.c` | 231 | 5 | 5 | 97.84% | 97.84% | 100.00% | 100.00% |
| `antic/alloc.c` | 39 | 2 | 2 | 94.87% | 94.87% | 100.00% | 100.00% |
| `antic/antic.c` | 112 | 4 | 4 | 96.43% | 96.43% | 97.24% | 97.24% |
| `antic/antl.c` | 87 | 1 | 1 | 98.85% | 98.85% | 98.37% | 98.37% |
| `antic/antl_io.c` | 91 | 3 | 3 | 96.70% | 96.70% | 96.88% | 96.88% |
| `antic/antl_read.c` | 930 | 77 | 77 | 91.72% | 91.72% | 88.68% | 88.68% |
| `antic/antl_read_ir.c` | 988 | 61 | 61 | 93.83% | 93.83% | 90.23% | 90.23% |
| `antic/antl_tree.c` | 1117 | 82 | 82 | 92.66% | 92.66% | 94.42% | 94.42% |
| `antic/antl_verify.c` | 776 | 37 | 37 | 95.23% | 95.23% | 94.66% | 94.66% |
| `antic/antl_write.c` | 615 | 21 | 21 | 96.59% | 96.59% | 97.56% | 97.56% |
| `antic/applesdk.c` | 59 | 3 | 3 | 94.92% | 94.92% | 96.15% | 96.15% |
| `antic/arena.c` | 23 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/arith.c` | 228 | 6 | 6 | 97.37% | 97.37% | 100.00% | 100.00% |
| `antic/ast_dump.c` | 685 | 105 | 9 | 84.67% | 98.69% | 81.40% | 98.78% |
| `antic/ast_walk.c` | 134 | 3 | 3 | 97.76% | 97.76% | 98.79% | 98.79% |
| `antic/coff.c` | 897 | 47 | 47 | 94.76% | 94.76% | 94.59% | 94.59% |
| `antic/cpu.c` | 18 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/diagnostic.c` | 24 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/docpage.c` | 486 | 49 | 49 | 89.92% | 89.92% | 90.54% | 90.54% |
| `antic/driver.c` | 1261 | 101 | 101 | 91.99% | 91.99% | 89.99% | 89.99% |
| `antic/driver_index.c` | 103 | 10 | 10 | 90.29% | 90.29% | 93.15% | 93.15% |
| `antic/driver_library.c` | 259 | 31 | 31 | 88.03% | 88.03% | 91.23% | 91.23% |
| `antic/driver_link.c` | 328 | 57 | 57 | 82.62% | 82.62% | 85.33% | 85.33% |
| `antic/driver_search.c` | 248 | 21 | 21 | 91.53% | 91.53% | 89.59% | 89.59% |
| `antic/header.c` | 1068 | 63 | 63 | 94.10% | 94.10% | 95.88% | 95.88% |
| `antic/ir.c` | 415 | 9 | 8 | 97.83% | 98.07% | 98.29% | 98.55% |
| `antic/ir_fold.c` | 98 | 3 | 0 | 96.94% | 100.00% | 97.59% | 100.00% |
| `antic/ir_print.c` | 368 | 6 | 6 | 98.37% | 98.37% | 99.63% | 99.63% |
| `antic/ir_verify.c` | 724 | 30 | 30 | 95.86% | 95.86% | 91.69% | 91.69% |
| `antic/layout.c` | 485 | 34 | 33 | 92.99% | 93.20% | 92.97% | 92.97% |
| `antic/linker.c` | 407 | 12 | 12 | 97.05% | 97.05% | 97.90% | 97.90% |
| `antic/llvm_debug.c` | 201 | 4 | 4 | 98.01% | 98.01% | 97.30% | 97.30% |
| `antic/llvm_emit.c` | 1920 | 87 | 87 | 95.47% | 95.47% | 96.05% | 96.05% |
| `antic/llvm_run.c` | 85 | 6 | 6 | 92.94% | 92.94% | 94.96% | 94.96% |
| `antic/llvm_target.c` | 13 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/lower.c` | 888 | 11 | 9 | 98.76% | 98.99% | 99.45% | 99.45% |
| `antic/lower_check.c` | 88 | 3 | 3 | 96.59% | 96.59% | 100.00% | 100.00% |
| `antic/lower_class.c` | 251 | 14 | 14 | 94.42% | 94.42% | 97.27% | 97.27% |
| `antic/lower_desc.c` | 819 | 20 | 17 | 97.56% | 97.92% | 97.73% | 98.05% |
| `antic/lower_eq.c` | 178 | 4 | 3 | 97.75% | 98.31% | 97.49% | 97.91% |
| `antic/lower_error.c` | 114 | 3 | 3 | 97.37% | 97.37% | 97.87% | 97.87% |
| `antic/lower_expr.c` | 1256 | 29 | 26 | 97.69% | 97.93% | 98.85% | 98.85% |
| `antic/lower_function.c` | 225 | 1 | 1 | 99.56% | 99.56% | 99.02% | 99.02% |
| `antic/lower_hash.c` | 160 | 8 | 7 | 95.00% | 95.62% | 96.08% | 96.64% |
| `antic/lower_hook.c` | 120 | 14 | 10 | 88.33% | 91.67% | 88.60% | 92.98% |
| `antic/lower_owning.c` | 330 | 11 | 10 | 96.67% | 96.97% | 97.15% | 97.34% |
| `antic/lower_pattern.c` | 33 | 1 | 1 | 96.97% | 96.97% | 100.00% | 100.00% |
| `antic/lower_place.c` | 331 | 29 | 13 | 91.24% | 96.07% | 88.43% | 95.37% |
| `antic/lower_simd.c` | 202 | 25 | 6 | 87.62% | 97.03% | 91.22% | 97.23% |
| `antic/lower_stmt.c` | 1053 | 18 | 12 | 98.29% | 98.86% | 97.94% | 98.51% |
| `antic/lower_sync.c` | 118 | 3 | 2 | 97.46% | 98.31% | 100.00% | 100.00% |
| `antic/lower_worker.c` | 127 | 5 | 3 | 96.06% | 97.64% | 100.00% | 100.00% |
| `antic/main.c` | 400 | 6 | 6 | 98.50% | 98.50% | 98.59% | 98.59% |
| `antic/memcheck.c` | 158 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/modpath.c` | 121 | 3 | 3 | 97.52% | 97.52% | 97.92% | 97.92% |
| `antic/notice.c` | 58 | 1 | 1 | 98.28% | 98.28% | 100.00% | 100.00% |
| `antic/optimize.c` | 1233 | 20 | 20 | 98.38% | 98.38% | 98.10% | 98.10% |
| `antic/platform.c` | 274 | 49 | 49 | 82.12% | 82.12% | 84.32% | 84.32% |
| `antic/ptrset.c` | 81 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/reach.c` | 160 | 1 | 1 | 99.38% | 99.38% | 98.75% | 98.75% |
| `antic/rt_abi.c` | 14 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/sha256.c` | 8 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/target.c` | 54 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/text.c` | 72 | 6 | 6 | 91.67% | 91.67% | 91.94% | 91.94% |
| `antic/userdirs.c` | 133 | 11 | 11 | 91.73% | 91.73% | 93.89% | 93.89% |
| `antic/warnings.c` | 98 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/whole.c` | 767 | 42 | 42 | 94.52% | 94.52% | 95.26% | 95.26% |
| `antic/whole_tables.c` | 997 | 46 | 46 | 95.39% | 95.39% | 96.28% | 96.28% |
| All 96 | 37400 | 2139 | 1979 | 94.28% | 94.71% | 94.51% | 95.04% |

All files of the measurement, `src/rt/` and the front end included, went
from 3685 unreached regions of 66919 to 3524, 94.49% to 94.73%, and from
94.51% of the lines to 94.83%. `sema_hash.c` of the front end went from 3
unreached regions to 2: the struct with a Mutex reaches line 346, which the
front-end report lists as a guard.

## The findings

- T114, severe: under `--trace writes` the `changed` hook gets the record
  of the first field of the class for a write of any of its fields. The IR
  takes `offset_of` of a field of the array type of the field list, which
  has none, and the back end gives 0. `docs/audit/tests-findings.md` holds
  the row.

## Regions no input reaches

A guard is a test for `NULL`, for a refused type or for a kind that no
caller passes.

- `lower.c` 145: the one caller passes `true`. 878 and 992 are guards.
- `lower_class.c` 177 and 391: the global and the thunk are asked for
  once. 503: an aggregate result is a pointer in the thunk already. 606:
  the checker refuses `T(args)` of a class without `construct`.
- `lower_place.c` 259 and 263: a static field is atomic, so it is read
  with `load()` and written with `store(v)` and is no place. 97, 202 and
  213 are guards.
- `lower_stmt.c` 919: `_` is an ordinary name, so every name of the form
  has a symbol. 980 and 1101: the checker refuses an `else` of a `let`
  that does not leave. 765, 957, 1169 and 1828 are guards.
- `lower_expr.c` 743, 745, 2145 and 2147: `lower_place` finds a place for
  every node these four pass.
- `lower_owning.c` 236, 280, 342 and 395: a struct and a tuple hold no
  `use` field. 249 and 292 are the `default` of a switch over the kinds
  with a teardown.
- `lower_worker.c` 362: `send` and `recv` refuse a pointer to a channel.
- `ir.c` 127, 429 and 577 end antic when a count leaves 32 bits. 236 is a
  `default`.
- `ir_print.c` 167 and 259: the parser refuses a struct without a field
  and a variadic function without a fixed parameter. 176 and 426 are
  guards.
- `ast_dump.c` 111 is a guard. 278: neither the parser nor the checker
  makes a descriptor node. 1030 to 1036: a function at module level has
  no contract and no `self`.

## Probes that reached nothing new

A later session need not repeat them.

- The address of a name that `if let` binds, and of one that a `catch`
  with a block yields, for `lower_expr.c` 420 and `lower_stmt.c` 340.
- `h as f16` of an `f16`, for `lower_expr.c` 1577.
- `yield 5;` in the handler of a call that stands as a statement, for
  `lower_stmt.c` 1499.
- An interface function that returns a struct, for `lower_class.c` 503.
- `==` and `hash()` of a class with an interface and a bitfield, and
  `hash()` of a struct with bitfields, for `lower_hash.c` 458.
- `>>` and `<<` of two simd structs with `i32` and `u32` lanes, in release
  and in dev mode, for `llvm_emit.c` 1381 and 1433.

## Rules the documents do not state

- The default `==` and the default hash of a struct that holds a Mutex
  pass over the lock and take the other fields.
- A `defer` whose block calls a function that returns `never` is taken.
  The function ends inside the defer, on `return;` and on `return v;`.
- `as c_wchar` of an `i64` and of a `u16` is taken, and `as` back to each.
- A default of a parameter may be a literal that holds a class value. An
  array of classes, a struct with a class field and a tuple with one are
  taken.
- `(r, f) = e;` takes a name `r` whose address the program took.
- A length computed from `size_of` is refused in the field of a class and
  in the default of one. The field of a struct that the class holds takes
  it.

## Where the task and the code differ

- `./c coverage` ran as `./c coverage part 1/2` and `part 2/2`, since one
  call ends after ten minutes.
- `emit_identity` pins every program of `tests/programs/`, so a new program
  there adds lines to its manifest. The program of T114 therefore stands
  in the matrix, and its case under `--trace writes` is registered in
  `tests/CMakeLists.txt` with an expected file in `tests/programs/`.
- CLAUDE.md asks for a report under two pages, and the task for the
  coverage of each file. The table of 96 files makes this report longer.

## Counts and checks

- 4456 tests on the host before and 4515 after, 368 expected failures
  before and 369 after.
- Host suite: 4515 of 4515 passed, 369 of them expected failures, two
  skipped, in 482 s, in `build/drive/logs/c4-suite-host.log`.
- ASan and UBSan: 4514 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2200` and `-I 2201,4514`. Every part passed. `asan` took 271 s,
  358 s and 296 s, and `ubsan` 249 s, 344 s and 222 s. The second and the
  third part run the setups of their fixtures again, so the three counts
  are 1150, 1062 and 2333. The logs are `c4-suite-asan-1.log` to `-3.log`
  and `c4-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c4-before-1.log` and `-2.log`, `c4-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Proof

`git log --oneline -3` after the push of the tests and of the counts,
before the commit of this report:

```text
39a6d0a4 State the counts of the first session on the rest of antic in CLAUDE.md
2478f0a6 Move the program of the changed hook into the matrix
92c090d1 Test the tree dumps, the module pattern of --trace and join under --no-hooks
```

`git rev-parse HEAD origin/main` after that push:

```text
39a6d0a4e738fd42cc8bb0032bdf8f8587ad4663
39a6d0a4e738fd42cc8bb0032bdf8f8587ad4663
```

`git status --short` printed nothing then. The reply of the session holds
the three outputs after the last push, since the hash of this commit is not
known inside it.

## Left for the next session

The scripts of this session stand in `build/cov4/`: `missed.py` lists the
unreached regions of an export of llvm-cov, `ctx.py` prints the source
around them, `p.sh` and `pm.sh` compile a probe with the antic of the
coverage tree and print the regions it reached, and `mk.sh` formats a
matrix program and writes its expected file from the three modes. The
counts below are unreached regions after this session.

Lowering, read and still open. Most of these need a library file or an
option, so a test of `tests/modules/` or a case of `tests/CMakeLists.txt`:

- `lower_class.c` 83, 101 and 207, `lower_owning.c` 711, `lower_place.c`
  454 and 517, `lower_function.c` 89 and `lower_expr.c` 2125: a class, a
  default, a singleton or an `extern fn` that a library file brought.
- `lower_class.c` 362 and `lower_desc.c` 1549: a table entry that neither
  the class nor the interface fills.
- `lower_hook.c` 45: `--trace` with a package above the module, as `com`
  for `com.example.x`. 129, 142 and 165: `--trace writes` on a field that
  `lower_listed_field` passes over. 77 and 93 are not read yet.
- `lower_desc.c` 519, 746, 826 to 830, 929 to 934, 967, 1039, 1092 and
  1341: a second request for a global of a class, and `--no-reflect`.
- `lower_expr.c` 304, 378, 592, 993, 1191, 1772, 1803, 1870, 1948, 2095,
  2097, 2116 and 2246. `lower_stmt.c` 1197 and 1494.
- `lower_eq.c` 100, 139 and 355, `lower_hash.c` 345, 346, 393 and 458 to
  462, `lower_error.c` 116, 127 and 178, `lower_sync.c` 215 and 217.
- `lower_simd.c` 126, 543 and 545: `simd.any` and `simd.all` of a mask
  above the cap, which takes a simd struct of 512 lanes of one byte.
- `ir.c` 607, 608 and 611, `ir_print.c` 234 and 236: two equal constants
  of the kinds none and float, and the guarantees `willreturn` and
  `nofree` in a dump.

Not read yet, with the unreached regions of each:

- The IR and the LLVM emitter: `llvm_emit.c` 87, `llvm_debug.c` 4, `llvm_run.c` 6, `optimize.c` 20, `whole.c` 42, `whole_tables.c` 46, `layout.c` 33, `abi.c` 5, `reach.c` 1, `arith.c` 6, `ir_verify.c` 30. A first look at `llvm_emit.c`
  showed the tests of `e->failed` as most of its 87.
- The drivers and the files beside them: `driver.c` 101, `driver_index.c` 10, `driver_library.c` 31, `driver_search.c` 21, `main.c` 6, `antic.c` 4, `platform.c` 49, `userdirs.c` 11, `applesdk.c` 3, `modpath.c` 3, `text.c` 6, `alloc.c` 2, `notice.c` 1, `docpage.c` 49, `header.c` 63, `ast_walk.c` 3.
- The library file: `antl.c` 1, `antl_io.c` 3, `antl_read.c` 77, `antl_read_ir.c` 61, `antl_tree.c` 82, `antl_verify.c` 37, `antl_write.c` 21. Most stand behind a damaged library file, as
  do the 30 of `ir_verify.c`, and `tests/run_antl_verify.cmake` holds the
  form of such a test.
- The linker steps: `driver_link.c` 57, `linker.c` 12, `coff.c` 47.
- `src/anti/`: `bind.c` 12, `bindapi.c` 46, `bindclang.c` 100, `bindexpr.c` 51, `bindmodel.c` 2, `bindtype.c` 24, `bindwrite.c` 48, `build.c` 39, `check.c` 22, `cursor.c` 2, `deps.c` 45, `doc.c` 75, `files.c` 21, `fmt.c` 36, `jsontree.c` 13, `licensing.c` 6, `main.c` 19, `memreport.c` 12, `platform.c` 1, `project.c` 3, `repo.c` 20, `sdk.c` 15, `symmap.c` 26, `syms.c` 68, `test.c` 20, `units.c` 14, `zip.c` 35.
