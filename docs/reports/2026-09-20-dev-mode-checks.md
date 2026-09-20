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
`ANTL_VERSION` is 27.

The failure routine. `anti_rt_check_failed(text, length, kind, a, b)` in
`rt/check.c`. The compiler builds the text, which names the file, the line
and the operation. The kind names the labels of the two values, and the unit
test `records_check_kinds` pins the compiler's mirror of `enum anti_check`
to the runtime's by reading the call lowering writes.

The checks. An index is one unsigned comparison against the count, so a
negative index fails the same test. A narrowing `as` is the round trip, with
a comparison against zero where the sign changes and the width does not.
Division compares the divisor before the operation, on every target.
A shift compares the count against `size_of` the type times eight.

`char` and an enum are checked with the integers. A value that becomes a
`char` must be at most `0x10FFFF` and outside the surrogates, which is two
unsigned comparisons. A value that becomes an enum must be one it declares,
which is one comparison per name joined by `or`. An enum converts as its
base type everywhere else.

Overflow. Three IR operations, `addov`, `subov` and `mulov`, give the result
of a signed `+`, `-` or `*` and record whether it left the range of its
type. The terminator `branchov` reads that and follows its operation with
nothing between them, which the verifier checks, so the arithmetic is
emitted once. Eddie chose the sequences: the overflow flag where the
instruction sets it, `smulh` against the sign of the result for a 64-bit `*`
on ARM64, and `smull` against the sign-extended result below it. The
specification's sentence now says that the back end detects overflow with
the target's cheapest sequence and names both mechanisms.

A dev build therefore pays one flag-setting instruction and one branch per
checked `+`, `-` or `*`. `adds x20, x19, x9` and `b.vc` on ARM64, `addq` and
`jno` on x86_64. The values a failure prints are widened inside the failure
block, so the path a program takes carries none of that.

Tests. `tests/checks/` holds sixteen programs, one per check. Each is built
in release and in dev mode: release runs to its end and carries no text of
the check, and dev prints the file, the line, the operation and the values,
then aborts. `--checks` reaches a release build and `--no-checks` clears a
dev build. A `.antl` compiled once is linked into both, and only the dev
build stops on the library's check.

## What the release build pays

Nothing in code. Dropping the checks turns every overflow operation back
into its plain arithmetic, and the rest leaves a pure condition that
`remove_unused_results` deletes, the loads included. The one difference is
the numbering of the anonymous globals, because the text of a check takes a
number before it is removed.

`ir_optimize_function` now merges the blocks before its first round. A
dropped check leaves a jump into the block that follows it, and the peephole
that fuses `t = op` with `x = copy t` needs the two adjacent. Without the
merge a round of propagation gave `t` a second use and the pair never fused,
which cost two moves in one function of `narrow_ops`. The same ordering
removes one block and one jump from `switch_stmt`, which is the only program
of the suite whose release assembly it changes.

## The path in the text

A check and an assertion name the source path under the first search root
that holds it. Outside every root it is the file name alone, which
`module_file_of_source` gives. It is the text the module path comes from, so
the two never disagree. A library file holds the same bytes whichever
checkout compiled it. A user's dev build of a library also names a path that
exists in their own tree. A message still names the path the command line
gave.

The two `geo.anti` fixtures of the doc-comment tests are float geometry now.
Their line and block forms put the code on different lines, so the libraries
no longer matched once a check recorded a line number. Float arithmetic is
not checked. The fixtures compare the doc text again and keep both forms.
The `scale_source` fixture of `test_modules.c` is unsigned for the same
reason, and its byte-by-byte library changed in two bytes: the type of the
constant and the opcode of `ret`.

## A sweep for false positives

Every program of `tests/programs` was built in dev mode and run. One stops:
`narrow_ops` adds 1 to an `i8` holding 127, which the suite runs in release
to watch it wrap. The check is right and the program means it. No other
program of the suite trips a check.

## State

455 tests pass on the development Mac and none is skipped. The ASan and the
UBSan builds run 454 each, without `no_paths`. Nothing of the section is left:
bounds, overflow, conversions, division and shifts are all built.
