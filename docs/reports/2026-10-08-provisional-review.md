# Eddie's review of the provisional entries of 2026-10-05 to 2026-10-08

Eddie reviewed the 42 `[provisional]` entries that the commits after `21014149` added
to `docs/decisions.md`. He kept 40 and changed entries 1 and 35. Four later entries,
the three on mimalloc of `8b4e226c` and the one on the alignment of ARM64 objects of
`23c5c8db`, came after the review and keep their tag.

| Commit | Change |
|---|---|
| `b6827bf7` | The tag comes off the 40 kept entries, and nothing else in them changes |
| `07ce2c66` | Safe folding of identical code on every target, entry 1 |
| `1400572b` | A build with a profile links through the default LTO, entry 35 |

## Entries kept

2 the relocation model `pic`, 3 `\` as a path separator on Windows, 4 the details of
`-> never`, 5 the six runtime functions that never return, 6 the cold arms of none
guards, 7 to 21 the never-return and memory classes of C functions, the table facts,
`dereferenceable`, the extension of results, `own`, the sizes of aggregates, `nuw`, the
range of enums, `inbounds` and the lifetimes, 22 the aliasing rules of views by `as`,
23 to 34 `!tbaa`, the runtime bitcode, the inline thresholds, `tune-cpu`, the LTO level,
the unwind tables, dropped allocations, outline atomics, one bitcode archive per mode,
the refusals of `--lto`, `main` under LTO and the name of the raw profile, and 36 to 42
the indexed profile, the profile runtimes, `/NODEFAULTLIB:libcmt.lib`, the frame
records of the Linux runtime, the copy into `dist/`, the ASan DLL and the bench
measurement in `tests/bench/ablate/`.

## Change 1: safe folding on every target

`--icf=safe` for ld.lld and ld64.lld and `/OPT:SAFEICF` for lld-link, each the
spelling the pinned lld accepts. llc writes `-addrsig` on every target, and the macOS
object runtime compiles with `-faddrsig`. The LTO of lld writes the table with no
option on ELF and COFF, and on Mach-O under `--icf=safe`, read from the objects
`--save-temps` kept. `link.exe` keeps `/OPT:NOICF`. Apple's `ld` keeps the table in
an output only without `-dead_strip`, which every link of antic passes.

Tests: `unit_link` takes the new options on 26 lines. `address_tables_<target>` reads
the table in the object of a program and in every object runtime, licence stub and
regex glue, and failed before on the program objects of five targets.
`distinct_addresses_lto_none_<target>` joins `distinct_addresses_<target>`. Both pass
on every target the three hosts run.

Size in bytes of the five programs of `docs/reports/2026-10-06-dead-code.md`, linked
by the antic of `b6827bf7` and of `07ce2c66`. Changes under 0.05 percent are left out.
hello, builder, objects and map_work changed by at most 128 bytes on every target.
macos-arm64 changed by at most 16, and windows-x86_64, which folded before, not at all.

| `mixed_work` | LTO before | LTO after | `--lto none` before | `--lto none` after |
|---|---:|---:|---:|---:|
| macos-x86_64 | 1,093,552 | 1,085,352 (-0.75%) | 1,067,872 | 1,063,760 (-0.39%) |
| linux-x86_64 | 1,533,160 | 1,526,888 (-0.41%) | 1,517,192 | 1,512,648 (-0.30%) |
| linux-arm64 | 1,523,488 | 1,520,672 (-0.18%) | 1,542,512 | 1,539,248 (-0.21%) |
| windows-arm64 | 893,952 | 888,320 (-0.63%) | 882,176 | 875,008 (-0.81%) |

Of the 120 programs, 116 ran, and each printed the output of its twin of before.
macOS ran on the Mac, macos-x86_64 under Rosetta at `v1`, Linux on anti-linux with
`qemu-x86_64` and Windows on anti-windows. mixed_work of macos-x86_64 did not run, since `v1` refuses
its regex library.

## Change 35: a profile with the default LTO

`lto_mode` no longer keeps the objects for a profile, and `profile_usable` no longer
refuses `--lto`. `run_lto` passes the options of the profile to `lto-pre-link<O3>` and
`thinlto-pre-link<O3>` as `run_opt` does to `default<O3>`. The LTO of lld takes no
profile of the instrumentation of IR: its options read a context-sensitive profile
and a sample profile alone, and the bitcode carries the counts. The other
refusals of entries 32, 35 and 36 stand, and the new entry names the two that changed.

Tests: `profile_guided` builds mixed_work in the default link and keeps the refusals.
`profile_lto_<mode>_<target>` builds `programs/loops.anti` with a profile of its own
run in the default link, under `--lto thin` and under `--lto none`, for the host and
its emulated target. It checks the object is bitcode, or is not under `none`, and that
`main` counts one call. `lto_default` expects bitcode for `--profile-generate`. All of
them failed before and pass on the Mac and both VMs.

`tests/bench/ablate/run.py`, macos-arm64, 15 runs, each program with a profile of its
own run against the same LTO build without one:

| Program | Without | With | Ratio | Executable without, with |
|---|---:|---:|---:|---:|
| builder | 58.6 ms | 57.5 ms | 0.98 | 106,736, 106,816 |
| map_work | 147.0 ms | 152.4 ms | 1.04 | 198,544, 182,432 |
| objects | 106.7 ms | 73.7 ms | 0.69 | 89,888, 89,856 |
| scalar_loop | 294.4 ms | 295.3 ms | 1.00 | 69,408, 69,408 |
| simd_loop | 125.7 ms | 125.1 ms | 1.00 | 69,408, 69,408 |
| mixed_work | 170.1 ms | 171.8 ms | 1.01 | 1,000,960, 1,004,480 |

A second run of map_work, 21 runs, gave 1.04 again.

## Re-pinned

`tests/link-identity/return42.macos-arm64.sha256`, by the safe folding and the macOS
runtime of `-faddrsig`: from `186b7ffa…711e` to `f67d3240…8034`. No other pin changed.

## Suites

| Host | Suite | Result | Time |
|---|---|---|---:|
| Mac, `1400572b` | host | 1687 of 1687, `sysroot_build_tools` skipped | 232 s |
| Mac, `6ac382f7` | asan, ubsan | 1686 of 1686 each | 460 s, 318 s |
| anti-linux, `3c047bd4` | host | 1619 of 1619, six skipped | 157 s |
| anti-windows, `1400572b` | host, five parts | 1607 of 1607, twelve skipped | 898 s |

No warning of our own code on any host. Two defects of the new tests came to light on
the VMs and were fixed in the unpushed commit, which became `3c047bd4` and then
`1400572b`. anti-linux ran the host program under qemu, since the list of targets of
`tests/CMakeLists.txt` gave the host an empty second element. anti-windows named the
object `generated.exe.exe.obj` in `tests/run_profile.cmake`, and its seven profile
tests passed once that suffix was fixed. Neither change touches the Mac, which ran its
host suite again at `1400572b` and the seven profile tests in both sanitizer trees.

## Note

The later provisional entry on ARM64 alignment lists "a profile" among the links of
objects. A profile now takes the LTO link unless `--lto none` is given. The entry
stays as written, since it is not part of this review.
