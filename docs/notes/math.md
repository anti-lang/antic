# anti.math

Draft of 2026-11-02 for Eddie. Nothing on this page is decided until
`docs/decisions.md` records it. Eddie let the module in on 2026-11-01, beside
`anti.graphics`, whose design is in `docs/notes/graphics.md`.

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
`PI`, `DEG2RAD` and `RAD2DEG`. Angles are radians, as in C and raymath.

A domain error gives NaN, as C gives it. No function fails or sets `errno`.

## Rules the module keeps

- The functions are pure. They allocate nothing, take no lock and may run in a
  worker.
- Every function links and gives its result on all six targets. A test of
  each compares results with reference values, in release, dev and
  `--memory-checks`.

## Open for Eddie

1. `f32` and `f64`. Anti has no overloading by signature, so one name cannot
   take both by its parameter alone.
   - A. One generic name per function, `sin<T: Float>(x: T) -> T`, compiled
     to the LLVM intrinsic of its type, `llvm.sin.f32` or `llvm.sin.f64`.
     `sqrt`, `floor`, `abs`, `min` and `max` then become one instruction. It
     needs a constraint `Float` in `anti.lang` and the compiler to know the
     functions. `math.sin(a)` takes the type of `a`.
   - B. The names of C, `sin` for `f64` and `sinf` for `f32`, as a plain Anti
     module over the C library. No change to the compiler, and the names a
     raylib program already writes.
   - Recommended: A, since a graphics program computes in `f32` and should
     write `math.sin` as any other program does.
2. The same result on every target. musl, glibc, Apple's libm and the UCRT
   of Windows give different last bits for `sin`, `pow` and the rest. A
   program then computes other values on each system. On 2026-11-01 and
   2026-11-02 Eddie chose identical results on every target for the native
   libraries and the runtime.
   - A. The runtime carries one implementation for all six targets, the libm
     of musl, MIT, which the Linux sysroots hold already, built with
     `ANTIC_FP_OPTIONS`.
   - B. Each target takes the libm of its system, which costs nothing and
     differs in the last bits.
   - Recommended: A, for the reason of those two answers.
