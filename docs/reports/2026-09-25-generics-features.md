# What can be generic and the other features with generics

The step builds "What can be generic" and "Other features with generics"
of "Generics" in round five of `docs/anti-language-additions.md`. It covers
the generics of a module's own source.

## What was done

- `N: int` sizes a type and reads as a value in a body. A length built of
  constant parameters alone, `[v; N]` or `-> [N * 2]int`, stands wherever a
  length does.
- A function of a class with type parameters of its own compiles a copy
  per class and per argument, `Box<int>.map<float>`. It does so on a generic
  class and on a plain one. No table, reflection list or C table struct names it. One
  that is `abstract` or replaced is refused alone, without the follow-on
  errors of the replacement checks.
- A type nested in a generic class takes the parameters of the classes
  around it. Each copy of the class has a copy of it, `Stack<int>.Node`.
- A generic synchronized or concurrent class keeps its rules in every copy.
  `parallel` and `dispatch` infer the arguments of a generic worker and check
  the worker rules with the concrete types.
- A generic struct takes its operators and hooks from generic `operator fn`
  functions of its module. A type argument of function type is the plain
  form. `v is Result.Ok` names the case of a copy.
- The formatter reads `may fail` in a list of type arguments and a type after
  `size_of(` and `alloc(`.
- Tests: the programs `generic_constants`, `generic_methods`,
  `generic_nested`, `generic_sync`, `generic_hooks`, `generic_may_fail`,
  `generic_fn_args` and `generic_variants`, each in release and dev mode.
  `errors/generic_copies.anti` holds the refusals of a copy, and
  `errors/generics.anti` gained a replaced function with type parameters.

## What failed and how it was fixed

- The copy of a generic class with a constant parameter crashed antic when
  a function of the body called another. The call recorded no value for the
  parameter. It now records the parameter's own value.
- `T(args)` and `alloc T(args)` passed a `keep own` function as one word, and
  the IR failed verification. `construct` now takes each argument by the form
  of its parameter, and an `own fn` field without a default is cleared before
  it runs. The local form aborted in `free` without the second fix.
  `programs/construct_keep_own.anti` pins both.
- A closure inferred at a type parameter reached a field of a copy, where
  only its code word was kept. Inference now binds the plain form.
- The dev runs of `generic_may_fail` and `generic_fn_args` link the object of
  `anti.lang`, as `sync_exits` does.
- `emit_identity` listed the new programs. The manifest was written again
  with `-DWRITE=yes`, and no earlier program changed its output.

The suites: the host passes 1033 of 1033, ASan 1032 of 1032 and UBSan
1032 of 1032, without the `no_paths` test in the two sanitizer builds.

Logs: `build/drive/logs/gate_build.log`, `gate_host.log`, `gate_asan.log`,
`gate_ubsan.log` and `suite1.log` to `suite4.log`.

## Provisional decisions

All stand under "Generics and collections" in `docs/decisions.md`:

- the names, the visibility and the absence from tables of the copy of a
  function with type parameters of its own;
- the parameters of a nested type, its name without arguments in the body,
  and the name of its copy;
- the inference of a generic worker from `f<int>(x)`, the chunk or the object
  and the arguments;
- a type argument of function type is the plain form;
- the generic `operator fn` of a generic struct and `a.f(b)` on a copy;
- lengths of constant parameters wherever a length stands;
- `v is Result.Ok` on a copy;
- a library file carries neither a function with type parameters of its own
  nor a nested type of a generic class yet.

## Questions for Eddie

- A library file does not carry the two new forms. They need a new format
  of the struct entry and of the declarations. Should that be the next step?
- `Box<Mutex>` copies a Mutex wherever the body copies a `T`, and no check
  of the copy refuses it. Should a type that holds a Mutex be refused as a
  type argument?
- The docs-style checker reads the `#` comments of `tests/CMakeLists.txt` as
  Markdown. It reports 258 findings before this step and 259 after it. The
  new one counts a comment and the code after it as one sentence. Every other touched file reports nothing.
- `anti fmt` reads a variable named `by` as the word of `parallel`, and
  writes `by * at` as `by *at`. The test renamed the variable.
- Linking with the pinned clang prints `ld: warning: ignoring -lto_library`
  for a `libLTO.dylib` that the pinned archive lacks. No compile warning
  exists.
