# Step runtime-lto of the optimization facts

The step `runtime-lto` of `docs/work-order-llvm-optimization.md`, choice D7
under condition C3. The default build does not change.

## What the step built

- The runtime archive holds the runtime as bitcode of every target and level,
  48 archives in `lib/<target>/<level>/bitcode/full/` and `bitcode/thin/`. The
  pinned clang builds them with `-flto=full` or `-flto=thin` and the flags of
  the objects. clang runs in the source tree on `src/rt/<file>.c`, because
  bitcode records the path of its source as given, and the test `no_paths`
  found the build path in the first build.
- `antic --lto full|thin` runs opt with `lto-pre-link<O2>`, or
  `thinlto-pre-link<O2>` with `--thinlto-bc`, and writes the bitcode as the
  object beside the program. llc does not run. lld links the bitcode with the
  runtime as bitcode of the mode at `--lto-O2`, or `/opt:lldlto=2` on Windows.
  A macOS link with `-g` keeps the LTO objects in `<program>.lto/`.
- `tools/pack-anti.cmake` leaves `bitcode/` out of the package, and
  `package_keys` refuses a package that carries it, until Eddie's yes under C3.
- `ANTIC_TEST_LTO=full|thin` of `tests/CMakeLists.txt` runs the suite behind
  the option. It is empty by default.
- `tests/bench/ablate/run.py` takes `--lto=full` as a build, a word
  `--name=value` passing two arguments.

## Tests

`lto_links` of `unit_link` failed before the code existed. It pins the link
lines of macOS with and without `-g`, of Linux with musl and with glibc and
of Windows. It checks the words of the option as well. Beside it:

- `cross_link_lto_full_<target>` and `cross_link_lto_thin_<target>` link
  `return42` for all six targets.
- `program_mixed_work_lto_full` and `program_mixed_work_lto_thin` run
  `mixed_work`.
- `lto_mode`, `lto_refused_dev`, `lto_refused_assembly`, `lto_refused_lib`,
  `lto_refused_platform`, `lto_refused_windows_host` and `lto_no_bitcode` check
  the refusals.

The suite behind the option, the host tree reconfigured with
`-DANTIC_TEST_LTO=full` and then `thin`, built 581 program tests of release
mode through the LTO:

| Mode | Passed | Time | Log |
|---|---:|---:|---|
| full | 1619 of 1619 | 224 s | `build/drive/logs/lto-full-suite.log` |
| thin | 1619 of 1619 | 204 s | `build/drive/logs/lto-thin-suite.log` |

The first run of each failed `std_backtrace`. The LTO inlines the program's
`main` into `anti.rt.main`, so its trace names that function, which the
entry of `docs/decisions.md` on the inlining of release mode covers.
`tests/std/backtrace.lto.expected` holds that output. The first run of full
also gave `--lto` to two tests whose options stand in one word joined by
commas, `--dev,--no-trace` and `--linker,platform`. The filter now splits
them.

## Measurement

`tests/bench/ablate/run.py <program> -- --lto=full --lto=thin`, 15 runs each,
logs `build/drive/logs/runtime-lto-bench-<program>.md`. Every build printed
the line of its C twin, and `mixed_work` its baseline. The object of an LTO
build is bitcode, so the executable is the size to compare.

| Program | Default | Full | Thin | Full / default | Thin / default | C twin |
|---|---:|---:|---:|---:|---:|---:|
| `scalar_loop` | 299.9 ms | 298.6 ms | 298.0 ms | 1.00 | 0.99 | 293.0 ms |
| `objects` | 141.4 ms | 155.5 ms | 141.5 ms | 1.10 | 1.00 | 107.0 ms |
| `builder` | 264.9 ms | 262.4 ms | 261.3 ms | 0.99 | 0.99 | 44.9 ms |
| `simd_loop` | 129.9 ms | 129.9 ms | 129.8 ms | 1.00 | 1.00 | 125.9 ms |
| `map_work` | 181.8 ms | 180.7 ms | 181.9 ms | 0.99 | 1.00 | 137.8 ms |
| `mixed_work` | 192.9 ms | 191.0 ms | 191.2 ms | 0.99 | 0.99 | none |

A second run of `objects` gave 142.0, 153.8 and 141.9 ms, full at 1.08,
`runtime-lto-bench-objects-repeat.md`. Full LTO inlines `objects.main` into
`anti.rt.main`, whose loop runs slower. The step did not find why.

| Program | Executable default, full, thin | Release compile default, full, thin |
|---|---|---|
| `scalar_loop` | 134,352 B, 70,576 B, 94,544 B | 41.7, 102.1, 93.7 ms |
| `objects` | 137,824 B, 91,040 B, 117,072 B | 53.2, 219.9, 134.0 ms |
| `builder` | 138,720 B, 91,360 B, 118,480 B | 69.2, 241.9, 143.4 ms |
| `simd_loop` | 134,352 B, 70,576 B, 94,544 B | 42.9, 103.7, 96.7 ms |
| `map_work` | 229,888 B, 183,184 B, 230,720 B | 308.5, 626.5, 370.2 ms |
| `mixed_work` | 1,039,872 B, 991,280 B, 1,033,648 B | 2010.8, 2614.1, 2348.8 ms |

Read under C1, thin lies within 2 percent of the default on every program,
and the default then wins on the release compile. Full is slower on
`objects`. Both shrink every executable, full by up to 47 percent.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the entry of the
option:

- The runtime as bitcode is one archive per mode, since ThinLTO imports
  nothing from a module of full LTO and ld64.lld cannot choose the mode.
- `--lto` is refused with `--dev`, `-S`, `-c`, `--lib` and `--linker
  platform`, for a Windows program that hosts plugins, and for an archive
  without the bitcode of the mode. A macOS link with `-g` keeps the LTO
  objects in `<program>.lto/`.
- Under `--lto` the program's `main` may become part of `anti.rt.main`, and a
  test of `tests/std` whose output follows that brings `NAME.lto.expected`.

## Question

- Should the runtime ship as bitcode? Condition C3 waits for your yes. A
  package with both modes adds 48 archives across the six targets and the two
  of glibc, 22 MB before compression. On macos-arm64 one is about 0.5 MB,
  against 0.2 MB for the runtime as objects. On these programs ThinLTO
  changes no time by more than 1 percent, and full LTO costs `objects` 8 to 10
  percent. Both shrink the executable, and both lengthen the release compile.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/runtime-lto-{host,asan,ubsan}-build.log`.
- Host suite: 1619 of 1619, 147 s, `build/drive/logs/runtime-lto-host-suite.log`.
- ASan suite: 1618 of 1618, 448 s, `build/drive/logs/runtime-lto-asan-suite.log`.
- UBSan suite: 1618 of 1618, 305 s, `build/drive/logs/runtime-lto-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite. The default text did not change,
  so no pin moved.
- The docs-style checker reports nothing on `docs/decisions.md` and this
  report.

State after the push of the step, before this report:

```console
$ git log --oneline -3
b517b7ec Run the suite behind --lto with the output of its inlining
d70201d9 Let run.py pass an option of antic that takes a value
4c936ced Link the program and the runtime as bitcode under --lto
$ git status --short
$ git rev-parse HEAD origin/main
b517b7ec0bd8229f828bdd922b191edaf66c72fa
b517b7ec0bd8229f828bdd922b191edaf66c72fa
```
