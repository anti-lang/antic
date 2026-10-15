# Solid: the findings of the area types

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_types_`, in one session on 2026-10-15. All nine
are fixed, and `tests/known-failures.txt` holds no line of a test of the
area. None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-types/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T020 | severe | `1ad91a56` | The checker read the value of an enum, the width of a bitfield, the N of `align(N)` and the length of a field's array before functions had types, so a call checked to the error type without a message. |
| T021 | severe | `1ad91a56` | The value of an enum kept a number alone and dropped a value computed from `size_of` without a message. |
| T022 | severe | `c387c1be` | `packed` aligned an atomic field and the word of a Mutex to 1, and ARM64 refuses an atomic access that crosses 16 bytes. |
| T023 | major | `a40d730c` | The descriptor of a variant without fields read the offset of a union the variant does not have. |
| T107 | major | `2680be29` | A copy of a generic put the argument in place of a constant parameter and kept the operation around it as a formula. |
| T024 | minor | `fe23e979` | After the refusal of the value of a `switch` or an `if let`, every arm was still checked against the error type. |
| T026 | minor | `3444e122` | `n is Shape.Circle` on a value that is no variant took `Shape` for a module. |
| T027 | minor | `8b7e56a0` | A name read through an enum fell to the message of a member of a class. |
| T113 | minor | `c75c8627` | Fixed by the commit of T071 of the classes step, which gave the conversion with two paths one message. |

- T020 and T021 share the declaration constants and one commit. The walk
  that refuses a call before the check also covers a constant that one of
  those declarations names before functions have types. The new test
  `matrix/types_enum_value_from_a_constant_of_a_call` shows that path.
  After that phase the call keeps the message that names what it calls,
  so `to_slice` and the array of more than 1048576 elements keep theirs.
  `mul_high` folds and passes the walk.
- T022: an atomic field and a field that holds a Mutex keep their
  alignment in a packed aggregate. The IR marks an atomic field, and the
  format version of a library file is 87. Two hex listings and the unit
  test of library files follow. The comments of the two tests named the
  offset `packed` would give, and now say it would. The new test
  `matrix/types_mutex_in_packed_struct` pins the size.
- T107 changes the name of the copy `length<int, 2 * 2>` of
  `programs/generic_constants` to `length<int, 4>`. Its six digests of
  `emit_identity` are written again in `83eb9f70`. `link_identity` is
  unchanged.
- T113: the expected output read the receiver as `*C`. Since
  `c75c8627` the message names the value the program wrote, as "Messages"
  in `docs/anti-object-model.md` gives it, so the test reads `t` and still
  expects the one message. Commit `90c1996c`.
- T027 adds the message `` `Kind` has no value `C` ``, which the finding
  asked for. No document described the old texts.

## Findings left open

None of the area.

## New findings

- T133, `clib_tight`: the C header of an `export packed class` writes the
  struct without packing, so C gives it 24 bytes where Anti gives 17.
- T134, `clib_tight_aligned`: the header of a packed struct with
  `align(N)` writes `#pragma pack`, which caps the `_Alignas` of its first
  field, so C gives it an alignment of 1 where Anti gives 16.

Both are listed in `tests/known-failures.txt` and fail for their reason
on the Mac and on both VMs, `win-tight.log` and `linux-tight.log`. They
are left for the step of their area. Commit `6aaf7c9e`.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- A value of an enum computed from `size_of` is refused, as the N of
  `align(N)` is.
- `packed` leaves the padding before an atomic field and before a field
  that holds a Mutex, and the aggregate takes that alignment. The format
  version of a library file is 87.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 148 lines, 128 of them tests, before the
step. It holds 128 lines, 108 of them tests, after it. 100 of them are
expected to fail on the Mac.

- The build of all three trees has no warning, `bh.log`, `ba.log` and
  `bu.log`.
- Host, `ctest --test-dir build/host -j14` in two parts: 1999 and 2985
  pass, `h1.log` and `h2.log`, two `mimalloc_environment` tests skip.
  `emit_identity` failed in the first part for the copy name of T107 and
  passed after `83eb9f70`, `ei2.log`. The second part repeats 21 tests
  that are fixtures of its tests.
- ASan: 1150, 1112 and 2731 pass, two skip, `a1.log` to `a3.log`.
- UBSan: 1150, 1112 and 2731 pass, two skip, `u1.log` to `u3.log`.
- T022 changes the layout on every target, so both VMs ran the suite at
  `83eb9f70`. anti-linux: 1000, 1512 and 2412 pass of 4886,
  `linux-1.log` to `linux-3.log`. anti-windows, in `build/host`: 999,
  912, 661 and 1941 pass of 4864, the tests of `--memory-checks` disabled,
  `win-1.log` to `win-4.log`. Each build warns six times, in mimalloc
  alone, as at the baseline.
- The docs-style checker reports nothing on every `.md` file the step
  touched.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
83eb9f70 Write the manifest of emit_identity for the folded copy names
351f2957 Record the commits of the findings the step types fixed
90c1996c Record T113 as fixed by the commit of T071
$ git status --short
$ git rev-parse HEAD origin/main
83eb9f70f625a0693a1a6d771330224e6009de25
83eb9f70f625a0693a1a6d771330224e6009de25
```
