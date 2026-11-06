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

## How it is built

- `src/std/anti/math.anti` declares every function as `fn sin<T: Float>(x: T) -> T`. The body calls the `f32` function of the runtime where `size_of(T) == 4` and the `f64` one otherwise, and converts with `as`.
- `src/native/libm.cmake` compiles the generic C of `src/math/` of musl into anti_rt at every level of every target, in its objects and its bitcode. It takes the flags of the build of musl and `ANTIC_FP_OPTIONS`. The source is the release that `tools/sysroot-pins` names.
- `src/native/libm-names.h` gives every function and table of that libm the prefix `anti_rt_libm_`. LLVM knows no such name, so it folds no call with the libm of the host. No call binds to the libm of a system either.
- `src/rt/libm.c` holds the nine exact functions. Each writes the LLVM intrinsic, and the four roundings call musl at x86-64-v1, which has no instruction for them.
- `std_math` holds the reference values of every function in `f32` and `f64`, in release, dev and `--memory-checks`. `std_math_macos-x86_64` runs the same program at `v1` under Rosetta against the same file. `std_math_constant` compares a call on a constant with one on a value of run time. `math_link_<target>` links the program for every target and refuses a function of a libm of the system, and `libm_symbols` reads every symbol of the libm in the runtime.
