# CPU levels

The "CPU levels" section of `docs/anti-language-additions.md` is implemented.
477 ctest tests pass on the Mac, and the ASan and the UBSan suites run 476 each.
Eddie answered every question of this report during the session.
`docs/decisions.md` carries the answers, and the section carries the two that
change what the archive holds.

## What was built

`src/cpu.c` holds the level table: six levels, the feature each gives the back
end, the `-march=` value that builds a native library and the `-mattr=` value
that lets llvm-mc assemble the level's instructions. `rt/cpu_level.h` holds the
id of each level and is included from both sides, so antic and the runtime read
one definition. `tools/cpu-levels` holds the same table for CMake, which needs
the `-march=` of each target before antic exists, and `cpu_levels_pin` compares
`antic --print-cpu-levels` with that file.

`--cpu` sets the level on every target and takes the names of the target's
architecture alone. The defaults are x86-64-v3 on both x86_64 targets,
`armv8.5` on macos-arm64, `armv8.2` on windows-arm64 and `armv8.0` on
linux-arm64. The level is a code-generation setting: the six targets stay six,
and it reaches the emitter, llvm-mc and the build of the runtime archive.

At x86-64-v3 the emitter writes the VEX forms of the scalar float
instructions. A binary form names its destination twice, and a move between two
float registers becomes `vmovaps`. The v1 and the v3 assembly of
`tests/dump/floats.anti` map line for line. `mulss %xmm3, %xmm0` becomes
`vmulss %xmm3, %xmm0, %xmm0`, and every other float instruction follows.

The runtime archive holds one `anti_rt` per target and level, in
`lib/<target>/<level>/`, with the licence stub of a bundled archive beside it.
antic links the one that `--cpu` names, so `--cpu v1` gives a program that runs
on hardware without AVX2. The level is also what makes an atomic operation one
instruction: clang writes `casal`, `ldaddal` and `swpal` at `armv8.2` and above
and a loop of `ldaxr` and `stlxr` below. The native libraries stay at the
default level, in `lib/<target>/`. `libs/CMakeLists.txt` reads the same table.

The level of a program is the level of the runtime it linked. Every build of
`rt/cpu.c` names it with `ANTI_CPU_LEVEL_ID`, and `rt/cpu.c` refuses to compile
without it, so no build can ship a runtime that checks the wrong processor.
`rt/cpu.c` reads the machine with CPUID on x86_64, `sysctlbyname` on macOS,
`AT_HWCAP` on Linux and `IsProcessorFeaturePresent` on Windows, and
`rt/start.c` calls the check before anything else. A machine below the level
reads
`anti: this program needs a processor with AVX2 (x86-64-v3, 2013 or later)` and
the process exits with 70. An ARM64 message names machines rather than
extensions: `this program needs an ARMv8.2 processor (Raspberry Pi 5, Apple
Silicon, or later)` and `this program needs an Apple Silicon Mac`. The lowest
level of an architecture never refuses, because every machine of it is at least
that.

`driver_run` refuses a level of the other architecture. The zero value of
`enum cpu_level` is v1, so a caller that builds its own options can forget to
set it. `anti test` was that caller.

## Tests

`cpu_level_v1`, `cpu_level_v2` and `cpu_level_v3` check that the assembly holds
the SSE or the VEX forms and none of the other. `cpu_level_armv8.0`,
`cpu_level_armv8.2` and `cpu_level_armv8.5` read the runtime library of the
target that defaults to each and check the atomics. `cpu_archive_levels` checks that
the archive holds a runtime for every level of every target it has one for.
`cpu_check_refuses` builds a macos-x86_64 program at the default level and runs
it under Rosetta, which has no AVX2. The refusal is then seen on a real machine
below the level. The unit
test `test_cpu.c` compiles `rt/cpu.c` with `ANTI_DEV_CPU`, which lets
`ANTI_CPU_LEVEL` stand in for the processor. It checks the table, the names,
the ids and the refusal of each level.

The six-target emit expectations, the four `.alloc` dumps of x86_64, the Windows
unwind data, the emit-identity manifest and the link-identity digest were
rewritten. `tests/unit/test_float.c` and `tests/unit/test_x86_64.c` now name v1
where they hold SSE text, so those expectations say which level they are.

## Answers

Eddie settled every open item during the session. `docs/decisions.md` holds
each one with its reason.

1. **The simulated level stays a test hook.** `ANTI_DEV_CPU` is compiled into
   the unit tests alone and is absent from the runtime of the archive. A
   shipped program therefore reads no environment variable of its own.
   `docs/notes/hosts-and-harness.md` records it under "Test hooks".

2. **The ARM64 messages name machines.** `armv8.2` reads "this program needs an
   ARMv8.2 processor (Raspberry Pi 5, Apple Silicon, or later)" and `armv8.5`
   "this program needs an Apple Silicon Mac". `armv8.0` never refuses, as `v1`
   never does: a machine of an architecture is at least its baseline.

3. **Windows on ARM64 reads both flags.**
   `PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE` and
   `PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE`. A machine with both reports
   `armv8.5`, which has no flag of its own. Every Windows-on-ARM machine sold is
   armv8.2 or later, and `rt/cpu.c` says so at the query.

4. **The vector byte cap is one constant**, `CPU_VECTOR_BYTE_CAP`, 256 today.
   The per-level widest register is gone, because the cap is the size of a
   `simd struct` and not the width of a register. Nothing reads it until simd
   structs exist.

5. **v2 stays a level with no codegen difference from v1.** It exists so that a
   program can state it. It also gives `popcnt` and the other v2 instructions a
   level to appear at, when a built-in uses them.

6. **`anti build --cpu` is a rule in `docs/tooling.md`**, waiting for
   `anti build` as the `-g` rule does.

7. **The docs-style checker is not run on CMake files.** It covers prose and
   code comments, and a CMake comment follows the comment rules by hand.

8. **The eighteen runtime builds stay.** They run once per runtime archive
   build, a release-day job, and the archive caches them by target and level.

## Open

- The refusal of a native library below its level has no site yet, because
  nothing in `libs/` builds one. `docs/decisions.md` holds the message it will
  write.
