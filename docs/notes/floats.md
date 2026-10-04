# Floating point

Choices made for floating point. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Chapter 17 float comparisons follow IEEE 754: `fne` holds for NaN, and `feq`, `flt`, `fle`, `fgt` and `fge` fail. The LLVM text writes `fne` as `fcmp une`. The others take the ordered predicates, `fcmp oeq`, `olt`, `ole`, `ogt` and `oge`.
- An integer constant of a float type is the bits of the float in the LLVM text. The text writes it as the hexadecimal form of a `double`, which LLVM reads for both float types. The optimizer leaves such a zero in a copy of `f64`.
- The text carries no fast-math flag, so llc contracts no `a * b + c` into an FMA.
- A float out of the range of an integer type converts with `llvm.fptosi.sat` or `llvm.fptoui.sat`, which saturate and give 0 for NaN on every target.
- An `f16` in the IR is an `i16` of its bits, and only `hext` and `htrunc` read it as a number. The checker writes each read of an `f16` as a cast to `f32` marked `promoted`, so lowering converts a read in `lower_cast` with every other `as`. `check_storage` checks an expression without that cast, for the target of an assignment, the operand of `&` and the place of an atomic call.
- The LLVM text converts an `f16` with `bitcast` to `half` and `fpext` or `fptrunc` on ARM64 and at x86-64-v3, where llc writes `fcvt` or `vcvtph2ps` and `vcvtps2ph`. At `v1` and `v2` it calls `anti_rt_f16_to_f32` or `anti_rt_f32_to_f16` of the runtime, which take and give the half in the low 16 bits of an `i32`. No call reaches the conversions of compiler-rt.
- The optimizer folds `hext` and `htrunc` of a constant with the functions of `src/rt/f16.h`, as the checker folds a constant `as f16`. The two functions of the runtime call the same ones, so a folded conversion and one at run time agree.
