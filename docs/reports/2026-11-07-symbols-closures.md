# Symbols of a release program, calls of closures and the archive key

Eddie answered the four questions of
`docs/reports/2026-11-07-release-key-closures.md` with the recommendations
of that report. The session recorded the answers and built them in the
order Q2, Q3, Q1, Q4. Q2 found the defect and stopped at its fix, so Q3
waits as well. Q1 and Q4 are built.

## What changed

- `e43f36c5` records the answers as "Answers of 2026-11-07, second set" in
  `docs/decisions.md`, with the line in the entry of the feature freeze.
- `86659024` adds `symbols_moved_frame`, finding T211, listed as a known
  failure with `host=macos`.
- `edddf53f` builds Q1: the test of the context word at a call through a
  value of two words.
- `30dea7d5` builds Q4: the digest of each archive kept by its identity.

## Q2: a frame of a release program

`symbols_moved_frame` builds `tests/anti-symbols/moved` with `anti build
--release`, runs it and resolves its trace with `anti symbols resolve`.
The program's function `stop` returns `never`, so it stands among the cold
code after the runtime. Its class with an `inject` field exports every
function. The `-g` link then folds the empty functions of `anti.log` with
those of the runtime, and the code after them moves by 64 bytes. The frame
of the capture lies at 0x222f8 of the program, inside `stop` at 0x222e0.
The map of the archive holds `stop` at 0x222a0 to 0x222e4, so the frame
resolves to `anti.io.exit io.anti:36` and not to `stop moved.anti:20`. The
defect is real.

Linking the release object again with `-object_path_lto` gave every
function the address it has in the program. Linking the `-g` object with
`-S` gave every function the address it has in the debug link. The bitcode of `-g` drives the difference, not the strip flag.
On linux-arm64 and linux-x86_64 the two links lay `stop` out alike, and on
macos-x86_64 it moves too. The line in `tests/known-failures.txt` carries
`host=macos` for that reason. The Linux suites did not run, since the VMs
stayed untouched.

The fix is not built. It needs a strip step of the pinned LLVM tools, and
the release `23.1.1-anti.8` of `anti-lang/llvm-tools` holds none: its
`bin/` holds lld, llc, opt, llvm-ar, llvm-mc, llvm-objdump, llvm-readobj,
llvm-profdata and llvm-cov. No `llvm-objcopy`, `llvm-strip` or `dsymutil`
stands in the archive or in the pinned clang. Question 1 below asks for
the route.

The session measured `anti build --release` of a scratch copy of
`build/pythagoras-tree` before any change, with the Debug `anti`. The
median of 11 runs is 2.483 s for a first build, 2.493 s after a change of
one module and 0.192 s with nothing changed. No figure after exists, since the link did not change.

## Q3: the two release calls at once

Not built. The answer builds it when the test of Q2 passes. The test
failed, and the call that the fix would remove still runs. Question 2
asks whether the two calls run at once until the fix lands.

## Q1: the context word at a call

The lowering marks a call through a function with its context
`IR_CALL_CONTEXT` in the field of the instruction, which the library file
carries already. The LLVM text reads each float that needs the select,
compares the context with null once, takes the selects in the branch of a
null context and hands the call a phi of each. A call with no such float
compares nothing. `dump_llvm_closure_floats_macos-arm64` holds the form.
`program_abi_nan` passes a NaN to C through `apply(nan_keep, x)` with a
null context, through a pointer of one word that C filled and through a
closure that hands it on to C. It runs in release, dev and
`--memory-checks`, and at `v1` under Rosetta, and every line gives the
canonical NaN. In a loop that calls a closure through `apply`, the release
code of arm64 has one `fcmp` less than before.

The cost, as in `docs/notes/math.md`, is the built form over the same
program without the selects, the median of 21 runs:

| Program | arm64 | x86_64 at `v1` | Before, arm64 | Before, x86_64 |
|---|---:|---:|---:|---:|
| `float_work` | 1.01 | 1.00 | 1.01 | 1.00 |
| `pythagoras_geometry` | 1.10 | 1.23 | 1.10 | 1.22 |

Neither program calls through a value of two words, so neither figure
moved beyond the spread of the runs.

## Q4: the key of unchanged archives

A build with nothing changed, the median of 11 runs:

| `anti` | macos-arm64 | linux-arm64 |
|---|---:|---:|
| Debug, before | 0.178 s | 0.619 s |
| release, before | 0.049 s | 0.134 s |
| Debug, after | 0.057 s | 0.063 s |
| release, after | 0.027 s | 0.032 s |

The release `anti` left linux-arm64 0.054 s above the 0.08 s of the key
before the archives, at the edge of the answer's limit of about 0.05 s, so
the session built it. `archive-digests.txt` in the cache of the user keeps
the digest of each archive beside its size, its time of change in
nanoseconds, its device and its file. `platform_identity` of the layer of
`anti` reads them, the device and the inode on POSIX, the volume and the
file index on Windows. The Windows branch passed `-fsyntax-only` of the
pinned clang against the mingw sysroot and never ran.
`anti_reuse_<command>_<mode>_same_size` gives a member of the native
library other bytes of the same length, and the second run links again.
`anti_reuse_<command>_<mode>_unreadable` takes the read permission of the
library away after the first run. It failed before the change with
`cannot read`, and the second run now writes no file.

## Suites

The host suite ran in three parts with `-j14` before each code commit.
Before `86659024`: 377 s, 269 s and 319 s for `-I 1,2100`, `-I 2101,4300`
and `-I 4301,6943`, every test passing. Before `edddf53f` the first run,
381 s, 268 s and 309 s up to test 6944, failed `fmt_canonical` on the
comments of the two new programs. After `anti fmt` the whole suite ran
again, 378 s, 297 s and 316 s, and passed. Before `30dea7d5`: 374 s,
266 s and 334 s up to test 6952, every test passing. The two
`mimalloc_environment` tests skipped each time.

Each sanitizer suite ran its 6950 tests at `30dea7d5` in six parts,
`ctest -I 1,1150` to `-I 6001,6950`: `asan` 245 s, 187 s, 256 s, 193 s,
441 s and 443 s, `ubsan` 252 s, 186 s, 256 s, 206 s, 460 s and 468 s.
Every test passed, with the two skips of `mimalloc_environment`. The VMs
stayed untouched, so no suite ran on Linux or Windows.

## Provisional lines

- The digest of each archive of the key comes from `archive-digests.txt`
  in the cache of the user. The answer names no place.
- The entry on calls of the runtime, whose name starts with `anti_rt_`,
  stays provisional. The part on calls through an address became a rule
  of Q1 and lost the tag.

## Questions for Eddie

1. T211: which route takes the strip step? (a) A release of
   `anti-lang/llvm-tools` adds `llvm-objcopy`, under the name `llvm-strip`
   as well, and `dsymutil` for the dSYM of macOS. Both then travel in
   every package. (b) The `-g` link keeps its LTO object, as
   `-object_path_lto` does on macOS, and a second link of that object
   without LTO drops the debug sections with `-S` or `--strip-debug`. That
   link takes no LTO, but its code must be checked to equal the debug
   link. (c) Another route you name.
2. Q3: until T211 is fixed both release calls run. Should they run at
   once in the meantime, 1.03 s against 2.0 s with the same bytes?
3. Q1: C can make a context. The header writes a parameter of two words of
   an exported function as a callback and a `void *`. C may then pass its
   own callback with a context that is not null. A call through it then
   hands C the NaN of the processor. Does the rule stand, or should a
   parameter of an exported function keep the select?
