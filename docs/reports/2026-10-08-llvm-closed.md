# The move to LLVM, closed

The session of `build/prompt-close-llvm.md`, 2026-10-07 and 2026-10-08. Both work orders
of LLVM are marked complete, and each item has a report of its own.

## The items

1. Pin anti.7 (`86dd1264`). The twelve digests matched `SHA256SUMS` and its signature,
   both archives downloaded fresh, and the ThinLTO link of `mixed_work` for linux-arm64
   passed three times of three on anti-linux. `2026-10-07-pin-anti7.md`.
2. Programs in `dist/` replaced on every host (`43c8fde6`). `anti_build_running` failed
   on anti-linux with "cannot write" before. `2026-10-07-dist-replace.md`.
3. The runtime as bitcode by default (`03319e26`), with the LTO at `O3` and the
   threshold 225, `--lto none` for the object link, and the bitcode of full LTO in every
   package. Four defects it brought out are fixed: the runtime of Linux without unwind
   tables (`e4894f0e`), the traps whose allocation opt removed (`4f72f64b`), the outline
   atomics that a Linux host added (`9d4205ec`) and Windows source paths (`aaad2bcd`).
   `2026-10-07-runtime-bitcode.md`.
4. Inline atomics (`bfcd51b2`), item 17 of the first sessions.
   `2026-10-07-inline-atomics.md`.
5. The remaining speed gaps, measured and explained, with no change of code.
   `2026-10-07-speed-gaps.md`.
6. Real x86_64 hardware through two runs of the workflow, and seven fixes of the test scripts,
   the runtime and `anti build`. `2026-10-08-x86_64-hardware.md`.
7. `docs/notes/llvm.md` describes the back end as it stands, the work orders are marked
   complete, and "State" and item 17 of CLAUDE.md are current.

## `tests/bench`

`tests/run_bench.cmake` through `cmake --build build/host --target bench`, 5 runs each,
after item 4. A release build links through full LTO. Each cell is Anti over its C twin,
then over the same cell of `2026-10-05-llvm-opt-measure.md`:

| Program | macos-arm64 | 2026-10-05 | anti-linux | 2026-10-05 |
|---|---:|---:|---:|---:|
| `scalar_loop` | 1.00 | 1.00 | 1.03 | 1.03 |
| `objects` | 1.00 | 1.33 | 0.77 | 0.76 |
| `builder` | 1.30 | 5.38 | 1.13 | 5.73 |
| `simd_loop` | 1.00 | 1.01 | 1.00 | 1.00 |
| `map_work` | 1.07 | 1.08 | 1.29 | 1.31 |

`builder` gained the fast path of 2026-10-06. The release compile grew with the LTO, on
the Mac from 42 to 102 ms for `scalar_loop` and from 317 to 640 ms for `map_work`. The C
twin of anti-linux links glibc, and against the same C linked with musl `map_work` runs
at 1.005.

## The workflow

| Run | linux-arm64 | linux-x86_64 | windows-x86_64 | windows-arm64 | macOS |
|---|---|---|---|---|---|
| 37688511333 | 697 of 1330 | 697 of 1330 | configure | configure | configure |
| 37697898147 | 1366 of 1366 | 1364 of 1366 | 1355 of 1357 | sysroots | configure |

Every failure of the second run but the macOS jobs has a fix and a test since. The ASan
runtimes of x86_64 passed there, and their note is gone.

## Provisional entries of the session

The LTO at `O3` and 225, the cases that keep the objects, `main` kept under LTO, the
runtime of Linux without unwind tables, without outline atomics and with frame records,
the dropped allocations, the separators of Windows paths and the DLL of `--memory-checks`
in `dist/`.

## Questions for Eddie

1. musl's allocator maps and unmaps every large block, and costs `map_work` about 30
   percent on Linux. Should the runtime keep large blocks itself, or link another
   allocator for Linux?
2. Should `--lto none` align every function at 16 bytes on ARM64? It gains 22 percent
   on `objects` in that link and grows `mixed_work` by 1.6 percent. The default build
   gains nothing from it.
3. `armv8.0` and the missing tuning of linux-arm64 stay. They change `map_work` by 1
   percent or less on anti-linux, and no hardware here runs linux-arm64 natively. The
   runner `ubuntu-24.04-arm` does, for a measurement you may want.
4. The macOS jobs of the workflow need SDK 26.5, which no hosted image carries. Should
   they take another SDK, another runner, or leave the matrix?
5. `mixed_work` on windows-arm64 runs at 1.03 of `--lto none`, in a `main` of about
   15,000 lines in both builds, and no change was found.
6. `da668e4` of `../llvm-tools` is local. Its `main` holds 11 commits more that you have
   not pushed.

## Gates of the last push

At `4c3a68c1`: host 1663 of 1663 in 248 s, ASan 1662 of 1662 in 455 s, UBSan 1662 of
1662 in 311 s, with no warning. anti-linux 1370 of 1370 in 206 s with the build, and
anti-windows 1349 of 1358 with its nine usual skips in 989 s with the build, `-j4`. The
VMs print the warnings of raylib's own headers alone. A tree configured with CMake 3.31
passed 1660 of 1660 at `8784c4c5`.
