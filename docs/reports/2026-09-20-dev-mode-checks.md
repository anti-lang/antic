# Dev-mode checks

"Dev-mode checks" of `docs/anti-language-additions.md` is built. Bounds,
signed overflow, a narrowing `as`, division and shifts are emitted on the
assert switch, decided in the back end, with `--checks` and `--no-checks`
overriding the mode. The choices are in `docs/notes/dev-checks.md`.

## What changed

The switch. A failure block carries `IR_FAIL_ASSERT` or `IR_FAIL_CHECK` in
its `fail` field, which replaces the `assert_fail` flag. `ir_drop_failures`
takes the kind and cuts the branch in either direction, so a check may test
for success or for the failure. `back_end` runs it once per kind. A library
file carries both, so the build that compiles the program decides.
`ANTL_VERSION` is 26.

The failure routine. `anti_rt_check_failed(text, length, kind, a, b)` in
`rt/check.c`. The compiler builds the text, which names the file, the line
and the operation. The kind names the labels of the two values, and the unit
test `records_check_kinds` pins the compiler's mirror of `enum anti_check`
to the runtime's by reading the call lowering writes.

The checks. An index is one unsigned comparison against the count, so a
negative index fails the same test. A narrowing `as` is the round trip, with
a comparison against zero where the sign changes and the width does not.
Division compares the divisor before the operation. A shift compares the
count against `size_of` the type times eight.

Overflow. Three IR operations, `addov`, `subov` and `mulov`, give 1 when a
signed `+`, `-` or `*` leaves the range of its type. Eddie chose the
sequences: the overflow flag where the instruction sets it, `smulh` against
the sign of the low half for a 64-bit `*` on ARM64, and `smull` against the
sign-extended result below it. The selector fuses the test with the branch
after it, as it fuses a comparison. The specification's sentence now says
that the back end detects overflow with the target's cheapest sequence and
names both mechanisms.

Tests. `tests/checks/` holds thirteen programs, one per check. Each is built
in release and in dev mode: release runs to its end and carries no text of
the check, and dev prints the file, the line, the operation and the values,
then aborts. `--checks` reaches a release build and `--no-checks` clears a
dev build. A `.antl` compiled once is linked into both, and only the dev
build stops on the library's check.

## What the release build pays

Nothing in code. A dropped check leaves a pure condition that
`remove_unused_results` deletes, the loads included. The one difference from
before is the numbering of the anonymous globals, because the text of a
check takes a number before it is removed. `emit_identity` was written
again for that reason, and its assembly is otherwise instruction for
instruction what it was.

## Paths in the expected files

A check carries the source path the command line gave, which is what
`assert` already writes. Every expected file that holds IR or a dev-mode
object therefore held the build machine's absolute path. The three runners
that compare antic's output byte for byte now run antic in `tests/` with
every path under it made relative, which `tests/relative_paths.cmake` does
once for all of them. Two runners that spelled one path differently compiled
one module into two programs, which `dev_modules` caught.

The two `geo.anti` fixtures of the doc-comment tests are float geometry now.
Their line and block forms put the code on different lines, so the libraries
no longer matched once a check recorded a line number. Float arithmetic is
not checked. The fixtures compare the doc text again and keep both forms.
The `scale_source` fixture of `test_modules.c` is unsigned for the same
reason, and its byte-by-byte library changed in two bytes: the type of the
constant and the opcode of `ret`.

## Questions

1. The file a check names is the path as typed. A library file therefore
   carries the path its author compiled from. A user's dev build of it then
   prints a path that does not exist on their machine. The alternative is
   the path under the search root that holds the module, which is what the
   module path already comes from. That would make a `.antl` the same bytes
   in any checkout, and drop most of the test plumbing above. It is recorded
   `[provisional]` as the path as typed, because that is what `assert` does
   today.
2. The check of `/` and `%` by zero is emitted for every target, not for
   ARM64 alone. x86_64 traps by itself, but the trap gives no file, line or
   values. A target-dependent check would put the host that wrote a `.antl`
   into it.
3. A conversion is range-checked when both types are integers. `char` is a
   Unicode scalar value with a rule of its own that nothing has decided, and
   an enum's range is its set of names. Neither is checked.
4. The overflow test repeats the arithmetic, because one IR instruction has
   one result. A dev build pays one extra arithmetic instruction per checked
   `+`, `-` or `*`.

## State

455 tests pass on the development Mac and none is skipped. The ASan and the
UBSan builds run 454 each, without `no_paths`.
