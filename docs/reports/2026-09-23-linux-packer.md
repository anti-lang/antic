# The Linux packer

## Findings

This step has no audit ID. `docs/audit/` names no finding of the packer.

- The packer left `src/rt/json.c`, `src/rt/symbols.c`, `src/rt/toml.c` and
  the `src/rt` include path out of anti. Fixed in `2a5ff63`. The macOS path
  took the same glob and missed the same files.
  `tools/sources.cmake` holds the sources and include directories of
  `antic_core`, `antic` and `anti`. `CMakeLists.txt` and
  `tools/pack-anti.cmake` both read that file. The compiler's sources are
  compiled without `src/rt`, as in the CMake build, because
  `src/rt/signal.h` hides the C library's `<signal.h>` from
  `src/antic/process.c`. The first attempt, one include path for every
  source, failed there. Every host now links from objects.
- The Linux VM did not build HEAD. `-Wconversion` of the host runtime
  refused `enum anti_entry` plus `enum anti_hook` in three macros of
  `src/rt/object.h`. A Mac builds no host runtime, so only Linux saw it.
  Fixed in `917f459` with casts to `int`.

## Test

`package_keys` (`tests/run_package.cmake`) unpacks the package of the host
and runs its anti. It runs `anti bind` on `tests/bind/raylib_api.json`,
`anti build --release` of `tests/anti-build/app` with its `anti.toml`, and
`anti symbols inventory` on the release program, off Windows as
`anti_symbols` does. A host other than Linux packs linux-arm64 as well.

- Red on the Mac: `build/drive/logs/package-red.log`, "deps.c: 'toml.h'
  file not found".
- Red on the Linux VM, HEAD with the `object.h` fix and the new test:
  `build/drive/logs/vm-package-red.log`, the same error.
- Green on the Mac: `build/drive/logs/package-green.log`. Green on the
  Linux VM, where the packer compiles anti for the package:
  `build/drive/logs/vm-package-green.log`.

## Gates

- Build: no compiler warning, `build/drive/logs/host-build.log`. The one
  `ld: warning: ignoring -lto_library` comes from Apple's linker on
  `antic_unit_tests`, and earlier sessions' logs hold it as well.
- Host suite 793 of 793, `build/drive/logs/host-suite.log`.
- ASan 792 of 792, `build/drive/logs/asan-suite.log`. UBSan 792 of 792,
  `build/drive/logs/ubsan-suite.log`.
- The docs-style checker: `docs/decisions.md` and `src/rt/object.h` report
  nothing. It skips `.cmake` files, so their comments were checked as
  copies named `.py`, in `build/drive/logs/style2.log`. None of its
  findings is on a changed line. It reads `CMakeLists.txt` as Markdown:
  33 findings before this step, 30 after, none on a changed line.

## The Linux VM suite

At `2a5ff63` the VM passes 672 of 679, three of them skipped
(`build/drive/logs/vm-suite.log`). `deps_dir`, `anti_bind_raymath` and
`anti_bind_refusals` failed under `-j4` and pass alone. `anti_build` ("the
map names no com.example.app.main") and `program_platform_linker`
(`__aarch64_swp1_acq_rel` undefined under GNU ld) fail alone too. They fail
the same way on HEAD with only the `object.h` fix, in
`build/drive/logs/vm-base.log`, so this step did not cause them. They are
left for a step of their own.

## Decisions

One entry under the packer entries of `docs/decisions.md`. It is not
provisional, because the task named the rule.

## Questions

- The Mac compiles `src/rt` for no host with `-Wconversion`, so a
  conversion warning of the runtime surfaces on the Linux and Windows VMs
  only. Should the cross build of the runtime take the host warning set?

## Proof

```text
$ git log --oneline -3
2a5ff63 Build the programs of a package from the source list of the build
917f459 Add the entry enumerations of the runtime as int
9dcfba4 Report the pinned downloads from ANTI_DEPS_DIR
$ git status --short
$ git rev-parse HEAD origin/main
2a5ff63fb3375612b995b3350638e65ac5b50806
2a5ff63fb3375612b995b3350638e65ac5b50806
```

The commit of this report follows these, and is pushed.
