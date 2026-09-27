# anti bind --clang as the second exception to the rule of host tools

The step builds Eddie's answer to the question of
`docs/reports/2026-09-27-distribution-target.md`. Status: DONE.

## What was done

- `96bbe6c`: when the clang that `anti bind --clang` finds is missing or of
  another major, the message names the major it needs. It also names the
  archive of the pinned clang for the user's host and the page of its release.
  `tools/clang-release.cmake` reads the three values from `tools/llvm-version`
  and `tools/clang-pin`, for the build, the bind tests and the packer. The
  refusals case gained a run with no clang on PATH, and the refusals of
  another version expect the new text. Both failed before the change.
- The documents: rule 6 under "Binary distribution" in `docs/decisions.md`,
  the section "Distribution" of `CLAUDE.md` and the section "Binary
  distribution" of `docs/tooling.md` name both exceptions. The two entries on
  `anti bind --clang` in `docs/decisions.md` and the paragraph of
  `docs/tooling-addendum.md` say it is such an exception. They also say that
  the bindings Anti ships are generated during Anti's own build.

## Proof

- Host: 1157 of 1157 passed.
- ASan through `ctest --preset asan`, with leak detection: 1156 of 1156.
- UBSan through `ctest --preset ubsan`: 1156 of 1156.
- The checker reports nothing on every `.md` file the step touched.
