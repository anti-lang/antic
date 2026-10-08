# The allocator of musl and the alignment of the object link

The two follow-ups of `2026-10-08-llvm-closed.md` that Eddie decided on 2026-10-08.

## 1. mimalloc on the Linux targets of musl (`8b4e226c`)

mimalloc 3.5.3 is a native library of the runtime archive: `tools/mimalloc-pin`,
`src/native/get-mimalloc.cmake`, `src/native/mimalloc.cmake`, its warnings in
`src/native/warnings.cmake` and its MIT text in `LICENSES/mimalloc.txt`. The build
compiles `src/static.c`, the one object of its own build for a static override, as
`lib/<target>/<level>/libmimalloc.a` for linux-x86_64 and linux-arm64. The musl link
names it after the runtime and before `libc.a`. The glibc mode and `--memory-checks` are
unchanged. `musl_allocator_<target>` reads the symbol table of a linked program in
release mode with each mode of `--lto` and in dev mode, and of a `--memory-checks`
program.

`tests/bench/ablate/run.py` on anti-linux at linux-arm64, 15 runs each, against musl's
allocator. Anti over C is against the C twin, which links glibc:

| Program | musl | mimalloc | Anti / C | Executable, musl | mimalloc |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 314.6 ms | 314.8 ms | 1.03, 1.03 | 83,576 B | 243,144 B |
| `objects` | 108.1 ms | 106.0 ms | 0.76, 0.74 | 121,816 B | 281,144 B |
| `builder` | 48.1 ms | 48.6 ms | 1.09, 1.09 | 125,344 B | 284,592 B |
| `simd_loop` | 128.4 ms | 130.0 ms | 1.00, 1.01 | 83,680 B | 243,272 B |
| `map_work` | 183.9 ms | 155.3 ms | 1.30, 1.10 | 284,944 B | 443,736 B |
| `mixed_work` | 199.5 ms | 174.5 ms | | 1,364,920 B | 1,523,488 B |

Two programs gain 15.6 and 12.5 percent, and four stay within 2 percent, so mimalloc wins
under the rule. Every executable grows by about 159 KB. On linux-x86_64 each of the six
grows by about 149 KB, and each prints under `qemu-x86_64` what it printed before.

The first run took mimalloc's defaults, and `builder` ran 114.2 ms against 48.0. The cause
is transparent huge pages: with `MIMALLOC_ALLOW_THP=0` it ran 50 ms. `MI_ALLOW_THP` is
therefore off, which costs `map_work` 157 against 150 ms and nothing elsewhere. This VM
was the only machine available, and real arm64 hardware may judge huge pages
differently.

## 2. Function alignment of the object link (`23c5c8db`)

llc writes every ARM64 object with `-align-all-functions=4`, and the object runtime of
each ARM64 target and level compiles with `-falign-functions=16`. The bitcode and the LTO
link are unchanged, and the executables of the default build are the same bytes.
`function_alignment_<target>` reads the code of a program's object and of every object
runtime. `--lto none`, 15 runs each:

| Program | Mac before | Mac after | anti-linux before | anti-linux after |
|---|---:|---:|---:|---:|
| `scalar_loop` | 290.5 ms | 290.5 ms | 307.2 ms | 306.6 ms |
| `objects` | 138.4 ms | 105.7 ms | 104.0 ms | 103.9 ms |
| `builder` | 58.1 ms | 58.3 ms | 48.5 ms | 47.6 ms |
| `simd_loop` | 125.0 ms | 124.6 ms | 127.1 ms | 127.0 ms |
| `map_work` | 146.7 ms | 146.1 ms | 151.2 ms | 150.6 ms |
| `mixed_work` | 169.3 ms | 169.5 ms | 174.1 ms | 174.8 ms |

`mixed_work` grows from 1,010,256 to 1,026,816 bytes on the Mac and from 1,535,856 to
1,542,512 on anti-linux. `objects` had no gap on anti-linux to close.

## Provisional entries

mimalloc at every processor level, its build with `MI_ALLOW_THP` off and its source record
as the pin. The alignment holds for every ARM64 object llc writes, dev mode included.

## Questions for Eddie

1. Executable size. mimalloc adds about 159 KB to every program of musl, so hello world
   triples. Its statistics are 5 KB of that. The rule let speed win.
2. Heap hardening. musl's allocator keeps its metadata out of band and checks it, and
   mimalloc's default build does not. `MI_SECURE=4` with checked frees cost `map_work` 1
   to 3 percent and `mixed_work` about 4 on anti-linux, and `builder` nothing. Should the
   musl targets take it?
3. Notices. mimalloc and musl are MIT and stand in every program of musl, but
   `anti_licenses` carries the texts of Anti packages alone.
   `docs/distribution.md` now lists the obligation.
4. mimalloc reads `MIMALLOC_*` variables at start, so a user can turn on its statistics or
   huge pages in any program of musl.

## Gates

At `60322802` the host suite passed 1669 of 1669 in 232 s, ASan 1668 of 1668 in 458 s and
UBSan 1668 of 1668 in 317 s, with `-j14` and no warnings from our code. The pinned clang
prints one `-Wattribute-alias` warning in mimalloc's `alloc-override.c`, which stays
unpatched under the rule for third-party sources. anti-linux passed 1601 of 1601 with six
skips, and anti-windows 1589 of 1589 with twelve skips, in four parts of `ctest -j4 -I`.

The first run of anti-windows failed `program_c_allocator`, since the Universal C Runtime
has no `aligned_alloc`. `60322802` moves the program to `tests/abi/`, and
`musl_allocator_<target>` runs it on a Linux host.
