# Back end of antic

This step audited the part of `src/antic/` after the front end, against
`docs/c-guidelines.md`. It read every line of `ir.c`, `ir.h`,
`ir_print.c`, `ir_verify.c`, `lower.c`, `lower.h`, `optimize.c`,
`whole.c`, `select.c`, `expand.c`, `mach.c`, `regalloc.c`, `x86_64.c`,
`arm64.c`, `emit.c`, `header.c`, `debug.c`, `linker.c`, `coff.c`,
`layout.c`, `arith.c`, `target.c` and `cpu.c`, with their headers. The
checks covered memory, ownership and errors, behaviour that depends on
the implementation, and structure and consistency. For the order of
arguments, a script listed every call with more than one argument that
calls a function (82 sites), and each site was read. Each finding below
was checked in the code. Where a program could reach it, it was also
run through the host antic of the main checkout with `--dump-ir`,
`--dump-opt` or `--dump-alloc`. The probe programs are under
`build/drive/logs/`. The findings of `tool-pass.md` for these files
stand and are not repeated here. Those are the division by zero at
`layout.c:71`, the dead store at `header.c:547`, the length of `lower.c`
and `x86_64.c`, and its judgement of the long functions it named. No
rule of the guidelines names a miscompile or a broken output file, so
five findings name no rule. Their severity follows the effect.

## Counts

| Severity | Rule | Findings |
|---|---|---|
| Severe | none, wrong code | 1 |
| Severe | 3 | 2 |
| Severe | 5 | 2 |
| Severe | 6 | 2 |
| Severe | 14 | 2 |
| Major | none, wrong output | 4 |
| Major | 3 | 1 |
| Major | 6 | 1 |
| Major | 9 | 2 |
| Major | 14 | 1 |
| Major | 17 | 1 |
| Minor | 1 | 1 |
| Minor | 3 | 2 |
| Minor | 5 | 1 |
| Minor | 6 | 1 |
| Minor | 9 | 1 |
| Minor | 11 | 1 |
| Minor | 12 | 1 |
| Minor | 14 | 1 |
| Minor | 16 | 1 |
| Minor | 18 | 1, covering the 50 functions of this area |
| Minor | 20 | 1 |
| Minor | 24 | 1 |
| Minor | 25 | 1 |
| Minor | 26 | 4 |
| Minor | 27 | 1 |

In all: 9 severe, 11 major and 19 minor findings.

## Severe

### Store forwarding reads a temporary that was assigned again

`src/antic/optimize.c:1011`, no rule, severe.

The IR is not in SSA form. A local variable is one temporary that
`ir_assign` writes again. `forward_stores` keeps the value of the last
store, `value = inst->a;`, and replaces a later load of the same address
with a copy of it, `make_copy(inst, value);`. It never checks whether
that temporary, or the base of the address, was written in between. For
`let b = a; p.x = b; b = 5; return p.x;` the optimized IR is
`store i64 %1, %0` then `ret i64 5`, so the function returns 5 instead
of `a`. The fix is to forget the held store when an instruction writes
its value or its base temporary.

### A third spilled read indexes past the scratch registers

`src/antic/regalloc.c:713`, rule 6, severe.

`map->scratch[k] = scratch[reads ? map->loads[iv->fp] : 0];` reads
`struct abi`'s `uint8_t scratch[2]` at index 2 when one instruction reads
three spilled registers. ARM64's `msub` reads three, and `emit_div`
gives it three distinct registers for `a % b`. The byte after the array
is `shadow_space`, 0 on ARM64, so the value is loaded into x0. A function
with thirty values live across a loop and a late `a % b` gives
`ldr x0, [sp, #128]` then `msub x16, x16, x17, x0`, which overwrites the
parameter the loop reads next. The float path reads `fp_save_size` the
same way. The fix is a third scratch register, or a refusal to select
an instruction that reads more spilled registers than the target has
scratch registers.

### A COFF section marked uninitialised is read without a bounds check

`src/antic/coff.c:452`, rule 14, severe.

`parse` checks the pointer and size of a section's raw data only when
`SCN_UNINITIALIZED` is clear (line 379). `read_sections` then forms
`const unsigned char *data = o->in->data + get(s->header + 20, 4);` for
every section, and reads `size` bytes of a `.drectve` through
`add_directives` or a `.debug$S` through `holds_lines`, whatever the
flag says. A `.drectve` with the flag set and a pointer and size of
`0xFFFFFFF0` reads far outside the input. The fix is to check the raw
data of every section that the join reads, or to refuse the flag on a
section with raw data.

### A packed bitfield loads and stores before its object

`src/antic/layout.c:165`, rule 5, severe.

`bit_unit` moves a unit that would end past the aggregate back with
`b->unit_offset = out->size - bytes;`. In a packed aggregate the unit can
be larger than the aggregate, and the subtraction wraps. For
`packed struct P { a: u32 : 20 }` the size is 3 and the unit is 4 bytes,
so the offset is `UINT64_MAX`. The ARM64 code of a read is
`ldr w9, [x0, #-1]`, and a write is a load and a store of the four bytes
at `x0 - 1`. The program reads and writes a byte outside the object. The
fix is to check `bytes <= out->size` and, where it fails, to use a
smaller unit or two accesses.

### An array size in bits wraps

`src/antic/layout.c:218`, rule 5, severe.

`bit = length * layout_size(l, element) * 8;` is computed in 64 bits
without a check, and the checker bounds an array length only from below.
For `struct Big { a: [2305843009213693953]u64 }`, `size_of(Big)` compiles
to 8 on ARM64. A local of such a type gets a frame of that size, and an
index then writes outside it. `(out->offsets[i] + size) * 8` at line 251
wraps in the same way. The fix is to refuse an aggregate whose size in
bits does not fit 64 bits.

### A Linux shared library passes NULL to `%s`

`src/antic/linker.c:435`, rule 3, severe.

`text_appendf(search, "-L%s", in->crt_dir);` runs for every Linux shared
library. `link_facts` in `driver.c:397` sets `crt_dir` only for the
platform linker, and the inputs are zeroed first, so a link with lld
passes NULL to `%s`. That is undefined, and what it prints depends on
the C library. The string is used only on the platform path at line 457.
The fix is to build it inside that branch.

### A step of 2^63 negates `INT64_MIN`

`src/antic/lower.c:6547`, rule 3, severe.

`uint64_t k = (uint64_t)-stride;` and `(uint64_t)-stride` at line 6602
negate an `int64_t`. `sema.c:7849` stores the step with
`(int64_t)v.as.integer`, so `for i in 0 as u64..n by 9223372036854775808`
gives `INT64_MIN`, and `-stride` overflows. The same value also makes
`down` true. The IR of that loop is a descending walk with `sdiv` and
`sgt` on an unsigned range. The fix is to keep the sign of the step
apart from its magnitude and to negate in `uint64_t`.

### A plugin copies an interface chain its library file describes

`src/antic/whole.c:2091`, rule 14, severe.

`copy_chain(m, global_value(m, chain->items[0].global), (size_t)chain->items[1].integer, n)`
takes both the source and the length from the constant of an interface
descriptor, which comes from another module's library file. Nothing
checks that `items[0]` is an address, `global_value` may return NULL,
and the length is never compared with the item count of the source.
`copy_chain` then reads `from->items[i]` for every `i` below it. A large
length also overflows the size that `ir_const_agg` passes to
`arena_alloc`. `*fields`, `*size` and `*length` at lines 2096 to 2106
are copied without a check of their kind. The fix is to check the kind,
the global and the length, and to report a damaged library.

### The IR printer looks up a function address among the globals

`src/antic/ir_print.c:491`, rule 6, severe.

A relocation with `fn` set holds a function index (`ir.h:291`). The
printer ignores the flag:
`symbol(out, m->globals[g->relocs[j].global]->module, ...`. The library
reader writes such relocations at `antl.c:2683` for the tables of every
class, and `dump_ir` in `driver.c` prints after the libraries are
loaded. With more functions than globals `--dump-ir` and `--dump-opt`
read past `m->globals`. Otherwise they print the wrong name.

## Major

### A long name is cut, and two symbols get one name

`src/antic/lower.c:2712`, rule 9, major.

`snprintf(name, sizeof name, "%.*s.%s", (int)t->name.length, t->name.text, part);`
writes into `char name[160]`, and the result is not checked. No limit
bounds an identifier. For a class name of 170 characters, `destroy` and
`copy` are cut to the same name, `find_function` finds the one function,
and the table's `copy` entry calls the teardown. The IR of such a class
holds one function where two belong. The same unchecked cut makes names
collide at line 3167 and 3178 (`init_name` into 128 bytes), 2659 and 2911
(`label[160]`, the interface sub-objects), 2853 and 2977 (thunk names
into 192 bytes), 8539 (`anti_%.*s_construct` into 160 bytes) and 8856.
In the C header, `c_name` at `header.c:93` cuts every name to 127 bytes,
and two fields that share that prefix get one C name. The fix is to
build these names in a `struct text`.

### A float literal longer than 127 characters loses its digits

`src/antic/lower.c:632`, rule 9, major.

`float_literal` copies the literal into `char digits[128]` with
`snprintf` and does not check the result. The lexer takes any number of
digits. `return 0.` followed by 130 zeros and `1e140` should give 1e9.
The IR is `ret f64 0`. A long mantissa also loses its exponent. The same
block stands in `sema.c:1133` and `sema.c:6733`, outside this area, so
the value the checker folds may differ from the one the back end
writes. One helper that parses the bytes with their length would serve
all three.

### The COFF join writes a 32-bit count into 16 bits

`src/antic/coff.c:910`, rule 17, major.

`put(header + 2, sections, 2);` stores a `uint32_t` sum of the kept
sections of every input. The section numbers of symbols and associative
records at lines 806 and 818 are stored in 2 bytes in the same way. Past
65279 sections the numbers also meet the reserved values -1 and -2. A
join with that many sections writes a corrupt object where it should
refuse. The fix is a check against the limit of the format.

### The name of a COFF section is parsed with `strtoul`

`src/antic/coff.c:357`, rule 14, major.

`unsigned long offset = strtoul((const char *)h + 1, NULL, 10);` reads
the 7 bytes of a long section name as a C string. The field is not
NUL-terminated. `strtoul` skips spaces, takes a sign and keeps reading
digits into the fields after the name. A name `/1234567` followed by
digit bytes gives another offset. Only the range check that follows
keeps the read inside the string table. The fix is to parse exactly the
7 bytes and to refuse any byte that is not a digit.

### The comparison of offsets reads bytes no store wrote

`src/antic/optimize.c:1063`, rule 3, major.

`same_offset` compares `a->as.integer == b->as.integer`, and
`same_address` at line 988 does the same. An `IR_SYM` operand is built as
`{IR_SYM, IR_VOID, {0}}` with `o.as.index = sym` (`ir.c:286`), which
writes 4 of the 8 bytes of the union. C11 6.2.6.1 leaves the other 4
unspecified. clang zeroes them today, so the result holds on this
compiler only. A compiler that does not would make `split_slots` see two
fields where one is, and the IR would differ between hosts, as the order
of arguments once made it. The fix is to compare by kind: `as.index` for
a symbol and a temporary, `as.integer` for an integer.

### The verifier passes an out-of-range function or global

`src/antic/ir_verify.c:80`, rule 6, major.

`return o->as.index < v->m->function_count;` and the line after return
false for an index outside the module without calling `fail`, so
`v.ok` stays true. The DESIGN comment at `optimize.c:11` makes
`ir_verify` the one check the passes rely on, and `reach_function`,
`mark_function` and the printer then index with the operand. The
verifier also never checks `inst->result` against `temp_count`, never
passes the arguments of a call to `operand_ok`, and `entry_set` at line
403 sets the bit of every parameter's temporary without a range check.
`antl.c` checks most of these indices today, so a lowering defect is
what reaches them.

### The C header writes a float constant that is not C

`src/antic/header.c:966`, no rule, major.

`text_appendf(out, "#define %.*s %.17g%s\n", ...)` prints a whole number
without a decimal point. `pub const S: f32 = 2.0;` becomes
`#define S 2f`, which does not compile. The `f64` form becomes `2`, an
`int`, so `1 / S` in C divides as integers. The fix is a format that
always writes a point or an exponent.

### The C header drops table functions past 64

`src/antic/header.c:765`, no rule, major.

`class_view` holds `const struct item *entries[64];`, and
`chain_functions` stops at the limit without a report. A class chain
with more than 64 public table functions, the root's among them, gets a
`_vtable` struct without its later slots and no wrapper for them. C then
reads a table whose layout differs from the one antic writes. The fix
is a list that grows, or an error at the limit.

### The debug directives do not escape the source path

`src/antic/debug.c:145`, no rule, major.

`text_appendf(out, "    %s %zu \"%s\"\n", ..., d->m->files[i]);` and
`.asciz "%s"` at line 298 write the path into an assembler string. The
path is the one the command line names, cut at the last `/` by
`module_file_of_source`. On Windows it can hold `\`. The pinned llvm-mc
refuses `.file 1 "src\Users\main.anti"` with "invalid escape sequence",
so `-g` fails on a path that another host accepts. A `"` in a path
breaks the string on every host. The fix is to escape `\` and `"` when
writing the string.

### Rematerialisation forgets the constant of register 0

`src/antic/regalloc.c:388`, no rule, major.

`uint32_t v = 0;` is only set by `defines_constant` when it finds a
definition. For every instruction that defines nothing, such as a store
or a branch, `else if (v < f->vreg_count) { definition[v].count = 0; }`
clears `definition[0]`. The second pass then leaves the uses of register
0 alone, but line 432 still drops its one constant definition, and the
uses read a register nothing wrote. Register 0 is IR temporary 0, so a
function without parameters whose first temporary is a constant the
optimizer kept would reach it. No such program was found. The optimizer
propagated the constant in every probe. The loop at lines 417 to 428,
`for (skip = false; !skip;) { ... skip = true; }`, runs exactly once.

## Minor

### Allocation failure is handled by fifty copies of one block

`src/antic/ir.c:21`, rule 26, minor.

`fputs("antic: out of memory\n", stderr); exit(70);` is written out 50
times in these files: 17 in `lower.c`, 9 in `ir.c`, 4 in `select.c`, 3
in `mach.c` and the rest one or two per file. `optimize.c:22`,
`whole.c:47`, `regalloc.c:20`, `layout.c:13` and `coff.c:142` each define
a `static void *allocate(size_t count, size_t size)` over `calloc`, in
three variants: `count + 1`, `count > 0 ? count : 1` and
`count == 0 ? 1 : count`. `mach.c` grows three arrays by hand at lines
16, 34 and 52 where `ir.c`'s `grow` serves the same purpose. One checked
allocator in a shared file would hold the message once, and the
overflow checks of the next finding.

### Sizes reach an allocation without an overflow check

`src/antic/ir.c:19`, rule 5, minor.

`resized = realloc(items, next * size);` multiplies without a check, and
so do `mach.c:17`, `35` and `57`, `debug.c:95`, `regalloc.c:269`,
`lower.c:1444`, `2869`, `2993`, `5253`, `5420`, `6205`, `6206` and
`7751` to `7755`, `ir.c:509` in `ir_const_agg`, and `whole.c:1746`,
`count * count` in `check_cycles`. The counts come from the program, so
none of these is reached in practice. `whole.c:901` sums `max` from item
10 of each class descriptor, which a library file supplies. A large
value there ends the run with "out of memory" instead of a report of a
damaged library. `emit.c:277` checks `g->relocs[j].offset + 8 > g->size`,
a sum that wraps. `antl.c` refuses such an offset first, so only the
wording of the check is at fault.

### Conversions of unsigned values to narrower signed types

`src/antic/x86_64.c:367`, rule 3, minor.

`case 8: return (int8_t)(uint8_t)v;` converts a value outside the range
of the signed type, which C11 6.3.1.3 leaves to the implementation.
Every immediate goes through it. The same conversion stands in
`arm64.c:233` to `236` and `294`, `select.c:374`, `optimize.c:55` and
`312`, `arith.c:92` to `95`, `ir_print.c:81` to `87`, `coff.c:400`,
`559`, `601` and `791`, and `layout.c:212`. clang defines the result as
the two's complement one. The rule rules out relying on it. `optimize.c:315`
folds `(float)d` for a double outside the range of `float`, which C11
6.3.1.5 leaves undefined without Annex F.

### Two calls with side effects in one argument list

`src/antic/lower.c:1710`, rule 3, minor.

`return ir_array_add(l->m, name, ir_aggregate(function_agg(l)), ir_sym_int(l->m, IR_I64, (uint64_t)n), NULL);`
calls `function_agg`, which may add an aggregate, and `ir_sym_int`,
which may add a symbol, in one argument list. `lower.c:1748` does the
same with `field_agg`. The two go into different tables, so the output
does not depend on the order today. `CLAUDE.md` forbids the form, since
one change to either function would make it depend on the order. The
other 80 candidate sites of the script were read and hold at most one
such call.

### Narrow and signed types hold sizes and positions

`src/antic/regalloc.c:469`, rule 16, minor.

`int end = 2 * (k + (int)block->count) - 1;` keeps positions in `int`,
built from `size_t` counts, and past 2^30 instructions the product
overflows. `cur->slot = (int)mach_slot_add(f, 8, 8);` at lines 639 and
644 narrows the same way. `ir.c:348`, `f->temp_capacity =
(uint32_t)capacity`, and `mach.c:46` and `66` narrow a `size_t` count
to `uint32_t`. `lower.c:502`, `532`, `561`, `588` and `774` count
`size_t` lists in `uint32_t`. `lower.c:1656`, `4815`, `5283` and `7111`
pass table indices through `int`. `coff.c:691`, `735`, `744`, `772`,
`911` and `919` write lengths as `(uint32_t)` without a check against 4
GiB.

### Fixed arrays filled up to a count nobody checks

`src/antic/regalloc.c:852`, rule 6, minor.

`frame->saved[frame->saved_count++] = (uint8_t)i;` loops over 64
registers into `saved[32]`. Only the 18 callee-saved registers of each
ABI keep it inside. `whole.c:437` fills `struct ir_field fields[16]`
with `count` entries, 13 at most today. `linker.c:28` and `36` add to
`argv` and `strings` without a check against `LINK_FIXED_ARGS` and
`LINK_MAX_STRINGS`, which rests on a hand count of each command.
`select.c:52` copies `count` operands into `operands[4]`.
`arm64.c:1722` reads `callee->params[i].agg` for an argument that may be
variadic, where `x86_64.c:1225` checks `i < callee->param_count`. Each
is correct for today's callers. A check at each would keep it so.

### `snprintf` results unchecked where only a message is cut

`src/antic/select.c:231`, rule 9, minor.

`vsnprintf(s->error, s->error_size, format, args);` does not check for
truncation. The same holds at `layout.c:38`, `coff.c:688`,
`header.c:729` and `943`, and `lower.c:5019`, where a long type name
cuts the text of a run-time check.

### Returned memory without a word on who frees it

`src/antic/lower.c:336`, rule 11, minor.

`cstr` returns `malloc` memory, and its comment does not say that the
caller frees it. `name_of_type` at line 185 has no comment.
`class_global` and `struct_global` at lines 1818 and 1879 hand out
`*module_out` and `*name_out` in the same way. `select_module`
(`select.h:185`) gives out allocated `mach_function`s, and
`whole_build` (`whole.h:24`) a `struct whole`, without naming
`mach_function_free` or `whole_free`.

### Releases repeated before each return

`src/antic/lower.c:1852`, rule 12, minor.

`class_global` and `struct_global` hold `module` and `name` and free both
before each of their returns, at lines 1852, 1865, 1894 and 1913.
`static_global` does the same at lines 1246 and 1254. At line 1914
`struct_global` frees `*name_out` and leaves the caller's pointer
dangling. `select_module` repeats `layouts_free(&layouts); return false;`
at `select.c:537` to `551` and `583`. One cleanup block at the end of
each would serve.

### Large input ends the run instead of being refused

`src/antic/optimize.c:747`, rule 14, minor.

`mark_reachable` recurses once per block along jumps. A function of a
few hundred thousand chained blocks, as a generated source file may
hold, overflows the stack. An explicit worklist would bound it. The
unchecked `max` of `whole.c:901`, named above, is the other case.
`emit.c:196` drops the second of two relocations less than 8 bytes apart
without an error.

### Threshold findings in this area

Rule 18, minor. `tool-pass.md` gave the judgement for `lower_stmt`,
`lower_expr_value`, `check_inst`, `instruction`, `print`, `split_slots`,
`remove_unused_functions`, `check_singletons`, `rematerialise_constants`,
`write_slots`, `reach_program`, `reach_calls`, the long parameter lists
of `emit`, `emit_program`, `emit_module`, `lower_checked` and
`whole_checked`, and the builders of `ir.c`. The rest:

- A split would be clearer. `class_view` (`header.c:763`, 189 lines) and
  `header_write` (`header.c:1020`, 159) are sequences of parts under
  their own comments: the struct view, the interface sub-objects, the
  prototypes and the wrappers, and the preamble, the vector types, the
  error class, the root and the tuples. `lower_for` (`lower.c:6487`, 161)
  handles a range and a sequence in one body, and each is a function's
  worth. `link_shared_command` (`linker.c:381`, 105) is one `switch`
  over the operating system, and one function per system, as
  `linux_ld` already is, would read better. `write_provides`
  (`whole.c:1985`, 162) builds one record per provided interface inside
  the loop over classes, and the record is a function's worth.
  `rematerialise_constants` has two passes, and the one-trip loop named
  above adds a level of nesting for nothing. `lower_module`
  (`lower.c:8915`, 112 lines, 8 parameters) passes options one by one
  and would take a struct.
- The size follows the shape of the problem. `build_into`
  (`lower.c:3398`, 170) and `lower_simd` (`lower.c:4499`, 163) dispatch
  over the forms of a constructor and over the simd built-ins.
  `emit_call` of `x86_64.c:1333` (133) and `arm64.c:1829` (118) follow
  the steps of a calling convention. `compute` (`layout.c:184`, 127)
  follows the C layout rules, and `class_descriptor` (`lower.c:2464`,
  121) the fields of a descriptor in order. `lower_let_value` (109) and
  `lower_call` (103) are dispatch. `write_sections` and `write_symbols`
  of `coff.c`, nest 5, follow the format. `ir_verify` and
  `ir_drop_failures`, nest 5, are one loop over blocks and instructions.
- Long parameter lists that are the shape of the problem:
  `emit_function` (8), `compare_lanes` (7), `scalar_check` (7),
  `enum_check` (8), `check_call` (8), `check_branch` (8), `write_bits`
  (7) and `fold_chunks` (8) of both back ends.

### Functions used in one file are exported

`src/antic/select.c:76`, rule 20, minor.

`select_part_register`, `select_refuse` (line 241) and
`select_is_overflow` (line 270) are declared in `select.h` and used only
in `select.c`. The same holds for `ir_sym_print` and `ir_vtype_print`,
used only in `ir_print.c`, and `ir_block_op`, used only in `ir.c`.

### `const` cast away or missing

`src/antic/lower.c:7126`, rule 24, minor.

`((struct symbol *)h->symbol)->ir = error;` writes through a pointer the
struct declares `const`. The same stands at lines 7278, 7316 and 7700,
and `layout.c:151`, where `bit_unit` writes the layout it takes as
`const struct layout *out`. `lower.c:3200` and `3235` cast
`field->value` to non-`const` to pass it to a function that takes
`const`, and `lower.c:1441` and `optimize.c:1112` cast `const` away in
the same way. `mark_function` and `mark_const` of `optimize.c:1303` and
`1333`, `interface_offset` of `whole.c:1446`, and `declare_function` and
`lower_function` of `lower.c:8200` and `8282` take pointers they only
read.

### Names outside their module's prefix

`src/antic/optimize.h`, rule 25, minor.

`optimize.h` exports `ir_optimize`, `ir_optimize_module`,
`ir_optimize_function` and `ir_drop_failures` under the prefix of the
IR. `select.c:15`, `26` and `34` define `mach_vreg`, `mach_preg` and
`mach_imm`. `target.h` exports `mangle`, `c_symbol`, `block_label`,
`object_format_name` and `convention_name` without `target_`.
`linker.h` exports `archive_command` and `relocatable_command` without
`link_`, and `layout.h` has `layouts_init` and `layouts_free` beside
`layout_`.

### The format attribute outside a platform file

`src/antic/select.c:219`, rule 1, minor.

`__attribute__((format(printf, 2, 3)))` is a compiler extension, under
`#if defined(__GNUC__) || defined(__clang__)`. It also stands at
`layout.c:26` and `ir_verify.c:20`, and at `sema.c:86` outside this
area. `tool-pass.md` read the rule the same way for the runtime.

### Dead code

`src/antic/lower.c:113`, rule 26, minor.

`bool failed;` is never set to true. The only writes are the `memset`
and `l.failed = false;` at line 9004, so the 134 tests of `l->failed`
are dead and `lower_module` always succeeds. `l->diags` is set and never
read. `select_fail` (`select.c:236`) is called by nothing.
`lower.c` includes `<stdarg.h>` and uses none of it. Before their definitions it declares
`rt_function` three times, at lines 828, 2680 and 3947, `load_table`
twice, at 831 and 2683, and `destroy_local` once more at line 7975,
after its definition. The NULL tests of `l->module_name` at lines 1861
and 2305 guard a case that every other use of the field omits. The
saving and restoring of `l->b` around `widen_operand` at lines 5025 to
5027 does nothing, and the `memset` at line 8373 clears memory that
`arena_alloc` has already zeroed. `ir_function_free`
(`ir.c:61`) sets the counts to 0 and leaves the capacities, so a later
`ir_temp` on the function would write through NULL. `drop_body` in
`optimize.c:1519` resets both and repeats the rest of it.

### Duplicated code between the two back ends

`src/antic/x86_64.c:364`, rule 26, minor.

`signed_value`, `widened`, `memory_at`, `append`, `emit_memcopy`,
`cond`, `block`, `jump` and `resolve_slot` are the same in `x86_64.c`
and `arm64.c`. `copy_memory`, `call_c`, `is_float_register` and
`copy_argument` differ only in opcodes or a register bound. In
`x86_64.c`, `match_copy` (line 419) has the body of `match_arith`, and
`emit_half_convert` builds the sequence of `call_c` by hand. `select.c`
is where the shared ones belong.

### Duplicated code elsewhere

`src/antic/lower.c:498`, rule 26, minor.

`signature`, `fatal_signature`, `provider_signature` and
`bound_signature` repeat one scan, count and declaration. `table_agg`,
`functions_agg` and `fields_agg` are one function, and each calls
`ir_agg_find` twice. The search for a global by module and name is
written six times, at lines 1242, 1848, 1890, 2116, 2304 and 3132.
`parallel_thunk` and `dispatch_thunk`, the packing blocks of
`lower_dispatch` and `lower_parallel`, and the `HANDLE_FATAL` blocks at
lines 7109 and 7262 repeat each other. In `emit.c` the `.globl` block
stands at lines 27 and 175, the loop of `.byte` rows three times, and
`.build_version` twice. `header.c:1037` writes the guard name in two
identical loops. `optimize.c:1441` repeats the class frees of
`ir.c:86`, and `whole.c:1563` and `1618` build `anti.rt.InjectSlot`
twice. `debug.c:137` declares a local `codeview` that hides the
function `codeview` of line 67, and line 185 writes out the same test.

### Comments that stand above the wrong item

`src/antic/lower.c:1674`, rule 27, minor.

"The aggregate of one field record of a descriptor." stands above
`function_agg`. The DESIGN comment of a narrowing `as` stands at
`lower.c:4902` above `integer_form` and again at 4989 above
`narrow_check`. Others stand at `lower.c:977`, `1921`, `2104`, `2160`,
`3346`, `3662`, `5518` and `7972`, and at `lower.h:11`,
`select.h:212`, `ir.h:290` and `optimize.c:1091`. `cpu.h:36` says of
`CPU_VECTOR_BYTE_CAP` that nothing reads it until simd structs exist.
They exist, and `sema.c` and `lower.c` read it.
