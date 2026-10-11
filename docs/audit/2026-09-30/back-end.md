# Back end of antic

This step audited the part of `src/antic/` after the checker: the IR, lowering in
`lower.c` and its six `lower_*.c` files, the optimizer, the whole-program pass,
selection, expansion, register allocation, both targets, the emitter, the C
header, debug information, COFF, layout and the linker driver. Every line of
`ir.h`, `ir.c`, `ir_print.c`, `ir_verify.c`, `lower.h`, `lower_lowerer.h`, the
seven lowering files, `optimize.c`, `whole.c`, `select.c`, `expand.c`,
`mach.c`, `regalloc.c`, `arm64.c`, `x86_64.c`, `emit.c`, `memcheck.c`,
`header.c`, `debug.c`, `coff.c`, `linker.c`, `layout.c`, `arith.c`,
`target.c` and `cpu.c` was read, with their headers. Seven foreground reviewers
each read one group of these files in full. Every finding below was then
checked in the code by this step. Sixteen were also run through the antic of
`build/host` at `238a4f1`, with `--dump-ir`, `--dump-opt`, `-S` or a program
that runs. The probes and their output are under `build/drive/probes/` and
`build/drive/logs/`. Structure comes first in each group, then the C
guidelines. Each finding says whether the first audit in
`docs/audit/2026-09-23/` reported it. The findings of `tool-pass.md` for these
files are not repeated. Their state is at the end.

## Counts

| Severity | Rule or question | Findings |
|---|---|---|
| Severe | One layout rule | 1 |
| Severe | None, wrong code | 4 |
| Severe | 3 | 1 |
| Severe | 5 | 1 |
| Severe | 14 | 2 |
| Major | The split of `lower.c` | 1 |
| Major | One teardown rule | 1 |
| Major | Duplicated logic that drifted | 7 |
| Major | One way to build a name | 1 |
| Major | One layout rule | 1 |
| Major | 3 | 1 |
| Major | 15 | 1 |
| Major | None, wrong code or output | 6 |
| Minor | Placement and dependencies | 1 |
| Minor | The IR free of sizes | 1 |
| Minor | 3 | 1 |
| Minor | 5 | 1 |
| Minor | 6 | 1 |
| Minor | 9 | 1 |
| Minor | 14 | 1 |
| Minor | 16 | 1 |
| Minor | 18 and 19 | 1 |
| Minor | 20 and 25 | 1 |
| Minor | 24 | 1 |
| Minor | 26 | 3 |
| Minor | 27 | 1 |
| Minor | Warnings | 1 |
| Minor | None, an unescaped name | 1 |

In all: 9 severe, 19 major and 17 minor findings. Of the 45, 33 are new and 6
are still open from the first audit. Another 6 add new places to a finding
still open, and none returned. Two are cases that a fix of the first audit
missed: store forwarding after S17, and rematerialisation after M35.

## Severe

### The checker and the back end lay out a Mutex word differently

`src/antic/sema_expr.c:2316`, one layout rule, severe, new.

The tool pass found the unit break `_` counted by `fixed_layout`. The lock
word is a second difference. The checker takes the word of a Mutex as a `u32`,
`*size = type_lane_bytes(t);`, so it counts 4 bytes. The back end makes
`IR_LOCK` 8 bytes on Windows at `layout.c:46`. For `struct S { m: Mutex, a:
i32 }` the checker accepts `v as S` from a `simd struct V { x: i32, y: i32 }`.
On `windows-x86_64` the IR is `memcopy %3, %0, mutexcast.S`, 16 bytes out of
the 8 of `V`. The cast also makes a Mutex out of bytes, which the language
forbids. The fix is the one the tool pass names: refuse the lock word and `_`
in `fixed_layout`, or check the size in the back end.

### Store forwarding forwards a load whose result is its own base

`src/antic/optimize.c:1072`, no rule, wrong code, severe, still open (S17).

The fix of S17 forgets the held address when a later instruction writes its
base. It checks before the `switch`, and the load itself then sets `held =
at`. A load whose result is the base of its own address therefore holds a
stale address. `p = p.link` lowers to `ptradd`, `load` and `copy`, and the
peephole at line 679 fuses the last two into `p = load`. The probe is this
function:

```anti
let p = n;
p = p.link;
return p.link.v;
```

Its optimized IR is:

```text
%1 = load ptr %0
%2 = ptradd %1, offset_of fwd.L.v
```

One load is gone, and the function reads `n.link.v`. Any walk of a list or a
tree that reads one field twice in a block after reassigning reaches it. The
fix is to forget the held address after a load whose result names its base
or its offset.

### Scalar replacement splits a slot whose address a local holds twice

`src/antic/optimize.c:1217`, no rule, wrong code, severe, new.

`split_slots` treats the result of a `ptradd` of the slot as that address for
good. The IR is not in SSA form, and the peephole fuses `let q = &s.y;` into
`q = ptradd s, off`. When `if c { q = &t.y; }` gives `q` a second
definition, both slots pass as not escaping and both are split. The rewrite
then turns `*q` into a copy of `s.y`. `pick(true, 1, 2)` of the probe returns
1, and the program prints `1 1` where it should print `2 1`. The fix is to
treat a `ptradd` result with more than one definition as an escape.

### Release devirtualisation calls the host's function on a plugin's object

`src/antic/whole.c:240`, no rule, wrong code, severe, new.

`devirtualise` makes a table call direct when every table of the program
holds one function at the slot. A host that loads plugins does not see the
tables of their classes. The probe declares `Greeter`, one host class `Local`
and `let g = lib.instance(Greeter) catch fatal;`. Its release IR calls
`g.words()` as `call agg @devirt.Local.words(%24)`, on the object of the
plugin. `docs/notes/whole-program.md:95` names this case as future work, and
plugins now exist. `tests/plugin/host.anti` has no host class of `Greeter`,
so no test sees it. The fix is to keep a call through an abstract class
indirect when `whole_hosts_plugins` holds or the module is a plugin.

### A range loop wraps its counter at the end of the type

`src/antic/lower_stmt.c:743`, no rule, wrong code, severe, new.

The counter of `for i in lo..hi by k` is kept in the type of the range, and
the IR wraps. The upward step at line 842 adds `k` past the maximum and the
test `counter < limit` holds again. `for i in 0 as u8..255 by 10` never
stops: the probe breaks at iteration 31, at `i == 44`. Downward, `span = high
- low` at line 741 and `limit = low + k` at line 743 wrap. `for i in -100 as
i8..100 by -50` runs zero times, where it should give 50, 0, -50 and -100.
The fix is to test the remaining distance before each step, in the unsigned
type of the width.

### The frame size wraps and the locals overlap

`src/antic/regalloc.c:993`, rule 5, severe, new.

`offset += f->slots[i].size;` sums the slots of a frame without a check.
Layout allows an aggregate of up to 2^61 bytes. Eight locals of
`[2305843009213693951]u8` wrap the sum, and the x86_64 frame limit at line
1039 then passes. The probe compiles to `subq $48, %rsp`, so every local and
the saved registers share 48 bytes. `(int64_t)offset` at line 992 also
converts a value past `INT64_MAX`. ARM64 has no frame limit at all
(`arm64.c:2618`). The fix is to check each sum and refuse a frame past the
limit of the target.

### The IR printer passes NULL to `%s`

`src/antic/ir_print.c:484`, rule 3, severe, new.

`text_appendf(out, "global %s.%s", g->module, g->name);` runs for the
globals of the runtime, whose module is NULL (`lower_desc.c:967`). That is
undefined behaviour. The C library of the Mac prints `(null)`, and 19 files
under `tests/` pin that output: 13 of `tests/dump/`, 4 of `tests/opt/`,
`test_whole.c` and `test_optimize.c`. `symbol()` at line 61 already handles
NULL. `ir_verify.c:35` and `38` print `v->f->module` the same way, which a
library function without a module reaches.

### A library constant of aggregate scalar type reads outside the layouts

`src/antic/layout.c:882`, rule 14, severe, new.

`read_const` of `antl.c` accepts any scalar up to `IR_LOCK` for an integer,
float or symbol constant, `IR_AGG` and `IR_VOID` included. `const_vtype` then
gives `ir_scalar(IR_AGG)`, whose aggregate is `IR_NO_AGG`, and `layout_size`
calls `layout_agg(l, UINT32_MAX)`. That reads `l->agg_state[UINT32_MAX]`.
`layout_data` at line 926 reaches it for every global of a library that
keeps its value. The fix belongs in the reader: refuse `IR_AGG` and `IR_VOID`
as the scalar of a constant that is not an aggregate.

### The back ends rely on simd rules the library reader does not check

`src/antic/arm64.c:1063`, rule 14, severe, new.

The checker requires lanes of one type, a power of two of them and a size in
multiples of 8 (`sema.c:607` to `620`), and a cap of 256 bytes. `read_tables`
of `antl.c:2873` checks only that each field is a lane. The back ends assume
the rest:

- `fold_chunks` indexes `fold_registers[level + 1]` of 7 entries. A simd
  aggregate of 2048 bytes reaches level 7, past the array.
- On x86_64, a simd aggregate of 3 `i32` is 12 bytes, and `vector_load` and
  `vector_store` at `x86_64.c:2063` move 16 with `movups`, 4 past the object.
- `expand.c:381` drops an odd lane, and `shape_of` at `arm64.c:754` takes
  the type of lane 0 for every lane.

The fix is to apply the checker's simd rules in `read_tables`.

## Major

### The split of `lower.c` left files without one purpose each

`src/antic/lower.c:1`, the split of `lower.c`, major, new in this audit.

The tool pass reported that the lowering files reach into each other. This
step judged each file. `lower_eq.c`, `lower_hash.c` and `lower_simd.c` have
one purpose each. The other four do not:

- `lower.c` (3323 lines) holds the shared helpers and `lower_module`. It
  also holds the hook sites (925 to 1042) and the dev checks (1044 to 1206),
  and places and addresses (1208 to 1407). It also holds pattern literals
  (1510 to 1585), function bodies and closures (1685 to 2103), and the
  singleton, `init` and `construct` of a class (2105 to 2325). The teardown,
  clear and copy of every owning value (2413 to 3025) and the class records
  (3027 to 3172) complete it.
- `lower_expr.c` (3199) holds class init functions and field defaults (47
  to 214), the `parallel` and `dispatch` thunks (2332 to 2750), and locks and
  channels (2752 to 2847). It also holds the declarations of runtime
  functions (2354 to 2414), which `lower.c` calls back.
- `lower_stmt.c` (2719) holds the lock calls that `lower_eq.c` and
  `lower_hash.c` use (144 to 321), and half of the teardown (1075 to 1335).
  It also holds the error channel of calls (1337 to 1458) and
  `lower_construct` (1464).
- `lower_desc.c` writes descriptors and tables, and also emits code: the
  interface and reach thunks (1712, 1841) and the construct runs (1913,
  1938).

The calls run both ways: `lower_expr.c` makes 538 calls into `lower.c`, and
`lower.c` 44 back. The opening comment of `lower_lowerer.h` names neither
`lower_eq.c` nor `lower_hash.c` and describes none of these parts. The first
parts to move are the owning values into a file of their own, and the class
functions beside `lower_desc.c`.

### Teardown is written in two places, and the copies drifted

`src/antic/lower_stmt.c:1110`, one teardown rule, major, new.

The teardown of an owning value is generated in two families. `lower.c` has
`lower_destroy_owned` (2477), `each_case`, `each_element`, `teardown_field`
and `class_teardown`, gated by `sema_needs_teardown`. `lower_stmt.c` has
`destroy_value`, `destroy_array`, `destroy_optional`, `destroy_local`,
`lower_clear_tables` and `drop_result`, gated by `lower_needs_teardown`.
`destroy(p)` at `lower_expr.c:3106` is a third caller. Copy has the same two
families, `lower_copy_owned` (2590) and `copy_field` (2886). Equality and
hashing are each in one file, `lower_eq.c` and `lower_hash.c`. The copies
drifted:

- `lower_needs_teardown` is false for `[N]?C`, `?[N]C` and `[N]own fn`,
  which `sema_needs_teardown` counts as owning. The probe tears down a
  `[2]?Res` field and runs `destruct 2`, while the same array as a local
  runs no `destruct` at all.
- A class local goes to `anti_rt_destroy` through its descriptor
  (`lower_stmt.c:1154`). A class field calls the `destroy` of its class
  directly.
- `class_teardown` destroys fields first to last (`lower.c:2866`), and a
  struct's parts go last to first (`lower.c:2525`). Two class values inline
  run their `destruct` in opposite orders in a class and in a struct.
- The `?Class` branch of `copy_field` (`lower.c:2952`) omits the
  `lower_check_table` that `lower_copy_owned` and `teardown_field` make.

The fix is one file of owning values with one predicate, and the statement
paths calling `lower_destroy_owned`. The tool pass reported the split and
the element loops. The drifts are new.

### The default `==` and the default hash disagree

`src/antic/lower_eq.c:306`, duplicated logic that drifted, major, new.

`compare_at` and `hash_at` of `lower_hash.c:360` walk the parts of one type
by two rules:

- A union returns at line 306 before `own_eq` is asked, and no part is
  compared. `hash_at` asks `own_hash` first. The probe gives a union
  `operator fn eq` and puts it in a struct. Two values that differ in the
  union compare equal, and the `eq` of the union never runs.
- An `f16` part compares its bits as `IR_I16`. The DESIGN at line 7 makes
  zero of either sign one value. The probe compares a class holding -0 and
  +0 in an `f16` and an `f32`: the `f32` alone compares equal, the object
  does not.
- A two-word function compares its code alone (line 285), and the hash
  mixes the code and the context (`lower_hash.c:391`). Two closures of one
  literal that `==` finds equal hash differently.
- `hash_at` gives a Mutex a constant, and `compare_at` has no such test. A
  `[2]Mutex` field reaches `compare_array` and reads the lock words.

The fix is one walk of the parts that both files take their rule from.

### The defaults of a class are written four times, and the singleton's are wrong

`src/antic/lower.c:2197`, duplicated logic that drifted, major, new.

The fields of a new object get their defaults in `lower_construct`
(`lower_stmt.c:1485`), `class_init` (`lower.c:2252`), the literal path
(`lower_expr.c:512`) and the singleton's `get` (`lower.c:2186`). The
singleton calls `lower_store_default` at the offset of the field, where the
others call `lower_store_field_default`, which writes a bitfield into its
unit. A singleton with `low: u8 : 3 = 5, flag: u8 : 1 = 1, high: u8 : 4 = 9`
prints `1 1 0`, where a plain class prints `5 1 9`. Only `lower_construct`
zeroes an `own fn` field without a default. The fix is one helper that
prepares an object, used by all four.

### ARM64 drops the low bits of a large argument offset

`src/antic/arm64.c:680`, duplicated logic that drifted, major, new.

`emit_imm12` writes a value above 4095 as `#v >> 12, lsl #12` and drops the
low 12 bits. Every caller guards it with `fits_imm12` except
`copy_argument` at line 680 and `incoming_address` at line 655. The x86_64
copies use a 32-bit displacement. A function with 519 `int` parameters and
then a `struct Pair { a: int, b: int }` returns 4 where it should return 42.
With 520 it returns 0. The fix is to guard both calls and load a larger
offset into a register first.

### Two passes decide what a program reaches, by different rules

`src/antic/whole.c:617`, duplicated logic that drifted, major, new.

`reach_program` says it marks "as the optimizer's removal of unused
functions marks them". The optimizer follows a live declaration to the
definition of the same module and name (`optimize.c:1454` and `1564`), and
`reach_program` does not. The optimizer picks the entries with `ir_in_unit`,
`reach_program` with `strcmp(f->module, entry)` at line 638. A function kept
by the optimizer can then miss the slot bitmaps, the registry or the
trampolines that `whole.c` writes from its own reach. The fix is one reach
function that both use.

### The link inputs of a shared library miss what a program's carry

`src/antic/driver.c:2780`, duplicated logic that drifted, major, new.

`struct link_inputs` is filled at `driver.c:748` for a program, at `2559`
and at `2780` for a library for C. The program's fill sets `frameworks`,
`linux_libraries`, `glibc` and `memory_checks`, and the library's sets none.
`link_facts` at line 464 still demands the Apple SDK when the module names a
framework. A macOS `--lib shared` with `link framework "X";` then needs the
SDK and links against the sysroot without `-framework X`. The lld flavours
and the sysroot paths are spelled both in `driver.c:382` and `434` and in
`linker.c`. The fix is one function that fills the inputs for every link.

### Two folders of the IR shift by different rules

`src/antic/layout.c:424`, duplicated logic that drifted, major, new.

`fold_op` folds `a << (b % 64)`, and `optimize.c:201` leaves a count at or
above the width unfolded. `docs/decisions.md:199` makes such a count in a
constant a compile error. The probe compiles on both targets without an
error:

```anti
const K: u64 = (1 as u64) << ((size_of(c_long) * 8) as u64);
```

It gives `movq $1, %rax` on Linux and `movabsq $4294967296, %rax` on
Windows. The fix is to refuse the count in `fold_op`, and to clamp it where `shift_wrap_sym`
of `lower_expr.c:2017` relies on the wrap.

### The header and lowering order the table of a class apart

`src/antic/header.c:937`, duplicated logic that drifted, major, new.

`chain_functions` rebuilds the order of the table that `table_of` of
`lower_desc.c:63` builds. Lowering matches an entry by name and parameter
count (`lower_desc.c:48`), and the header by name alone (`header.c:960`).
Two statics of one name in a chain are two slots in antic's table and one in
the header's `_vtable`, so C reads every later slot one off. A `pub` static
also gets a wrapper that reads `self`, which it does not have. The probe
header holds:

```c
static inline int64_t anti_C_make(void)
{
    return ((const C_vtable *)self->base.vtable)->make(self);
}
```

The fix is one routine for the table order, and no wrapper for an entry
without `self`.

### Names in the C header collide

`src/antic/header.c:158`, one way to build a name, major, new.

- `element_c_name` writes no end to a nested tuple. `((int, int), int, int)`
  and `((int, int, int), int)` both become
  `anti_tuple_tuple_int_int_int_int`, and the probe header defines that
  struct twice. Every function element becomes `fn`.
- The wrapper of a table entry is `anti_<C>_<f>`, the pattern of the helpers
  `init`, `descriptor`, `vtable` and `as_<I>`. A `pub fn init(self)` gives
  `void anti_C_init(C *self);` and then `static inline int64_t
  anti_C_init(C *self)`.
- `variant_view` writes `t->name` raw at line 560, and `element_c_name` uses
  `type_name`. The declarations use `types_c_name`, so the copy of a generic
  variant exported as `R` gets `enum Result<int>_tag`.

The fix is one helper that builds every C name, over `types_c_name`, with a
delimiter for a nested tuple.

### Descriptor items are read by bare numbers in two files

`src/antic/whole.c:2390`, one layout rule, major, new.

`lower_desc.c:1336` to `1450` writes the class descriptor with literal item
indices, and `whole.c` reads them the same way: 3 for the size, 6 for the
field count, 11 for the functions, 12 to 14 for the versions (`whole.c:1121`,
`1139` and `2390` to `2398`). The function record and the version record
are read by number too. Only `DESCRIPTOR_ITEMS` is shared. A change of order
in one file makes the other copy the wrong item, and the kind checks catch
only some of it. The fix is named item constants in one header.

### A store may be placed after its block ended

`src/antic/lower_expr.c:388`, rule 3, major, new.

The call below reads `l->b` in the argument list of a call that can change
it:

```c
ir_store(l->f, l->b, lower_ir_type_of(e->type), lower_converted(l, e), dest);
```

Signed `+` emits an overflow check that ends the block, so the result
depends on which argument C evaluates first. On the Mac
`let x: ?int = a + b;` stops antic with "the IR of argorder.anti fails
verification" and "branchov is not the last instruction". A compiler that evaluates right to
left, as clang for Windows does, compiles it. `lower_expr.c:34` passes
`lower_expr` beside `l->b` in the same way. `lower_expr.c:2731` and `2978`
are safe only because the callee restores `l->b`. The fix is to bind each
call to a local first, as `CLAUDE.md` asks.

### An `own` pointer to an owning struct frees its buffer alone

`src/antic/lower.c:2832`, no rule, a leak, major, new.

`teardown_field` deletes an `own *C` of a class and destroys the elements of
an `own []C`. Every other `own` pointer or slice gets `anti_rt_give` of its
buffer, whatever its element owns. For `own p: ?*Pair` with `struct Pair { r:
Res, k: int }`, `delete(b)` of the owner prints nothing: the `destruct` of
`Res` never runs. `copy_field` copies such a buffer as bytes, so two objects
share what the element owns. `docs/anti-object-model.md:197` says such a
value goes with its owner. The fix is to tear down and copy the elements
with `lower_destroy_owned` and `lower_copy_owned`.

### `alloc` writes through the result of `malloc` unchecked

`src/antic/lower_stmt.c:1748`, no rule, the specification, major, new.

`docs/anti-language-additions.md:81` says out of memory is fatal for `alloc
T { }` and `alloc T(args)`. Lowering calls `malloc` and stores the table
into the result with no test, at `lower_stmt.c:1748`, `lower_expr.c:3016`
and `lower.c:2181` for a singleton. A failed allocation writes through
NULL, and a store at a large field offset can land in mapped memory. The fix
is a runtime allocation that calls the failure routine.

### ARM64 accepts an `f32` offset its load cannot encode

`src/antic/arm64.c:1977`, no rule, wrong output, major, new.

`fits_address` and `memory()` at line 1920 give `IR_F32` the scale of 8
bytes, although `width()` at line 216 says 32 bits. An offset that is a
multiple of 8 up to 32760 passes, where `ldr s0` takes multiples of 4 up to
16380. `struct Big { pad: [5000]f32, x: f32 }` and `return b.x;` give `ldr
s0, [x0, #20000]`, and llvm-mc refuses it on every ARM64 target. The fix is
the scale of 4 bytes for `IR_F32` in both places.

### A field of a class never gets its C type declared

`src/antic/header.c:716`, no rule, the header is not C, major, new.

`aggregate` writes the types of a struct's fields before the struct through
`emit_uses` at line 897. `class_fields` and `class_view` do not, and
`emit_tuples` runs over function signatures alone. The probe class `C` with
`pub p: (int, int)` gets `struct anti_tuple_int_int p;` inside `struct C`,
and the tuple is defined only further down. A field of a `?T` or of a struct
declared later fails the same way. The fix is to call `emit_uses` on every
field in `class_fields`.

### A float constant that is infinite is written as `inf.0`

`src/antic/header.c:1259`, no rule, wrong output, major, still open (M32).

The fix of M32 adds `.0` to a whole number. `sema_const.c` folds `1.0 / 0.0`
to infinity, and `%.17g` prints `inf` or `nan`. The probe header holds
`#define INF inf.0`. The fix is to write the C forms of infinity and NaN, or
to refuse a constant that is not finite. `INT64_MIN` is written as
`((int64_t)-9223372036854775808)` at line 1283 and in `enum_view`, a literal
that does not fit its type.

### Rematerialisation drops a constant that a memory operand reads

`src/antic/regalloc.c:420`, no rule, wrong code, major, new.

The fix of M35 rewrites every register operand that reads a constant and
then drops the definition. The rewrite loop at line 396 handles `MACH_VREG`
operands alone. A constant used as the base or the index of a `MACH_MEM`
operand keeps its old register, whose one definition is dropped. `address_of`
of `arm64.c:1961` makes such a base for a load from a constant address. No
source program was found, since the optimizer propagates such constants.
IR that skips the optimizer reaches it. The fix is to keep the definition
unless every use was rewritten.

### The COFF reader and the archive index have few malformed inputs

`src/antic/coff.c:309`, rule 15, major, still open (M31).

`tests/unit/test_coff.c` now feeds an uninitialised section with large
offsets, bad long names and too many sections. No test feeds a truncated
object, a symbol or string table outside the data, a relocation past the
end, an auxiliary count past the table or a section number past the count.
`coff_archive_exports` with `read_index` and `member_at` has no test.
Every read traced in `coff.c` is bounds-checked, so the gap is the tests.

## Minor

### Placement and the direction of includes

`src/antic/mach.c:9`, placement and dependencies, minor, new and still open
(M29).

- `mach.c` includes `select.h` for `struct target_desc` in `mach_print`.
  The layer of machine code depends on the selector. `regalloc.c` and
  `emit.c` include it for the same struct. A header of the target
  description would serve all three.
- `select_module` at `select.c:524` folds the layouts, expands, runs the
  optimizer again and adds the memory checks, all in place on the IR. The
  memory checks are split between it and `memcheck_declare` of
  `driver.c:1242`.
- `layout.c:607` to `742` rewrites bitfield access and `c_wchar` in the IR,
  which the comment of `expand.c:8` claims for expansion.
- `emit.c:392` to `418` checks the relocations of library globals, a check
  the reader owes under rule 14.
- `coff.c:995` to `1192` holds the archive reader of `driver.c`, and
  `link_relative` at `linker.c:802` is a path helper.
- Lowering, `optimize.c` and `whole.c` include `target.h` for
  `RUNTIME_MODULE`, `RUNTIME_ROOT` and `INJECT_SLOT_PREFIX`.
  `lower_eq.c:5` and `lower_hash.c:6` include it and use nothing of it.
- `target_host` at `target.c:163` is host detection outside the platform
  layer, which M29 of the first audit named.

### Sizes written into the IR

`src/antic/lower.c:1534`, the IR free of sizes, minor, new.

`ir_global_add(l->m, ..., empty, sizeof empty, 8)` gives the slot of a
pattern a literal size, with the comment "8 bytes on every target".
`whole.c:1902` does the same for a plugin holder. `layout_data` at
`layout.c:917` writes `bytes`, `size` and `align` into the globals of the IR
module, so the module can be laid out once only.

### Conversions the implementation defines

`src/antic/x86_64.c:688`, rule 3, minor, new.

`float narrow = (float)value;` is undefined for a double outside the range
of `float`. `arm64.c:1141` uses `arith_to_f32` for the same value, so the
back ends differ. Only a library constant reaches it. `fold_float` at
`optimize.c:260` divides by a zero `double`, which C11 6.5.5 leaves
undefined without Annex F. `arm64.c:1321` converts the raw `uint64_t` of a
narrow constant to `int64_t`, which a damaged library can make negative.

### Sizes computed without an overflow check

`src/antic/ir.c:644`, rule 5, minor, new.

`arena_alloc(m->arena, (g->reloc_count + 1) * sizeof *relocs)` has an
unchecked product. Each call copies the whole list, so a global with n
relocations leaves n(n+1)/2 records in the memory of the module. The class
tables and the registry grow that way. `lower_stmt.c:1519` passes `2 *
e->as.call.arg_count + 1` to `ir_alloc` before its own check.

### Fixed arrays that rest on a count

`src/antic/regalloc.c:664`, rule 6, minor, new.

`spill_map` holds 4 entries, `MACH_MAX_OPERANDS`, and `place` fills it with
`k = map->count++` and no bound. `BORROW_LIMIT` at line 22 allows 8
registers in one instruction. The largest instruction today reads 4. The
offsets of ARM64 stack arguments at `arm64.c:674` and `1833` are not
checked against the range of `str` and `ldr`. A large argument area then
fails in the assembler instead of in a diagnostic.

### Names cut in fixed buffers

`src/antic/lower.c:450`, rule 9, minor, new.

`snprintf(symbol, sizeof symbol, RUNTIME_ROOT "%s", sym->item->runtime);`
writes into `char symbol[64]` unchecked, with a name a library file
supplies. A long name is cut and can find another function.
`write_replacement` at `whole.c:1898` returns without a holder when an
interface name passes 240 bytes, so `--anti.inject` cannot replace it.
`regalloc.c:1041` and `1068` format errors with `snprintf`, where `ir_format`
marks a cut.

### Recursion and values a reader passes

`src/antic/optimize.c:1361`, rule 14, minor, new.

`mark_function` recurses once per call along the call graph, and a chain
of a few hundred thousand functions overflows the stack. The first audit
fixed `mark_reachable` with a worklist. A COFF section number from 0xFF00 to
0xFFFD reads as -256 to -3 at `coff.c:403` and passes into the joined object
unrefused. `object_symbols` at `coff.c:1112` treats the same values as
undefined.

### Narrow types for counts

`src/antic/ir.c:398`, rule 16, minor, new.

The function count, and the counts at `ir.c:215`, `248`, `257`, `263`,
`319`, `325`, `481` and `495`, are narrowed from `size_t` to `uint32_t`
without the guard `ir_temp` has at line 451. `IR_NO_AGG` and `IR_NO_INDEX`
are `UINT32_MAX` and collide with entry 2^32-1. `lower_expr.c:2283` narrows a
slot the same way, and `hint_vreg` of `regalloc.c:62` is an `int` filled
from a `uint32_t`.

### Thresholds and file lengths

`src/antic/lower_stmt.c:2121`, rules 18 and 19, minor, still open.

The judgements of the first audit and the tool pass stand for the functions
they named. For those they did not judge:

- A split would be clearer. `lower_stmt_kind` (354 lines, nest 5) has
  `STMT_SWITCH` inline for 115 lines, where every other statement calls a
  helper. `lower_function_body` (`lower.c:1815`, 149 lines) binds the
  parameters, the captures and the scope in three parts. `lower_call`
  (`lower_expr.c:2185`) grew to 146 lines, and its choice of target is a
  function's worth. `copy_field` (114) shrinks once the teardown finding
  above is fixed.
- `whole.c` (2573 lines) splits into the analysis and the writers of the
  runtime tables, `write_registry` to `write_provides`.

### Functions exported for one file, and names outside their prefix

`src/antic/memcheck.h:43`, rules 20 and 25, minor, new and still open.

`memcheck_leaks` and `lower_copies_parts` (`lower_lowerer.h:557`) are
exported and used in their own file alone. Still open from the first audit:
`optimize.h` exports `ir_optimize` and three more under `ir_`, `target.h`
exports `mangle`, `c_symbol`, `block_label`, `object_format_name` and
`convention_name`, `linker.h` exports `archive_command` and
`relocatable_command`, and `layout.h` has `layouts_init` and
`layouts_free`. `mach_preg` and `mach_imm` are defined in `select.c:32` and
`40`.

### `const` missing or cast away

`src/antic/memcheck.h:55`, rule 24, minor, new and still open.

`memcheck_function` takes `struct ir_module *m` and only reads it. `lower.c`
frees names with `free((char *)fields[i].name)` at lines 216, 1991 and 2063.
`v->type = (struct type *)t;` at `lower.c:1602` is still open from the first
audit.

### Out-of-memory blocks still written by hand

`src/antic/lower.c:1540`, rule 26, minor, still open.

The first audit counted 50 copies in this area. Four remain. `lower.c:1536`
grows the pattern list with `realloc`, unchecked, and exits with 1 where
`ir_out_of_memory` exits with 70. `expand.c:365` and `465` and `coff.c:1058`
call `calloc` and write the message themselves.

### Repeated blocks

`src/antic/optimize.c:94`, rule 26, minor, new and still open.

- `operand` and `operand_at` of `optimize.c` have one body.
  `optimize.c:1433` and `whole.c:47` hash a module and a name with FNV-1a,
  each mixing its own way.
- `is_error_class` stands at `header.c:102` and `sema_export.c:25`.
- The symbol of a global, `c_symbol` when exported and `mangle` otherwise,
  is written at `mach.c:175` and at `emit.c:240`, `264`, `366` and `530`.
- `"anti.rt"` is spelled 14 times in `whole.c`, beside 9 uses of
  `RUNTIME_MODULE`. The name `"[N]i64"` is built by hand at `lower_desc.c:1205`,
  `lower_stmt.c:331` and `lower_simd.c:334`.
- `lower_sync_call` of `lower_expr.c:2772` repeats `lower_rt_call`, and eight
  sites of `lower_expr.c` build `ir_call` of `lower_rt_function` by hand.
- The count of array elements stands a third time at `lower_eq.c:218`.
- Too many operands is reported at `select.c:58` and aborts at `mach.c:26`.
- Between the back ends, `jump`, `emit_memcopy`, `copy_memory`, `call_c`,
  `is_float_register`, `copy_argument` and `address_of` are still two copies,
  from the first audit at `x86_64.c:364`. `lane_width` of `x86_64.c:1770` has
  the body of `width`.

### Dead code

`src/antic/lower.c:3188`, rule 26, minor, new and still open.

`lower_module` takes `diags` and returns true on every path, as `lower.h:24`
says, which leaves a dead parameter and a dead result. The first audit
named it at `lower.c:113`. New: the `else` at `lower_desc.c:1352` cannot
run, and `own = false;` at `lower_desc.c:1817` is a dead store.
`|| inst->type == IR_I8` at `arm64.c:322` repeats `is_arith_type`. No caller
reads the result of `mangle` (`target.c:112`). `optimize.c:3` includes
`<stdio.h>` and uses none of it.

### Comments that no longer say what the code does

`src/antic/lower_lowerer.h:4`, rule 27, minor, new and still open.

- The opening comment of `lower_lowerer.h` omits two files, as above.
- "The seven functions of anti.lang.Object" at `header.c:1098` stands above
  16 slots.
- The COFF DESIGN at `target.c:84` says a symbol holds letters, digits and
  `_` alone. `symbol_name` keeps `.` and writes `$xx`.
- `ir.h:464` says the checked memory is for the back end, and the front end
  uses it.
- `lower_stmt.c:873` cites "Chapter 2" above `destroy_value` and belongs
  above `lower_assign`. The comment of `coalesce` stands above
  `coalesce_value` at `lower_expr.c:1215`, and that of `context_aggregate`
  above `worker_param` at `2416`.
- Still open from the first audit: the comment of `lower_address` stands
  above `coalesce` at `lower_expr.c:836`.

### A warning silenced with no reason given

`src/antic/arm64.c:406`, the Warnings rule, minor, new.

`(void)s;`, `(void)inst;` and `(void)t;` silence an unused parameter 26
times without a comment: 11 in `arm64.c`, 8 in `x86_64.c`, 5 in `lower.c`
and 1 in `target.c`. Most are callbacks of a table whose signature is fixed,
where the warning is wrong. The rule then asks for a comment giving the
reason at that line.

### The CodeView name of a function is not escaped

`src/antic/debug.c:440`, no rule, minor, still open (M34).

`.asciz \"%s\"` writes the name of a function raw into the `S_LPROC32` record,
while `compile_unit` writes the same name through `quoted()` at line 385.
Names hold no `"` or `\` today.

## The tool pass in this area

- The generic trees of a library file, severe: still open. Lowering also
  trusts `l->loop` at a `break` (`lower_stmt.c:2389`), `variant_case` at
  `lower_expr.c:338` and `1970`, `enum_value` at `lower_expr.c:2992`, and
  `tuple->fields[i]` at `lower_stmt.c:545`.
- The unit break in `fixed_layout`, severe: still open at
  `sema_expr.c:2290`. The Mutex word above is a second case.
- The zero-byte simd lane at `layout.c:96`, severe: still open.
- The allocator in the IR, major: still open. `ir_vformat` and `ir_format`
  of `ir.c:73` are the same kind of helper.
- The header sections of `lower_lowerer.h`, the element loops, the shared
  blocks of the back ends and the `neon` name of `header.c:619`, minor:
  still open.

Of the first audit's findings in this area, these are fixed: S17 to S25, M8,
M21, M24 to M26, M28, M33 and M35. The exceptions are the cases named above. The commits
that fixed S17 to S19, S22 to S25, M8, M21, M28 and M35 added tests.
