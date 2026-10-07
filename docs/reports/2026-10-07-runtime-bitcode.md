# The runtime as bitcode, by default

Item 3 of the session that closes the move to LLVM. A release build links the program
and the runtime as bitcode through full LTO by default (`03319e26`). `--lto none` links
both as objects, and `--lto thin` stays. Every package carries the bitcode of full LTO.
Four defects that the default brought to light are fixed in `e4894f0e`, `4f72f64b`,
`9d4205ec` and `aaad2bcd`.

## What changed

- antic resolves the mode in `driver_run`: full for a release build that links a program
  with lld, the objects for dev mode, `-S`, `-c`, `--lib`, `--linker platform`, a
  profile, `--memory-checks` and a Windows program that hosts plugins. `anti build` and
  `anti run` take `--lto full|thin|none` with `--release`.
- The LTO runs at `O3` with the inline threshold 225, the pipeline of release mode. At
  `O2` it ran `builder` in 1.28 and `map_work` in 1.20 of the time of `--lto none` on
  macos-arm64.
- The program's `main` is `noinline` and stands in `llvm.compiler.used`. lld inlined it
  into the runtime's C `main`, and the optimizer gave it the name of its alias
  `anti.rt.main`, so traces, maps and PDBs named no `main` of the program.
- The packer keeps `bitcode/full/` and leaves `bitcode/thin/` out. The bitcode adds 11.5
  MB to a package, 0.82 MB after xz. `package_keys` refuses a runtime without it.
- `link_identity_macos-arm64` moved from `5267da9c…` to `f67737f4…`, and the manifest of
  `emit_identity` and 24 golden LLVM texts took the new `main`.

## Defects found

1. The AddressSanitizer dylib of macOS ignored the options hook of an LTO program.
   ld64.lld marks no definition its LTO wrote as overriding the dylib's weak one, and a
   report ended with SIGABRT. `--memory-checks` keeps the objects and refuses `--lto` on
   macOS.
2. On ELF the runtime's unwind tables put the debug CFI of every Anti function into
   `.eh_frame` in the link with `-g`. The symbols archive then laid a Linux program out
   0x140 bytes later than the release link. `anti_symbols` failed on anti-linux. The
   bitcode of a Linux target now has no unwind tables. Test `symbols_layout_<target>`.
3. On windows-arm64 the inlined hooks call nothing. opt then removed the `malloc` of
   2^60 bytes in two traps that never use the object, as clang does in C. The traps hand
   the object to `printf`.
4. The clang driver of a Linux host added `+outline-atomics` to the bitcode of
   linux-arm64 and to the glibc objects, and the Mac did not. The runtime differed by
   host, and `builder` ran in 1.21 of the time of `--lto none` there. Both hosts now
   build with `-mno-outline-atomics`. Test `runtime_hosts`.
5. antic split a Windows source path at `/` alone, so `run.py` on anti-windows could not
   build. `\` separates there as well now. `run.py` also names the program `.exe`.

## Speed

`tests/bench/ablate/run.py <program> -- --lto=none`, 15 runs each, on idle machines.
Each cell is the median of full LTO over that of `--lto none`. macos-x86_64 ran under
Rosetta at `v2`, the level Rosetta runs.

| Program | macos-arm64 | macos-x86_64 | linux-arm64 | windows-arm64 |
|---|---:|---:|---:|---:|
| `scalar_loop` | 1.00 | 1.00 | 1.00 | 1.00 |
| `objects` | 0.73 | 0.97 | 0.99 | 0.76 |
| `builder` | 0.99 | 0.98 | 0.97 | 0.99 |
| `simd_loop` | 1.00 | 0.95 | 1.00 | 1.00 |
| `map_work` | 1.00 | 1.01 | 0.99 | 1.00 |
| `mixed_work` | 0.99 | none | 0.95 | 1.03 |

`mixed_work` needs `anti.regex`, whose library ships for `v3` alone, so it does not
build at `v2`. linux-x86_64 and windows-x86_64 run only under emulation here, and item 6
covers real hardware. On macos-arm64 `objects` now runs at 1.00 of its C twin, where the
object link runs at 1.36.

## Size

Executables in bytes, full LTO over `--lto none`:

| Program | macos-arm64 | macos-x86_64 | linux-arm64 | linux-x86_64 | windows-arm64 | windows-x86_64 |
|---|---:|---:|---:|---:|---:|---:|
| hello | 0.76 | 0.58 | 0.72 | 0.70 | 0.69 | 0.70 |
| `objects` | 0.80 | 0.79 | 0.78 | 0.79 | 0.80 | 0.81 |
| `builder` | 0.94 | 0.82 | 0.79 | 0.81 | 0.85 | 0.87 |
| `map_work` | 0.96 | 0.98 | 0.91 | 0.94 | 0.98 | 0.99 |
| `mixed_work` | 0.99 | 1.02 | 0.99 | 1.01 | 1.02 | 1.03 |

hello world of macos-arm64 is 69,504 bytes, against 69,552 in the table of Eddie's
decision. At `O3` `mixed_work` grows by up to 2.6 percent on x86_64 and windows-arm64.

## Remaining

`mixed_work` on windows-arm64 runs in 1.03 of the time of `--lto none`, 502.4 against
485.8 ms over 31 runs. Timed per part, `queues` took 1.17 and `paths` 1.31, `lists` 0.90.
`push`, `pop` and `fix` of `PriorityQueue<int>` have the same optimized IR in both
builds. Both builds inline every part into one `main` of about 15,000 lines, and under
LTO that `main` holds the hook dispatch of the runtime at 71 sites, and `paths` stays a
call. No change of the compiler or the library was found that removes the difference.

## Gates

At `4f72f64b`: host 1646 of 1646 in 282 s, ASan 1645 of 1645 in 455 s, UBSan 1645 of
1645 in 310 s, no warning. anti-linux 1354 of 1354 at `e4894f0e`. anti-windows 1331 of
1342 with the nine usual skips at `e4894f0e`, and the two traps passed after
`4f72f64b`. The fixes `9d4205ec` and `aaad2bcd` go out with item 4 and its gates.
