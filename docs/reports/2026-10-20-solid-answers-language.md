# The step answers-language

The step builds R009, R017, R037, R051, R054, R066, R100 and R134 of
"Answers of 2026-10-18" in `docs/decisions.md`, one commit per rule. The
logs stand in `build/drive/logs/` under the prefix `answers-language-`.

## What changed

| Rule | Tests | Commit |
|---|---|---|
| R009 | `matrix_flow_switch_on_char_*`, two refusals `flow_switch_on_char_*` | `f97b8017` |
| R017 | `matrix_errors_fail_of_any_text_*`, `errors_fail_of_a_number`, `error_lang_error_text_maker` | `f26e47f1`, `5019434f` |
| R037 | `matrix_values_compound_on_simd*`, three refusals | `2435d064` |
| R051 | `types_align_above_4096`, `types_align_of_the_largest_value` | `e84e0e87` |
| R054 | `types_enum_value_from_earlier_values`, two refusals | `46333559` |
| R066 | `types_generic_literal_inferred_from_its_fields`, one refusal, `error_generics` | `1001b3e6` |
| R100 | `classes_private_members_of_nested_classes`, one refusal | `51df91ea` |
| R134 | `plugin_refused_needy`, `plugin_refused_holder`, `unit_plugin_table` | `596e28d0`, `39537e1e` |

- R009. `switch` takes a `char`, and the refusals of `bool` and of a type
  parameter name `char` among what it takes. Before the change the
  program `flow_switch_on_char` was refused, in `answers-language-t1.log`.
- R017. `fail` of an `f"..."` calls the protected `Error.of_text`, which
  takes over the memory, and any other `str` the protected
  `Error.of_copy`. The private `transient kept` of `Error` holds the
  bytes, and the `copied` hook gives a copy that `dup` makes bytes of its
  own. An error is 128 bytes now.
- R037. Lowering copies the old value out, evaluates the operand and
  writes the simd operation back. `%=`, `&=` on float lanes and a value
  of another simd struct stay refused, and so does a type parameter.
- R051. An N above 4096 is refused with the largest named. Before the
  change `align(8192)` compiled.
- R054. The bare names of the earlier values are constants of the base
  in a scope of the expression. A later value or the value itself is
  refused with one message.
- R066. A value of a field whose type names a parameter is checked once
  with no type expected and binds the parameter. `none` and an anonymous
  function give nothing.
- R100. The private and the protected members of the classes inside one
  class of the module are visible to each other.
- R134. `instance_at` may fail with the message of the runtime, and the
  checker gives the call the `*I` it names. The runtime gives a reason
  for a library that is not open. Every caller changed.
- The overview, the object model, `docs/anti-language-additions.md` and
  chapters 4, 6, 7, 8, 9, 10, 15 and 21 of the guide state the rules.
- `e3d90d38` makes the test of T150 deterministic, and `e31c8c00` formats
  the new sources and moves three declarations, which `fmt_canonical`,
  `anti_check` and `header_sections` refused in the first run.

## Findings

The step closed no finding of the table. It recorded three new ones,
each with its test in `tests/known-failures.txt`, left for the step of
its area:

- T148. A `?T` field of a class value with the default `none`, read from
  a library file, crashes antic in `lower_store_constant`. R017 met it.
- T149. A field default that names a class declared later in a library
  damages the library file. R017 met it, and `KeptText` would have
  needed it.
- T150. `alloc C { }` of a class aligned above 16 gives an object at a
  multiple of 16 alone. R051 met it.

`tests/known-failures.txt`: 26 lines before, 33 after. It names 7 tests
before and 14 after, 12 of which run on the Mac.

## `[provisional]` entries

Four stand under "What the solid run decided": R017 in the code, the
makers, `kept` and `dup` with its `--no-hooks` limit. R054, the bare
names as numbers of the base. R066, structs alone with `none` and an
anonymous function giving nothing. R100, protected members and two
nested classes of one class included.

## `emit_identity` and `link_identity`

- `f26e47f1` moves 51 programs, 306 texts: `Error.new` fills the new
  field `kept`. `answers-language-emit-r017.log` names the moved lines.
- `39537e1e` moves `link_identity_macos-arm64`: the runtime holds the
  text `the library is not open`. The test passed with that line removed.

## Gates

- Zero warnings in the three trees: `answers-language-build26.log`,
  `answers-language-asan-build.log` and `answers-language-ubsan-build.log`.
- Host, 5194 tests: `-I 1,2000` 2000 passed in 346 s and
  `-I 2001,5194` 3220 passed in 311 s, as ctest counts that range. The
  12 expected failures of the Mac failed as listed, and the two
  `mimalloc_environment` tests skip. `answers-language-host-final-a.log`
  and `-b.log`.
- ASan, 5192 tests: 1150 in 244 s, 1119 in 164 s, 1457 in 177 s and 1497
  in 501 s for `-I 1,1150`, `1151,2250`, `2251,3700` and `3701,5192`.
  `answers-language-asan-a.log` to `-d.log`.
- UBSan, 5192 tests: 1150 in 243 s, 1119 in 139 s, 1457 in 144 s and 1497
  in 339 s, the same parts. `answers-language-ubsan-a.log` to `-d.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every
  `.md` file of the step, in `answers-language-docs-final.log`.

## Not run on the target

- R009: `matrix_flow_switch_on_char_*` on Linux and Windows.
- R017: `matrix_errors_fail_of_any_text_*` on Linux and Windows.
- R037: `matrix_values_compound_on_simd_*` and
  `matrix_values_compound_on_simd_above_the_cap_*` on Linux and Windows.
- R051: `matrix_types_align_of_the_largest_value_*` on Linux and Windows.
- R134: `plugin_refused_needy`, `plugin_refused_holder` and
  `unit_plugin_table` on Linux and Windows, whose loaders differ.

## Questions

- The feature freeze in `CLAUDE.md` still says the cases wait for Eddie.
  The task did not name `CLAUDE.md`, so it stands as it was.

## State

After the push of `e31c8c00`, before the commit of this report:

```text
$ git log --oneline -3
e31c8c00 Format the sources of the answers and file their declarations
e3d90d38 Keep the objects of the test of T150 alive together
39537e1e Pin the link of return42 after the message of R134
$ git status --short
$ git rev-parse HEAD origin/main
e31c8c00fbaf35928df16d3ef7c691c43246ea34
e31c8c00fbaf35928df16d3ef7c691c43246ea34
```
