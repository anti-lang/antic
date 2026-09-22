# The wrapping and saturating operators and Flags

"Wrapping and saturating operators" and "Flags" of `docs/anti-language-additions.md` are
built: `+% -% *% <<%`, `+| -| *|`, `mul_high`, the built-in `Flags` and the flags form
`let (result, flags) = e;`. A carry in is `adc` on x86_64 and `adcs` on ARM64. The
development Mac passes 661 of 661 tests, and the ASan and UBSan builds pass 660 each,
without `no_paths`. The build has no warnings. The VMs did not run.

## What was done

1. `fa87e9d`. The lexer reads the seven operators as one token each, and the parser gives
   each the precedence of its plain operator. `+% -% *%` lower to the plain operation
   without the dev-mode check. `<<%` masks the shift to 0 for a count outside the width.
   `mul_high` is a built-in call that the checker turns into a binary expression, and a
   function or local of that name wins over it. The IR gains `smulh`, `umulh` and six
   saturating operations. `src/expand.c` expands the saturating ones in the back end after
   the layout, where `c_long` has its width and `c_wchar` its signedness. `src/arith.c`
   computes the constants for the checker, the optimizer and the symbolic folder.
2. `85a6993`. `Flags` is a struct of `anti.lang` that the compiler declares, with no
   descriptor. The flags form destructures the `(T, Flags)` of one `+ - * << >>` or unary
   `-`, and its flags name assigns a Flags variable in scope. `(result, flags) = e;`
   assigns two names. The checker counts the fields each function reads, and lowering
   writes one flag operation and one read per such field. x86_64 selects `add`, `adc`,
   `sub`, `sbb` and `neg` with a `set` per flag at every width. ARM64 selects `adds`,
   `adcs`, `subs`, `sbcs` and `negs` with a `cset` at 32 and 64 bits. The back end expands
   every other flag operation. A plain `a + b + f.carry` keeps the dev-mode check. The C
   header writes `struct anti_Flags` where an exported signature or struct names it.
3. `609519d`. The decisions gain the section "Wrapping and saturating operators and
   `Flags`". `docs/notes/flags.md` holds the choices of the passes. The status line of the
   syntax overview and the state of the handover follow.
4. The tests. `programs/wrapping`, `saturating`, `mul_high`, `flags` and `carry_chain` run
   on ARM64, under Rosetta on x86_64 and in dev mode. `flags` covers each operation and each
   flag at 8, 16, 32 and 64 bits, the carry and the borrow in, the reuse of a flags name, a
   parameter as the flags name, the plain form and a function that reads one flag.
   `carry_chain` adds and subtracts four-word numbers, written out and as a loop.
   `checks/overflow_carry.anti` traps a plain carry in. `test_x86_64.c` and `test_arm64.c`
   pin `adcq`, `sbbq`, `btl`, `adcs` and `sbcs` and the flag reads, `test_lower.c` pins that
   an unread flag is not computed, `test_arith.c` pins the constant helper, and
   `clib_flags` passes `Flags` to C and back.

## What failed and how it was fixed

- The library format moved to 42 and then 43. `test_modules.c` and `antl_scale` hold the
  bytes of a library, and only the version and the op numbers after the new operations
  changed. Both were rewritten.
- `emit_identity` failed on the five new programs alone. The manifest was written on the
  Mac with `-DWRITE=yes` and gained 30 lines and lost none.
- The message buffer of an operator held 8 bytes and cut `mul_high` short. It holds 16.
- Five mutations were caught: the carry condition of ARM64 (`unit`, `flags`,
  `carry_chain`), no `bt` before `adc` (`unit`, `flags` under Rosetta), no mask on `<<%`
  (`wrapping`), `|` for `&` in the unsigned `-|` (`saturating`) and no field counted as read
  (`unit`, `flags`, `checks`). Logs: `build/drive/logs/mut1.log` to `mut5.log`.
- The host link prints `ignoring -lto_library`, as in every earlier session.

## Provisional entries

Under "Wrapping and saturating operators and `Flags`" in `docs/decisions.md`: the count of
`<<%`, the name and constants of `mul_high`, the meaning of each flag for each operation,
no trap and no fixed result for a shift count outside the width, the refusal of `-` before
a literal, the carry in of any Flags value with no `borrow` field, `Flags` as a struct of
`anti.lang` and `struct anti_Flags` in C, the names of the flags form, the fields a
function reads, and no compound assignment of the new operators.

## Questions

- `+| -| *|` expand into portable operations, about six instructions where ARM64 has
  `adds` and `csinv` for an unsigned sum. Should the back ends select the short forms?
- A carry goes through its bool, `add; setb; bt; adc`. Should a carry that comes straight
  from the word below keep the flag instead?

## Proof

Taken after the push of the code, before the commit of this report. Logs:
`build/drive/logs/test-final.log`, `asan-test.log` and `ubsan-test.log`.

```text
$ git log --oneline -3
609519d Record the operators and Flags in the decisions and the handover
85a6993 Build Flags and the flags form of an arithmetic operation
fa87e9d Build the wrapping and saturating operators and mul_high
$ git status --short
$ git rev-parse HEAD origin/main
609519de6f96f67f3ea1542541cafcd7b4086884
609519de6f96f67f3ea1542541cafcd7b4086884
```

Suites: host 661 of 661, ASan 660 of 660, UBSan 660 of 660.
