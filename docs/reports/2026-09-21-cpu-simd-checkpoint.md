# The CPU levels and the simd cap, a checkpoint

A check before the large additive features: the CPU levels and the vector byte
cap of the simd structs stand the same in `docs/decisions.md`,
`docs/anti-language-additions.md` and `docs/anti-syntax-overview.md`, and no
document claims the simd structs are built. No code changed. Commit `bc39c40`
changed the two documents that disagreed.

## What agreed

- The six levels, `v1`, `v2` and `v3` on x86_64 and `armv8.0`, `armv8.2` and
  `armv8.5` on ARM64, and the default of each target. The three documents,
  `tools/cpu-levels` and `CLAUDE.md` name the same ones.
- One runtime per target and level in `lib/<target>/<level>/`. The native
  libraries at the default level alone, the refusal at link and the message of
  the start-up check.
- The cap. `src/cpu.h` holds `CPU_VECTOR_BYTE_CAP = 256`, `tests/unit/test_cpu.c`
  checks the value, and nothing else reads it.
- The simd structs. The overview says "Not built yet." `src/` holds no parse of
  `simd`, and no other document, `README.md` or `CHANGELOG.md` names them as
  built. `simd` among the contextual words of the overview is the language's
  list and makes no claim of status.

## What disagreed, and the change

1. The additions called the cap "the widest vector register of any level antic
   knows". Eddie's answer in `5c01ad1` dropped the per-level widest register:
   the cap is one constant, 256 bytes, and caps the size of a `simd struct`.
   The line now says that.
2. The additions gave `armv8.2` the half-precision conversion. The `f16` entry
   of the decisions writes `fcvt` at every ARM64 level, since ARMv8.0 has it,
   and the additions' own `f16` line says one instruction on ARM64. `armv8.2`
   adds half-precision arithmetic, the `+fullfp16` of `tools/cpu-levels`, and
   the line now says that.
3. The additions named `v1` alone for the runtime conversion of `f16`. The
   decisions and the overview name `v1` and `v2`, and the line now does.
4. The Built line of the overview said the runtime archive was built for the
   default level of each target. It came from `707ad87`, before `18b6a1f` made
   a runtime per level, and now names one runtime per target and level.
5. The overview's simd section named the cap without its value. It now gives
   the size rule of the additions and the cap, 256 bytes, a constant of the
   level table.

## Gates

The docs-style checker reports nothing on both files,
`build/drive/logs/docs-style.log`. The commit changes `docs/` alone, so by
`CLAUDE.md` it needs no build and no suite. No test reads either document.

No `[provisional]` entry was added.

## Question

The overview gives the x86_64 baseline "for a release build", as the additions
do. The default of `tools/cpu-levels` is the same in dev mode, which
`docs/decisions.md` assumes for the suite's Rosetta programs. Should both say
the default applies to every build?
