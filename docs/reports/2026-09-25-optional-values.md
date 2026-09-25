# Optional values

"Optional values" of round five is built. `?T` stands before any type. A
`?T` of a value is the value and one flag byte, and `?*T` stays one pointer.

## What was done

- `TYPE_OPTIONAL` is a struct of `value` and `has` with C layout, so layout,
  passing and returning are the struct's. The parser reads `?` before any
  type, and `??T` as two.
- The checker records `to_optional` where a T stands for a `?T`, and
  lowering writes the value and the flag. A `?T` compares with `none`,
  narrows after a test, and works with `if let`, `let ... else`, `??`, `?.`
  on a `?T` base and the guards of `catch`. It is read as a T only after a
  test, and every other use says `` `n` may be `none`, test it before
  reading it ``.
- `?T` of a type parameter compiles per copy. Where the argument is a
  pointer, the copy's `?T` is that `?*U`, one word.
- `?Match` is a `?T` of `Match`. The runtime writes the flag after the
  match, 104 bytes in all.
- The C header writes `struct anti_opt_<T>` with `value` and `has`.
- A `?T` of a class value that needs the teardown is torn down, copied by
  `dup` and refused by `=` where it holds an object.
- The library format is version 63.
- `docs/notes/optional-values.md` holds the choices of the passes.

## Tests

- `programs/optional_values.anti` runs a `?T` of `int`, `u8`, a tuple, a
  `str`, a `bool`, a struct, a class value, a class value with memory of its
  own, a field of a class and a type parameter, in a function and in a
  generic struct. Log: `build/drive/logs/opt-run2.log`.
- `errors/optional_values.anti` holds the refusals.
- `clib_optional` calls four functions of `?int` and `?Spot` from C.
- `tests/modules/generics/` returns a `?T` from a library in both modes.
- `test_nullable` keeps the refusals of `?.` and adds a `?T` base.

## What failed and how it was fixed

- The first draft let `p?.x` give a `?U` of a value field. "Small things"
  refuses a result that is no pointer, so that stays refused, on a `?T`
  base as well.
- `lower_store_value` stored a wrapped value by the type of the value, and
  the IR verifier refused a `may fail` function of `?int`. It now builds the
  `?T` in place.
- `pattern_methods.err` refused `?Regex`, which is now a type. The case is
  gone. `emit_identity` was written again from this Mac, since the programs
  of patterns change with `?Match`. The golden library files
  `scale.antl.hex` and `pick.antl.hex` were written again for version 63.
- Four messages of the nullable rules now name `?T` beside `?*T`, and the
  unit tests follow.

## Provisional decisions

All in `docs/decisions.md` under "Generics and collections": `?` before a
type that may be `none` makes a `?T` of it, and a copy's `?T` of a pointer is
`?*U`. The header names `anti_opt_<T>` with `value` and `has`. `none` writes
the flag alone. A `?T` compares with `none` alone. A literal takes its type
from T. A narrowed `?T` keeps its narrowing through `+=`. `if let` takes
every value that may be `none`. `?.` takes a `?T` base and still gives
`?*U` alone. `catch` guards a `?T`. A `?T` of a class value is torn down
and copied. A `?T` field has the type id none. A generic `?T` parameter
infers T from `?U`, `?*U` or U.

## Questions for Eddie

- "Small things" refuses `p?.x` of a field that is no pointer "since Anti
  has no optional values". Should `?.` now give `?U` of any field?
- `recv(c)` gives `?*T`, a provisional that reasoned that `?` stood before
  `*` and `fn` alone. Should it give `?T` now?
- `serialize` passes over a `?T` field. Should it write the value or `null`?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1037
of 1037, ASan 1036 of 1036 and UBSan 1036 of 1036. Logs:
`build/drive/logs/opt-host-final.log`, `opt-asan-test.log` and
`opt-ubsan-test.log`. The docs-style checker reports nothing on every
touched file but `tests/CMakeLists.txt`, whose `#` comments it reads as
Markdown, with 290 findings before and after.

## Proof of the push

After the push of `1d7a919`:

```text
$ git log --oneline -3
1d7a919 Report the optional values
4b4bb38 Record the optional values in the decisions and the overview
bb6c902 Record the assembly of the programs of optional values
$ git status --short
$ git rev-parse HEAD origin/main
1d7a91905a796cf142e427aee80fe7873207b675
1d7a91905a796cf142e427aee80fe7873207b675
```
