# Step probe: two hundred combinations of the language

The step wrote 200 new tests in `tests/matrix/`, each a combination that no
cell of `docs/notes/test-matrix.md` holds, as a programmer from C++ and from
Java would write them. It fixed nothing. Commit `f09bdf6f` holds the tests,
the six findings and their lines of `tests/known-failures.txt`. The drafts
and the helper that built each draft in the three modes are under
`build/probe/`, and the logs under `build/drive/logs/`, named `pr-*`.

## The tests

174 programs and 26 refusals. A program runs in dev mode, in release mode
and with `--memory-checks`, so the 200 register 548 tests of ctest.

| Area | Programs | Refusals | Defects | Grades |
|---|---|---|---|---|
| Flow | 25 | 0 | 0 | |
| Values | 22 | 3 | 0 | |
| Types | 24 | 1 | 1 | severe |
| Ownership | 18 | 8 | 1 | severe |
| Errors | 21 | 4 | 1 | major |
| Threads | 22 | 3 | 0 | |
| Classes | 21 | 5 | 3 | 2 major, 1 minor |
| Modules | 21 | 2 | 0 | |
| All | 174 | 26 | 6 | 2 severe, 3 major, 1 minor |

The rate is 6 defects in 200 tests, 3.0 per hundred. The test run found 124
in about 2,950, 4.2 per hundred. Counted per test of ctest, the step found 6
in 548, 1.1 per hundred. Flow, values, threads and modules gave none. The
defects stand where generics, ownership and the class model meet. One binds
a case of a generic variant in a generic function. One copies an `own`
pointer between two fields, and three concern the members of a class.

## Findings recorded

- T183, severe. An arm of `switch` or an `if let` that binds a case of a
  generic variant inside a generic function gives the slot the open case
  type `Result<T, E>.Ok`, so a `str` comes out cut short. Release prints
  bytes past the text and dev stops with a segmentation fault. The
  `--memory-checks` test passes by chance and is not listed.
- T184, minor. Two fields of one name in a class: the message calls the
  class a `struct`.
- T185, major. A field and a `pub fn` of one name in one class are taken,
  and no call of the function compiles.
- T186, major. A failing call with `catch` is refused as an argument, as an
  operand, after `return`, after `yield` and in an `{expr}` of an `f"..."`,
  against the entry on both forms of a failing call.
- T187, severe. `=` from one `own ?*Node` field into another compiles, and
  the node is freed twice.
- T188, major. A `concrete fn` over a `protected` or private function of the
  base is taken and replaces nothing.

None was closed. `tests/known-failures.txt` held 133 lines before and 142
after, 114 entries before and 123 after. The Mac expects 112 tests to fail
before and 121 after, since two entries are `host=windows`.

## Questions for Eddie

- The overview says that compound assignments derive from an `operator fn`,
  and so does "Operators" in `docs/anti-object-model.md`. Decision 27 of
  "Decisions of 2026-10-11" says that `+=` is not derived from `operator fn
  add`, and antic follows the decision. The two documents contradict it.
- No form moves an object out of an `own` field into another, so a list
  whose nodes own their successor cannot push at its front. T187 asks for a
  refusal, and the move it leaves open is a feature.
- A simd struct does not meet `add`, so `twice<T: add>` refuses a `Vec4`.
  The entry on the built-in types names the numbers alone.
- A call does not infer `T` from a derived class: `drain(&c)` with `c` a
  `Countdown inherits Source<int>` is refused for `*Source<T>`, and
  `drain<int>(&c)` compiles.
- A `const` of a class body is reached as `History.SIZE` inside the body as
  well. The bare `SIZE` is unknown there.

The drafts met rules a programmer from C++ or Java does not expect. Among
them: `char` converts from `u32` alone, `?.` ends at a pointer, a parameter of
a type parameter returns only when `own`, a second `dispatch` of an object in
flight does nothing, `Number` excludes the unsigned types, `join_all` is a
built-in name and a struct takes no `own` field. Each is documented, and
each draft was rewritten.

## `[provisional]` entries

None.

## Gates

- No warning of our code: `pr-build3.log`, `pr-asan-build.log` and
  `pr-ubsan-build.log`. The step changed no source under `src/`.
- Host, 6359 tests, `-I 1,2100`, `2101,4200` and `4201,6359`: 2100, 2127
  and 2166 passed in 385 s, 247 s and 292 s, `pr-host-1.log` to `-3.log`.
  The later parts run the fixtures of the first again. The 121 expected
  failures fail as listed, and the two `mimalloc_environment` tests skip.
- ASan, 6357 tests, `-I 1,1150`, `1151,2250`, `2251,3500`, `3501,4500`,
  `4501,5400` and `5401,6357`: 1151, 1109, 1277, 1012, 907 and 957 passed
  in 242 s, 192 s, 176 s, 136 s, 458 s and 352 s, `pr-asan-1.log` to
  `-6.log`.
- UBSan, 6357 tests, the same parts and the same counts, in 241 s, 174 s,
  141 s, 113 s, 180 s and 303 s, `pr-ubsan-1.log` to `-6.log`.
- `emit_identity`, `emit_identity_texts` and `link_identity_macos-arm64`
  pass unchanged.
- The docs-style checker reports nothing on the `.md` files of the step.

## Not run on the target

Nothing. The step added tests and changed no code.

## State at the push

The state after the push of `f09bdf6f`, before the commit of this report:

```text
$ git log --oneline -3
f09bdf6f Probe the language with 200 new matrix programs and refusals
5f0d59f7 Report the step fuzz-fix
9c4c62af Record the commits of T180 to T182 and the campaign of fuzz-fix
$ git status --short
?? docs/reports/2026-10-22-solid-probe.md
$ git rev-parse HEAD origin/main
f09bdf6f6b1420b796a204dee0cfd9e836d5b915
f09bdf6f6b1420b796a204dee0cfd9e836d5b915
```
