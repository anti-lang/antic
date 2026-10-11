---
title: "Targets"
description: "The six targets, --target and --cpu, the processor levels, cross-building, script mode, -g and debugging, the build id and what a release build does with LLVM."
summary: "The six targets, `--target` and `--cpu` with the processor levels, cross-building from any host, script mode, `-g` and debugging, the build id, and what a release build does with LLVM."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T20:23:44+02:00
draft: true
weight: 200
tags: [compilers, programming-languages]
keywords: [targets, cross compilation, processor levels, cpu, debugging, build id, LLVM optimization]
---

antic compiles for six targets, and every host builds for all six. A target is an operating system with a processor architecture. A processor level is a setting of the code generation inside one target, chosen with `--cpu`. Every command of this chapter was run on macos-arm64 with Anti 0.2.0, on an Apple M4 Pro. The page shows each output as the tool printed it.

## The six targets

`antic --print-targets` prints the targets with the object format, the calling convention and the triple of each.

```text
target          format  convention      triple
linux-x86_64    ELF     System V AMD64  x86_64-unknown-linux-gnu
linux-arm64     ELF     AAPCS64         aarch64-unknown-linux-gnu
macos-x86_64    Mach-O  System V AMD64  x86_64-apple-macos
macos-arm64     Mach-O  Apple ARM64     arm64-apple-macos
windows-x86_64  COFF    Windows x64     x86_64-pc-windows-msvc
windows-arm64   COFF    Windows ARM64   aarch64-pc-windows-msvc
```

The name of a target is also the name of its directories in the runtime archive, `lib/<target>/` and `sysroot/<target>/`. `antic --print-host-target` prints the target antic runs on.

| Target | C library of a program | Linker | Default level |
|---|---|---|---|
| linux-x86_64 | musl, linked statically, with mimalloc as the allocator | `ld.lld` | `v3` |
| linux-arm64 | musl, linked statically, with mimalloc as the allocator | `ld.lld` | `armv8.0` |
| macos-x86_64 | libSystem of macOS 11.0 or later | `ld64.lld` | `v3` |
| macos-arm64 | libSystem of macOS 11.0 or later | `ld64.lld` | `armv8.5` |
| windows-x86_64 | `ucrtbase.dll`, through the import libraries of mingw-w64 | `lld-link` | `v3` |
| windows-arm64 | `ucrtbase.dll`, through the import libraries of mingw-w64 | `lld-link` | `armv8.2` |

A Linux program that reaches a `link linux` line, or that loads a plugin, links dynamically against glibc 2.35 instead. [Chapter 17]({{% relref "17-c-interop" %}}) describes that mode. `int` is 64 bits on all six targets. `c_long` is 32 bits on the two Windows targets and 64 bits on the other four.

The program of this chapter has one function per difference between the levels.

```anti
//! name: levels
import anti.io;
import anti.text;

export class Meter
{
	pub atomic ticks: int = 0,
}

export fn blend(a: f64, b: f64, t: f64) -> f64
{
	return a + (b - a) * t;
}

export fn tick(m: *Meter) -> int
{
	return m.ticks.add(1);
}

export fn widen(samples: *f16, i: int) -> f32
{
	return samples[i];
}

fn main() -> int
{
	let m = Meter { ticks: 0 };
	tick(&m);
	tick(&m);
	let mid = blend(2.0, 4.0, 0.5);
	let line = f"ticks {m.ticks.load()}, mid {mid}";
	io.println(line);
	free(line.ptr);
	return 0;
}
```

```text output
ticks 2, mid 3
```

Where it lives: the table of the targets is `infos` of `src/antic/target.c`, and the triples and data layouts of the LLVM text are in `src/antic/llvm_target.c`. `link_command` of `src/antic/linker.c` writes the link of each target.

## --target and --cpu

`--target <name>` chooses the target and `--cpu <level>` the processor level. `antic`, `anti build` and `anti run` take `--cpu`, and `antic` and `anti build` take `--target`. Without `--target` the build is for the host, and without `--cpu` it takes the default level of the target.

```sh
antic --target linux-arm64 --cpu armv8.2 -o levels levels.anti
anti build --release --target windows-x86_64
anti build --release --target all
```

`anti build --target all` builds for every target that `[targets]` of the manifest lists under `all`, which is all six by default. Each target gets a directory of its own, `build/<target>/<mode>/` and `dist/<target>/<mode>/`.

A name that antic does not know is refused before it reads the source.

| Command line | Message |
|---|---|
| `--target amiga-68k` | `antic: unknown target amiga-68k` |
| `--target linux-x86_64 --cpu armv8.2` | `antic: armv8.2 is no processor level of linux-x86_64` |
| `--cpu v4` on macos-arm64 | `antic: v4 is no processor level of macos-arm64` |

The level reaches three places.

- The LLVM text. Every function carries the attributes `"target-cpu"` and `"target-features"` of the level, so llc chooses instructions the level has.
- The runtime. The runtime archive holds one `libanti_rt.a` per target and level, in `lib/<target>/<level>/`, with its bitcode for LTO beside it. A Linux target holds `libmimalloc.a` per level as well. The link takes the files of the level of the program.
- The check at start, which the next section describes.

The native libraries of the runtime archive are built once per target, at the default level. A program below that level that reaches one is refused at the link. A program that imports `anti.regex`, built with `--target linux-x86_64 --cpu v1`, stops with this line.

```text
antic: anti.regex is built for x86-64-v3, this program targets v1
```

Where it lives: `cpu_from_name` and `cpu_default` of `src/antic/cpu.c`, `driver_run` of `src/antic/driver.c` for the refusals, and `link_runtime_library` of `src/antic/driver_link.c` for the runtime of a level.

## The processor levels

A level names a set of instructions. x86_64 has `v1`, `v2` and `v3`, the levels x86-64, x86-64-v2 and x86-64-v3 of the System V ABI. ARM64 has `armv8.0`, `armv8.2` and `armv8.5`.

| Level | Instruction sets beyond the level below | What antic writes with them |
|---|---|---|
| `v1` | SSE and SSE2 | scalar and vector floats in SSE registers |
| `v2` | SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT, CMPXCHG16B | no instruction in the three functions below |
| `v3` | AVX, AVX2, BMI1, BMI2, FMA, F16C, LZCNT, MOVBE | the VEX forms of the float instructions, `f16` conversions in one instruction, a 256-bit simd struct in one register |
| `armv8.0` | NEON and floating point | an atomic operation as a loop of exclusive load and store |
| `armv8.2` | LSE, half-precision arithmetic, the dot products | an atomic operation in one instruction |
| `armv8.5` | the extensions of ARMv8.3 to ARMv8.5, pointer authentication and BTI among them | the code of `armv8.2` in the three functions below |

`tools/cpu-levels` holds the exact feature string of each level, and the test `cpu_levels_pin` compares it with the table of antic.

### x86_64

The release build at `v1` writes `blend` with the SSE forms and `widen` as a call of the runtime.

```asm excerpt=levels target=linux-x86_64 mode=release option=cpu-v1
<blend>:
subsd %xmm0, %xmm1
mulsd %xmm2, %xmm1
addsd %xmm1, %xmm0
retq
...
<widen>:
movzwl (%rdi,%rsi,2), %edi
jmp
```

- `subsd`, `mulsd` and `addsd` take two operands, and the second is also the destination.
- `widen` loads the 16 bits of the `f16` into `%edi` and jumps to `anti_rt_f16_to_f32`, a tail call. The disassembly shows the jump without the name of its target.

At `v3`, the default of both x86_64 targets, the same source gives the VEX forms and no call.

```asm excerpt=levels target=linux-x86_64 mode=release
<blend>:
vsubsd %xmm0, %xmm1, %xmm1
vmulsd %xmm2, %xmm1, %xmm1
vaddsd %xmm1, %xmm0, %xmm0
retq
...
<widen>:
vpinsrw $0x0, (%rdi,%rsi,2), %xmm0, %xmm0
vcvtph2ps %xmm0, %xmm0
retq
```

- A VEX instruction takes three operands, the last one the destination.
- `vcvtph2ps` is the F16C conversion from half to single precision. `vpinsrw` puts the 16 bits into the register first.
- `blend` is a multiply and an add at `v3` as well. The LLVM text carries no flag that lets LLVM contract the two, so the FMA of the level does not fuse them. The result is the same on every level.

`tick` is `lock xaddq` on all three levels. `v2` gives the code of `v1` for the three functions.

### ARM64

linux-arm64 defaults to `armv8.0`, where an atomic add is a loop.

```asm excerpt=levels target=linux-arm64 mode=release
<tick>:
add x8, x0, #0x8
ldaxr x0, [x8]
add x9, x0, #0x1
stlxr w10, x9, [x8]
cbnz w10,
ret
```

- `ldaxr` loads the counter and marks the address as exclusive.
- `stlxr` stores the sum when no other thread wrote the address in between, and sets `w10` to 0 then.
- `cbnz` goes back to the load when the store failed.

At `armv8.2` the loop is one instruction.

```asm excerpt=levels target=linux-arm64 mode=release option=cpu-armv8.2
<tick>:
mov w8, #0x1
add x9, x0, #0x8
ldaddal x8, x0, [x9]
ret
```

`ldaddal` adds `x8` to the word at `x9` and leaves the old value in `x0`. The target macos-arm64 writes the same instruction by default, since its level is `armv8.5`. `blend` is `fsub`, `fmul` and `fadd` on every ARM64 level, and `widen` is a load and one `fcvt`.

### The check at start

Every program checks the processor once at its start, in `main` of the runtime, before that function calls the `main` of the program. The check compares the level of the machine with the level of the runtime that was linked. A machine below that level ends the program with status 70 and one line.

macos-x86_64 defaults to `v3`, and Rosetta of macOS offers no AVX2. The default build of the program therefore stops under Rosetta, and the builds with `--cpu v1` and `--cpu v2` run.

```sh
antic --target macos-x86_64 -o levels levels.anti
./levels
```

```text
anti: this program needs a processor with AVX2 (x86-64-v3, 2013 or later)
```

| Level | Message of a machine below it |
|---|---|
| `v2` | `this program needs a processor with SSE4.2 (x86-64-v2, 2009 or later)` |
| `v3` | `this program needs a processor with AVX2 (x86-64-v3, 2013 or later)` |
| `armv8.2` | `this program needs an ARMv8.2 processor (Raspberry Pi 5, Apple Silicon, or later)` |
| `armv8.5` | `this program needs an Apple Silicon Mac` |

`v1` and `armv8.0` are the lowest level of their architecture and never refuse a machine. The check reads CPUID on x86_64 and asks the operating system on ARM64. Where a system has no query for a feature, the check assumes the level and lets the program start.

The check costs one call at start. A level changes no size of a value and no layout. `antic -c` writes the same bytes for the module of this chapter at `v1` and at `v3`, and for windows-arm64 as for linux-x86_64.

Where it lives: the table and the defaults are in `src/antic/cpu.c`, and `src/antic/llvm_target.c` holds the attributes of each level. `anti_rt_cpu_check` and the messages are in `src/rt/cpu.c`, compiled once per level, and `src/rt/start.c` calls the check.

## Cross-building

A package builds for all six targets with no network and no tool of the host. It holds the runtime of every target and level, the native libraries of every target and the sysroot of every target. The six builds below ran on the Mac, one after the other.

```sh
anti build --release --target all
```

| File under `dist/` | Bytes |
|---|---:|
| `linux-x86_64/release/levels` | 273200 |
| `linux-arm64/release/levels` | 293320 |
| `macos-x86_64/release/levels` | 74184 |
| `macos-arm64/release/levels` | 91744 |
| `windows-x86_64/release/levels.exe` | 60416 |
| `windows-arm64/release/levels.exe` | 55296 |

A Linux program is larger because it holds musl and mimalloc. Each directory also holds `NOTICE.txt` and `levels-symbols.zip`, the symbols archive of [chapter 18]({{% relref "18-the-tool" %}}).

| Target | Sysroot of the package |
|---|---|
| The two Linux targets | the headers and the static libraries of musl, and a second sysroot of glibc 2.35 with its X11 and OpenGL libraries |
| The two macOS targets | the headers of the C library, the text stub `libSystem.tbd` and the version of the SDK |
| The two Windows targets | the headers and the import libraries of mingw-w64 |

The frameworks of Apple are the one thing a package lacks. A program with a `link framework` line links on a Mac that has the SDK. On another host, `anti sdk import <bundle>` unpacks the stubs that `anti sdk export` packed on a Mac.

`anti run` builds for the host alone, so a cross-built program is copied to its target to run. `anti check --targets all` runs the front end once per target and reports an error that one target alone has.

Where it lives: `src/anti/build.c` loops over the targets, and `src/antic/driver_link.c` finds the runtime, the libraries and the sysroot of a target in the runtime archive.

## Script mode

The specification describes a script, and antic 0.2.0 builds none. `docs/anti-language-additions.md` describes a file whose first line is `#!/usr/bin/env anti`, which `anti file.anti` compiles into a cache and runs. The lexer refuses that first line. `anti` has no command that takes a source file as its first argument, and `anti hello.anti` prints the usage and exits with 2.

```anti refused
//! error: unexpected character `#`
#!/usr/bin/env anti
```

A single file runs today in two commands, with no project and no manifest.

```sh
antic -o hello hello.anti
./hello
```

## -g and debugging

`antic -g` writes the source position of every statement and one entry per function, and the link keeps the debug sections. `anti build` passes `-g` in dev mode and never in release mode. A release build of `anti build` writes the symbols archive instead, which [chapter 18]({{% relref "18-the-tool" %}}) describes.

The debug information holds lines and function names. It holds no variable, no parameter and no type.

```llvm excerpt=levels target=linux-x86_64 mode=dev option=debug
define dso_local noundef i64 @tick(ptr noundef nonnull dereferenceable(16) %p0) #0 !dbg
b0:
...
  %v3 = atomicrmw add ptr %v2, i64 1 seq_cst, align 8, !dbg
...
!llvm.dbg.cu =
...
distinct !DICompileUnit(language: DW_LANG_C11,
...
!DIFile(filename: "levels.anti", directory: "")
...
distinct !DISubprogram(name: "tick",
```

- `!dbg` on the `define` line names the `DISubprogram` of the function, and `!dbg` on an instruction names a `DILocation` with its line.
- The compile unit names the language as C11, since DWARF has no code for Anti. A debugger then reads the unit with its C rules.
- A `DIFile` holds the path of a source under its search root and an empty directory. No path of the machine that compiled reaches the program.
- The `DISubprogram` holds the name as the source writes it. A copy of a generic is `app.List<int>.push` there, although its symbol escapes the brackets.

llc turns these nodes into a DWARF line table on ELF and Mach-O and into CodeView on COFF. On Mach-O the debug information stays in the objects beside the program: `levels.o`, and under LTO `levels.lto/0.arm64.lto.o`. ELF holds it in the program. A Windows link writes a PDB in every build, with `-g` and without it.

lldb stops at a file and a line of a release build with `-g` and prints the frames by their Anti names. The session below sets a breakpoint on the `return` of `tick`.

```text
(lldb) breakpoint set -f levels.anti -l 16
Breakpoint 1: 2 locations.
(lldb) run
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x0000000100000730 levels`tick at levels.anti:16 [opt] [inlined]
(lldb) bt
  * frame #0: 0x0000000100000730 levels`tick at levels.anti:16 [opt] [inlined]
    frame #1: 0x0000000100000730 levels`levels.main at levels.anti:27 [opt]
    frame #2: 0x0000000100005f98 levels`main + 504
    frame #3: 0x000000018a9f84e4 dyld`start + 6992
(lldb) frame variable
error: no variable information is available in debug info for this compile unit
```

The file of the session is the block above without its first line, so `tick` returns at line 16. The breakpoint has two locations because `tick` stands in the program once as a function and once inlined into `main`. Frame 0 is the inlined copy, and frame 2 is `main` of the runtime, which has no line. `frame variable` has nothing to print.

`-g` changes no instruction of the program. `llvm-objdump -d` of the pinned tools prints the same 9,996 lines for the program of the session and for the build without the option. The debug sections make the Mach-O file 95632 bytes against 91312.

Where it lives: `src/antic/llvm_debug.c` writes the nodes, and `link_command` of `src/antic/linker.c` keeps or strips the sections: `-S` of `ld64.lld` and `--strip-debug` of `ld.lld` in a link without `-g`.

## The build id

The build id of a program or a shared library is a SHA-256, written as 64 lowercase hex digits. The link computes it from these inputs.

- The LLVM text of the module that links.
- Every object file the command line adds.
- The runtime library of the target and the level.

The digest leaves these out.

- The licence notice, since the notice holds the id.
- Every path, so that two links of one program in two directories carry one id.
- What `-g` added to the text: each `!dbg` and the metadata nodes of the debug information.

The id stands in the notice of the program, as the line `build <id>` after the begin marker `ANTI_LICENSES_BEGIN`. The program of this chapter was linked four times for macos-arm64, and all four links carry the same id.

| Link | Build id |
|---|---|
| `antic -o levels levels.anti` | `43fa9a0f682feb3d2ec2f67a1aa8421478b30b20d06ef40d814703166ab2f7ed` |
| the same command in another directory | the same |
| with `-g` | the same |
| with `--lto none` | the same |
| with `--target linux-x86_64` | `db301077f528bbef671b3ab65d9f7d2b005379de2a007d19b8f44117229eba65` |

A change of the source or of the runtime library gives another id, and so does a compiler that writes another LLVM text. The test `program_build_id` of the suite links one program twice, with `-g` and without it, and compares the ids.

The id ties a trace to the symbols of its build. A raw stack trace prints one `module` line per loaded binary with its id. `anti symbols` matches each id to a symbols archive, as [chapter 18]({{% relref "18-the-tool" %}}) shows. `-g` does not change the id, so a trace of a stripped release program resolves against the archive of the same build.

Where it lives: `build_id` and `identified_notice` of `src/antic/driver.c`, with the spans of the debug information from `src/antic/llvm_debug.c`. The runtime reads the id of a loaded module in `src/rt/platform_posix.c` and `src/rt/platform_windows.c`.

## LLVM in a release build

The code of such a build passes through `opt` and then through either `llc` or the LTO of `lld`, all three of the pinned tools. [Chapter 2]({{% relref "02-how-antic-works" %}}) describes the passes of antic that come before them.

| Build | `opt` | Code generation | Link |
|---|---|---|---|
| Dev | not run | `llc -O1` per module | objects |
| Release, `--lto full`, the default | `lto-pre-link<O3>` | the LTO of `lld` at `--lto-O3` | bitcode of the program and of the runtime, as one module |
| Release, `--lto thin` | `thinlto-pre-link<O3>` | the LTO of `lld` at `--lto-O3` | bitcode with summaries, the runtime from `bitcode/thin/` |
| Release, `--lto none` | `default<O3>` | `llc -O2` | objects |

Every release pipeline runs with `-inline-threshold=225`, in `opt` and in the LTO of `lld`. `lld-link` takes the level as `/opt:lldlto=3`. Under full LTO the runtime is part of the one module that LLVM compiles. A call of a runtime function is then inlined like a call of a function of the program.

A library of `--lib`, a link with `--linker platform`, a build with `--memory-checks` and a Windows program that loads plugins take no LTO. Each links objects as `--lto none` does.

The function attributes carry the level into LLVM. The module of the release build names the processor of `v3`.

```llvm excerpt=levels target=linux-x86_64 mode=release
"target-cpu"="x86-64-v3"
```

llc and the linker add these steps in both modes.

| Step | Option | Effect |
|---|---|---|
| A section per function and per datum, on ELF and COFF | `-function-sections`, `-data-sections` | the link drops what nothing reaches |
| Dropped code | `--gc-sections`, `-dead_strip`, `/OPT:REF` | no function or datum that the program never reaches is in it |
| Folding of identical code | `--icf=safe`, `/OPT:SAFEICF`, with the table `-addrsig` writes | two functions with the same code share one copy, unless the program takes the address of either |
| Alignment of functions on ARM64, in a link of objects | `-align-all-functions=4` | every function starts on a 16-byte boundary |

`--profile-generate` and `--profile-use` add the instrumentation of a profile to the `opt` step. [Chapter 21]({{% relref "21-performance" %}}) describes them with the other measurements.

Where it lives: `llvm_run`, `run_opt` and `run_lto` of `src/antic/llvm_run.c` start `opt` and `llc`, and `lto_mode` of `src/antic/driver.c` chooses the kind of link. `drop_unused` and `lto_level` of `src/antic/linker.c` write the options of the link.
