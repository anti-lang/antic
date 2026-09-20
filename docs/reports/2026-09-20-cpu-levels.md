# CPU levels

The "CPU levels" section of `docs/anti-language-additions.md` is implemented.
476 ctest tests pass on the Mac, and the ASan and the UBSan suites run 475 each.

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

The runtime archive is built for the default level of each target. That is what
makes an atomic operation one instruction: clang writes `casal`, `ldaddal` and
`swpal` for macos-arm64 and windows-arm64, and a loop of `ldaxr` and `stlxr`
for linux-arm64. `libs/CMakeLists.txt` reads the same table, so a third-party
library added there is built the same way.

antic writes the level of a program as the 32-bit global `anti_cpu_required`
beside the runtime entry, in the module that defines `main`. `rt/cpu.c` reads
the processor with CPUID on x86_64, `sysctlbyname` on macOS, `AT_HWCAP` on
Linux and `IsProcessorFeaturePresent` on Windows, and `rt/start.c` calls the
check before anything else. A machine below the level reads
`anti: this program needs a processor with AVX2 (x86-64-v3, 2013 or later)` and
the process exits with 70.

## Tests

`cpu_level_v1`, `cpu_level_v2` and `cpu_level_v3` check that the assembly holds
the SSE or the VEX forms and none of the other. `cpu_level_armv8.0`,
`cpu_level_armv8.2` and `cpu_level_armv8.5` read the runtime library of the
target that defaults to each and check the atomics. `cpu_check_refuses` builds a
macos-x86_64 program at the default level and runs it under Rosetta, which has
no AVX2. The refusal is then seen on a real machine below the level. The unit
test `test_cpu.c` compiles `rt/cpu.c` with `ANTI_DEV_CPU`, which lets
`ANTI_CPU_LEVEL` stand in for the processor. It checks the table, the names,
the ids and the refusal of each level.

The six-target emit expectations, the four `.alloc` dumps of x86_64, the Windows
unwind data, the emit-identity manifest and the link-identity digest were
rewritten. `tests/unit/test_float.c` and `tests/unit/test_x86_64.c` now name v1
where they hold SSE text, so those expectations say which level they are.

## Questions

1. **The runtime archive holds one level per target.** `--cpu v1` lowers the
   code antic writes and not the runtime the program links. A program built
   with `--cpu v1` therefore carries a v3 runtime and still does not start on a
   machine without AVX2. The section says `--cpu v1` and `--cpu v2` stay
   available "for a program that must run older hardware", which that does not
   deliver. Should the archive carry a second `anti_rt` per lower level, with
   antic linking the one that matches `--cpu`?

2. **Rosetta and the macos-x86_64 program tests.** A macos-arm64 host runs 85
   macos-x86_64 programs under Rosetta, which has no AVX2. It therefore runs no
   program of the default level. Rather than drop that coverage, the build
   writes a second runtime root at v1, `build/runtime-v1`, for the suite alone.
   Those tests compile with `--cpu v1` against it, and the archive that ships
   is unchanged. If question 1 is answered with a per-level runtime in the
   archive, this second root goes away.

3. **The simulated level.** The refusal is exercised on a real machine under
   Rosetta and, portably, in the unit tests, where `rt/cpu.c` is compiled with
   `ANTI_DEV_CPU` and reads `ANTI_CPU_LEVEL`. A released runtime carries neither.
   The alternative is to compile the switch in always, at one `getenv` per start.
   A user on a v3 machine could then see the message a v1 user would get.

4. **The message for ARM64.** The section gives the x86 line. The ARM64 lines
   are `the ARMv8.2 extensions (armv8.2, 2017 or later)` and the same for 8.5,
   and `ARMv8-A (armv8.0, 2012 or later)`.

5. **Windows on ARM64 has no query above armv8.2.** `IsProcessorFeaturePresent`
   answers for the dot products and for nothing higher. A program built with
   `--cpu armv8.5` for windows-arm64 is therefore not refused there. The check
   refuses only a machine that is known to be below the level.

6. **The vector byte cap.** The section calls it a constant in the level table
   and "the widest vector register of any level antic knows". That is 32 bytes
   at v3. "Simd structs" gives 256 bytes to start, which no vector register has.
   Both are in the table, as `CPU_VECTOR_REGISTER_BYTES` and
   `CPU_SIMD_STRUCT_CAP`, and nothing reads either yet.

7. **v2 changes no instruction antic writes.** The language has no operation
   that asks for SSE4.2 or `popcnt`. v1 and v2 therefore differ in the assembler
   and the runtime archive alone. The same holds for every ARM64 level in the
   emitter: the level's instructions there are the atomics of the runtime.

8. **`docs/tooling.md` was not changed.** `--cpu` is an antic option and the
   table in that file describes `anti build`, which is not built. If `anti build`
   should forward `--cpu`, that is a change to the tooling document as well.

9. **The docs-style checker and CMake.** `tests/CMakeLists.txt` had 105 findings
   before this session. Each is the checker reading a comment's last line as a
   heading, or a `foreach` line as prose. The nine that the new block adds are
   of those two kinds. The comments of the C sources and the `.md` files report
   nothing.
