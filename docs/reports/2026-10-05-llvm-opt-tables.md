# Step tables of the optimization facts

The step `tables` of `docs/work-order-llvm-optimization.md`: "Tables and
dispatch", under condition C2.

## What the step built

- `enum ir_access` in `src/antic/ir.h`, in the field of a load or a store.
  Lowering marks the store of a table pointer when an object is made, in
  `prepare` of `lower_owning.c` and `lower_store_interface_tables`, as
  `IR_ACCESS_TABLE`. It marks each load of the table pointer in
  `lower_load_table` the same way: a call through the table, `is`, a bound
  function and the thunk of a qualified body. Each load of an entry, the
  descriptor of `is` among them, is `IR_ACCESS_ENTRY`.
- The LLVM text writes `!invariant.group !N` on the first and
  `!invariant.load !N` on the second, with `!N = !{}`, where the module
  sets `table_facts` and an access names one.
- `writes_tables` of `struct ir_function`, which the library file carries as
  bit 32 of the flags of a function. The format is version 78.
- `tables_written_once` in `src/antic/whole.c`, which sets `table_facts`.
- `ir_print` shows ` !table`, ` !entry` and ` writes tables`. `ir_verify`
  refuses a store of an entry, a fact on an access that is not a pointer and
  an unknown kind.

## Correction of the work order

The work order said "The table pointer of an object is written once, when
the object is made", and asked for `llvm.launder.invariant.group` after any
other store. "Ownership and copies" of `docs/anti-object-model.md` says `=`
copies table pointers, so `*p = Shape { }` through a `*Shape` that points at
a `Circle` gives the object the table of `Shape`. The code does that, and
`tests/programs/table_rewrite.anti` now pins it. A launder after that store
gives a new pointer to the code that uses its result alone. Every other
pointer to the object, and a caller across the call, keeps forwarding the old
table. With the facts forced on for every program, `table_rewrite` printed
`30 30`, `40 1` and `70` where the rule gives `3 3`, `4 0` and `7`. The text
of that run is `build/drive/logs/tables-forced-table_rewrite.ll`. The step
kept the behaviour and wrote the facts only where every table pointer is
written once. The paragraph of the work order says so now.

## Every store of a table pointer

Lowering:

| Store | Kind |
|---|---|
| `prepare`, `lower_owning.c`: the table of the class at the object | construction, `!invariant.group` |
| `lower_store_interface_tables`, `lower_class.c`: each interface sub-object | construction, `!invariant.group` |
| `=` of a class value, the memcopy of `lower_assign` | the place's own table, or a place whose table is zero, or `*p` and `p[i]` of a class that is not final, which marks the function |
| the copy of the out pointer of a call that fills a place | as `=` |
| the generated `copy` of a class, a memcopy into the memory of `dup` | new memory |

`src/rt/`:

| Store | When |
|---|---|
| `anti_lang_Object_copy`, `object.c`: memcpy of the object | into the memory of `dup`, before Anti code sees it |
| `build`, `plugin.c`: `sub->table` set to the loader's table of stubs | after `init`, before the object reaches the host |
| `stubs_of` and `build`, `plugin.c`: the entries of that table | before an object points at it |
| the case of a variant, `registry.c`: memset to zero before `fill` | inside `deserialize`, before Anti code sees the object |

No store gets `llvm.launder.invariant.group`. The reason stands in the third
provisional entry below.

## Tests

- `marks_table_accesses` in `unit_lower`, `table_accesses` in `unit_ir` and
  `decides_table_facts` in `unit_whole`. They did not compile before the
  change. `decides_table_facts` covers release and dev, a plugin, a library
  for C, a mark read from a library file, a union that holds a class value
  and an allocator outside the standard library.
- `unit_modules`: bit 32 of the flags of a function reads and bit 64 is
  refused.
- `dump_llvm_tables_macos-arm64`, golden `tests/dump/tables.macos-arm64.ll`.
- `direct_calls`, `tests/run_direct_calls.cmake`: builds
  `programs/table_hook.anti` with `--keep-llvm`, writes the bitcode of opt as
  text and refuses a call through a pointer in `table_hook.main`. It failed
  before the change and passes after it: the done condition of the step.
- `program_table_hook`, release mode, C2: a `created` hook writes `n`, and
  calls through a class table, an interface table, `is` and a loop over
  `[]*Shape` read the value it wrote.
- `program_table_rewrite`, release mode: `=` through a pointer, through a
  second pointer and inside a hook gives the object the table of `Shape`.
  Both program tests passed before the change, since they pin behaviour.
  The forced run above shows that `table_rewrite` catches a gate that is
  missing.

## Re-pinned values

| Pin | Old | New |
|---|---|---|
| SHA-256 of `emit-identity/programs.sha256` | `918fa48c…e209d044`, 1206 lines | `3b978a20…d9941c5b`, 1218 lines |
| `link-identity/return42.macos-arm64.sha256` | unchanged | unchanged |

780 lines of the manifest changed, 12 of them new for the two programs.
`scale.antl.hex` and `generics/pick.antl.hex` change in the version byte, and
the IR goldens `devirt.ir`, `devirt.dev.ir`, `devirt.opt`, `devirt.dev.opt`
and `devirt_plugin.opt` show the marks.

## Measurement

`tests/bench/ablate/run.py <program> opt_no_tables.sh`, 15 runs each. The
wrapper removes both facts from the text, so it builds the program as it was
before the step. Its object of `map_work`, 108,720 B, equals the one of the
step `effects`. Logs: `build/drive/logs/tables-bench-<program>.log`.

Before, the build through `opt_no_tables.sh`:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 299.7 ms | 294.7 ms | 1.02 | 1,896 B | 43.5 ms |
| `objects` | 141.3 ms | 106.8 ms | 1.32 | 13,448 B | 54.4 ms |
| `builder` | 265.0 ms | 45.1 ms | 5.87 | 15,328 B | 70.5 ms |
| `simd_loop` | 126.0 ms | 122.5 ms | 1.03 | 2,016 B | 43.7 ms |
| `map_work` | 181.4 ms | 137.0 ms | 1.32 | 108,720 B | 318.2 ms |
| `mixed_work` | 194.1 ms | none | | 668,496 B | 2044.1 ms |

After, the baseline of the same runs:

| Program | Anti | To before | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 300.0 ms | 1.00 | 1.02 | 1,896 B | 40.8 ms |
| `objects` | 141.3 ms | 1.00 | 1.32 | 13,448 B | 50.8 ms |
| `builder` | 264.6 ms | 1.00 | 5.86 | 15,328 B | 66.6 ms |
| `simd_loop` | 126.1 ms | 1.00 | 1.03 | 2,016 B | 40.4 ms |
| `map_work` | 180.9 ms | 1.00 | 1.32 | 108,640 B | 303.5 ms |
| `mixed_work` | 193.9 ms | 1.00 | | 667,872 B | 1989.1 ms |

Every build printed the line of its C twin, or of its baseline for
`mixed_work`. All medians lie within 2 percent. The objects of `map_work` and
`mixed_work` shrink by 80 B and 624 B. The compile times of the wrapper
include its `sed`. The facts stand in every program of the table that has
classes: 63 loads and stores with `!invariant.group` in `map_work` and 361 in
`mixed_work`. The guard of run D already calls `Error.fatal` directly since
the step `noreturn`, so `map_work` gains nothing here.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the entries of the
step `effects`:

- The table facts stand only where every table pointer of the program is
  written once, as the pass over the whole program decides.
- What makes a function write tables, and why a union and an allocator
  outside the standard library count as well.
- The LLVM text writes no `llvm.launder.invariant.group`.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/tables-{host,asan,ubsan}-build.log`.
- Host suite: 1556 of 1556, `build/drive/logs/tables-host-suite.log`.
- ASan suite: 1555 of 1555, 409 s, `build/drive/logs/tables-asan-suite.log`.
- UBSan suite: 1555 of 1555, 286 s, `build/drive/logs/tables-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite.
- The docs-style checker reports no error on the `.md` files of the step. It
  warns on line 3 of the work order, as it did before the step.
- One full host suite ran in the background by mistake, a tool call without
  its timeout. It ran to its end, and nothing else ran beside it. The gates
  above ran in the foreground.

State after the push of the step, before this report:

```console
$ git log --oneline -3
e58df09a Add the wrapper that removes the table facts
5e12c952 Add the table facts of a whole program
c14fc24d Report step effects of the optimization facts
$ git status --short
$ git rev-parse HEAD origin/main
e58df09a18969b3ca389c24b78b917f57f613aed
e58df09a18969b3ca389c24b78b917f57f613aed
```
