# Step pgo of the optimization facts

The step `pgo` of `docs/work-order-llvm-optimization.md`, choice D7 under
condition C3. It stopped before any code: the pinned release holds no
`llvm-profdata`.

## What was checked

`tools/llvm-pin` and `tools/clang-pin` name `23.1.1-anti.5` of
`anti-lang/llvm-tools`. The release carries twelve archives,
`llvm-tools-<tag>-<host>.tar.xz` and `clang-<tag>-<host>.tar.xz` for the six
hosts, with `SHA256SUMS` and its signature. The list is in
`build/drive/logs/pgo-release-assets.txt`.

The two archives of macos-arm64 under `build/deps/llvm/downloads/` and
`build/deps/clang/downloads/` match the pinned digests. Their listing is in
`build/drive/logs/pgo-archive-contents.txt`:

- `llvm-tools` holds `lld`, `llc`, `llvm-objdump`, `llvm-mc`, `llvm-ar`,
  `llvm-readobj` and `opt`. No `llvm-profdata`.
- `clang` holds `bin/clang` and, under `lib/clang/23/lib/`, the builtins,
  the crt objects, `libunwind.a` and the sanitizer runtimes. No
  `libclang_rt.profile` for any target.

No name in either listing contains `profile` or `profdata`.

## What the release must add

A release `23.1.1-anti.6` of `anti-lang/llvm-tools`, on the LLVM version
of `tools/llvm-version`, that adds:

1. `bin/llvm-profdata` to `llvm-tools-<tag>-<host>.tar.xz` of all six
   hosts. It merges the `.profraw` files of a `--profile-generate` run into
   the `.profdata` file that `--profile-use` reads.
2. The profile runtime of compiler-rt for each target and level that antic
   links, beside the builtins in `clang-<tag>-<host>.tar.xz`:
   `libclang_rt.profile.a` for `x86_64-unknown-linux-musl`,
   `aarch64-unknown-linux-musl`, the two glibc triples and the two Windows
   triples, and `libclang_rt.profile_osx.a` in `lib/darwin/`. A program
   that `-fprofile-generate` instruments calls `__llvm_profile_*` of that
   archive to write its counters at exit. The pinned release has none, so
   an instrumented program cannot link even with `llvm-profdata` present.
   If antic is to put the runtime in the runtime archive instead, the
   recipe of `src/native/` needs the compiler-rt source of the same
   version, which the pinned clang archive does not carry either.

`tools/llvm-pin` and `tools/clang-pin` then take the twelve new digests, as
the step `pins` of `docs/work-order-llvm-back-end.md` did for anti.4 and
anti.5. `tools/get-llvm.cmake` names `llvm-profdata` among the tools it
installs, `tools/check-llvm.cmake` checks its version, and the test
`pinned_tools` checks it as it checks `opt` and `llc`.

## Gates and state

The step changed no code, so no suite ran. This report is the only change.

`git log --oneline -3`, `git status --short` and `git rev-parse HEAD
origin/main` after the commit are in the reply of the session.
