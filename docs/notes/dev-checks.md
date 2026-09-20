# Dev-mode checks

The choices inside the pass. The rules are in "Dev-mode checks" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md`.

## The switch

Lowering always emits a check. Every failure arm is a block whose `fail`
field holds `IR_FAIL_CHECK`, and an assertion's arm holds `IR_FAIL_ASSERT`.
`ir_drop_failures(program, kind)` replaces the branch into such a block with
a jump to the other arm, and the passes that follow remove the block, the
call and the text.

`back_end` in `src/driver.c` runs it twice, once per kind. Release mode drops
both and dev mode keeps both. `--checks`, `--no-checks`, `--asserts` and
`--no-asserts` override the mode. The build that compiles the program decides,
so a library file carries every check and every assertion.

A branch reaches the drop in either direction. A check that tests for success
branches to its failure arm when the test fails. The overflow test branches to
the failure arm when it holds. The drop reads which arm carries the kind and
jumps to the other one.

Nothing of a dropped check survives the optimizer. The condition is pure.
`remove_unused_results` deletes it once the branch is gone, the loads included.
The only trace in a release build is the numbering of the anonymous globals.
The text of a check takes a number before it is removed.

## The failure routine

`anti_rt_check_failed(text, length, kind, a, b)` in `rt/check.c`. The compiler
builds the text, which names the file, the line and the operation. The kind
names the labels of the two values, and `rt/std.h` holds it as `enum
anti_check`. `src/lower.c` mirrors it as `enum check_kind`, and the unit test
`records_check_kinds` pins the numbers of the two together by reading the call
that lowering writes.

## The checks

Bounds. One unsigned comparison of the index against the count of elements. A
negative index is a large unsigned value and fails the same test. An array
takes the count from its type, and a `str` and a slice read it beside the
pointer. `first_element` returns both, so the base is evaluated once. A raw
pointer has no count and is not checked.

Overflow. `IR_ADD_OV`, `IR_SUB_OV` and `IR_MUL_OV` give 1 when the signed
operation leaves the range of its type. The arithmetic is repeated: the
flag-setting instruction writes a register that nothing reads, and the ordinary
operation still produces the value. `select_is_overflow` lets the selector fuse
the test with the branch after it, so the fused form is the flag-setting
instruction and a conditional branch.

The sequences are in `overflow_flags` of `src/arm64.c` and `src/x86_64.c`.
ARM64 has no arithmetic narrower than 32 bits. A narrow type therefore operates
in 32 and compares the result with its own sign extension. It has no
flag-setting multiply either. A 32-bit `*` reads the whole product from
`smull`, and a 64-bit `*` compares `smulh` against the sign of the low half.
x86_64 sets the overflow flag for all three. Its `imul` has no two-operand form
for 8 bits, which takes the same route through the 32-bit registers.

Narrowing. `narrow_check` in `src/lower.c`. The value goes to the target type
and back to the source with the target's signedness. A value the target cannot
hold comes back changed. The round trip is blind to a change of sign alone,
because a target of the same width keeps every bit. That case is a signed
comparison against zero. A signed source and an unsigned target need it at
every width. An unsigned source and a signed target need it only where the
target is no wider. A target-sized type needs no case of its own, because the
round trip passes wherever the conversion turns into a copy.

Division. The divisor is compared against zero before the operation, so a
divisor of zero never reaches the instruction. The check is emitted for every
target, not for ARM64 alone, because the IR is target-independent.

Shifts. The count widens to 64 bits by the signedness of the type and is
compared, unsigned, against the width. The width is `size_of` the type times
eight, a symbolic value the back end folds, so a target-sized type is right on
every target. A negative count widens to a large unsigned value and fails the
same comparison.

## Compound assignment

`x += y` lowers its own operation, so `lower_assign` calls `binary_checks`
before it, as `lower_binary` does.

## The path in the text

The text holds the source path the command line gave, which is what `assert`
already writes. A test that compares antic's output byte for byte therefore
runs antic in `tests/` with every path under it made relative, which
`tests/relative_paths.cmake` does for `run_dump.cmake`, `run_antl.cmake` and
`run_dev.cmake`. Two runners that spell one path differently would compile one
module into two programs, which `dev_modules` compares.
