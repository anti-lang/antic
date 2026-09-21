# `may fail` in the test programs

The last step of the breaking rewrite to `may fail`. `1daa328` is the
change. The test programs now write their failing functions with
`may fail`, apart from two that test the hand-written form.

## What there was to rewrite

- Five sources under `tests/` declared a failing function by hand:
  `out_place_forms`, `out_through_pointer` and `out_slot` under
  `programs/`, and the listings `errors` and `catch_place`. Their calls
  already used `catch`, `try` and `yield`.
- No test declares an `extern fn` that returns `?*Error`, and no call
  passes the out pointer as an argument. The `out` that `json.unquote`
  and `text_member` take is a builder parameter.

## What changed

- `value`, `small`, `places`, `fails`, `made`, `passed_on`, `inner`,
  `outer` and both `risky` are `may fail`. Each returns its value and
  leaves with `fail`. A `may fail` function that never fails is a
  warning, and the runner takes a warning as a failure. `small`, `made`,
  `inner` and `risky` therefore each gained an argument and a real
  failure path.
- `outer` stores `*into = try inner(42);` through a pointer parameter.
  `passed_on` stores `*into = try made(3);` through a pointer to a local
  and returns the local. Both keep `*p = try f();` as the subject, and
  `main` still binds an aggregate with `catch fatal`.
- `Box.make` in `out_slot.anti` keeps the hand-written form, and the
  file now says why. The zeroed table of a `catch` binding exists for
  the `=` a callee writes through its out pointer. `return v;` in a
  `may fail` function builds the value there with `build_into` and
  destroys nothing. That leaves `program_out_slot`, which the decision
  names as the pin, as the one test of the rule.
- `tests/run_std_may_fail.cmake` is now `tests/run_may_fail_only.cmake`.
  It reads `ROOT` at every depth and skips the declarations in `SKIP`.
  `std_may_fail_only` runs it over `std/anti`, and the new
  `tests_may_fail_only` runs it over `tests/` with two skips.
- The `.err` of both listings moved down two lines. The messages are
  unchanged.

## What failed and how it was fixed

- The red run of `tests_may_fail_only` named the eleven functions of
  the five files and nothing else. Log: `build/drive/logs/red.log`.
- `emit_identity` failed for `out_place_forms` and `out_through_pointer`
  on all six targets. The manifest was rewritten on the Mac with
  `-DWRITE=yes`, and no other program changed. Logs:
  `emit_identity.log`, `emit-write.log`.
- The docs-style checker flagged three comments. One was a banned
  phrase in the new comment of `passed_on`. A sentence of 26 words and
  a filler word predate this step. All three were rewritten.

## Gates

- Zero warnings in all three builds. ld prints the `-lto_library` note
  it printed before.
- Host: 529 of 529. ASan and UBSan: 528 of 528 each, with no sanitizer
  report.
- The docs-style checker reports nothing on the touched `.md` files, on
  the `.anti` files through `.rs` copies and on the CMake comments
  through `.py` copies.
- Logs under `build/drive/logs/`: `build.log`, `test.log`, `focus.log`,
  `asan-build.log`, `asan-test.log`, `ubsan-build.log`, `ubsan-test.log`,
  `docs-style.log`, `docs-style-anti.log` and `docs-style-cmake.log`.

## Provisional entries

One, under "Failing functions" in `docs/decisions.md`: the test
`tests_may_fail_only` and the two declarations it lets through, the
`construct` of `construct_forms` and `Box.make` of `out_slot`.

## Questions

- `out_slot` keeps the hand-written form so that the zeroed table stays
  tested. Should the table instead get a test of its own that calls a
  C function through a binding? C writes the out slot without `=`, so
  that test would not read the table either.
