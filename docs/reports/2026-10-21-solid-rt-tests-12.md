# Step rt-tests, twelfth session

The session went on from the list of `2026-10-21-solid-rt-tests-11.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/antic/` and `src/anti/` that a program, an option or
a library file reaches. Logs are under `build/drive/logs/`, named
`rt12-*`. The list of the regions not reached is `rt11-end-unreached.txt`
at the start and `rt12-end-unreached.txt` at the end. The probes are in
`build/rt12/`.

## Coverage per directory

Regions reached. The start is `rt11-end-summary.txt`, measured at
`195061b7`, whose sources equal those of `b330bf3f`. The end is
`507682b3`, `rt12-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt12-end-1.log` to `-5.log`. The first run of part 5 failed
`fmt_canonical` on the comment of `hosts.anti`, which `507682b3` fixed,
and part 5 then passed again, `rt12-end-5b.log`. 14 places of the list of
the start are reached, and the end lists none that the start did not.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.53% of 9252 | 95.53% of 9252 | 414, 414 | 95.91% | 95.91% |
| `src/antic/` | 98.08% of 55587 | 98.11% of 55587 | 1065, 1053 | 98.13% | 98.15% |
| `src/anti/` | 98.28% of 11663 | 98.29% of 11663 | 201, 199 | 98.31% | 98.33% |

## The tests

- antic, matrix refusal: a singleton whose base declares `get`, the arm
  of `declare_get` for a `get` of the base, `54fc08cc`.
- antic, matrix program: an operator of a generic class with a type
  parameter of its own, `operator fn add<U>(self, other: Box<U>)`. The
  declaration passes R067 through the arguments of a copy in
  `names_param`. The use is refused, T172, `91cf5fed`.
- anti, `tests/bind/functions.h`: a function declared twice under two
  names of its parameter, which binds once, `98a70442`.
- anti, `tests/bind/messages.cmake`: `clang_struct_without_a_field`, an
  empty struct of GNU C, which has no list of fields in the AST,
  `39dc6c90`.
- anti, `tests/doc/`: `operators.anti` with two `operator fn eq` that
  share their name. The page of the interface heads them `eq:Level` and
  `eq:Width`. The page of `--private` is T173, `a79d637d`.
- antic, `tests/modules/generic_plugin`: `supports` of a library of
  plugins in a generic of a library module. The checked tree of the
  library file carries the descriptor of the interface. The writer and
  the check of the tree reach it for the first time, `4bdf6ed1`.
- antic, `tests/modules/generic_loops`: a range without a name and a
  walk through a pointer to a type parameter, each in a generic of a
  library module, `25fa5e03`.
- `507682b3` takes the canonical form of the comment of `hosts.anti`.

## Findings

The step closed none. Two are new, with their tests.

- T172, major: an `operator fn` of the body of a generic class with a
  type parameter of its own takes no argument for it from its operands.
  `sema_operator_copy` of `src/antic/sema_generic.c` gives the signature
  of an operator with an owner back unchanged, so `a + b` is refused with
  `` expected `Box<U>`, found `Box<byte>` ``. The three modes of
  `matrix_types_generic_operator_of_a_class_named_through_the_class` hold
  it. Left for the step of the rules of types.
- T173, minor: `anti doc --private` and `--dev` head two operators that
  share their name `eq` both, where the `[provisional]` entry on shared
  names under "Generics and collections" heads each `eq:Point`.
  `tree_item` of `src/antic/docpage.c` takes the name of the item, and
  `interface_item` the name of the symbol. `anti_doc_operators_private`
  holds it. Left for the step of `anti doc`.

The message of T172 names `Box<byte>` for a source that writes `u8`.
That follows the `[provisional]` reading of R136: copies of generics keep
the first name of a type.

`tests/known-failures.txt` held 117 lines and 98 entries, 96 expected to
fail on the Mac. It holds 121 lines and 102 entries, 100 expected to fail
on the Mac. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/antic/driver_link.c` 36 to 54, `find_crt_dir`: the platform
  linker for Linux runs on a Linux host alone. On the Mac antic refuses
  it before the link, `--linker platform --target linux-arm64`.
- `src/antic/sema_generic.c` 646: `meets_hook` answers every hook of
  `hook_names` before `builtin_meets` could give false.
- `src/antic/lower_expr.c` 1483 to 1485: a read of an `f16` gives an
  `f32`, so no conversion starts from an `f16` and ends in one.
- `src/anti/repo.c` 346, `index_dir`: `deps.c` refuses a package name
  that is no name before it asks for an index.
- `src/anti/bindclang.c` 1279: `clang -E -dD` writes a blank after the
  name of a macro without a value, so the name never ends the line.
- `src/anti/bindclang.c` 1554: the host is a target of Anti on the Mac.

## Not run on the target

None. Every change of the session runs on the Mac.

## Gates

- No warning of our code: `rt12-hb6.log`, `rt12-asan-build.log` and
  `rt12-ubsan-build.log`.
- Host, 5694 tests, `-I 1,2000` and `-I 2001,5694`: 2000 and 3720 passed,
  in 357 s and 328 s. Part 2 runs 26 fixtures of part 1 again. 100
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt12-host-1.log` and `rt12-host-2.log`.
- ASan, 5692 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5692`: 1151, 1105, 1576, 906 and 994 passed in 238 s, 182 s,
  183 s, 456 s and 365 s, `rt12-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1105, 1576, 906 and 994 passed in 236 s,
  160 s, 137 s, 171 s and 293 s, `rt12-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report, on
  `docs/audit/tests-findings.md` and on
  `tests/doc/operators.private.expected.md`.

## State at the push

```text
$ git log --oneline -3
507682b3 Take the canonical form of the comment of hosts.anti
25fa5e03 Test two generic loops of a library module
4bdf6ed1 Test supports of a plugin library in a generic of a library
$ git status --short
$ git rev-parse HEAD origin/main
507682b3cb874ab605dd2d05fef31c0159492412
507682b3cb874ab605dd2d05fef31c0159492412
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1053 places: `llvm_emit_function.c`, `driver_link.c`,
  `sema_generic.c`, `sema_const.c`, `antl_tree.c` and `sema_copies.c`
  have the most. Generic functions of a library module whose bodies hold
  rare forms reached `antl_tree.c` and `antl_verify.c` this session, and
  the same path may reach more of `antl_read.c` and `sema_copies.c`.
- `src/anti/`, 199 places: `bindclang.c`, `bindwrite.c`, `repo.c`,
  `fmt.c`, `deps.c` and `build.c`.
- `src/rt/`, 414 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `registry.c`. Most of `object.c` and `registry.c` are
  the arms of a missing descriptor.
