# Dev-mode checks

The choices inside the pass. The rules are in "Dev-mode checks" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md`.

## The switch

Lowering always emits a check. Every failure arm is a block whose `fail`
field holds `IR_FAIL_CHECK`, and an assertion's arm holds `IR_FAIL_ASSERT`.
`optimize_drop_failures(program, kind)` replaces the branch into such a
block with a jump to the other arm, and the passes that follow remove the
block, the call and the text.

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
value of the expression.

The LLVM text writes each of the three as the signed `with.overflow`
intrinsic of its width, `llvm.sadd.with.overflow.i64` and the others, and the
`IR_BRANCH_OV` after it branches on the `i1` of the pair. llc chooses the
sequence of each target from the intrinsic.

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

Bitfields. `lower_bitfield_check` in `src/antic/lower_check.c`, by R048 of "Rules of
2026-10-13". The type of a bitfield is an integer of a fixed width, so the two ends
of a field of n bits are numbers of the IR. An unsigned value is compared with 2^n.
A signed value plus 2^(n-1) is compared with 2^n as unsigned, which holds both ends
in one comparison. `lower_assign` calls it for `=` and for a plain compound
assignment, and `prepare` of `lower_owning.c` for a field of a literal. A number
of the IR that fits gets no test, since the checker held it. `+%=` and the other
wrapping forms keep the low bits and need none. `lower_bitfield_saturate` holds the
result of `+|=`, `-|=` and `*|=` to the ends of the field, by R049. The IR has no
smaller-of-two, so each end is a saturating add or subtract of the type and its
inverse.

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

The least value by -1, by R031 of "Rules of 2026-10-13". A signed `/` and `%`
get a second check after the one of the divisor. It fails when the divisor is
-1 and the left operand is the least value of its type. The least value is the
one value that is below itself less one, since that difference wraps to the
largest. The test therefore names no limit, and a target-sized type is right on
every target. The failure prints `overflow in /` with both operands.

Unary `-`, by the same rule. A signed negation is `subov` of 0 and the value
with its `branchov`, so the least value is the one that fails. The failure
prints `overflow in -` with the one value. A build without the checks turns the
operation back into the subtraction that a negation emits, so its LLVM text
keeps its bytes. The text of the check takes a number among the constants of
the module, which moves the numbers after it.

`<<%`, by R032. A signed count is compared against zero before the shift, and
the failure prints `shift count out of range for <<%`. A count at or above the
width gives 0 and is no failure.

Not checked: unary `-` on a simd struct, `%` and `<<%` on its lanes, and the
flags form, which never stops.

## Compound assignment

`x += y` lowers its own operation, so `lower_assign` calls `binary_checks`
before it, as `lower_binary` does.

## The path in the text

`modpath_file_of_source` in `src/antic/modpath.c` gives the path of the source under
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

`optimize_function` merges the blocks before its first round. A dropped
check leaves a jump into the block that follows it, and the peephole that
fuses `t = op` with `x = copy t` needs the two adjacent. A round of
propagation between the two gives `t` a second use, and the pair never fuses.
