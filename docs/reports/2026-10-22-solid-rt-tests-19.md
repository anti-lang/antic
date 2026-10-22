# Step rt-tests, nineteenth session

The session went on from the list of `2026-10-22-solid-rt-tests-18.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/anti/` and `src/antic/` that a program, a
dump or a unit case reaches, and gave the reason for regions nothing
reaches. Logs are under `build/drive/logs/`, named `rt19-*`. The list of
the regions not reached is `rt18-end-unreached.txt` at the start and
`rt19-end-unreached.txt` at the end, which `build/rt19/unreached.py`
writes from the export of the report. The probes are in `build/rt19/`.

## Coverage per directory

Regions reached. The start is `rt18-end-summary.txt`, measured at
`7bcd2caa`. The end is `rt19-end-summary.txt`, measured at `6308898e`
with `./c coverage part 1/5` to `5/5`, `rt19-end-1.log` to `-5.log`.
In that run `fmt_canonical` failed on the three new fixtures, which
`b0bb5ee0` wrote in the form of `anti fmt`. It changes no region.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 96.00% of 9252 | 96.01% of 9252 | 370, 369 | 96.18% | 96.22% |
| `src/antic/` | 98.20% of 55587 | 98.20% of 55587 | 1003, 1000 | 98.26% | 98.27% |
| `src/anti/` | 99.07% of 11663 | 99.09% of 11663 | 109, 106 | 99.06% | 99.08% |

`src/rt/` gained three regions of `sync.c` and lost the `0` of
`copy_path` of `platform_posix.c` 483. The run of the session before
reached that one, which needs a frame that `dladdr` gives no file for.

## The tests

- runtime, `matrix_ownership_delete_of_a_channel_never_made`: `delete`
  of a channel in an element of `alloc(T, n)` the program has not filled,
  which `alloc` zeroes because the element holds a class value with a
  `destruct`. `anti_rt_chan_held` and `anti_rt_chan_delete` of `sync.c`
  take the null handle, `a543a8e5`.
- antic, `dump_types_checked_nodes`: the descriptor of the interface that
  `lib.instance` takes, `dump_expr` of `ast_dump.c`, `f5876493`.
- antic, `ir_inject_marks` and `ir_effects_may_free`: an `inject` field
  without `final` beside one with it, and the effects of `anti_rt_grow`
  without `nofree`, `class_record` and `effects` of `ir_print.c`,
  `45c49f31`.
- anti, `unit_syms` and `unit_syms_messages`: a frame of a report of
  `--memory-checks` whose module is a universal file without the named
  processor, `thin_slice` of `memreport.c`, `6308898e`.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
133 lines and 114 entries, 112 expected to fail on the Mac, before and
after. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/antic/ast_dump.c` 1055 to 1061: the parser takes `abstract`,
  `concrete` and `self` in a class body alone, whose functions
  `dump_module` writes at 956 and 960.
- `src/antic/driver.c` 1558, 1563 and 1586: `anti doc` parses and checks
  every source before it asks antic for a page. A source that does not
  parse, has no module name or fails the checker stops it first.
- `src/antic/driver_link.c` 36 to 54 and 171 to 176: `--linker platform`
  for a Linux target needs a Linux host, and the Mac refuses the link
  before `find_crt_dir`.
- `src/anti/repo.c` 412 to 434: every package `fetch_all` passes comes
  from the lock file, which `lock_package_valid` of `deps.c` checks, or
  from an index, which `best_version` and `index_modules` check.
- `src/anti/deps.c` 373: `antl_get_cstr` gives `""` for an empty string
  and never NULL.
- `src/anti/jsontree.c` 60 to 62: the second scan of a string reads the
  bytes the first one accepted. 90 to 92: a list of more than
  `SIZE_MAX / 16` values.
- `src/anti/bindwrite.c` 125 to 130: clang gives a parameter of a
  function type its pointer type, through a typedef too. A description
  gives a callback as a pointer, so `spell` meets no bare function type. `build/rt19/b` holds both probes.

## Not run on the target

- `report_universal` of `tests/unit/test_syms.c` writes its line itself
  on Windows, which `unit_syms_messages` of anti-windows would show.

## Gates

- No warning of our code: `rt19-hb1.log` to `-hb6.log`, `rt19-cb0.log`,
  `rt19-cb1.log`, `rt19-asan-build.log` and `rt19-ubsan-build.log`.
- Host, 5786 tests, `-I 1,2000` and `-I 2001,5786`: 2000 and 3813 passed,
  in 365 s and 326 s. Part 2 runs 27 fixtures of part 1 again. 112
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt19-host-1.log` and `rt19-host-2.log`.
- ASan, 5784 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5784`: 1151, 1106, 1577, 906 and 1086 passed in 237 s, 185 s,
  184 s, 443 s and 350 s, `rt19-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1086 passed in 145 s,
  149 s, 136 s, 174 s and 295 s, `rt19-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
b0bb5ee0 Write the three new fixtures in the form of anti fmt
6308898e Test a report frame of a universal file without its processor
45c49f31 Test the inject marks and the effects without nofree in the IR
$ git status --short
$ git rev-parse HEAD origin/main
b0bb5ee0beceed8bffe2ec1cda1b04eec5208ad4
b0bb5ee0beceed8bffe2ec1cda1b04eec5208ad4
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 106 places. `bindwrite.c` 18 and `fmt.c` 12 hold the
  most. `build/rt19/untriaged.py anti` lists 54 lines no report has
  given a reason for.
- `src/antic/`, 1000 places. `build/rt19/untriaged.py antic` lists those
  no report has given a reason for. `sema_generic.c`, `sema_copies.c`,
  `whole_tables.c`, `coff.c` and `llvm_emit_function.c` hold the most.
- `src/rt/`, 369 places: `trace.c` of ELF and PE, `object.c` 243 and
  341, `plugin.c` 642, 719 and 864, which need a damaged library, and
  `patterns.c` 405, a `value` of a walk before its first `next`.

CONTINUE
