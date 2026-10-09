# The LLVM tools into the package

Step `tools` of `docs/work-order-distribution.md`, 2026-10-09. The LLVM tools of the
host travel in `bin/` of every package beside antic and anti, and the installers
download none.

## What the step built

- `tools/pack-anti.cmake` copies `bin/` of the runtime archive, the eleven pinned tools
  and `llvm-version`, into `bin/` of the package, and writes `VERSION` into the root
  from `tools/version`. `llvm-pin` and `llvm-version` left the package's `tools/`.
- The runtime archive holds the tools of the machine alone, and a release packs six
  hosts on the Mac. For another host the packer reads `TOOLS/<host>/bin`, which
  `tools/get-llvm.cmake -DHOST=<host>` lays out from the pinned release as `bin/` of the
  runtime archive, checked against the pin, the manifest and the signature. Step 1 of
  `./r` fills `build/deps/llvm-tools/<host>` for the five other hosts, and step 3 passes
  the directory as `TOOLS`. The packer refuses a host without its directory, or whose
  `llvm-version` is not the pin, before it compiles anything. `fetch_release` takes the
  host as an optional argument and makes its destination absolute, since
  `file(ARCHIVE_EXTRACT)` unpacked nothing into a relative one.
- `tools/install.sh` and `tools/install.ps1` lost the download of the tools from
  `anti-lang/llvm-tools` and the copies of lld, 54 and 45 lines.
- antic and anti look the tools up by the one rule of `runtime_archive` in
  `src/antic/userdirs.c`: `bin/` of the archive above their own `bin/`, or of the data
  directory of the user. The rule is unchanged, and an installed antic finds the tools in
  `bin/` of the data directory, where the installer leaves the package's `bin/`.

## What it tested

Test first, each red on the old tools in `build/drive/logs/red-tests.log`.

- `package_keys` requires the eleven tools, `bin/llvm-version` and `VERSION` in the
  archive and refuses `tools/llvm-pin` and `tools/llvm-version`. From the unpacked
  package, with `PATH` holding its `bin/` alone, it runs `antic --version`,
  `anti --version`, a hello program and `anti build --release` of the project of
  `tests/anti-build/app`, with no `--runtime` and no `--llvm-mc`. It packs linux-arm64
  from a stand-in `TOOLS` directory and reads the stand-in `ld.lld` back, and checks the
  refusal without `TOOLS`. The test runs first in the suite with `COST 300`, since a cold
  tree ran it last and alone and the host suite took 540 s.
- `release_pins` refuses an installer that names `llvm-pin`, `llvm-version` or
  `llvm-sums`.
- `release_dry_run` points the cache of its stand-in at a deps directory of the test,
  with the real tools behind a link. It checks that step 1 fetches the tools of every
  host but this machine into `llvm-tools/<host>/bin`, and none for this machine.
- By hand, `build/drive/logs/get-llvm-*.log`: `tools/get-llvm.cmake -DHOST` fetched the
  tools of the five other hosts into `build/deps/llvm-tools/`. `file` reads
  `ld.lld` of linux-arm64 as ELF aarch64, `lld-link.exe` of windows-arm64 as PE32+
  Aarch64 and `ld64.lld` of macos-x86_64 as Mach-O x86_64. The packer then packed a real
  linux-arm64 package on the Mac, `build/drive/logs/pack-linux-arm64.log`, whose
  `bin/ld.lld` is ELF aarch64 and whose `tools/` holds neither pin.

## Sizes

| Measure | Value |
|---|---:|
| macos-arm64 package of `package_keys` before the tools | 20,533,160 bytes |
| The same with the tools | 125,271,420 bytes |
| linux-arm64 package packed on the Mac with the real tools | 120,095,104 bytes |
| Time of that pack, compile of antic and anti included | 2 min 35 s |
| xz of one copy of lld, 73 MB, single thread | 16.5 s |
| Tools archive of linux-arm64, downloaded | 53 MB |

The four names of lld are four copies of 73 MB, which xz compresses one by one. The
work order decided that with "copied as they are". The time of `package_keys` grew from 110 s
to about 200 s.

## Decisions

- `[provisional]` under "The release script" in `docs/decisions.md`: the tools of the
  other five hosts, `build/deps/llvm-tools/<host>`, the refusals of the packer and the
  content of `VERSION`.
- Decision 1 of the work order said `antic --version` prints the LLVM pin. The test
  `antic_version` and steps 3 and 10 of `./r` read the one line `antic <version>`, so the
  sentence now says a package names the pin of its tools. Step 10 in
  `docs/work-order-release-script.md` carries the same sentence, which the step
  `release-check` reads. It is not touched here, since that step owns steps 3, 5 and 10
  of that document.
- "Binary distribution" in `docs/decisions.md`, `docs/tooling.md`, `docs/distribution.md`
  under "The LLVM tools", `CLAUDE.md` and step 1 of `docs/work-order-release-script.md`
  say that the tools travel inside and where the packer takes them from.

## Gates

| Suite | Result | Time |
|---|---|---:|
| Mac host | 1692 of 1692, `sysroot_build_tools` and the two `mimalloc_environment` skipped | 540 s before `COST` and 366 s with it, `build/drive/logs/host-suite.log` and `host-suite-2.log` |
| Mac ASan | 1691 of 1691, the same three skipped | 507 s, `build/drive/logs/asan-suite.log` |
| Mac UBSan | 1691 of 1691, the same three skipped | 454 s, `build/drive/logs/ubsan-suite.log` |
| anti-linux | 1633 of 1633 in two parts of `ctest -I`, six skipped | 190 s and 167 s, `build/drive/logs/vm-linux-suite-*.log` |
| anti-windows | 1623 of 1623 in three parts of `ctest -I`, twelve skipped | 645 s, 273 s and 36 s, `build/drive/logs/vm-windows-suite-*.log` |

`package_keys` took 262 s on the Mac, 167 s on anti-linux and 272 s on anti-windows.
Each VM ran it first in its part, so the part's time is the test's own.

## The dry run

`./r --dry-run` did not run to its end here, for two reasons that stand in the script.
Its preflight refuses a main that origin does not hold, and this step is not pushed
before its gates pass. Its step 2 exports the commit and runs the three suites of the
Mac in one step of 40 minutes or more, where a step that stops is run again from its
start, and this session runs no command past ten minutes and none in the background.
What stands in for it: `release_dry_run` runs the script end to end over stand-ins,
with the fetch of step 1 and the `TOOLS` of step 3, and the real
`tools/get-llvm.cmake -DHOST` fetched the five tools archives that step 1 fetches, and
the real packer packed linux-arm64 from them. The run of `./r --dry-run` after the push
is the one gate of this step that is open.

## State

Before the commit of this report, which follows it. Neither is pushed, since the
dry run above is open.

```text
$ git log --oneline -3
23784665 Put the LLVM tools of the host into bin/ of every package
5d3d3c50 Add the work order of the binary distribution
feab21fb Record the decisions on the size of musl programs and on MI_PROFILE
$ git status --short
?? docs/reports/2026-10-09-dist-tools.md
$ git rev-parse HEAD origin/main
2378466588260281f7b1fb7b319cffcce3d04da8
5d3d3c5090b884547dda91d400f1d5c7b9c12872
```
