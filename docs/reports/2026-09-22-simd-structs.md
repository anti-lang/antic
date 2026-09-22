# Simd structs

"Simd structs" of `docs/anti-language-additions.md` is built: the `simd struct`
declaration, the element-wise operators, the masks of the comparisons,
`simd.select`, `simd.any` and `simd.all` of the new module `anti.simd`, the
built-ins `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max` and `dot`,
`as` to an array or a plain struct of the same bytes, the vector types of C in
the header, and the split of a wide value into the native width of the target
and the level. The development Mac passes 702 of 702 tests, and the ASan and
UBSan builds pass 701 each, without `no_paths`. The build has no warnings. The
VMs did not run.

## What was done

1. `e675ec2`. `simd` is a contextual word before `struct`. The checker takes one
   lane type of a fixed width, a power of two of lanes and a size of a multiple
   of eight bytes, and warns above the vector cap. It gives the element-wise
   operators, the mask of a comparison, which no program can spell, the three
   functions of `anti.simd`, the eight built-ins and `as` between a simd struct
   and an array or a plain struct of the same bytes. `select` may follow `.`.
2. `e80058b`. Six simd instructions of the IR work on values in memory, as
   `memcopy` does, and hold no width. The back end expands each one it has no
   instruction for into one scalar operation per lane. Lowering writes `f16`
   lanes, an integer division and a shift as scalar operations with their
   checks. Every operation on a simd struct above the cap is a loop. A sum
   and the least and greatest lanes fold the upper half onto the lower half.
   The library format is version 46.
3. `efaf249`. ARM64 selects every simd operation on registers of sixteen bytes,
   or of eight for a simd struct of eight. A simd struct of 16 bytes is the
   vector type of C. It passes in a vector register on ARM64 and in an xmm
   register under System V. Windows x64 takes a pointer to a copy and returns
   it in xmm0.
4. `145fb86`. x86_64 selects them with SSE2, with SSE4.1 of v2 for the wider
   lanes, and in the VEX forms on registers of 32 bytes at x86-64-v3.
5. `090a2d4`. The header writes the vector type of each architecture for an
   export simd struct of 16 bytes. One of another size becomes the struct of
   its lanes with its alignment.
6. `8c2c521`. The decisions gain "Simd structs", `docs/notes/simd.md` holds the
   choices of the passes, and the overview, the site page and CLAUDE.md follow.
7. `55cd910`. The program test gains the simd structs of 32 bytes of every lane
   width.
8. `a80d8e1`. x86_64 compares lanes with `<=` the way `pcmpgt` gives it.

## The tests

`program_simd` runs every operation and every lane type on ARM64 and, through
Rosetta, on x86_64 at v1, in release mode and in dev mode. Its line `halving`
shows the order of a float sum: in another order the lanes of that value give 1
or 0, not 2. `program_abi_simd` calls C functions of `tests/abi/simd.c` that
take and return `float32x4_t`, `int32x4_t` and `float64x2_t` by value. One of
them takes nine vectors, so that the ninth goes to the stack, and one calls an
Anti function back.
`clib_simd` calls an Anti library from C through the header, and `tests/dump/simdlib.h`
pins it. The simd structs of 32 bytes in the program cover the narrowing and the
widening of a mask at every lane width. They are two registers where the Mac
runs them and one at x86-64-v3, which llvm-mc assembles and no machine here
runs, since Rosetta has no AVX2. `simd_width` counts the instructions of one add of 32 bytes per target
and level: one `vaddps %ymm` at x86-64-v3, two `addps %xmm` at v1 and v2 and two
`fadd v` on ARM64. `program_simd_big` runs the loops of a 512-byte simd struct
under the warning of its declaration, `bounds_simd` covers the dev-mode check of
the last lane of a `load`, and `listing_error_simd_decl` and
`listing_error_simd_ops` pin the messages of the declaration and the operators.

## What failed and how it was fixed

- A review of the comparisons found that x86_64 swapped the operands of
  `a <= b` on integer lanes along with those of `a >= b`. `a <= b` is the
  negation of `a > b` and needs no swap. The program test now compares every
  lane kind with `<=`, `>=` and `!=`, and it printed `ile 0010` before the fix.
- The first x86_64 shuffle wrote `vpshufd $7007` for a simd struct of 32 bytes
  at v3. `pshufd` moves the 32-bit parts inside each half of a wide register,
  so a wide shuffle is no longer native and takes the expansion.
- The library format changed, so `antl_scale`, the unit test of the format and
  the manifest of `emit_identity` were written again. No other line of the
  manifest changed.
- The logs of the session are under `build/drive/logs/`: `build.log`,
  `test.log`, `asan-test.log` and `ubsan-test.log`.

## Questions for Eddie

- Every entry of "Simd structs" in `docs/decisions.md` is `[provisional]`. The
  ones with the widest reach are the mask without a spelling, the wrapping of
  integer lanes and the order of a fold. The fourth is the Windows x64 rule
  that a vector passes as a pointer to a copy. That is what MSVC does with
  `__m128`, and not what the section says.
- A shuffle is native in one register alone, and every other one is a move per
  lane. `tbl` on ARM64 and `vpermps` on x86_64 need a constant index vector,
  which antic has no data section for in a function. Should a later session add
  one?
