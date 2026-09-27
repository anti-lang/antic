# Shared<T> of the add5/shared lane onto main

The step brings the two commits of `add5/shared`, `d912ce5..add5/shared`,
onto `main` in order. The branch stays. Status: DONE.

## What was done

- `5f8a40c`, the pick of `d6eb4a4`: `Shared<T>` in `anti.mem`. Its conflicts
  stood in `tests/CMakeLists.txt` alone, and both sides stay. `deserialize_dev`
  links the dev objects of `anti.lang` and `anti.mem`, `std_shared_dev` stands
  after the collection tests of `main`, and `shared_handles` joins the error
  listings beside `handles`. The lane's dev objects take the suffix
  `${anti_object}` of `main`. `src/std/anti/mem.anti` merged without a
  conflict, and its tests pass under the owning-value rules and the compiled
  default `==` of `main`.
- `31d9b3c`, the pick of `0d6248b`, without a conflict.
- `4cfe8ac`: the entries of `docs/decisions-shared.md` stand at the end of
  "Generics and collections" in `docs/decisions.md`, and the file is gone.
  `docs/anti-syntax-overview.md` and `CLAUDE.md` name `Shared<T>` as built.
- `cc816b9`: `deserialize.anti` imports `anti.mem`, so its six entries of
  `tests/emit-identity/programs.sha256` were written again on the Mac with
  the command of `docs/notes/value-rules.md`.

No pinned library file changed.

## Proof

- Host: 1160 of 1160 passed.
- ASan through `ctest --preset asan`, with leak detection: 1159 of 1159.
- UBSan through `ctest --preset ubsan`: 1159 of 1159.
- The checker reports nothing on every `.md` file the step touched.
- `add5/shared` still points at `0d6248b`.

The picks were checked one at a time only by the tests of `Shared<T>`. The
full suites ran on the final tree, where `emit_identity` needed the fourth
commit.
