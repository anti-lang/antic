# The remaining speed gaps

Item 5 of the session that closes the move to LLVM. Each gap was measured with
`tests/bench/ablate/run.py` and read in the optimized IR, as "How a fact is measured" of
`docs/work-order-llvm-optimization.md` asks. No change of the compiler or the library
came out of it, so no test was added. The measurements stand on `e6b44b0d`, where a
release build links through full LTO.

## `HashMap.set`

`set` is no longer a call. Under the default build the LTO inlines `set`, `put` and
`resize` of `SlotTable` into `main` of `map_work`, and the optimized `main` calls nothing
of the map. With `--lto none`, `-pass-remarks=inline` of the pinned opt on the text
gives, with the escapes of the names written out:

```text
'HashMap<int, int>.set' inlined into 'map_work.main' with (cost=350, threshold=525)
'SlotTable<(int, int)>.put' not inlined into 'HashMap<int, int>.set' because too costly to inline (cost=345, threshold=225)
```

The fatal path of 2026-10-05, `noreturn cold`, took the cost of `set` below its
threshold. `put` stays a call in the object link alone. On macos-arm64 `map_work` runs
at 1.07 of its C twin in both links, 148.4 and 150.3 against 138.6 ms.

## `objects`

The cause is the place of the four virtual functions in the object link. llc aligns a
small function at 4 bytes, so `Square.area`, `Square.grow`, `Rect.area` and `Rect.grow`
stand in 0x54 bytes from 0x800. The loop calls two of them through the table for each
shape, alternating between the classes. The LTO writes the same instructions, and clang
the same alignment, but both place the functions apart. Aligning every function at 16
bytes, `-align-all-functions=4` for llc and for the LTO of ld64.lld, on macos-arm64, 15
runs each:

| Program | LTO | LTO, aligned | `--lto none` | `--lto none`, aligned |
|---|---:|---:|---:|---:|
| `scalar_loop` | 291.1 ms | 292.6 ms | 291.2 ms | 294.6 ms |
| `objects` | 105.6 ms | 107.0 ms | 138.9 ms | 107.9 ms |
| `builder` | 59.3 ms | 60.4 ms | 59.9 ms | 60.4 ms |
| `simd_loop` | 127.1 ms | 126.9 ms | 127.2 ms | 127.1 ms |
| `map_work` | 148.4 ms | 149.4 ms | 150.3 ms | 150.3 ms |
| `mixed_work` | 172.8 ms | 172.2 ms | 174.2 ms | 173.7 ms |

The C twin of `objects` runs in 106.6 ms. In the default build, the LTO, the alignment
changes no program by more than 2 percent and grows `mixed_work` from 1,000,960 to
1,017,504 bytes. Under C1 of the work order the smaller build wins, so the compiler
stays as it is. The object link would gain 22 percent on `objects`, and that choice is a
question below.

## `map_work` on linux-arm64

On anti-linux, 21 runs each, against the C twin with glibc and the same C linked
statically against musl, as an Anti program is:

| Build | Median | To the twin with glibc |
|---|---:|---:|
| Anti, `armv8.0` | 177.8 ms | 1.29 |
| Anti with `"tune-cpu"="apple-m1"` | 178.2 ms | 1.30 |
| Anti, `armv8.2` | 178.8 ms | 1.30 |
| Anti, `armv8.5` | 178.3 ms | 1.30 |
| Anti, `armv8.5` and `apple-m1` | 178.2 ms | 1.30 |
| C twin, glibc | 137.3 ms | 1.00 |
| C twin, musl | 176.9 ms | 1.29 |

The level and the tuning each move `map_work` by 1 percent or less. The whole gap is the
C library. musl returns every large block to the system and maps a new one: the twin
with musl makes 261 calls of `mmap` and 260 of `munmap` and takes 82,239 minor page
faults, the Anti program 61,702, and the twin with glibc 12 calls of `mmap` and 2,245
faults. Against the twin with musl, Anti runs at 1.005. No hardware here runs
linux-arm64 natively, so the part of the VM stays unmeasured, and the default level
and the tuning stay as decided.

## Questions for Eddie

1. musl's allocator costs a Linux program of many large blocks about 30 percent of its
   time on `map_work`. The runtime could keep large blocks itself, or link another
   allocator for Linux. Either is a decision of the runtime.
2. Should the object link, `--lto none`, align every function at 16 bytes on ARM64?
   It gains 22 percent on `objects` and grows `mixed_work` by 1.6 percent.
3. The level `armv8.0` and the missing tuning of linux-arm64 need a measurement on
   hardware that runs linux-arm64 natively before any change, and none exists here.
