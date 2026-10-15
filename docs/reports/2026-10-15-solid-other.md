# Solid: the findings of the other areas

The step fixed every open finding of `docs/audit/tests-findings.md` whose
test does not start with `matrix_`, in one session on 2026-10-15. Of
those, 33 are fixed, and `tests/known-failures.txt` holds the three lines
of T127 alone, the question of R081 that the freeze keeps as it is. The
logs lie in `build/drive/logs/solid-other/`.

## Findings closed

Severe first. One sentence gives the cause of each.

| Finding | Commit | Cause |
|---|---|---|
| T017 | `f99d5ad1` | `build_slice` made the part of an array or a slice without a check of `lo` or `hi`. |
| T043 | `333b8b99` | `home_file` of a copy was never set, and `here` named the module that lowered the copy. |
| T054 | `7d616b5c` | The runtime frees a job at its `join`, so nothing could tell a second `join` from the first. |
| T092 | `4fb683e9` | Every `created` hook counted, and a value of a class without a teardown ends with no hook. |
| T093 | `9af42df6` | The optimizer of a host dropped what `main` does not reach, which a plugin binds against. |
| T117 | `131b1c2a`, `91c9e91f` | `declare_enum` took the base from the signed bits of a value above `INT64_MAX`. |
| T122 | `db7e7346` | `layout_fold` folded an operation on `c_wchar` inside a symbolic value with the unsigned form. |
| T130 | `c4b31a64`, `af65b82e` | An export of a static program of musl took the calls of musl and mimalloc to `read` and `write`. |
| T133 | `454c20b8` | The header wrote the struct of a packed class without `#pragma pack`. |
| T134 | `454c20b8` | `#pragma pack` caps the `_Alignas` of a member, which carried `align(N)`. |
| T025 | `c08075c5` | The formatter took the `fn` of a type alias after `=` for an anonymous function and waited for its body. |
| T029 | `e49e1d9e` | The verifier wanted a type on the class before a static field, and the copy put the global into the client. |
| T030 | `1449bc59` | A callee under the name of a module was rewritten as a value, and a class is none. |
| T061 | `ba14f572` | The library file named a `Job` without the result that makes it one type. |
| T076 | `731318ad` | The record of a class carried its functions alone. |
| T085 | `61496389` | The reader checked a constant of a `?T` against `?T` and took `none` for pointers alone. |
| T094 | `9dd02b40` | The argument of `N: int` took a constant of the module alone. |
| T099 | `bfe03ca0` | anti copied the deliverable through the soname link into `dist/`. |
| T103 | `68fd6aa2` | A statement refused before its first token left the recovery at the same `if`. |
| T115 | `81ee3c0c` | No constant folds the name of a function, so the file carried no default. |
| T116 | `1f43971e` | `_Noreturn` is no keyword of C++. |
| T118 | `1f6281ce` | The walk of a record passed over the enum declared inside it. |
| T119 | `ea15d5e5` | `%.17g` writes `1e+20` without the point an Anti float literal needs. |
| T120 | `7ecc809e` | The operand of a cast was read as a primary expression. |
| T121 | `72087afd` | A host that converts no `f16` itself linked neither conversion of the runtime. |
| T124 | `79bd2182` | `doc_html` read the inline markup of each line by itself. |
| T131 | `80e29ef5` | antic ran `ln -sf` of the host for the soname link. |
| T063 | `e8b299d8` | The report of a conflict marked the reverse order alone. |
| T088 | `7015fa42` | The driver stopped on the cycle before the checker reached the `import`. |
| T095 | `ed23e785` | The copy pass reported at a position of the body of a generic of another module. |
| T098 | `f83f21e5` | The reader left a function out by giving it a result the writer reported again. |
| T123 | `72777aea` | The outline reported the mistake, and the front end ran on the source again. |
| T132 | `d4548114` | anti left its standard output in the text mode of the C runtime of Windows. |

`97b901c0` writes the commit of each into its row.

- T029: the library of its test reached a field without a marker from
  the functions of its module, which "Visibility" refuses. The test
  failed for that and never reached the defect. The field is `pub` now.
  The subject of the test stays.
- T025: two sources that the defect had laid out take the canonical
  form, `tests/dump/nodes.anti` and a program of the matrix. The refusal
  of the second names its new line.
- T117: the check S32 of the malformed header of `run_anti_bind.cmake`
  expected the signed bits of 2^63 in an enum of `unsigned long long`,
  which is this defect. It expects 2^63 in an enum of `c_ulonglong` now.
- T116: the entry on `never` in `docs/decisions.md` and the specification
  said the header writes `_Noreturn`. Both name `ANTI_NORETURN` now, with
  its two spellings, since the finding rests on the decision that a
  header compiles as C and C++.
- T092 keeps the count in the two hooks, as the entry on the count under
  "Plugins" says. T130 changed 53 lines in the excerpts of the guide for Linux, whose
  exports are `internal` there.

## Changes the step made beside the findings

- `emit_identity` changed by the fixes of T017, T029, T092 and T093,
  each in the numbering of globals or the number of a hook. Its manifest
  is written again in each commit. `link_identity_macos-arm64` changed by
  the runtime of T054, T063 and T092, `fc871d0d`.
- The library file is format 88, by T076 and T061.
- A fix added no option, no command and no public item of the library.
  The header of C gains the macros `ANTI_ALIGNED` and `ANTI_NORETURN`,
  each written where a header names it, by T133, T134 and T116. The
  platform layer of the tools gains `platform_symlink` and
  `platform_binary_stdout`, by T131 and T132, and the runtime one kind of
  check, by T054. None is a public item.

## Findings left open

T127 alone, which the freeze keeps.

## New findings

None. The two regressions the VMs showed, `program_abi_structs` on Linux
and `anti_bind_huge_enum` on Windows, came from fixes of this step and are
fixed by `af65b82e` and `91c9e91f`.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`, one each for
T017, T054, T092, T093, T133 and T134, T130, T076 and T115.

## Counts and gates

`tests/known-failures.txt` held 75 lines, 55 of them tests, before the
step. It holds 22 lines, 3 of them tests, after it.
Before the step 47 of the tests expected a failure on the Mac and 8 named
another host. After it the 3 tests of T127 expect one on every host.

- The build of all three trees has no warning of our own code,
  `bh2.log`, `ba2.log` and `bu2.log`. The six warnings there are those
  of mimalloc, which builds with the flags of its own project.
- Host at `91c9e91f`, through `ctest --preset host -j14` in two parts,
  4970 tests: 2000 and 2991 pass, `h3.log` and `h4.log`, the second
  repeating 21 tests that are fixtures of its tests. Two
  `mimalloc_environment` tests skip, and the three tests of T127 fail as
  listed.
- ASan at `91c9e91f`, 4968 tests: 1150, 1112 and 2737 pass, two skip,
  `a4.log` to `a6.log`.
- UBSan at `91c9e91f`, 4968 tests: 1150, 1112 and 2737 pass, two skip,
  `u4.log` to `u6.log`.
- anti-linux, 4892 tests, at `af65b82e` in three parts of `ctest -j6 -I`:
  every test passes apart from the three of T127, `linux-1.log` to
  `linux-3.log`. At `91c9e91f` the 124 tests of bind, the ABI, the guide,
  `repo_layout` and `known_failures` pass again. A first run at
  `81ee3c0c` failed `program_abi_structs_linux-x86_64`, fixed by
  `af65b82e`, and `repo_layout`, which refused the logs the run had left
  in the top level of the checkout.
- anti-windows, 4870 tests, at `af65b82e` and `91c9e91f` in parts of
  `ctest -j4 -I`, `win-1.log` to `win-5.log`: every test passes apart
  from the three of T127 and the tests of `--memory-checks`, which the
  host disables. `anti_bind_huge_enum` failed at `af65b82e` and passes at
  `91c9e91f`. Two parts ran past the limit of one call and went on in
  the background until stopped, so a process on the VM finished one of
  them.
- The docs-style checker reports nothing on every `.md` file the step
  touched.

## State before this report

```text
$ git log --oneline -3
91c9e91f Keep an enum above INT64_MAX an int when binding for Windows
af65b82e Keep the exports of a program global where it links an object of the user
97b901c0 Record the commits of the findings the step other fixed
$ git status --short
?? docs/reports/2026-10-15-solid-other.md
$ git rev-parse HEAD origin/main
91c9e91f2f11931b4be4175984bd79dcea0b41ca
91c9e91f2f11931b4be4175984bd79dcea0b41ca
```
