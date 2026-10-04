# LLVM back end for antic

Design and work order for compiling Anti programs through the LLVM optimizer
and code generator. The reader is the session that implements it. Every
question that came up while writing this has an answer below. Every
decision in it is taken. Four of them reverse or extend entries of
`docs/decisions.md`, and Eddie decided those on 2026-10-02. Each stands in
its own section. One back end is under "What stays and what goes". The
defined results are under "Defined results in release mode". The toolchain
release is under "Toolchain and packaging". Lines-only debug information is
under "Debug information".

Contents:

- [Why move](#why-move)
- [What stays and what goes](#what-stays-and-what-goes)
- [Pipeline](#pipeline)
- [Integration route](#integration-route)
- [Types and layout](#types-and-layout)
- [Calling convention](#calling-convention)
- [Instruction mapping](#instruction-mapping)
- [Defined results in release mode](#defined-results-in-release-mode)
- [Globals, sections and constructors](#globals-sections-and-constructors)
- [Attributes and metadata](#attributes-and-metadata)
- [Debug information](#debug-information)
- [Build id and identity tests](#build-id-and-identity-tests)
- [Memory checks](#memory-checks)
- [Targets, CPU levels and object formats](#targets-cpu-levels-and-object-formats)
- [Toolchain and packaging](#toolchain-and-packaging)
- [Dev mode and release mode](#dev-mode-and-release-mode)
- [What to expect from the generated code](#what-to-expect-from-the-generated-code)
- [Compile time](#compile-time)
- [Tests](#tests)
- [Documentation changes](#documentation-changes)
- [Questions an implementer asks](#questions-an-implementer-asks)
- [Work order](#work-order)
- [Later options, out of scope](#later-options-out-of-scope)

## Why move

antic has its own optimizer and two instruction-selection back ends. The
optimizer runs constant folding, copy propagation, peepholes, dead code
elimination, block merging and reachability. It has no SSA form, no
inlining, no common subexpression elimination, no loop transformations and
no alias analysis. `src/antic/optimize.c` is 1544 lines. The register
allocator in `src/antic/regalloc.c` is a linear scan with one interval per
virtual register and no splitting or coalescing beyond copies. Selection in
`src/antic/x86_64.c` and `src/antic/arm64.c` is one pattern per IR
instruction with addressing-mode folding.

That is enough for correct code and too little for fast code. A loop that
indexes a slice recomputes the base and the stride on every pass. A call of
a two-line method stays a call. A value loaded twice is loaded twice. C
compiled with clang at `-O2` runs the same loops with hoisted addresses,
inlined callees and one load.

Anti already depends on LLVM 23.1.1 for llvm-mc, lld, llvm-ar, llvm-objdump
and llvm-readobj, pinned in `tools/llvm-pin` and built in
`anti-lang/llvm-tools`. The optimizer and the code generator of the same
release come from the same source tree and the same build. Adding them adds
no new upstream, no new licence and no new trust anchor.

The move keeps the Anti IR and the `.antl` library format. It keeps the
front end, the lowering, the whole-program pass and the Anti optimizer. It
replaces the path from typed IR to object file. The native back end is
deleted at the end of the work order. Eddie decided this on 2026-10-02.
One back end means one semantics, one set of goldens and one place to fix.

## What stays and what goes

Stays unchanged:

- Lexer, parser, checker, lowering, `src/antic/whole.c`.
- `src/antic/ir.h`, `ir.c`, `ir_fold.c`, `ir_print.c`, `ir_verify.c`.
- `src/antic/optimize.c`. It runs before the translation, on every build.
- `src/antic/antl*.c` and `ANTL_VERSION`. The library file is untouched.
- `src/antic/layout.c`. It folds sizes and offsets per target before the
  translation, as it does before selection today.
- `src/antic/memcheck.c`. It inserts its calls before the translation.
- `src/antic/linker.c` and every lld call. llc writes the objects that lld
  links.
- The runtime in `src/rt/`, the native libraries and the standard library.

New:

- `src/antic/llvm_target.c`, `llvm_target.h`: triple, data layout string,
  CPU attributes and relocation model per target and CPU level.
- `src/antic/abi.c`, `abi.h`: target-independent classification of every
  parameter and result, ported from `locate` and `locate_result` of the two
  native targets.
- `src/antic/llvm_emit.c`, `llvm_emit.h`: the translation of an
  `ir_module` after layout into LLVM IR text.
- `src/antic/llvm_debug.c`: the `!DI*` metadata of `-g`.
- `src/antic/llvm_run.c`: running `opt` and `llc` on the text.
- `docs/notes/llvm.md`: the notes of the new back end.

Deleted in the step `switch`. Eddie decided on 2026-10-02 on one back end. The
native back end goes once the LLVM back end passes the suite on every
target. One implementation per IR operation, 12,000 lines less, one
semantics in dev and release mode. The alternative was to keep it as
`--backend native` for dev mode. That would have doubled the work of every
IR change and the test matrix. The files:

- `src/antic/select.c`, `expand.c`, `regalloc.c`, `x86_64.c`, `arm64.c`,
  `emit.c`, `debug.c`, `mach.c`, `coff.c` and their headers. These are the
  native back end, 12,000 lines together. From the step `emit-core` to the step `vm` the option
  `--backend native|llvm` selects one of the two, so the suite can run
  both and compare. The step `switch` removes the option and the files.
- `tests/unit/test_x86_64.c`, `test_arm64.c`, `test_emit.c` and the other
  unit tests of those files.
- The goldens `tests/dump/*.alloc` and `*.s`, with `--dump-select` and
  `--dump-alloc`. `--dump-llvm` and `*.ll` goldens replace them.
- The functions `anti_rt_f16_to_f32` and `anti_rt_f32_to_f16` of the
  runtime, which only the native back end called.

## Pipeline

Today `back_end` in `src/antic/driver.c` runs these steps after lowering:

1. `whole_checked` where the program is whole.
2. `ir_drop_failures` for asserts and checks that the mode drops.
3. `ir_optimize` or `ir_optimize_module`.
4. `memcheck_declare` under `--memory-checks`.
5. `select_module`. Inside it, `layouts_init`, `layout_resolve` and
   `layout_data` fold every symbolic size, then `expand.c` rewrites the
   operations a target has no instruction for, then selection.
6. `regalloc_function` per function.
7. `emit_program` or `emit_module`, then `emit_constructor`,
   `emit_licenses`, `emit_names`.
8. llvm-mc, then lld.

The LLVM back end replaces steps 5 to 8:

5. `layouts_init`, `layout_resolve` on every function and `layout_data` on
   the module. The translation never sees a symbolic size. `expand.c` does
   not run. Every operation it expands has an LLVM form, listed under
   "Instruction mapping".
6. `llvm_emit_module` writes one `.ll` text for the whole program, or for
   the one module in dev mode. The text goes to `<output>.ll` in the build
   directory. `--dump-llvm` prints it and exits with status 2, as
   `--dump-select` does.
7. `llvm_run` runs `opt` on the text and `llc` on the result. `-S` writes
   the assembly that `llc -filetype=asm` produces to the output file and
   stops. Without `-S` llc writes the object.
8. lld, unchanged. `--linker platform` is unchanged. The objects are
   standard ELF, Mach-O and COFF.

The translation runs after `memcheck_declare`, so the calls of
`__asan_loadN` and `__asan_storeN` are plain calls in the text.

`has_main`, the plugin checks, the hook checks and the export list of
`extras` run as today. They read the IR, not the machine code.

## Integration route

Three routes exist. The first is chosen.

Text and pinned binaries. antic writes LLVM IR as text and runs the `opt`
and `llc` programs of `anti-lang/llvm-tools`. antic stays a C11 program
with no LLVM header and no C++ in its build. A reader who builds antic from
source needs no LLVM libraries. The text is readable with `--dump-llvm` and
goes into test goldens. The text format of one LLVM release is stable, and
the release is pinned. Cost: two process starts per compilation and one
parse of the text. llc on a 10 MB text file parses it in under a second.

libLLVM linked into antic. Faster by the parse and the process starts.
Rejected. antic would link a C++ library of 100 MB. A build from source
would need the LLVM headers and CMake of the pinned release. The "from
scratch" rule of decision 18 would then cover the build as well as the
code.

Bitcode through lld's LTO. antic writes bitcode, and lld runs the LLVM
pipeline during the link with ThinLTO parallelism. Rejected for the first
version. `--lib static` must hand C users real objects. `-S` must write
assembly. Dev mode writes objects per module. Those three need llc anyway.
Listed under "Later options".

The two programs added to the toolchain are `opt` and `llc`. No `llvm-as`
is needed, because both read text.

opt runs as:

```sh
opt -passes='default<O2>' -o <output>.bc <output>.ll
```

llc runs as:

```sh
llc -O2 -filetype=obj -relocation-model=pic -o <output>.o <output>.bc
```

Dev mode runs llc alone, at `-O1`, see "Dev mode and release mode". The
`-S` flag replaces `-filetype=obj` with `-filetype=asm`. The CPU and the
features go as function attributes in the text, not as flags, so the text
alone decides the code. The relocation model is `pic` on Linux and macOS
and `static` on Windows, where lld-link builds position-independent
executables from the relocations alone.

antic finds both programs as it finds llvm-mc: `archive_tool` in
`src/antic/driver.c` looks in `<runtime>/bin/` and falls back to the
search path. `--opt <path>` and `--llc <path>` override, as `--llvm-mc`
does. `--llvm-mc` goes with the native back end in the step `switch`.

The intermediate files `<output>.ll` and `<output>.bc` are deleted after
the link unless `--keep-llvm` is given. `--dump-llvm` and `--keep-llvm`
are the two ways to read the text.

## Types and layout

LLVM types used by the translation:

| IR type | LLVM type |
|---|---|
| `i8` `i16` `i32` `i64` | `i8` `i16` `i32` `i64` |
| `f32` `f64` | `float` `double` |
| `ptr` | `ptr` |
| an `f16` value | `i16`, converted with `half` at `hext` and `htrunc` |
| a simd struct below the cap | `<N x T>` of its lanes |
| any other aggregate in memory | bytes behind a `ptr`, addressed with `getelementptr i8` |
| a `bool` in the IR | `i8` in memory, `i1` after `trunc` at each test |

The IR has no aggregate values. Every aggregate lives in memory, in a slot
or behind a pointer. The IR reads fields with offsets that
`layout_resolve` has folded to integers. So the translation needs no LLVM
struct types for code. Field access is `getelementptr i8, ptr %base, i64
<offset>` followed by a typed load or store. Opaque pointers make this the
form clang itself writes.

Struct types appear in three places only:

- Global initialisers, see "Globals, sections and constructors".
- `byval`, `sret` and coerced parameters, see "Calling convention".
- The `alloca` of a slot, which takes `[N x i8]` with `align A`.

The data layout string of each target is not written by hand. The step `targets` of the
work order runs the pinned clang once per triple:

```sh
clang -target x86_64-unknown-linux-gnu -S -emit-llvm -x c /dev/null -o -
```

The `target datalayout` line of that output goes into
`src/antic/llvm_target.c`. The test `llvm_datalayout_pin` repeats the
command with the pinned clang of `build/deps/clang` and compares. A mismatch
after a toolchain upgrade fails the build instead of miscompiling.

`layout.c` and the data layout must agree on size and alignment of every
type. They do: both follow the C rules of the target, and `tests/abi`
checks `layout.c` against C today. The test `llvm_layout_agree` adds a
direct check. It compiles a module with one struct per combination of field
types through `--dump-llvm`, then runs `opt -passes=print<...>`. The
simpler form: it compares `size_of` folded by `layout.c` against the
`llvm.objectsize`-free value clang gives for the same C struct, in the
existing `tests/abi` suite. One case per `tests/abi` struct is enough.

## Calling convention

LLVM IR does not implement the C calling convention for aggregates. clang
classifies every parameter and result and writes the result into the
signature. antic does the same. The classification exists today in
`locate` and `locate_result` of `src/antic/x86_64.c` and
`src/antic/arm64.c`, producing `struct arg_location` with register parts,
stack offsets, `indirect` and `copy`. `src/antic/abi.c` ports the rules,
not the registers, into this form:

```c
enum abi_kind {
    ABI_DIRECT,      /* a scalar in its own type */
    ABI_COERCE,      /* an aggregate as one or two integer or float words */
    ABI_BYVAL,       /* an aggregate copied to the stack by the caller */
    ABI_INDIRECT,    /* a pointer to a copy the caller makes */
    ABI_SRET,        /* a result written through a hidden first pointer */
    ABI_VECTOR       /* a simd struct of 16 bytes as its vector type */
};

struct abi_param {
    enum abi_kind kind;
    char types[2][16];      /* LLVM types of the coerced words */
    size_t word_count;
    uint64_t size;
    uint64_t align;
    bool sign_extend;       /* i8 and i16 parameters, signext or zeroext */
};

void abi_classify(enum target t, const struct ir_function *f,
                  struct abi_param *params, struct abi_param *result);
```

Rules per convention, as the two native files hold them today:

- SysV x86_64: an aggregate up to 16 bytes goes as one or two eightbytes.
  Each eightbyte is `i64` when it holds any integer, `double` when it holds
  one `f64`, `<2 x float>` when it holds two `f32`, and `float` when it
  holds one. Above 16 bytes, or with an unaligned field, it is `byval`.
  A result above 16 bytes is `sret`.
- AAPCS64 Linux: an aggregate up to 16 bytes goes as `[N x i64]`, a
  homogeneous float aggregate of up to four members goes as `[N x float]`
  or `[N x double]`. Above 16 bytes it is `ABI_INDIRECT`. A result above 16
  bytes is `sret`.
- Apple arm64: AAPCS64 with the Apple rules for variadic arguments and for
  arguments narrower than 8 bytes on the stack. LLVM applies the latter
  when the triple says `apple`. The translation marks `i8` and `i16`
  parameters `signext` or `zeroext` on every target, which both
  conventions accept.
- Windows x64: an aggregate of 1, 2, 4 or 8 bytes goes as an integer of
  that size. Any other size is `ABI_INDIRECT`. A result of another size is
  `sret`. A variadic float is also copied to the integer register, which
  LLVM does when the function is declared variadic.
- Windows arm64: AAPCS64 with the Windows rules for variadics. LLVM applies
  them from the triple.

A coerced aggregate is passed by loading the words from the slot and
rebuilding the slot in the callee with stores. A `byval` parameter is
declared `ptr byval([N x i8]) align A`. An `sret` result is `ptr
sret([N x i8]) align A`. An `ABI_INDIRECT` argument is a `ptr` to a copy
the caller writes into a fresh slot.

Every function uses the C convention of the target. No `fastcc`. Reason:
Anti functions are exported to C through `--lib`, called from plugins and
compared against C in `tests/abi`. One convention keeps one set of rules.
LLVM inlines across the C convention without loss.

Variadic C functions are declared with `...` and called with the promoted
types, as lowering already gives them.

Indirect calls carry the signature of the IR, so the `call` names the
function type. Table calls name the descriptor and the slot. The
translation loads the slot through `getelementptr` and calls the loaded
pointer. The whole-program pass has already devirtualised what it could.

## Instruction mapping

Every `enum ir_op` of `src/antic/ir.h` has one row. `T` is the LLVM type
of the instruction's `type`. Wrapping operations carry no `nsw` and no
`nuw`, because Anti arithmetic wraps and the flag would let LLVM assume it
does not.

| IR op | LLVM form |
|---|---|
| `ADD SUB MUL` | `add sub mul` |
| `SDIV UDIV SREM UREM` | `sdiv udiv srem urem` on a guarded divisor, see "Defined results in release mode" |
| `AND OR XOR` | `and or xor` |
| `SHL SHR_S SHR_U` | `shl ashr lshr` with the count masked to the width |
| `FADD FSUB FMUL FDIV` | `fadd fsub fmul fdiv`, no fast-math flags |
| `NEG NOT FNEG` | `sub 0, x`, `xor x, -1`, `fneg` |
| `COPY` | the operand itself, no instruction |
| `EQ NE SLT SLE SGT SGE ULT ULE UGT UGE` | `icmp` with the predicate, then `zext i1 to i8` |
| `FEQ FNE FLT FLE FGT FGE` | `fcmp oeq`, `fcmp une`, `fcmp olt ole ogt oge`, then `zext` |
| `ADD_OV SUB_OV MUL_OV` | `llvm.sadd.with.overflow.T` and the two others, result and `i1` flag |
| `BRANCH_OV` | `br` on the `i1` of the preceding `*.with.overflow` |
| `MULH_S MULH_U` | `sext` or `zext` both to twice the width, `mul`, `lshr`, `trunc`. At 64 bits this is `i128` arithmetic, which llc selects as `mul`/`imul` with the high half |
| `ADD_SAT_S ADD_SAT_U SUB_SAT_S SUB_SAT_U` | `llvm.sadd.sat.T llvm.uadd.sat.T llvm.ssub.sat.T llvm.usub.sat.T` |
| `MUL_SAT_S MUL_SAT_U` | `mul` in twice the width, then `llvm.smin`/`llvm.smax` or `llvm.umin` to the bounds, then `trunc` |
| `ADD_FL SUB_FL` | result as `add`/`sub`. Flags: overflow from `llvm.sadd.with.overflow`, carry from `llvm.uadd.with.overflow`, zero as `icmp eq 0`, sign as `icmp slt 0`. A carry in adds a second `with.overflow` on the carry and ors the flags |
| `MUL_FL` | result as `mul`. Overflow from `llvm.smul.with.overflow`, carry from `llvm.umul.with.overflow`, zero and sign as above |
| `SHL_FL SHR_S_FL SHR_U_FL` | the shift, with carry as the last bit shifted out and zero and sign as above |
| `NEG_FL` | `sub 0, x`, overflow as `icmp eq x, INT_MIN`, carry as `icmp ne x, 0`, zero and sign as above |
| `FLAG` | the `i1` computed for that bit, `zext` to `i8`. The translator keeps the four `i1` values of the preceding flag operation in a small table keyed by its result temporary |
| `TRUNC SEXT ZEXT` | `trunc sext zext` |
| `SITOF UITOF` | `sitofp uitofp` |
| `FTOSI FTOUI` | `llvm.fptosi.sat.T.F` and `llvm.fptoui.sat.T.F`, see "Defined results in release mode" |
| `FEXT FTRUNC` | `fpext fptrunc` |
| `HEXT` | `bitcast i16 to half`, `fpext half to float` |
| `HTRUNC` | `fptrunc float to half`, `bitcast half to i16` |
| `SLOT` | `alloca [N x i8], align A` in the entry block, with N and A from `layout_size` and `layout_align` of `of` |
| `LOAD STORE` | `load T, ptr %p, align A` and `store T %v, ptr %p, align A`, with A the natural alignment of T or 1 when the IR marks the access unaligned |
| `PTRADD` | `getelementptr i8, ptr %p, i64 %n` |
| `MEMCOPY` | `llvm.memcpy.p0.p0.i64` with the folded size and `i1 false` |
| `ADDR` | the global or function symbol as a `ptr` constant |
| `BITLOAD` | `load` of the unit type, `lshr`, `and` with the mask, `sext` or `zext` to T |
| `BITSTORE` | `load` of the unit, `and` with the inverse mask, `shl` the value, `or`, `store` |
| `VBINARY` | `load <N x T>`, the lane operation on vectors, `store`. A comparison gives `<N x i1>`, which `zext` to `<N x i8>` stores as the mask |
| `VUNARY` | `load`, `fneg` or `sub zeroinitializer, x` or `xor x, splat -1`, `store` |
| `VSPLAT` | `insertelement` into `poison`, `shufflevector` with a zero mask, `store` |
| `VSELECT` | `load` the mask as `<N x i8>`, `trunc` to `<N x i1>`, `select`, `store` |
| `VSHUFFLE` | `shufflevector` with the constant lane indices of the IR |
| `VREDUCE` | `llvm.vector.reduce.add`, `.fadd` with `-0.0` start and no `reassoc`, `.smin`, `.smax`, `.umin`, `.umax`, `.fmin`, `.fmax`. A dot product is `fmul` then `.fadd` |
| `CALL` | `call` with the classified signature, see "Calling convention" |
| `JUMP` | `br label` |
| `BRANCH` | `trunc i8 to i1`, `br i1` |
| `RET` | `ret T %v` or `ret void`, or a store through the `sret` pointer followed by `ret void` |

Simd operations above the cap and on `f16` lanes are already loops over
scalar operations after lowering. The translation sees them as scalars.

A simd struct of the IR with `simd` set and a size at or below the cap maps
to `<N x T>`. LLVM legalises a vector wider than the registers of the CPU
level into two or more registers. So the cap of `cpu.h` can stay or rise.
It stays in the first version.

The IR is not in SSA form. A temporary is written once per block in most
functions and more than once across blocks in loops. The translation gives
every IR temporary an `alloca` in the entry block, stores each definition
and loads each use. `opt` runs `sroa` and `mem2reg` first in every
pipeline, which turn these into SSA values with `phi` nodes. This is what
clang does for every C local, and it costs nothing after `opt`. Dev mode
without `opt` keeps the loads and stores, and `llc -O1` runs a fast
register allocator over them.

Every basic block of the IR becomes one LLVM block with the label
`b<index>`. A block whose `fail` is `IR_FAIL_ASSERT` or `IR_FAIL_CHECK`
gets branch weights that mark it cold:

```llvm
br i1 %c, label %b7, label %b8, !prof !0
!0 = !{!"branch_weights", i32 1, i32 2000}
```

The runtime function the failure arm calls gets `cold noreturn` where the
IR marks it as never returning.

## Defined results in release mode

Three operations have a result that today depends on the target in release
mode. `docs/anti-language-additions.md` line 148 says that division by zero is
checked in dev mode on every target. Release keeps the raw instruction,
where ARM64 returns zero and x86_64 traps. The same holds for a shift count
at or above the width and for a float conversion out of range.

LLVM gives these cases poison or immediate undefined behaviour, and its
optimizer deletes code that follows them. A raw `sdiv` by a constant zero
folds to poison, and the branch after it goes with it. So the translation
defines each result, and the definition is the ARM64 hardware result on
every target:

- `x / 0` and `x % 0` give 0 for signed and unsigned. `INT_MIN / -1` gives
  `INT_MIN`, and `INT_MIN % -1` gives 0. Emitted form:

```llvm
%zero = icmp eq i64 %y, 0
%minus1 = icmp eq i64 %y, -1
%safe = select i1 %zero, i64 1, i64 %y
%q0 = sdiv i64 %x, %safe
%neg = sub i64 0, %x
%q1 = select i1 %minus1, i64 %neg, i64 %q0
%q = select i1 %zero, i64 0, i64 %q1
```

  LLVM folds every select whose condition it can decide. So a division by
  a non-zero constant costs the same as in C. Unsigned division needs the
  `%zero` guard alone.

- A shift count is taken modulo the width: `shl i64 %x, (and i64 %n, 63)`.
  llc drops the `and` on x86_64 and ARM64, where the instruction masks.

- A float out of the range of the integer type saturates, and NaN gives 0.
  `llvm.fptosi.sat` and `llvm.fptoui.sat` define exactly that. ARM64 has
  the behaviour in `fcvtzs`. x86_64 gets a compare and a select around
  `cvttsd2si`.

Dev mode keeps the checks that lowering writes before each of these, which
report the file and the line. The defined result stands behind the check.

Eddie decided this on 2026-10-02. It changes the sentence at line 148 of
`docs/anti-language-additions.md` and the decision that records it. Both
say "the same result on every target" afterwards. The alternative, raw
instructions, needs inline assembly under LLVM, and inline assembly stops
the optimizer around every division. Without a definition, every division
in release mode would be undefined behaviour under LLVM. No language ships
that.

Signed overflow of `+ - *` wraps, as today. The translation writes plain
`add`, `sub` and `mul`, which wrap by definition in LLVM.

Float arithmetic carries no fast-math flag. No contraction of `a * b + c`
into an FMA happens, because llc contracts only under `contract` or
`fast`. This matches decision 202.

## Globals, sections and constructors

`layout_data` lays out every global of the module with the C rules.
`emit.c` writes bytes and relocations today. The translation writes each
global as a packed struct constant whose members and padding give the same
offsets:

```llvm
@anti.class.geometry.Point = internal constant <{ i64, ptr, [8 x i8], ptr }>
    <{ i64 2, ptr @geometry.Point.name, zeroinitializer, ptr @geometry.Point.area }>, align 8
```

A packed struct `<{ ... }>` has no implicit padding. Every gap is an
explicit `[N x i8]` member. So the offsets are the ones `layout_data`
computed, independent of the data layout string. A member that holds an
address is `ptr` to a global or a function. A member of bytes is
`[N x i8] c"..."`.

Linkage:

- A function or global that the IR marks exported is `dso_local` with
  default visibility on ELF and Mach-O, and carries `dllexport` on COFF.
  `emit_names` keeps writing the `.def` export list for lld-link, so the
  `dllexport` attribute is optional and is left out.
- Everything else is `internal`, which lets LLVM delete the unused and
  inline the rest.
- An `extern` function or global is a declaration with `external` linkage.
- A plugin's `provides` lines and the host exports stay as today, read from
  the IR before translation.

Sections:

- `emit_package` writes the package bytes into a section today. The
  translation writes `@anti.package` as a `private constant [N x i8]` with
  a `section` attribute. The section name per object format is the one
  `emit.c` uses.
- `emit_licenses` becomes `@anti.licenses` in the same way, with the build
  id substituted as today.
- `emit_constructor` for a shared library becomes an entry in
  `@llvm.global_ctors`, which llc turns into `.init_array`,
  `__mod_init_func` or `.CRT$XCU` per format.
- `patterns.start`, the constructor function of a module, goes into
  `@llvm.global_ctors` the same way. Its body translates as any function.

Thread-local storage and `anti_rt_*` symbols are declarations with the
names lowering gives them. Mach-O symbol names get their leading `_` from
llc, not from the translation. The translation writes the symbol name of
the IR. llc applies the mangling of the triple, as llvm-mc did for the
assembly.

## Attributes and metadata

Function attributes on every definition:

- `"target-cpu"` and `"target-features"` from the CPU level, see "Targets,
  CPU levels and object formats".
- `"frame-pointer"="non-leaf"` on every target but Windows, `"none"` on
  Windows. Reason: `anti_rt_trace_walk` follows the frame records, as
  `docs/notes/traces.md` says, and the ASan report of `--memory-checks`
  walks them with `fast_unwind_on_fatal=1`. Apple's ABI keeps the frame
  pointer as well. Windows walks the stack with `RtlVirtualUnwind` over the
  unwind data, so it needs no record. Eddie decided it on 2026-10-04, after
  the step `vm` found the Linux traces empty with `"none"`.
- `nounwind` on every Anti function. Anti has no exceptions and its
  failure channel is a return value. The runtime functions are C and get
  it too.
- `uwtable(sync)` on Windows, so lld-link writes the unwind tables the
  system needs to walk a stack through an Anti frame.
- `"stack-probe-size"="4096"` where `abi.probe_stack` holds today, which is
  Windows. llc emits `__chkstk` calls as the native back end does.
- `noreturn cold` on the failure functions of the runtime that lowering
  marks as never returning.

Parameter and return attributes:

- `nonnull` and `dereferenceable(N)` on a `ptr` parameter whose Anti type
  is a non-optional reference, with N the folded size of the referenced
  type. The checker guarantees both.
- `noalias` on a parameter of an `own` pointer. The owner is the only
  holder by the rules of the language. No other access to the object
  overlaps the call.
- `noalias` on the result of the allocation functions of the runtime,
  which return fresh memory.
- `signext` or `zeroext` on `i8` and `i16` parameters and results, from
  the signedness the signature records.
- `readonly` and `nocapture` on a pointer parameter that the function
  never writes through, where the IR records that fact. The IR does not
  record it today. The first version leaves the attribute out.

Module metadata:

- `!llvm.module.flags` with `"wchar_size"`, `"PIC Level"` 2 where the
  relocation model is `pic`, `"uwtable"` 2 on Windows, and the debug flags
  of `-g`.
- `!llvm.ident` with `antic <version>`.

No type-based alias analysis metadata in the first version. Reason: Anti
lets `as` view a struct as bytes and a simd struct as an array, and the
rules of what may alias what would have to be written and tested first.
Without TBAA, LLVM still disambiguates with its own analysis of
`getelementptr` offsets, `noalias` and `alloca` escapes, which covers the
loops that matter. TBAA is a later option.

## Debug information

`-g` writes line information today, with a `.loc` directive per statement.
The translation writes the same information as metadata:

- One `!DICompileUnit` per text with `language: DW_LANG_C11`, until DWARF
  gives Anti a code. `producer: "antic <version>"`.
- One `!DIFile` per source file of the program.
- One `!DISubprogram` per function, with `line` of its declaration and
  `unit` the compile unit. `type: !DISubroutineType(types: !{null})`, so
  no parameter types are described.
- A `!dbg !DILocation(line: L, column: 0, scope: <subprogram>)` on every
  instruction whose IR `line` is not 0. An instruction with line 0 carries
  no `!dbg` and inherits nothing. `opt` and llc handle instructions
  without a location.
- The module flags `"Debug Info Version"` 3 and, on COFF, `"CodeView"` 1.
  llc then writes DWARF on ELF and Mach-O and CodeView on COFF, as llvm-mc
  did from the directives.

Variables, types and parameters are not described, as today. Eddie
decided on 2026-10-02 that `-g` keeps this scope: lines only. Adding
variables means `llvm.dbg.declare` on the slot of each local with a
`!DIExpression()` and a `!DILocalVariable` with a type. That is a later
option.

Inlined code keeps the right lines, because LLVM carries the location of
every inlined instruction with `inlinedAt`. A backtrace of a release build
shows the line of the inlined callee. The native back end never inlined,
so it never produced such a line.

## Build id and identity tests

`build_id` in `src/antic/driver.c` digests the assembly text without the
ranges `-g` added, recorded as `debug_spans`. Under LLVM it digests the
`.ll` text in the same way. `llvm_emit_module` records the spans of what
`-g` adds: every ` , !dbg !N` suffix and every metadata line that starts
with `!` and describes debug information. The digest skips them. A `-g`
build and a plain build of one program then share one build id, as today.

The digest covers the input to `opt`, not its output. The text is a
deterministic function of the IR and the target, so two machines produce
one digest for one program. `opt` and `llc` are deterministic for a pinned
release and one input, so the objects agree too. The tests
`emit_identity` and `link_identity_<target>` check both.

`tests/emit-identity/programs.sha256` holds digests of the assembly per
program and target. With the LLVM back end it holds digests of the `.ll`
text that `--dump-llvm` prints. Every digest changes once, in the step that
switches the default. The report of that step records the old and the new
values. `tests/link-identity/*.sha256` hold digests of linked programs and
change once in the same way.

## Memory checks

`--memory-checks` inserts a call of `__asan_loadN` or `__asan_storeN`
before every load and store, after layout. `memcheck_declare` runs before
the translation and the calls translate as plain calls of external
functions. Behaviour is unchanged: the runtime of AddressSanitizer in the
archive reports a use after free, a double free, an access outside a heap
block and the leaks at exit. `MEMCHECK_OPTIONS_HOOK` and `MEMCHECK_KEPT`
translate as today.

An external call before every memory access stops LLVM from moving or
removing any load or store. So a `--memory-checks` build runs at roughly
dev-mode speed whatever the mode. That is acceptable for a checking build.

`MEMCHECK_OPTIONS` carries `fast_unwind_on_fatal=1` since `7bc666f1`, so the
ASan report walks Anti frames by their frame pointers. The LLVM back end
keeps those frame records with `"frame-pointer"="non-leaf"`, see "Attributes
and metadata". The report then sees the whole stack, as under the native
back end.

The alternative is the `asan` pass of `opt` with `sanitize_address` on
every function. It inlines the shadow checks and adds red zones around
stack slots and globals, which also catches an overflow of a local array.
It needs the ASan module constructor and the same runtime. It is a later
option, because the behaviour of `--memory-checks` is documented and
tested as it is.

## Targets, CPU levels and object formats

`struct target_info` in `src/antic/target.c` holds the triple per target
already. The triples are the ones llvm-mc takes. llc takes the same ones.
The macOS triples need the minimum version appended for the load command
that `.build_version` wrote: `arm64-apple-macos<major>.<minor>` and
`x86_64-apple-macos<major>.<minor>`, with the version from the sysroot
pin as `emit.c` reads it today. `llvm_target.c` composes that triple.

`tools/cpu-levels` holds per level the `clang -march=` value and the
`llvm-mc -mattr=` value. The LLVM back end writes function attributes
from the same rows:

| Level | `"target-cpu"` | `"target-features"` |
|---|---|---|
| `v1` | `x86-64` | `""` |
| `v2` | `x86-64-v2` | `""` |
| `v3` | `x86-64-v3` | `""` |
| `armv8.0` | `generic` | `"+v8a,+neon,+fp-armv8"` |
| `armv8.2` | `generic` | `"+v8.2a,+neon,+fp-armv8,+lse,+fullfp16,+dotprod"` |
| `armv8.5` | `generic` | `"+v8.5a,+neon,+fp-armv8,+lse,+fullfp16,+dotprod"` |

The first version writes these six rows into `llvm_target.c` and adds two
columns to `tools/cpu-levels`, so `cpu_levels_pin` checks them with
`antic --print-cpu-levels` as it checks the others. The feature strings
are checked once against the output of
`clang -march=<value> -S -emit-llvm` of the pinned clang in the step `targets`,
and the check joins `llvm_datalayout_pin`.

An `f16` conversion is `fpext half to float` on every level. llc selects
`vcvtph2ps` at `v3` and calls `__extendhfsf2` of compiler-rt below it. The
runtime archive holds `libclang_rt.builtins.a` for every target already,
so the symbol resolves. The calls of `anti_rt_f16_to_f32` and
`anti_rt_f32_to_f16` that the native back end makes at `v1` and `v2` are
not emitted by the LLVM back end. The step `switch` removes the two functions
from the runtime.

Object formats: `llc -filetype=obj` writes ELF, Mach-O and COFF from the
triple. The suffixes `.o` and `.obj` of `target_info` stay. COFF objects
from llc carry the `.drectve` section that lld-link reads, so the
`emit_names` export list is still passed as today and nothing changes in
`linker.c`.

The Windows stack probe and the Apple frame pointer come from the function
attributes under "Attributes and metadata". The relocation model comes
from the llc flags under "Integration route".

## Toolchain and packaging

`anti-lang/llvm-tools` builds the LLVM tools from the pinned 23.1.1 source
for six hosts. Two releases came out for this work. `23.1.1-anti.4` of
2026-10-02 added `opt` and `llc` to the archive
`llvm-tools-<tag>-<host>.tar.xz` of every host. `23.1.1-anti.5` of
2026-10-03 added a static `libunwind`, which the AddressSanitizer runtime
of a `--memory-checks` program needs on Linux for `_Unwind_Backtrace` and
`_Unwind_GetIP`. The shipped set is now seven tools, llvm-mc, lld,
llvm-ar, llvm-objdump, llvm-readobj, opt and llc, plus `libunwind.a`. The
LLVM version is unchanged. `SHA256SUMS` of each release is signed by the
release key of release@anti-lang.com, and the signature verifies against
`tools/keys/release.pem`. The clang archives record `HOST_LINK_VERSION`
1267, pinned in the recipe, so the Darwin driver behaves as the one of
anti.3 whatever Xcode the build machine has.

antic moves straight from anti.3 to anti.5. The digests of `SHA256SUMS` of
anti.5, for `tools/llvm-pin`:

| Host | `llvm-tools-23.1.1-anti.5-<host>.tar.xz` |
|---|---|
| linux-x86_64 | `291df4e2ed3aea068c487590ff40ab5d40739fa74197bdac01fc1087968a9d30` |
| linux-arm64 | `2483862435b441a3dc16f401618f668fd4ef0c6c8b041524463ae3bd00debe63` |
| macos-arm64 | `9a1259d5323a4f499696afb27a165711dbd3ef4835e81ddb2a7ef7f6032cb185` |
| macos-x86_64 | `a8e4a75377429085c013950a9833778ec1e2f902d0e0b1461bd20fcc77aae7fe` |
| windows-x86_64 | `ce8ec49e0f8c5950725eb001f6a8cfb353477906f36a8d1700cdd58133ad422e` |
| windows-arm64 | `b72d4de4322ae7a58af2aa72d4c9bb27866e21741b931db323ba609c077ea99b` |

For `tools/clang-pin`:

| Host | `clang-23.1.1-anti.5-<host>.tar.xz` |
|---|---|
| linux-x86_64 | `59bf17bd7a87187887d05d51004a1e284cce1e984e1e52246d1d741ddd942569` |
| linux-arm64 | `c426ef46094e8f9dd79cd1004043e9630d610d452c985bc4bfc05a31bf9aa500` |
| macos-arm64 | `e7a3ec5870dee4ae34d3abca615eafa446e3da99969c35f22a0d5a9244625dfd` |
| macos-x86_64 | `5e070e3b9309ade298dc5b9fde1921cd0f24f76078279081c54094343e1d5fcf` |
| windows-x86_64 | `f1d5101672e26e7cd0e8c5ca2f46c046614cc9b399a22a49d89a42cb8ee8a968` |
| windows-arm64 | `4952bc8759280dca0a05f2322ea7221461ef196ae927c1d2a157fad4d8384a5a` |

Both pin files keep their header comment and the lines `release=` and
`file=`. The line `tag=@VERSION@-anti.3` becomes `tag=@VERSION@-anti.5`,
and the six digest lines take the values above. `tools/llvm-version`
stays `23.1.1`. Before the session trusts the digests, it downloads
`SHA256SUMS` and `SHA256SUMS.sig` of the release, verifies the signature
as `tools/fetch-release.cmake` does, and compares every line.

In antic, step `pins` of the work order makes these changes:

- `tools/llvm-pin` and `tools/clang-pin` as above.
- `tools/check-llvm.cmake`: `opt` and `llc` join the version check loop
  and the copy loop that fills the runtime archive's `bin/`. The header
  comment names seven tools.
- `tools/get-llvm.cmake`: the header comment names seven tools.
- `tests/run_pinned_tools.cmake` checks `opt` and `llc` as it checks the
  others.
- `libunwind.a` goes from the anti.5 archive into the runtime archive
  beside the `libclang_rt` libraries, for the targets it exists for. The
  Linux link of `--memory-checks` in `src/antic/linker.c` adds it after
  the AddressSanitizer runtime. `docs/reports/2026-10-03-vm-check.md`
  names the three Linux tests that fail without it:
  `anti_memory_checks`, `memory_checks` and `memory_checks_list`.
- `docs/decisions.md` line 21 and `docs/distribution.md` under "The LLVM
  tools" name the seven tools and `libunwind`. The distribution page also
  records the growth of the archive, with the sizes from the llvm-tools
  reports. The `libunwind` entry beside `--memory-checks` in
  `docs/decisions.md` loses its open-item wording.
- `tools/pack-anti.cmake` packs what `bin/` holds, so it needs no change
  unless it lists the tools by name. Check it.
- The installer scripts change nothing. They install the package, and the
  package holds the tools.

A fresh configure then downloads anti.5 through `tools/fetch-release.cmake`,
checks each archive against its pin line and against `SHA256SUMS`, and
verifies `SHA256SUMS.sig`. `emit_identity` and `link_identity_<target>`
should not move in this step. llvm-mc, lld and clang come from the same
source at the same version, and only the new binaries differ. If a digest
moves, the report of the step says which one and why before any re-pin.

`anti-lang/llvm-tools` also builds the clang of `tools/clang-pin`. The
data layout and feature checks of step `targets` run with that clang in
the test suite, so the pinned clang is a test dependency and not a
run-time one. Users of Anti get `opt`, `llc` and `libunwind.a` and no C
compiler, as decision 296 says.

The licence of `opt`, `llc` and `libunwind` is the one of llvm-mc and
lld, Apache 2.0 with LLVM Exceptions, already in `licenses/` of the
archive. No new file.

## Dev mode and release mode

Release mode compiles the whole program into one text. `opt` runs
`default<O2>` and llc runs at `-O2`. One text means LLVM inlines across
every Anti module and across the standard library, whose IR comes from
the `.antl` files. The runtime in C is not in the text and is not inlined.

Dev mode compiles one module into one object and must stay fast. It runs
`llc -O1` on the text without `opt`. llc at `-O1` runs its own fast
register allocator and local folding. That is about the level of the
native back end. The time of a dev build grows by the start of one process
and the parse of the text. The step `measure` measures it. If a module of 5,000 lines
takes more than one second in dev mode, try `llc -O0` with `-fast-isel`
first. The combined tool under "Later options" is the second remedy.

From the step `emit-core` to the step `vm` the option `--backend native|llvm` selects the back
end on any build. Every test then runs under both, and the outputs compare.
The native back end is the reference during that time. The step `switch`
removes the option. From then on every build goes through LLVM.

A program linked from objects of both back ends works during the
transition. Both follow the C convention and the same layout. The test
`mixed_backends` checks it in the step `flags` and goes in `switch`.

## What to expect from the generated code

The question behind the move is whether Anti programs then run as fast as
C programs compiled with clang. The answer per area:

- Scalar loops over arrays, slices and structs: yes. The loads and stores
  of the IR are the ones clang writes for the same C. `opt` hoists the
  addresses, unrolls, vectorises and eliminates common subexpressions the
  same way.
- Calls of small functions and methods: yes. LLVM inlines them with the
  same heuristics, since every Anti function is in the text with `internal`
  linkage. Devirtualised calls of the whole-program pass inline as well.
- Code that calls the runtime for strings, maps, allocation and atomics:
  the call stays a call. C code that calls a library has the same cost.
  Shipping the runtime as bitcode and linking with LTO would remove it,
  see "Later options".
- Loads behind pointers without `noalias`: C with strict aliasing
  disambiguates an `int*` from a `float*` through TBAA. The first version
  has no TBAA, so a loop that stores through one pointer and loads through
  another of a different type may reload where clang does not. `own`
  parameters and slots escape less than C locals, which recovers part of
  it. Measure before adding TBAA.
- Division and shifts in release mode: one compare and one or two selects
  more than C. LLVM folds them whenever the divisor or the count is known.
  A division by a variable costs 20 to 90 cycles on current cores, and the
  guard costs 1 to 2.
- Float to integer conversion: `fptosi.sat` costs a compare and a select
  more than C on x86_64. Nothing on ARM64.
- Bounds checks, overflow checks and asserts: release mode drops them
  today and keeps dropping them. Dev mode keeps them and runs slower than
  C, as today.
- Saturating and flags operations: the intrinsics select to the same
  instructions the native back end picks, `adc`, `sbb`, `adds`, `adcs`.
- Simd: vector types select to the same instructions. LLVM also
  vectorises scalar loops that the programmer did not write as simd.

So release builds land within the range of clang `-O2` for the same
algorithm, with the exceptions listed. The step `measure` of the work order puts numbers on it. It measures the programs of
`tests/programs` and the benchmark programs it adds.

## Compile time

A release build runs `opt` and `llc` over one text of the whole program.
Both are single-threaded. For the standard library and a program of 20,000
lines of Anti, expect the text at 5 to 15 MB and `opt` plus `llc` at 5 to
20 seconds on the development Mac. The native back end does the same work
in under a second.

Mitigations in the first version:

- Dev mode runs llc alone, without `opt`.
- `anti build` runs one release build per program, not per edit.
- `--keep-llvm` lets a session rerun `opt` alone while debugging the
  translation.

Mitigations as later options:

- Split the text per Anti module, run `opt` and `llc` per module in
  parallel and link the objects. Inlining across modules then needs
  ThinLTO through lld, see "Later options".
- The combined tool `anti-llc` in `anti-lang/llvm-tools`. It runs the
  pipeline and the code generator in one process. That saves the bitcode
  write, the bitcode read and one process start.

The step `measure` records the compile time and the run time of every
program in `tests/programs` under both back ends.

## Tests

Existing tests and what happens to each:

| Test | Change |
|---|---|
| `tests/programs/*`, `program_*` | run under `--backend llvm` in release and dev mode, in addition to today |
| `tests/abi`, `program_abi_simd`, `clib_*` | run under `--backend llvm`. They check the ABI port of `abi.c` |
| `tests/checks`, `bounds_*`, `overflow_*` | run under `--backend llvm` in dev mode. They check that the guards and checks survive `opt` |
| `tests/opt/*.opt`, `--dump-opt` goldens | unchanged. The Anti optimizer runs before the translation |
| `tests/dump/*.alloc`, `*.s`, `--dump-select`, `--dump-alloc` | deleted in the step `switch` |
| `tests/dump/*.ll`, `--dump-llvm` | new goldens, one per program and target that has an `.alloc` or `.s` golden today |
| `emit_identity` | digests of the `.ll` text, re-pinned once |
| `link_identity_<target>` | re-pinned once |
| `test_x86_64.c`, `test_arm64.c` unit tests | deleted in the step `switch` |
| `cpu_levels_pin` | two new columns |
| `pinned_tools` | checks `opt` and `llc` too |
| `run_anti_memory_checks.cmake` | runs under `--backend llvm` too |
| plugin, `--lib static`, `--lib shared`, `--closed`, `--no-runtime` tests | run under `--backend llvm` too |

New tests:

- `llvm_datalayout_pin`: compares the data layout string and the feature
  strings of `llvm_target.c` against the pinned clang.
- `llvm_verify`: runs `opt -passes=verify` over the `--dump-llvm` text of
  every program in `tests/dump` and `tests/programs`. A text that fails
  the verifier fails the test with the verifier's message.
- `llvm_division`: a program that divides by zero, by minus one at
  `INT_MIN`, shifts by the width and converts `1e30` to `int` in release
  mode, and checks the defined results on every target.
- `llvm_flags`: the four flags of every flag operation against a table of
  cases, in release mode. It checks the `with.overflow` mapping apart from
  `program_flags`.
- `llvm_build_id`: a `-g` build and a plain build give one build id.
- `mixed_backends`: two dev objects of two back ends link and run.
- `llvm_inline_lines`: a release build with `-g` of a program with an
  inlined callee has a line table that names the line of the callee. It
  reads the table with the pinned `llvm-dwarfdump`, or with
  `llvm-objdump --dwarf=frames` if `llvm-dwarfdump` is not in the archive.
  If neither reads it, the test is left out and the report says so.

The VM run of `docs/vm-setup.md` covers linux-x86_64, linux-arm64,
windows-x86_64 and windows-arm64. macos-arm64 runs on the Mac and
macos-x86_64 under Rosetta. Every step of the work order that touches
code ends with the suite on the Mac. The steps `vm` and `switch` also end
with the VM run.

## Documentation changes

In the step that switches the default:

- `docs/decisions.md` line 18: the back end is the LLVM optimizer and code
  generator of the pinned release, writing LLVM IR text. The two native
  instruction-selection back ends are gone. Reason: one semantics, one set
  of goldens, and the code quality of LLVM at `-O2`.
- Line 19: antic writes LLVM IR, llc writes the object. `-S` writes the
  assembly llc produces. llvm-mc leaves the shipped set when nothing calls
  it any more, which is a separate decision after the step `switch`.
- Line 21: seven tools.
- Line 72: the guarantee about loads and stores through unseen pointers
  is replaced. The reason
  `volatile` is not needed stands on its own. There are no device
  registers, no signal handlers and no `setjmp`. Threads use `atomic`
  through the runtime, whose calls are opaque to LLVM.
- The entry behind `docs/anti-language-additions.md` line 148: release
  mode gives the ARM64 result on every target. The four cases are listed
  under "Defined results in release mode".
- Line 296: packages carry `opt` and `llc`.
- Line 348: the runtime archive holds them.
- A new entry: the integration route and why libLLVM and LTO were not
  chosen.
- `docs/anti-language-additions.md` line 148 and
  `docs/anti-syntax-overview.md` where it describes release mode.
- `docs/notes/llvm.md`: the notes of the back end, in the form of the
  other notes files.
- `docs/notes/selection.md`, `regalloc.md`, `emitter.md`, and the parts
  of `floats.md`, `flags.md` and `simd.md` that describe selection: deleted
  or cut to what still holds. Git keeps the history.
- `docs/distribution.md` and `docs/site/runtime-archive`: the two tools.

## Questions an implementer asks

| Question | Answer |
|---|---|
| Where does the translation read sizes and offsets? | From the IR after `layout_resolve` and `layout_data`. Call them where `select_module` calls them today, in the new `llvm_back_end` branch of `back_end` in `driver.c`. |
| Does `expand.c` run? | No. Every operation it expands has a row in "Instruction mapping". |
| Does the Anti optimizer still run? | Yes, before the translation, on every build. It shrinks the text and its goldens stay valid. |
| Is the IR in SSA form? | No. Give every temporary an `alloca` and let `sroa` and `mem2reg` build the SSA form. |
| How are `bool` values typed? | `i8` in memory and in temporaries, `i1` after `trunc` at a branch or a select, `zext` back after a compare. |
| How does a block reference work? | `b<index>` labels. The entry block of the IR is the entry block of the function. The allocas go to its start. |
| Who writes the function prologue? | llc. The translation writes no frame code, no spills and no callee saves. |
| How is a slot addressed? | `alloca [N x i8], align A`, and the slot's temporary is the `ptr`. |
| How do aggregates pass to C? | Through `abi_classify`, see "Calling convention". Port the rules of `locate` and `locate_result`. |
| How do `i8` and `i16` parameters extend? | `signext` or `zeroext` from the signedness the signature records, on every target. |
| What is the LLVM form of a table call? | A `load ptr` of the slot of the descriptor followed by `call` with the signature of the IR. |
| What about `patterns.start`? | An entry in `@llvm.global_ctors` with priority 65535. |
| How are globals laid out? | Packed struct constants with explicit `[N x i8]` padding, from `layout_data`. |
| Which symbols get a leading `_` on macOS? | None in the text. llc applies the mangling of the triple. |
| What linkage do functions get? | `internal` unless exported or `extern`. Exported functions are `dso_local` with default visibility. |
| Does `nsw` go on `add`? | No. Anti wraps. No `nuw` either. |
| Which fast-math flags? | None. |
| Division by zero in release? | The guarded form of "Defined results in release mode". |
| Shift count at or above the width? | Mask the count with `width - 1`. |
| Float conversion out of range? | `llvm.fptosi.sat` and `llvm.fptoui.sat`. |
| How do flags operations translate? | The result as the plain operation, each flag from a `with.overflow` intrinsic or a compare. Keep the four `i1` values per flag operation for the `FLAG` reads. |
| What does `MULH_S` at 64 bits become? | `i128` multiplication and a shift. llc selects the high-half multiply. |
| How is `f16` handled? | `i16` bits, `bitcast` to `half` at `hext` and `htrunc`. compiler-rt supplies the conversion below `v3`. |
| How are simd values typed? | `<N x T>` for a simd struct at or below the cap, loaded and stored around each operation. Masks are `<N x i8>`. |
| Where do `__asan_loadN` calls come from? | `memcheck_declare`, which runs before the translation. Translate them as calls. |
| How does `-g` work? | `!DILocation` per instruction with a line, `!DISubprogram` per function, `!DIFile` per source, one `!DICompileUnit`. Lines only. |
| How is the build id computed? | SHA-256 of the `.ll` text without the spans of `-g`, with `build_id` as today. |
| Which data layout string? | The one the pinned clang prints for the triple. `llvm_datalayout_pin` checks it. |
| Which triple on macOS? | The one of `target_info` with the minimum version appended, from the sysroot pin. |
| Which CPU attributes? | The table in "Targets, CPU levels and object formats", as function attributes. |
| How does antic find `opt` and `llc`? | `archive_tool` in `driver.c`, as for llvm-mc. `--opt` and `--llc` override. |
| What runs in dev mode? | `llc -O1` on the text, without `opt`. From `emit-core` to `vm`, `--backend native` still selects the old path for comparison. |
| What runs in release mode? | `opt -passes='default<O2>'` then `llc -O2 -filetype=obj`. |
| Does `-S` still work? | Yes. `llc -filetype=asm` writes it. |
| Does `--linker platform` still work? | Yes. The objects are standard. |
| Does the `.antl` format change? | No. `ANTL_VERSION` stays. |
| Do library files stay byte-identical across hosts? | Yes. Nothing before the translation changes. |
| Which tests get re-pinned? | `emit_identity` and `link_identity_<target>`, once, in the step `switch`, with old and new values in the report. |
| How do I check the text is valid? | `opt -passes=verify` on it. The test `llvm_verify` does this for every dump program. |
| How do I debug a miscompile? | `--keep-llvm`, then `opt -O2 -print-after-all` on the kept text, or bisect with `-opt-bisect-limit=N`. |
| What if llc rejects an attribute or intrinsic? | The pinned release is 23.1.1. Check the LangRef of that release. Every intrinsic in this document exists in it. |
| Can I change the IR to make the translation easier? | No. The IR, `ir.h` and the `.antl` format are out of scope. Ask Eddie if a case needs it. |
| Can I change decisions.md? | Only the lines listed under "Documentation changes", in the step `switch`, in the words of this document. |
| What about Windows ARM64 and `--memory-checks`? | Unchanged. `memcheck_available` says no, and the driver refuses as today. |
| Where do the intermediate files go? | Next to the output, as `<output>.ll` and `<output>.bc`, deleted unless `--keep-llvm`. |
| Which process runner do I use? | The one `linker.c` and the llvm-mc call use. Add no new spawn code. |
| What does a failed `opt` or `llc` print? | `antic: opt failed` or `antic: llc failed` with the tool's stderr before it, in the form of the llvm-mc failure. |
| Does the whole-program pass change? | No. |
| Does the plugin format change? | No. A plugin compiles as one module through the same translation. |
| How are `extern` C variables declared? | `@name = external global [N x i8], align A`, with N from the layout. Reads load the right type at offset 0. |
| What about `c_wchar` and other width-by-target types? | Resolved by `layout_resolve` before the translation, as today. |
| How much code is the translation? | Expect 2,500 to 3,500 lines of C for `llvm_emit.c`, 400 for `abi.c`, 200 for `llvm_target.c`, 300 for `llvm_debug.c`, 200 for `llvm_run.c`. |

## Work order

The driver `drive-llvm.sh` at the repository root runs the steps, one
fresh headless session per step, in the main tree, one after another. It
has the shape of `drive-fixes-2.sh`: completed step ids stand in
`build/drive/llvm-completed`, each step's output in
`build/drive/logs/llvm-<id>.out`, and `--list` and `--redo <id>` work as
there. The design stands in the repository as
`docs/work-order-llvm-back-end.md`, and every step prompt points the
session at the sections it needs. Each step ends with the three suites
passing on the Mac, a push, and a report `docs/reports/<date>-llvm-<id>.md`.
A step that cannot finish writes `BLOCKED:` with the reason as its last
line, and the driver stops. The two VM steps need anti-linux and
anti-windows running, as `vm-run` of the fix run did.

The twelve steps, by id:

`pins`, the toolchain. The release `23.1.1-anti.5` of
`anti-lang/llvm-tools` exists. Make the changes listed under "Toolchain
and packaging": both pin files, `tools/check-llvm.cmake`,
`tools/get-llvm.cmake`, `tests/run_pinned_tools.cmake`, `libunwind` in the
runtime archive and the Linux `--memory-checks` link, the documentation
lines. Configure a fresh build so the archives download and verify. Done
when the three suites pass on the Mac, `pinned_tools` passes with seven
tools, the three Linux `memory_checks` tests pass on anti-linux, and
`emit_identity` and `link_identity_<target>` are unchanged.

`targets`, targets and layouts. `src/antic/llvm_target.c` with triples,
data layout strings and the six CPU rows. `tools/cpu-levels` gains two
columns. Tests `llvm_datalayout_pin` and `cpu_levels_pin`. Done when both
pass on the Mac.

`abi`, the ABI port. `src/antic/abi.c` with `abi_classify` for the five
conventions, ported from `locate` and `locate_result`. A unit test
`test_abi.c` with one case per rule, with the expected classification
written from the ABI documents, not from the native code. Done when the
unit test passes.

`emit-core`, the translation skeleton. `src/antic/llvm_emit.c` and
`llvm_emit.h`, the options `--dump-llvm` and `--backend native|llvm`, the
driver branch in `back_end` of `src/antic/driver.c` that runs layout,
memcheck and the translation. Types, the `alloca` per temporary, blocks,
`COPY`, `JUMP`, `BRANCH`, `RET`, functions and their attributes. Test
`llvm_verify` runs `opt -passes=verify` over the text of every program in
`tests/dump` and `tests/programs`. Done when `llvm_verify` accepts a
program of control flow alone.

`emit-arith`, scalar operations. Integer and float arithmetic, bitwise
operations, shifts, compares, conversions, `f16`, and the guards of
"Defined results in release mode". Done when the scalar programs of
`tests/programs` pass under `--backend llvm` on macos-arm64 in release and
dev mode, through `opt` and `llc` run by hand from the kept text.

`emit-memory`, memory and calls. `SLOT`, `LOAD`, `STORE`, `PTRADD`,
`MEMCOPY`, `ADDR`, `BITLOAD`, `BITSTORE`, globals as packed structs,
sections, constructors, calls through `abi.c`, indirect and table calls.
Done when `tests/abi` and the `clib_*` tests pass under `--backend llvm`.

`emit-wide`, the wide operations. Overflow, flags, saturating, `MULH`,
and the six simd operations. Done when `program_flags`,
`program_saturating`, `program_mul_high`, `program_carry_chain`,
`program_wrapping` and `program_simd` pass under `--backend llvm`.

`emit-run`, the tool run. `src/antic/llvm_run.c` runs `opt` and `llc`,
with `-S`, `--keep-llvm`, `--opt`, `--llc`, the intermediate files and the
error messages of "Integration route". Done when a release build and a dev
build link and run end to end under `--backend llvm` without any command
run by hand.

`flags`, the rest of the options. `-g` through `llvm_debug.c`,
`--lib static`, `--lib shared`, plugins, `--closed`, `--no-runtime`,
`--memory-checks`, the build id. Tests `llvm_build_id`, `llvm_division`,
`llvm_flags`, `mixed_backends`, `llvm_inline_lines`. Done when every test
of the suite that runs a program passes under both back ends on the Mac,
for macos-arm64 and macos-x86_64 under Rosetta.

`vm`, the other four targets. The VM run of `docs/vm-setup.md` with
`--backend llvm` on linux-x86_64, linux-arm64, windows-x86_64 and
windows-arm64. Fix what fails. Done when the VM run passes under both back
ends.

`switch`, the switch and the deletion. LLVM is the only back end. Remove
`--backend`, `--llvm-mc`, `--dump-select` and `--dump-alloc`. Delete the
files listed under "What stays and what goes" and their unit tests. Delete
the `.alloc` and `.s` goldens, `mixed_backends`, and the two `f16`
functions of the runtime. Re-pin `emit_identity` and
`link_identity_<target>`. The report holds the old and the new values.
Make every documentation change of "Documentation changes" and write
`docs/notes/llvm.md`. Done when the suite and the VM run pass. Also done
only when `grep` finds no reference to a deleted file or option in
`src/`, `tests/`, `tools/` and `docs/`.

`measure`, the numbers. A benchmark directory `tests/bench` holds five
small programs, each with a C version compiled by the pinned clang at
`-O2`:

- a scalar loop over a slice
- a method-heavy object loop
- a string builder
- a simd loop
- a map workload

Record the run time of the Anti program and of the C program. Record the
compile time of each Anti program in release and dev mode. Measure on
macos-arm64 and linux-x86_64. Build the same programs once from the commit
before `switch` and record those numbers too. The report holds the table.
No pass or fail, numbers only.

## Later options, out of scope

- ThinLTO through lld: write bitcode per module, let lld run the pipeline
  with `--thinlto-jobs`. Removes the single-threaded release build and
  keeps cross-module inlining. Needs `--lib static` and `-S` to keep the
  llc path.
- The runtime as bitcode in the archive, linked with LTO, so string and
  allocation calls inline.
- TBAA metadata, after the aliasing rules of `as` are written down.
- `llvm.dbg.declare` for locals and parameters, with `!DIBasicType` and
  `!DICompositeType` from the Anti types.
- The `asan` pass for `--memory-checks`, with stack and global red zones.
- `anti-llc`, one process for `opt` and `llc`.
- `readonly` and `nocapture` on parameters, once the IR records that a
  function does not write through a pointer.
- A higher simd cap, since LLVM legalises wide vectors itself.
- Profile-guided optimisation with `llvm-profdata`, which the pinned
  release can build.
