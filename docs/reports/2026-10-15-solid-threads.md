# Solid: the findings of the area threads

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_threads_`, in one session on 2026-10-15. All
seven are fixed, and `tests/known-failures.txt` holds no line of a test
of the area. None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-threads/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T055 | severe | `06030417` | The job of a `dispatch` kept the address of a slot of the caller's frame. A later turn of a loop wrote it again, and a return ended it. |
| T060 | severe | `dfbe2c4a` | The checker took all eight atomic operations on every type an atomic holds, `add` on a `bool` and `sub` on a pointer among them. |
| T053 | severe | `d5038f8b` | The checker followed no job, so a second `join` of one compiled and read the freed job. |
| T057 | major | `8050f846` | A static field and a constant of a class body were evaluated only when code named them. |
| T059 | major | `075a0aca` | The checker held an atomic local to the word types and no atomic field. |
| T058 | major | `437f48c5` | A copy of a generic made `chan T` with the argument in place and skipped the check of a channel written out. |
| T062 | major | `ec1a261a` | The check of a returned pointer into the fields looked at `&`, slices and locals alone. It saw no literal and no result of a private function. |

- T055 changes the call of the runtime: `anti_rt_dispatch` takes the
  size of the context and copies it into the job. The manifest of
  `emit_identity` and the digest of `link_identity_macos-arm64` are
  written again in that commit. The excerpt of chapter 13 of the
  developer's guide shows the new argument.
- T053: the second `join` of decision 23 is refused where a block
  declares the job and two of its statements join it. A copy of the job
  or a `join` in a branch is left to the trap of dev mode. T054 holds
  that trap, and the step left it to its area.
- T057 also refuses a default that loads another static field. The new
  refusal `matrix/threads_static_default_is_a_load` holds it. Its doc
  comment had to be rewritten by `anti fmt`, `8cb0c5f9`.
- T059: `hook` of `Fatal` in `anti.lang` is an atomic `?fn()`, so a
  plain function pointer stays atomic. By R136 a message names a type as
  the file spells it, so three expected files read `f64` now where they
  read `float`. The test of the static field marks it `pub`, since a
  static field without it is private to its class.
- Chapter 13 of the guide named the defects of T053 and T055, and
  chapter 14 that of T062. Both say what the language does now.
- Every program of the area, 82 tests, ran ten times with
  `ctest --repeat until-fail:10`, `repeat10.log`. None varied.

## Findings left open

None of the area. The step recorded no new finding.

`no_llvm_cov_packed` fails on anti-windows in a tree named `build`, the
tree `test.cmd` of `docs/vm-setup.md` configures, and passes in
`build/host`. CMake wraps the message of `tools/check-llvm.cmake`
between `holds` and `llvm-cov` once the path is long enough, and the
test matches the two words on one line. The first run of this step
in `build` shows it, `win-1-failed.log`. The suite then ran in
`build/host`, as the baseline did. No line of
`tests/known-failures.txt` can hold a failure of one tree name, so it
has no row. Should the test match across the wrap, or `test.cmd` use
`build/host`?

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- The atomic `add` and `sub` take an integer, and `and` and `or` an
  integer or a `bool`. A pointer takes the other four.
- What the checker sees of a second `join`, by decision 23.
- An atomic field holds what an atomic local holds, an `f16` and a plain
  function pointer.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 181 lines, 161 of them tests, before the
step. It holds 148 lines, 128 of them tests, after it.

- Host, `ctest --preset host -j14` in two parts: 1999 and 2979 pass,
  `h1.log` and `h2.log`. `fmt_canonical` failed in the first part on the
  new test, and passed after `8cb0c5f9` (`fmt2.log`). Two
  `mimalloc_environment` tests skip, and the 128 listed tests fail as
  expected.
- ASan: 1150, 1112 and 2725 pass, two skip, `a1.log` to `a3.log`.
- UBSan: 1150, 1112 and 2725 pass, two skip, `u1.log` to `u3.log`.
- The build of all three trees has no warning, `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- The runtime changed, so both VMs ran the suite. anti-linux: 1000, 1512
  and 2406 pass of 4880, five skip, `linux-1.log` to `linux-3.log`.
  anti-windows, in `build/host`: 999, 912, 719 and 1878 pass of 4858, 14
  skip, `win-1.log` to `win-4.log`. Each build warns six times, in
  mimalloc alone, as at the baseline.
- `emit_identity` and `link_identity_macos-arm64` changed by T055 alone.
- The suites ran at `8cb0c5f9`. The commit after it and this report
  change `docs/` alone.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
cc879f3c Record the commits of the findings the step threads fixed
8cb0c5f9 Write the test of a static default that loads in canonical form
ec1a261a Refuse a pointer into the fields inside a literal or a private result
$ git status --short
$ git rev-parse HEAD origin/main
cc879f3c8157f711f4e6e4215cd307485bb68a39
cc879f3c8157f711f4e6e4215cd307485bb68a39
```
