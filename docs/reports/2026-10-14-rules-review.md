# The review of the rules of 2026-10-13

The step checked the work of the ten reports `docs/reports/*-rules-*.md` of
this run against "Rules of 2026-10-13" in `docs/decisions.md`. It built no
rule. It changed one cell of `docs/audit/tests-findings.md` and wrote this
report.

## The rules by state

| State | Rules | Count |
|---|---|---|
| Change, built | every Change but R081 | 98 |
| Change, not built | R081, held by the expected failure T127 | 1 |
| Keep | each with its test | 88 |
| Eddie | R009, R017, R037, R051, R054, R066, R074, R083, R100, R134 | 10 |

R097 and R153 held already when their step began, and their tests pass
on the tree before the run as well. R174 is a change of
`docs/notes/bind.md` alone.

## How the step checked

- The tree before the run, `d4307903`, built from `git archive` in
  `build/review/base-build`, with the downloads of `build/deps`.
- `build/review/runold.py` ran every command of the 4947 tests of
  `build/host` with the binaries of that tree, and once with those of
  `build/host`. The second run gave 374 failures, the list of the known
  failures, and two tests its limit of 240 s stopped, `package_keys` and
  `antl_diamond_right`. The lists are `build/review/old.txt` and
  `build/review/new.txt`, and the logs `build/drive/logs/rev-old0.log` to
  `rev-new2.log`.
- `build/review/change_map.txt` gives each test of each Change rule its
  result in both trees. Every built Change rule has at least one test that
  fails on the old tree and passes now. R182 is held by a case of
  `unit_fmt`, which the runner cannot replay, so its new `test_fmt.c` was
  built against the old code: 2 checks failed, in
  `rev-base-unit-fmt.log`.
- Eight of those failures were read through, in
  `build/review/spot-old.txt`. Each fails for the reason of its rule. A
  refusal compiled, the count of `destroyed` stood at 0, blocks stood at
  no multiple of their alignment, and a `dispatch` counted its arguments.
- The tests of each Change rule were read against the wording of the rule.
  The step read the header and the expected text of each, and the body
  where a part of the rule needed it.

## Keep rules drawn at random

`random.seed(20261014)` drew forty of the 88 Keep rules:

| Area | Rules |
|---|---|
| Flow and errors | R001, R008, R013, R018, R024 |
| Values and types | R028, R038, R042, R052, R053, R057, R064 |
| Ownership and threads | R071, R077, R080, R087, R089, R095, R098, R099 |
| Classes | R101, R103, R106, R107, R118, R119, R120 |
| Modules and antic | R124, R142, R143, R144, R146, R148, R154 |
| anti | R175, R176, R186 |
| Wording | R190, R195, R197 |

Each test the reports name for them exists and passes. A break of the
rule changes the output or the message the test compares.
R190, R195 and R197 hold the wording through expected failures of open
findings, and T002 and T089 among them are fixed and pass.

## The cases that wait for Eddie

None changed. The tests found for them have no commit since `d4307903`:
`flow_switch_on_char`, `errors_fail_of_a_text_in_a_variable`,
`errors_fail_of_an_interpolated_text`, `values_compound_*_simd`,
`types_generic_literal_of_another_generic_without_arguments`,
`ownership_destroy_of_a_part_of_a_singleton` and
`classes_private_field_of_a_nested_class`. Programs in `build/review/`
showed antic as the questions of "Open" state it: `align(4294967296)`
compiles, an enum value that names an earlier one is `unknown name`, the
literal of R066 is refused, `=` to an `own` field leaves the old object
alive, and `delete` of a parameter without `own` compiles. No commit of
the run added a function to `anti.plugin`.

## Findings

- Closed by this run, each `fixed` with its commit and with no line left
  in `tests/known-failures.txt`: T002, T052, T056, T089, T097 and T125.
  Their 14 tests pass.
- Opened by this run and still open: T126 and T127.
- Left for the fix run: 121 open findings of 127 rows, held by 374 tests
  of `tests/known-failures.txt`.

## What the step fixed

- The row of T056 named `matrix_threads_const_of_a_struct_with_a_mutex_dev`,
  which R047 replaced by the refusal
  `matrix_threads_const_of_a_struct_with_a_mutex_refused`. Its state now
  names that test.

## Where the task and the code differed

- R081 is a Change and is not built. The step of ownership found that the
  rule as written makes `add` of `Set<*Key>` delete an object the set
  holds, against "Principles" under "Collections" in
  `docs/anti-language-additions.md`, and recorded a `[provisional]` entry.
  The review leaves it so, since building it changes the collections.
  The question stands in the report of that step.
- R125 is built for the modules of a project, the case of the test run.
  antic reads an imported module from its library file alone. The cycle
  it meets there is that of T088, whose message names no `a -> b -> a`.
  T088 is the fix run's.
- R136 takes the spelling per file, by a `[provisional]` entry. A file
  that writes both `i64` and `int` names both `i64`.
- R167: the two cases hold the line that the lock file is written again.
  Both run with `--offline` and stop at the index, and no test reads a
  lock file that a build wrote again.

## Counts and checks

- Tests: 4947 on the host, 4945 in each sanitizer tree, unchanged.
- Host suite: every test passed, 374 of them expected failures, and the
  two `mimalloc_environment` tests skip. `-I 1,2000` in 385 s and
  `-I 2001,4947` in 256 s, in `rev-host-suite-a.log` and
  `rev-host-suite-b.log` of `build/drive/logs/`.
- ASan: `-I 1,1150`, `-I 1151,2250` and `-I 2251,4945` in 283 s, 392 s and
  353 s, all passed, in `rev-asan-suite-a.log` to `rev-asan-suite-c.log`.
- UBSan: the same parts in 242 s, 352 s and 269 s, all passed, in
  `rev-ubsan-suite-a.log` to `rev-ubsan-suite-c.log`.
- The three trees built with zero warnings, in `rev-host-build.log`,
  `rev-asan-build.log` and `rev-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports `0 error(s), 0 warning(s)` on
  `docs/audit/tests-findings.md` and this report.

## Not done

- Neither VM ran the suite.
- The runner replays a command and not the build of a test. The unit
  tests other than `unit_fmt` therefore ran in the old tree with their old
  sources. Each rule they hold has a test of another kind as well.

## Proof

`git log --oneline -3` before the commit of this report:

```text
6018b6d4 Report the step of the rules of the commands of anti
9b3fe0f3 Record the step of the rules of the commands of anti
68953f4c End the last line of a notice before the lines of its sources, by R189
```

The hash of this commit is not known inside it. The reply of the session
therefore holds `git log --oneline -3`, `git status --short` and
`git rev-parse HEAD origin/main` after the push.
