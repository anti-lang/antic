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

`back_end` in `src/antic/driver.c` runs it twice, once per kind. Release mode drops
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

`anti_rt_check_failed(text, length, kind, a, b)` in `src/rt/check.c`. The compiler
builds the text, which names the file, the line and the operation. The kind
names the labels of the two values, and `src/rt/std.h` holds it as `enum
anti_check`. `src/antic/lower_lowerer.h` mirrors it as `enum check_kind`, and the unit test
`records_check_kinds` pins the numbers of the two together by reading the call
that lowering writes.

## The checks

Bounds. One unsigned comparison of the index against the count of elements. A
negative index is a large unsigned value and fails the same test. An array
takes the count from its type, and a `str` and a slice read it beside the
pointer. `first_element` returns both, so the base is evaluated once. A raw
pointer has no count and is not checked.

Overflow. `IR_ADD_OV`, `IR_SUB_OV` and `IR_MUL_OV` give the result of the
signed operation and record whether it left the range of its type. The
terminator `IR_BRANCH_OV` reads that. The two are adjacent, which the verifier
checks, so the back end emits the arithmetic once and branches on what it left.
`binary_checks` therefore returns the operation's result, which becomes the
value of the expression. The selector keeps the operation in `s->overflow` for
the branch that follows it.

The sequences are in `emit_overflow` of `src/antic/arm64.c` and `src/antic/x86_64.c`, and
`overflow_cond` gives the condition each one leaves. ARM64 uses the V flag of
`adds` and `subs`. It has no arithmetic narrower than 32 bits, so a narrow type
operates one width up and compares the result with its own sign extension. It
has no flag-setting multiply either. A 64-bit `*` takes its result from `mul`
and compares `smulh` against the sign of that result. A 32-bit `*` takes the
low half of `smull` and compares it against its own sign extension. x86_64 sets
the overflow flag for all three, so `two_operand` and `emit_mul` serve
unchanged. Its `imul` has no two-operand form below 32 bits, which takes the
route through the wider registers.

Dropping the checks turns every overflow operation back into its plain
arithmetic, so a build without them emits what it emitted before the checks
existed.

Narrowing. `narrow_check` in `src/antic/lower_expr.c`. The value goes to the target type
and back to the source with the target's signedness. A value the target cannot
hold comes back changed. The round trip is blind to a change of sign alone,
because a target of the same width keeps every bit. That case is a signed
comparison against zero. A signed source and an unsigned target need it at
every width. An unsigned source and a signed target need it only where the
target is no wider. A target-sized type needs no case of its own, because the
round trip passes wherever the conversion turns into a copy.

Conversions to `char` and to an enum replace the two tests with one of their
own, in `scalar_check` and `enum_check`. A `char` must be at most `0x10FFFF`
and outside the surrogates, which is one unsigned comparison and one more on
the value less `0xD800`. An enum value must be one the enum declares, which is
one comparison per name joined by `or`. `integer_form` gives the type a value
converts as, since an enum converts as its base type, and `enum_value` extends
the sign of a declared value whose base type is signed.

Division. The divisor is compared against zero before the operation, so a
divisor of zero never reaches the instruction. The check is emitted for every
target, x86_64 included, whose own trap is a signal with no message.

Shifts. The count widens to 64 bits by the signedness of the type and is
compared, unsigned, against the width. The width is `size_of` the type times
eight, a symbolic value the back end folds, so a target-sized type is right on
every target. A negative count widens to a large unsigned value and fails the
same comparison.

## Compound assignment

`x += y` lowers its own operation, so `lower_assign` calls `binary_checks`
before it, as `lower_binary` does.

## The path in the text

`module_file_of_source` in `src/antic/modpath.c` gives the path of the source under
the first search root that holds it, and its file name alone outside every
root. `lower_checked` records that as the module's file, which an assertion
and a check name, while a message keeps the path the command line gave. The
same text gives the module path, so the two never disagree, and a library file
holds the same bytes whichever checkout compiled it.

## The values a failure prints

They are widened to 64 bits inside the failure block, which runs only when the
check fails. The path a program takes therefore pays the test and the branch
alone. It also keeps an overflow operation next to its branch, since nothing
may come between them.

## An ordering in the optimizer

`ir_optimize_function` merges the blocks before its first round. A dropped
check leaves a jump into the block that follows it, and the peephole that
fuses `t = op` with `x = copy t` needs the two adjacent. A round of
propagation between the two gives `t` a second use, and the pair never fuses.
