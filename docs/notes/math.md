# anti.math

The design of the module. Eddie let it in on 2026-11-01, beside
`anti.graphics`, whose design is in `docs/notes/graphics.md`. He approved both
designs for building on 2026-11-06, and answered the two questions of the
draft. "Answers of 2026-11-06" in `docs/decisions.md` records them, and
"Standard interfaces" in `docs/anti-language-additions.md` holds the rules.

## Intent

A program computes a sine, a square root or a clamp with `import anti.math;`
and no `extern fn`. Today no module of the library has them, so the Pythagoras
tree declares `sinf` and `cosf` itself. raylib leaves the same functions to
the C library and adds a few helpers in raymath. `anti.math` gives both,
under the names a C or raylib programmer knows.

The module needs no window and no native library. It is used by
`anti.graphics` and by any program that computes.

## Contents

The functions of the C library:

| Group | Functions |
|---|---|
| Trigonometry | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` |
| Hyperbolic | `sinh`, `cosh`, `tanh` |
| Powers and logarithms | `sqrt`, `cbrt`, `pow`, `exp`, `exp2`, `log`, `log2`, `log10`, `hypot` |
| Rounding | `floor`, `ceil`, `round`, `trunc`, `fmod` |
| Parts | `abs`, `min`, `max`, `copysign`, `is_nan`, `is_inf` |

The scalar helpers of raymath, under its names without the prefix:

| Function | raymath | Gives |
|---|---|---|
| `clamp(x, lo, hi)` | `Clamp` | `x` limited to `lo..hi` |
| `lerp(a, b, t)` | `Lerp` | `a + (b - a) * t` |
| `normalize(x, lo, hi)` | `Normalize` | where `x` stands in `lo..hi`, as 0 to 1 |
| `remap(x, a0, a1, b0, b1)` | `Remap` | `x` moved from one range to another |
| `wrap(x, lo, hi)` | `Wrap` | `x` wrapped into `lo..hi` |

The constants are `PI`, `TAU`, `E`, `DEG2RAD` and `RAD2DEG`, as raylib's
`PI`, `DEG2RAD` and `RAD2DEG`, in `f32`. `PI_F64`, `TAU_F64`, `E_F64`,
`DEG2RAD_F64` and `RAD2DEG_F64` hold the same values with the digits of an
`f64`, by Q1 of "Answers of 2026-11-06, third set". Angles are radians, as
in C and raymath.

A domain error gives NaN, as C gives it. No function fails or sets `errno`.

## Rules the module keeps

- The functions are pure. They allocate nothing, take no lock and may run in a
  worker.
- Every function links and gives its result on all six targets. A test of
  each compares results with reference values, in release, dev and
  `--memory-checks`.

## One name for f32 and f64

Anti has no overloading by signature, so one name cannot take both types by
its parameter alone. Each function is one generic name,
`sin<T: Float>(x: T) -> T`, by Q1 of "Answers of 2026-11-06". The constraint
`Float` stands in `anti.lang`, and `f32` and `f64` meet it. `math.sin(a)`
takes the type of `a`, so a graphics program computes in `f32` and writes
`math.sin` as any other program does.

`sqrt`, `floor`, `ceil`, `trunc`, `round`, `abs`, `copysign`, `min` and `max`
are exact in IEEE. Each may become the LLVM intrinsic of its type, one
instruction where the target has it.

## The same result on every target

musl, glibc, Apple's libm and the UCRT of Windows give different last bits for
`sin`, `pow` and the rest. On 2026-11-01 and 2026-11-02 Eddie chose identical
results on every target for the native libraries and the runtime. By Q2 of
"Answers of 2026-11-06" the runtime carries one libm for all six targets, the
libm of musl, MIT, built with `ANTIC_FP_OPTIONS`. Every function of the
module gives its result on every target, and no target takes the libm of its
system.

## One NaN on every target

By Q2 of "Answers of 2026-11-06, third set" a program sees the same bits of a
NaN on all six targets. The canonical NaN below is the quiet NaN with a clear
sign and no payload: `0x7fc00000` in `f32` and `0x7ff8000000000000` in `f64`.
It is the default NaN of ARM64 and the NaN that LLVM folds a constant to.
This section is the design. Eddie chose option B by Q1 of "Answers of
2026-11-06, fourth set", and "The built form" below gives what is built.

### Where the bits differ

`tests/bench/ablate/nan_bits.anti` prints the bits of each case below. It ran
in release mode on macos-arm64 and on macos-x86_64 at `v1` under Rosetta,
which runs no `v3` code on this Mac. `p` is `0x7fc00001` and `n` is
`0xffc00002`, both read at run time.

| Case | arm64 | x86_64 |
|---|---|---|
| `0/0`, `inf - inf`, `0 * inf`, `sqrt(-1)` | `7fc00000` | `ffc00000` |
| `log(-1)`, `asin(2)`, `pow(-1, 0.5)`, `fmod(1, 0)`, `sin(inf)` | `7fc00000` | `ffc00000` |
| `p + n`, `n + p`, `p + 1`, `n * 1`, `sin(n)`, `floor(n)` | as the input | as the input |
| `min(p, n)`, `max(n, p)` | one input | one input |
| `-p`, `abs(n)`, `copysign` | the bits | the bits |
| `(0/0) as f64`, `(0/0) as f16` | positive | negative |
| `copysign(1, 0/0)` | `1` | `-1` |
| `(0/0) as i32` | `0` | `0` |

- The default NaN of an invalid operation is positive on ARM64 and negative
  on x86_64. That is the one difference a program meets without writing a
  NaN itself.
- musl makes the NaN of a domain error with an operation of the processor,
  so every function of `anti.math` gives that default NaN.
- A NaN operand gives its own bits, quieted, on both. With two NaN operands
  x86_64 takes the first source and ARM64 the first operand. LLVM commutes
  `fadd` and `fmul`, so which one wins depends on the instruction order,
  and nothing makes two targets agree. In the probe both took `p`.
- `min` and `max` give the operand that is a number. Two NaNs give one of
  them, through `fminnm` on ARM64 and a compare and a blend on x86_64.
- A conversion between `f32`, `f64` and `f16` keeps the sign and the top of
  the payload on both, so the sign of the default NaN survives it. The
  software conversions of `f16` at `v1` and `v2` follow the same rule.
- Negation, `abs` and `copysign` change the sign bit alone, which is the
  same on every target, so they show the sign of the default NaN.
- A float converted to an integer saturates, so a NaN gives 0 on both.
- The C of the runtime makes floats as well: the reader of `src/rt/text.c`,
  `deserialize` and the libm. clang compiles each with the default NaN of
  its target.

### Where a program sees the bits

- A bit view: a union of a float and an integer, or a pointer that reads
  the bytes of a float as another type. Bytes in memory count too, which a
  file, a socket or C reads.
- `x.hash()`, which keeps the bits of a NaN by the entry on hashing in
  `docs/decisions.md`. A NaN key of a hashing collection then lands in
  another bucket on another target.
- `copysign` of a NaN and every test of the sign bit through the bits.
- C: every float passed to C as an argument, in a struct or in memory, and
  every float an exported function returns. `printf` of C prints `-nan` on
  x86_64.
- An `f16` is bits in memory.
- `==` does not see them. It compares a float by value, the default `==` of
  a struct, a tuple, a variant and an array too, and a NaN equals nothing
  on every target. `serialize`, `f"..."` and `text.Builder` write every NaN
  as `nan`, so they do not see them either.

### Three options

A. The canonical NaN after every operation that can make a NaN, on every
target. antic writes `fcmp uno` and a `select` of the canonical NaN after
each `+`, `-`, `*`, `/` and `%` of a float, each conversion between two
float types, each call of a float function of the runtime or of C, and each
lane of a simd operation. The C of the runtime does the same at each float
it makes. A NaN is canonical in every register and every byte, apart from
one that a program writes as bits, which is the same on every target.
Negation, `abs` and `copysign` keep the bits they are given.

B. The canonical NaN where a program can see the bits, on every target.
antic writes the same `select` at a store of a float to memory that a view
or C can reach, which is every store but one to a local whose address is
never taken. It writes it at each float argument of a C function and
result of an exported one, at `hash`, at the sign operand of `copysign`,
at a write of a union and at a conversion to `f16`. A NaN in a register may
differ, and no operation of the language can tell.

C. The default NaN of the processor on ARM64 and option A on x86_64. The
bit DN of FPCR makes every operation of ARM64 give the canonical NaN, a NaN
operand included, at no cost per operation. The runtime sets it on every
thread it starts. x86_64 has no such mode, so its two targets take A.

### Cost

`tests/bench/ablate/run.py` built each option as a wrapper of the LLVM text
of the program: `opt_nan_ops.py` for A, `opt_nan_escape.py` for B and
`opt_nan_dn.py` for the ARM64 half of C. The wrappers edit the text of the
program alone, and the select of a call stands where the program calls the
runtime. Each column is the median of 21 runs to the baseline, in release
mode on an Apple M4 Pro. x86_64 ran at `v1` under Rosetta, whose times are
not those of an x86_64 processor. The VMs are down, so no real x86_64 ran.

| Program | A arm64 | A x86_64 | B arm64 | B x86_64 | DN arm64 |
|---|---:|---:|---:|---:|---:|
| `float_work`, particles and a series, 771 ms | 1.30 | 1.19 | 1.01 | 1.03 | 1.00 |
| `pythagoras_geometry`, the tree without a window, 162 ms | 2.21 | 3.48 | 1.10 | 1.22 | 1.00 |
| `pythagoras_frames`, the tree over the Recorder, 689 ms | 1.12 | none | 1.04 | none | 1.00 |

- `float_work` and both programs of the tree stand in `tests/bench/ablate/`.
  The two of the tree read the library files of the copy of pythagoras-tree
  in `build/pythagoras-tree` through `--option=-I`.
- `pythagoras_frames` links `anti.raylib`, which the runtime archive holds
  at `v3` alone, so it has no column at `v1`.
- C costs what DN costs on ARM64 and what A costs on x86_64.
- A costs most where a loop computes many floats per store, as `grow` of
  the tree does.

### Gaps

- A: a NaN that a program writes as bits, or that C hands in, keeps its
  bits until an operation. That is the same on every target, since it came
  as bits. The C of the runtime needs the select at every float it makes,
  by hand.
- B: the rule holds only while every place that reads bits stands on the
  list. A new feature that reads bits joins the list in the commit that
  builds it. The wrapper of the measurement left the union of
  `nan_bits.anti` out, since it keeps locals apart, and x86_64 still showed
  `ffc00000` there. The built form takes a union and a local whose address
  is taken as memory a view reaches. Memory that C writes keeps the NaN of
  C, which the program reads as bits that came in.
- C: LLVM drops an operation it can prove is the identity, as `n * 1`. ARM64
  then keeps the payload of `n`, `ffc00002` in the probe with DN, while A on
  x86_64 gives the canonical NaN of the select. A NaN written as bits
  therefore differs between the targets. A thread that C starts and that
  calls into Anti has no DN. The rule then needs a select after every
  operation on ARM64 as well, which is A.

### Recommendation

B. It gives the one NaN at every place a program can see it. On the float
code above it costs a tenth of what A costs. It adds a tenth to the
geometry of the tree on ARM64 and a fifth on x86_64, and 4 percent to the
frames of the tree. A gives the same guarantee at 2.2 to 3.5 times the time
of the geometry of the tree. C is not complete, as its gap shows. B needs
the list of the places that read bits to be held by a test, as
`diagnostics_covered` holds the messages.

### The built form

`src/antic/llvm_emit_nan.c` writes form B into the LLVM text. The entries of
"Standard interfaces" in `docs/decisions.md` give the rule. The gap of the
prototype is closed. A union, a local whose address is taken and any slot
read with a second type are memory a view reaches. A NaN that folding gives
is the canonical one in the text and in the bytes of a constant. `copysign`
of the runtime gives a NaN the canonical sign.

A call through a function value of two words tests its context word once, by
Q1 of "Answers of 2026-11-07, second set". The selects stand in the branch of
a null context, and a phi gives each float to the one call. A closure takes
the other branch, and opt folds the test where the context is the address of
a frame. In a loop that calls a closure through `apply`, the release code of
arm64 lost its `fcmp` there. Neither program of the table calls through a
value of two words, so the figures stand: measured again on 2026-11-07,
`float_work` gave 1.01 and 1.00 and `pythagoras_geometry` 1.10 and 1.23.

`tests/bench/ablate/opt_nan_off.py` takes every select of the canonical NaN
out of the text, so the measurement compares the built form with the same
program without it. Each figure is the median of 21 runs to that program, in
release mode on the Apple M4 Pro on 2026-11-07. x86_64 ran at `v1` under
Rosetta.

| Program | B arm64 | B x86_64 | Prototype arm64 | Prototype x86_64 |
|---|---:|---:|---:|---:|
| `float_work` | 1.01 | 1.00 | 1.01 | 1.03 |
| `pythagoras_geometry` | 1.10 | 1.22 | 1.10 | 1.22 |
| `pythagoras_frames` | 1.04 | none | 1.04 | none |

`pythagoras_frames` runs over the Recorder with `--option=--inject
--option=anti.graphics.Display=anti.graphics.Recorder.shared`, and without it
the program opens a window.

## How it is built

- `src/std/anti/math.anti` declares every function as `fn sin<T: Float>(x: T) -> T`. The body calls the `f32` function of the runtime where `size_of(T) == 4` and the `f64` one otherwise, and converts with `as`.
- `src/native/libm.cmake` compiles the generic C of `src/math/` of musl into anti_rt at every level of every target, in its objects and its bitcode. It takes the flags of the build of musl and `ANTIC_FP_OPTIONS`. The source is the release that `tools/sysroot-pins` names.
- `src/native/libm-names.h` gives every function and table of that libm the prefix `anti_rt_libm_`. LLVM knows no such name, so it folds no call with the libm of the host. No call binds to the libm of a system either.
- `src/rt/libm.c` holds the nine exact functions. Each writes the LLVM intrinsic, and the four roundings call musl at x86-64-v1, which has no instruction for them. Dev mode and `--lto none` keep each one a call of the runtime, by Q3 of "Answers of 2026-11-06, fourth set".
- `std_math` holds the reference values of every function in `f32` and `f64`, in release, dev and `--memory-checks`. `std_math_macos-x86_64` runs the same program at `v1` under Rosetta against the same file. `std_math_constant` compares a call on a constant with one on a value of run time. `math_link_<target>` links the program for every target and refuses a function of a libm of the system, and `libm_symbols` reads every symbol of the libm in the runtime.
