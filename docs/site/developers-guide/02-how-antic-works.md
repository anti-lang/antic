---
title: "How antic works"
description: "The passes of antic from source to executable, its IR and the LLVM module, dev and release builds, LTO, the runtime archive and how to read the excerpts."
summary: "The passes from source to executable, the IR of antic and the LLVM module, dev and release builds, whole-program compilation and the LTO of the runtime, the runtime archive and the allocator, what an executable holds beside the program, and how to read and make the excerpts of this guide."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T15:30:00+02:00
draft: true
weight: 20
tags: [compilers, programming-languages]
keywords: [compiler passes, intermediate representation, LLVM, link-time optimization, runtime archive, dump options, keep-llvm]
---

antic is a C11 program that turns one Anti source file into an executable in thirteen steps. It writes LLVM IR as text and runs `opt`, `llc` and `lld` of its own package on it. It links no LLVM library and calls no tool of the host. The later chapters show what a feature becomes in three of those steps: the IR of antic, the LLVM module and the machine code.

## Passes from source to executable

The function `compile` of `src/antic/driver.c` runs the front end, `back_end` and `llvm_back_end` run the passes over the IR, and `driver_run` runs the tools and the link. The table lists the steps in the order they run.

| Step | Reads | Writes | Source |
|---|---|---|---|
| Lexer | the source bytes | tokens | `lexer_lex` in `src/antic/lexer.c` |
| Parser | tokens | the syntax tree | `parser_parse` in `src/antic/parser.c` and the six `parser_*.c` files |
| Library loading | the `import` lines | the interface and the IR of each imported module | `driver_find_libraries` and `driver_load_libraries` in `src/antic/driver_search.c` |
| Checker | the tree and the interfaces | the checked tree, with a type on every expression | `sema_check` in `src/antic/sema.c` and the `sema_*.c` files |
| Lowering | the checked tree | the IR of antic | `lower_module` in `src/antic/lower.c` and the `lower_*.c` files |
| Whole-program pass | the IR of every module | direct calls, the class registry and the tables the runtime reads | `whole_program` in `src/antic/whole.c`, with `src/antic/whole_tables.c` |
| Dropped failures | the IR | the IR without the checks and the assertions the mode leaves out | `optimize_drop_failures` in `src/antic/optimize.c` |
| Optimizer of antic | the IR | a smaller IR, without the functions nothing reaches | `optimize_program` and `optimize_module` in `src/antic/optimize.c` |
| Layout | the IR and the target | the IR with every size and offset as a number | `layout_init`, `layout_data` and `layout_resolve` in `src/antic/layout.c` |
| LLVM text | the IR | one LLVM module as text | `llvm_emit_module` in `src/antic/llvm_emit.c`, with `src/antic/abi.c` and `src/antic/llvm_target.c` |
| `opt` | the LLVM text | bitcode after `default<O3>`, in release mode alone | `run_opt` and `run_lto` in `src/antic/llvm_run.c` |
| `llc` | the text or the bitcode | the object, or the assembly under `-S` | `llvm_run` in `src/antic/llvm_run.c` |
| Link | the object and the runtime archive | the executable | `driver_link_program` in `src/antic/driver_link.c` and `src/antic/linker.c` |

A build with `--memory-checks` adds one step after the layout, `memcheck_function` of `src/antic/memcheck.c`. Chapter 12 describes it.

The parser keeps the `tests` and `fixtures` blocks of a file under `--tests` alone. Every other build drops both right after parsing, in `drop_test_blocks` of `src/antic/driver.c`.

The checker makes one compiled copy of each generic for each set of type arguments the module uses. `sema_strip_generics` then removes the generics, so lowering and every later step see no type parameter.

A library file, `<module>.antl`, holds the public interface of a module and its IR. `antic -c` writes one after the checker and stops. The standard library stands in `std/` of the runtime archive as library files, one per module. A program that imports `anti.io` reads `std/anti/io.antl` and gets the IR of `anti.io` with it.

## Walk-through program

The program below goes through every step in the sections that follow. It prints one line when it runs with no argument, since the list of arguments then holds the name of the program alone.

```anti
//! name: walk
import anti.io;

fn area(w: int, h: int) -> int
{
	return w * h;
}

fn main(args: []str) -> int
{
	if area(args.len, 7) == 7 {
		io.println("one argument");
	}
	return 0;
}
```

```text output
one argument
```

### Tokens

`antic --dump-tokens walk.anti` prints one token per line and stops: the line and the column, the kind and the text. These are the first six tokens of line 4, the header of `area`.

```tokens excerpt=walk target=linux-x86_64 mode=release
4:1   keyword  fn
4:4   ident    area
4:8   symbol   (
4:9   ident    w
4:10  symbol   :
4:12  keyword  int
```

`int` is a keyword, as every built-in type name is. A `//` comment produces no token, and a `///` doc comment is one token of the kind `doc`.

### Syntax tree

`--dump-ast` prints the tree the parser built, one node per line, with the children of a node indented under it. This is `area`.

```ast excerpt=walk target=linux-x86_64 mode=release
function area
  param w
    type int
  param h
    type int
  result
    type int
  block
    return_stmt
      multiplicative *
        ident w
        ident h
```

The tree carries names and no meaning yet. `ident w` is a name, and nothing says that it is the parameter.

### Checked tree

`--dump-types` prints the same tree after the checker, with the type of every declaration and expression in a second column.

```types excerpt=walk target=linux-x86_64 mode=release
function area              fn(int, int) -> int
  param w                  int
    type int
  param h                  int
    type int
  result
    type int
  block
    return_stmt
      multiplicative *     int
        ident w            int
        ident h            int
```

Every expression now has exactly one type. Inference ends in the checker, and no later step chooses a type.

## The IR of antic

The IR is typed three-address code in basic blocks. Its scalar types are `i8`, `i16`, `i32`, `i64`, `f32`, `f64` and `ptr`. A `bool` is an `i8`, a `char` is an `i32`, and signedness stands in the operation, as in `sdiv` and `udiv`. The IR is not in SSA form: a temporary such as `%2` may be assigned more than once.

`--dump-ir` prints the IR after lowering. This is `area` and the start of `main`.

```ir excerpt=walk target=linux-x86_64 mode=release
fn walk.area(%0: i64, %1: i64) -> i64 {
b0:
    %2 = mulov nsw i64 %0, %1
    branchov %2, b1, b2
b1:
    %3 = addr @walk.0
    call void @anti_rt_check_failed(%3, 26, 1, %0, %1)
    unreachable
b2:
    ret i64 %2
}
fn walk.main(%0: agg []str) -> i64 {
b0:
    %5 = slot str
    %1 = ptradd inbounds %0, offset_of []str.len
    %2 = load i64 %1 !type i64
    %3 = call i64 @walk.area(%2, 7)
    %4 = eq i8 %3, 7
    branch %4, b1, b2
```

A function is named by its module and its name, `walk.area`, with no mangling. `int` is `i64` on all six targets.

`mulov` multiplies and records whether the signed product left the range of `i64`. `branchov` reads that record and goes to `b1` on overflow. Block `b1` is the failure arm of an overflow check. It calls `anti_rt_check_failed` of `src/rt/check.c` with a text, its length of 26 bytes, the kind of the check and the two operands. The text is the global `walk.0`, which names the file, the line and the operation.

Lowering writes that check in every mode, and the same holds for a bounds check, a division check and an assertion. The dev and the release IR after lowering are the same text. The step after the whole-program pass removes the failure arms the mode leaves out.

The parameter of `main` is `agg []str`, an aggregate that travels as a pointer in the IR. A `[]str` is the struct `{ ptr: ptr, len: i64 }`. `args.len` is a `ptradd` to the field and a `load`.

The offset of the field is the symbol `offset_of []str.len` and no number. The IR holds no size, no offset and no alignment. That keeps the IR of a module equal for every target, so one library file serves all six. `src/antic/layout.c` lays every aggregate out for the target. It replaces each symbol with its number right before the LLVM text is written.

`%5 = slot str` reserves memory in the frame for the `str` that `main` passes to `io.println`. The front end builds the `str` there with two stores, the pointer and the length 12.

`--dump-opt` prints the IR after the whole-program pass and the optimizer of antic. That optimizer folds constants, propagates copies, applies peephole rules and removes dead code. It then removes every function and global that `main` does not reach. For `walk` the dump holds four functions: `anti.io.print`, `anti.io.println`, `walk.area` and `walk.main`. `anti.io.eprint`, `anti.io.eprintln` and `anti.io.exit` are gone.

```opt excerpt=walk target=linux-x86_64 mode=release
fn walk.area(%0: i64, %1: i64) -> i64 {
b0:
    %2 = mulov nsw i64 %0, %1
    branchov %2, b1, b2
```

`--dump-opt` shows the checks in both modes, since it prints before the build drops them. The LLVM text is the first output where the two modes differ.

## The LLVM module

`llvm_emit_module` translates the IR into LLVM IR text, one module per run of antic. Each temporary of the IR becomes an `alloca` in the entry block. A definition stores into it and a use loads from it. The `sroa` and `mem2reg` passes of `opt` build the SSA form in release mode. Each block `b<n>` of the IR is the LLVM block of the same name.

This is `area` in dev mode, as `llc` receives it.

```llvm excerpt=walk target=linux-x86_64 mode=dev
define hidden noundef i64 @walk.area(i64 noundef %p0, i64 noundef %p1) #0 {
b0:
  %t0 = alloca i64, align 8
...
  %v0 = load i64, ptr %t0, align 8
  %v1 = load i64, ptr %t1, align 8
  %v2 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %v0, i64 %v1)
  %v3 = mul nsw i64 %v0, %v1
  %v4 = extractvalue { i64, i1 } %v2, 1
  store i64 %v3, ptr %t2, align 8
  br i1 %v4, label %b1, label %b2, !prof !3
...
  call void (ptr, i64, i32, i64, i64) @anti_rt_check_failed(ptr %v5, i64 26, i32 1, i64 %v6, i64 %v7)
  unreachable
...
b2:
  %v8 = load i64, ptr %t2, align 8
  ret i64 %v8
}
```

Line by line:

- `define hidden` makes `area` a global symbol that the link sees and a shared library does not export. A dev object holds one module, and another object may call the function.
- `noundef` on the result and the parameters tells LLVM that an `int` is never an undefined value.
- `%t0 = alloca` is the slot of a temporary. `%p0` and `%p1` are stored into `%t0` and `%t1`, and `%v0` and `%v1` are the loads.
- `llvm.smul.with.overflow.i64` is the `mulov` of the IR. It gives the product and one bit that is set on overflow.
- `mul nsw` computes the product again as the value of the expression. `nsw` holds because the branch leaves on overflow. `llc` merges the two into one multiplication.
- `extractvalue ... 1` takes the overflow bit, and the `br` goes to `b1` when it is set. `!prof !3` marks that side as cold, so `llc` places it after the return.
- `anti_rt_check_failed` never returns, so `unreachable` follows the call.
- `b2` loads the product and returns it.

In release mode the module holds the whole program, and `opt` has run over it. This is `main` as `llc` receives it. The module holds no `area`, no `anti.io.print` and no `anti.io.println`.

```llvm excerpt=walk target=linux-x86_64 mode=release
define internal noundef i64 @walk.main(i64 %p0.0, i64 %p0.1) #0 {
b0:
  %v8 = icmp eq i64 %p0.1, 1
  br i1 %v8, label %b1, label %b2
...
  tail call void @anti_rt_write(i32 1, ptr nonnull @walk.1, i64 12)
  tail call void @anti_rt_write(i32 1, ptr nonnull @anti.io.1, i64 1)
  br label %b2
...
  ret i64 0
}
```

Line by line:

- `define internal` keeps `main` out of the symbol table of the link. A whole program keeps every function internal, so `opt` may inline or remove each of them.
- `%p0.0` and `%p0.1` are the pointer and the length of `args`. The System V convention passes a struct of 16 bytes in two registers, and `abi_classify` of `src/antic/abi.c` splits the parameter.
- `icmp eq i64 %p0.1, 1` is all that is left of `area(args.len, 7) == 7`. The overflow check is dropped in release mode, so the product is a plain `mul` that wraps. `opt` inlined `area` and solved `len * 7 == 7` to `len == 1`.
- The two calls of `anti_rt_write` are `io.println`, inlined: the text of 12 bytes and the line feed of 1 byte, both to file descriptor 1. `anti_rt_write` is a function of `src/rt/io.c`.

The runtime enters the program through `anti.rt.main`, an alias of the `main` of the program. That `main` is `noinline` and stands in `llvm.compiler.used`, so it keeps its own frame and its name in a stack trace.

The processor and its features stand in the attributes of each function, as `"target-cpu"="x86-64-v3"` here. The command lines of `opt` and `llc` carry neither, so the text alone decides the code.

## Machine code

`llc` writes the object from the module. An `asm` excerpt of this guide is `llvm-objdump -d` of that object. This is `area` in dev mode for linux-x86_64.

```asm excerpt=walk target=linux-x86_64 mode=dev
<walk.area>:
pushq %rbp
movq %rsp, %rbp
subq $0x20, %rsp
movq %rdi, -0x18(%rbp)
movq %rsi, -0x10(%rbp)
imulq %rsi, %rdi
movq %rdi, -0x8(%rbp)
jo 0x24 <walk.area+0x24>
movq -0x8(%rbp), %rax
addq $0x20, %rsp
popq %rbp
retq
```

`pushq %rbp` and `movq %rsp, %rbp` build the frame record. On Linux and macOS every function that calls another keeps one, by the attribute `"frame-pointer"="non-leaf"`. `subq $0x20, %rsp` makes room for the four slots. The two `movq` lines after it store the parameters `w` and `h`, which arrive in `rdi` and `rsi`. `imulq` multiplies and sets the overflow flag. `jo` jumps to the failure arm at offset `0x24` when the flag is set. The other path loads the product into `rax` and returns. Dev mode runs `llc` at `-O1` with no `opt` before it. The slots of the temporaries stay in memory, and nothing is inlined.

The overflow check of a multiplication costs one `jo` on x86_64. On arm64 the same module gives four instructions, since ARM64 has no overflow flag for a multiplication.

```asm excerpt=walk target=linux-arm64 mode=dev
smulh x8, x0, x1
...
mul x9, x0, x1
cmp x8, x9, asr #63
...
b.ne 0x34 <walk.area+0x34>
```

`smulh` computes the upper 64 bits of the product and `mul` the lower 64. The product fits when the upper half equals the sign of the lower half, which `cmp x8, x9, asr #63` tests.

This is the whole `main` of the release build for linux-x86_64.

```asm excerpt=walk target=linux-x86_64 mode=release
<walk.main>:
cmpq $0x1, %rsi
jne 0x37 <walk.main+0x37>
pushq %rbp
movq %rsp, %rbp
leaq (%rip), %rsi
movl $0xc, %edx
movl $0x1, %edi
callq 0x20 <walk.main+0x20>
leaq (%rip), %rsi
movl $0x1, %edx
movl $0x1, %edi
callq 0x36 <walk.main+0x36>
popq %rbp
xorl %eax, %eax
retq
```

`cmpq $0x1, %rsi` compares the length of `args` with 1, and `jne` skips the printing. The frame is built on the printing path alone. Each call takes the descriptor 1 in `edi`, the address of the text in `rsi` and its length in `edx`. `llvm-objdump -d` prints no relocations. A `callq` therefore shows the address after itself, where the linker later writes `anti_rt_write`. `xorl %eax, %eax` is the `return 0`.

## Dev and release builds

`antic` builds in release mode unless `--dev` is given. `anti build` builds in dev mode unless `--release` is given.

| | Dev | Release |
|---|---|---|
| Unit of compilation | one module per object, linked as objects | the whole program in one LLVM module |
| Checks of dev mode | kept | dropped, and `--checks` keeps them |
| `assert` | kept | dropped, and `--asserts` keeps them |
| Tracing of a `trace` class | on | off, and `--trace` turns it on |
| Linkage of a function | `hidden` | `internal` |
| `opt` | not run | `default<O3>` or `lto-pre-link<O3>`, inline threshold 225 |
| `llc` | `-O1` | `-O2`, or none under LTO |
| LTO with the runtime | none | full, and `--lto thin` or `--lto none` choose another |
| Warnings | printed | refused as errors |

The checks of dev mode start with the overflow check of `+`, `-` and `*` and the bounds check of an index. A division, a shift and a narrowing conversion have one each, and later chapters name the rest. `--no-checks` drops them from a dev build and `--no-asserts` the assertions. The build that compiles the program decides, so a library file carries every check and a release build of the program drops the ones in its libraries too.

A release build defines what a dropped check guarded. A signed `+`, `-` or `*` wraps, a division by zero gives 0, and a shift takes its count modulo the width. Chapter 3 shows each.

`back_end` of `src/antic/driver.c` holds the rule for the checks and the assertions, and `lower_options` the rule for tracing.

## Whole-program compilation and LTO

A release build of antic compiles the whole program in one run. The program holds the IR of the module on the command line. It also holds the IR of every library file that module imports, directly or through another import. The whole-program pass and the optimizer of antic therefore see every function and every class.

The whole-program pass uses that view in these steps, which chapter 9 describes.

- A call through a class table becomes a direct call where one function alone can be its target.
- The class registry that `anti.reflect` reads is written.
- The singleton check runs, `check_singletons` of `src/antic/whole.c`.
- The bitmaps of the table slots the program uses are written.

 In dev mode the pass runs for the module that holds `main`, when that module links.

The link of a release build is a full LTO link. `opt` runs `lto-pre-link<O3>` and writes bitcode as the object. `lld` then links that bitcode with the runtime. The archive holds the runtime as bitcode in `lib/<target>/<level>/bitcode/full/`. LLVM then runs its passes over the program and the runtime as one module, at `--lto-O3`. A call of `anti_rt_write` may then be inlined like a call of a function of the program.

Dev mode, `--lib` and `--linker platform` link objects and refuse `--lto full` and `--lto thin`. A build with `--memory-checks` and a Windows program that loads plugins link objects unless `--lto` is given. A macOS build with `--memory-checks` refuses `--lto`. `--lto none` asks for the link of objects in release mode. That link runs `opt` with `default<O3>` into `<output>.bc` and `llc -O2` on the bitcode. `lto_mode` of `src/antic/driver.c` holds the rule.

Every link drops the functions and data that nothing reaches, with `--gc-sections`, `-dead_strip` or `/OPT:REF`. `llc` writes each function and each datum into a section of its own for that, on ELF and COFF.

## The runtime archive and the allocator

The runtime archive is the directory antic reads beside its own executable. A package is one runtime archive with `antic` and `anti` in its `bin/`. In the source tree the build writes the same layout to `build/host/runtime/`.

| Path | Contents |
|---|---|
| `bin/` | `opt`, `llc`, `lld`, `ld.lld`, `ld64.lld`, `lld-link`, `llvm-ar`, `llvm-mc`, `llvm-objdump`, `llvm-profdata`, `llvm-readobj` and `llvm-version` |
| `std/` | the standard library, one `.antl` per module |
| `lib/<target>/<level>/` | `libanti_rt.a`, the runtime as objects, and `bitcode/full/` and `bitcode/thin/` with the runtime as bitcode |
| `lib/<target>/` | the native libraries PCRE2, SQLite, Mbed TLS, miniaudio and raylib, the glue of `anti.regex` and the profile runtime of compiler-rt |
| `lib/<target>-glibc/` | the runtime of the two Linux targets built against glibc |
| `sysroot/<target>/` | the C library and the system libraries each link needs |
| `include/` | the headers of the native libraries |
| `licenses/` | the licence text of each component |

antic looks for the archive in two places, in this order: the directory above its own `bin/`, then the data directory of the user. `--runtime <dir>` names another. `runtime_archive` of `src/antic/userdirs.c` holds the rule.

`libanti_rt.a` is the runtime, written in C under `src/rt/`. It holds the entry point, the failure routines of the checks, the output functions, the thread pool and reflection. A program calls its functions as `anti_rt_*`. One runtime exists per target and processor level, so a program built with `--cpu v1` links a runtime compiled for `v1`.

A Linux program links statically against musl, and its allocator is mimalloc. `lib/<target>/<level>/libmimalloc.a` stands before `libc.a` on the link line. `malloc`, `free`, `calloc`, `realloc` and `aligned_alloc` of the program, the runtime and musl therefore resolve to mimalloc. `link_command` of `src/antic/linker.c` writes that line. A program that names a library with `link linux` links dynamically against glibc 2.35 and keeps the allocator of glibc.

A macOS program uses the allocator of `libSystem`, and a Windows program the one of `ucrtbase.dll`. The `alloc` and `free` of the language call `malloc` and `free` on every target. Chapter 12 describes `anti.mem`.

## Contents of an executable

An executable holds the code of the program, the parts of the runtime the program reaches and a licence notice. A Linux executable also holds the C library and the allocator. The table gives the sizes of `walk` for linux-x86_64 at the default level `v3`.

| Build | Command | File size in bytes |
|---|---|---|
| Release, full LTO | `antic --target linux-x86_64 -o walk walk.anti` | 227,768 |
| Release, objects | `antic --target linux-x86_64 --lto none -o walk walk.anti` | 257,368 |
| Dev | `antic --dev --target linux-x86_64 -o walk walk.anti io.o` | 258,488 |

The sizes are of antic 0.2.0 at commit `ca620e14`, read with `stat`. The dev build needs `io.o`, the dev object of `anti.io`, which `antic --dev --target linux-x86_64 -o io <archive>/std/anti/io.antl` writes.

`llvm-objdump -h` and `llvm-objdump -t` of the release file give its parts.

| Part | Bytes | Contents |
|---|---|---|
| `.text` | 150,923 | all code. `walk.main` is 66 bytes of it |
| `.rodata` | 20,148 | constants. `anti_licenses`, the notice, is 8,259 bytes of it |
| `.eh_frame` | 10,980 | unwind tables |
| `.data.rel.ro` and `.data` | 10,920 | initialised data |
| `.bss` | 59,576 | zeroed data, which takes no bytes of the file |
| `.symtab` and `.strtab` | 26,686 | the symbol table and its names |

The rest of `.text` is the runtime, musl and mimalloc. The start of a program runs in this order, in `main` of `src/rt/start.c`:

1. `anti_rt_streams_binary` sets the standard streams to bytes, which matters on Windows.
2. `anti_rt_cpu_check` compares the processor with the level the runtime was built for. It ends the program with a message on a processor below that level.
3. `anti_rt_init` prepares the runtime.
4. The arguments become a `[]str`. Every argument that starts with `--anti.` is an option of the runtime and is taken out of the list.
5. `anti_rt_conf_start` reads the runtime configuration.
6. The environment becomes a second `[]str`.
7. `anti.rt.main` runs, and its result is the exit status.

The notice names the licence of the runtime and of each library the program links. A program of musl carries the notices of musl and mimalloc. `anti license --from <binary>` prints it.

## Reading the excerpts

Every example of this guide is checked by the test `guide_examples`, which runs `tests/run_document_examples.cmake` over each page. The marks below tell the test what to check.

A block with a `main` is a program. The test builds it in release mode and runs it, and the `text output` block after it holds everything it prints. A `//! name:` line on top names the block for the excerpts below it.

```anti
//! name: hello
import anti.io;

fn twice(x: int) -> int
{
	return x * 2;
}

fn main() -> int
{
	io.println("hello");
	return twice(21) - 42;
}
```

```text output
hello
```

A block without a `main` goes through the front end alone. Its statements stand in the body of a function the test writes around them.

A block marked `refused` does not compile, and its `//! error:` line holds a text of the first error antic prints.

```anti refused
//! error: expected `int`, found `str`
let x: int = "a";
```

An excerpt names its block, a target and a mode. Each line of an excerpt stands in the output of antic for that block, in the order shown. A line of `...` stands for lines left out. Runs of blanks count as one blank, and the numbers of names such as `%12` and `.LBB0_3` are not compared.

| Fence | Output it quotes |
|---|---|
| `tokens`, `ast`, `types`, `ir`, `opt` | what `antic --dump-<name>` prints |
| `llvm` in dev mode | the LLVM text antic writes, which `llc` receives |
| `llvm` in release mode | the module after `opt` with `default<O3>`, which `llc` receives |
| `asm` | `llvm-objdump -d` of the object |

The release excerpts come from a build with `-S`, which links nothing. They show the module of the program before the LTO link, which may still inline the runtime into it.

In dev mode `twice` keeps its overflow check, and the constant 2 stands in the call of the intrinsic.

```llvm excerpt=hello target=linux-x86_64 mode=dev
define hidden noundef i64 @hello.twice(i64 noundef %p0) #0 {
...
  %v1 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %v0, i64 2)
  %v2 = mul nsw i64 %v0, 2
  %v3 = extractvalue { i64, i1 } %v1, 1
  store i64 %v2, ptr %t1, align 8
  br i1 %v3, label %b1, label %b2, !prof !3
...
  unreachable
...
  ret i64 %v6
}
```

In release mode `opt` has put `twice` into `main` and folded `twice(21) - 42`. `main` returns the constant 0, and the module holds no `twice`.

```llvm excerpt=hello target=linux-x86_64 mode=release
define internal noundef i64 @hello.main() #0 {
b0:
  tail call void @anti_rt_write(i32 1, ptr nonnull @hello.1, i64 5)
  tail call void @anti_rt_write(i32 1, ptr nonnull @anti.io.1, i64 1)
  ret i64 0
}
```

A function shows in a release excerpt only when `main` reaches it and `opt` has not inlined every call of it. An example of the later chapters marks a function `export fn` where a release excerpt must show it. An exported function is `dso_local` and never `internal`, so `opt` keeps it under its plain name.

```asm excerpt=hello target=linux-x86_64 mode=release
<hello.main>:
...
xorl %eax, %eax
popq %rbp
retq
```

## Making your own

Each dump option prints to the standard output and stops the compilation after its step. All of them take `--dev` and `--target <name>`.

| Option | Prints |
|---|---|
| `--dump-tokens` | the tokens, one per line |
| `--dump-ast` | the syntax tree |
| `--dump-types` | the checked tree with its types |
| `--dump-ir` | the IR after lowering, before the whole-program pass |
| `--dump-opt` | the IR after the whole-program pass and the optimizer of antic, with the checks still in |
| `--dump-llvm` | the LLVM text antic writes, before `opt` |

`--keep-llvm` keeps the files that antic otherwise deletes when `llc` ends. `<output>` is the path of `-o` as given.

| Build | Files kept |
|---|---|
| Dev | `<output>.ll`, the text that `llc` reads |
| Release with `-S` or `--lto none` | `<output>.ll`, the text before `opt`, and `<output>.bc`, the module after `opt` |
| Release with LTO | `<output>.ll` alone. The object `<output>.o` is the bitcode after `lto-pre-link<O3>` |

These commands reproduce the three kinds of excerpt for a file `walk.anti`. The tools `opt` and `llvm-objdump` stand in `bin/` of the runtime archive.

```sh
antic --dev --target linux-x86_64 -S --keep-llvm -o walk.s walk.anti
cat walk.s.ll

antic --target linux-x86_64 -S --keep-llvm -o walk.s walk.anti
opt -S -o - walk.s.bc

antic --target linux-x86_64 --lto none -o walk walk.anti
llvm-objdump -d walk.o
```

The first command writes the dev module as `walk.s.ll`. The second writes the release module as bitcode, which `opt -S` prints as text without running a pass. The third leaves the object `walk.o` beside the executable, and `llvm-objdump -d` disassembles it.

The test removes the `//!` lines of a block and writes `import anti.lang;` in front of a block that does not import it. A line number inside an excerpt counts the lines of that module.

Where it lives: `compile`, `back_end`, `llvm_back_end` and `driver_run` of `src/antic/driver.c` hold the order of the steps. `src/antic/ir_print.c` writes the IR dumps and `src/antic/llvm_emit.c` the LLVM text. `src/antic/llvm_run.c` runs the tools, and `src/rt/start.c` starts a program.
