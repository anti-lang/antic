# Step tbaa-rules of the optimization facts

The step `tbaa-rules` of `docs/work-order-llvm-optimization.md`, choice D6.
The driver stops after this step until Eddie accepts the rules.

## What the step built

- "Aliasing of views" in `docs/anti-language-additions.md`, after "Simd
  structs", with its line in the contents. Fourteen rules say which accesses
  may refer to the same memory once `as` gives a view.
- A paragraph under "Pointers" in `docs/anti-syntax-overview.md`, and its
  status line: the release build that uses the rules is not built yet.
- One provisional entry in `docs/decisions.md`, after those of `arith`.

The step writes no metadata and changes nothing under `src/`. It adds no test,
since no fact enters the LLVM text. The program tests of C2 come with the step
`tbaa`, one per rule at the edge of its guarantee.

## The rules and the programs that rely on them

Each name is a program of `tests/programs`.

| Rule | Programs |
|---|---|
| Memory holds a type | Memory from `alloc` takes the type of its stores: `alloc_owning`, `arith_facts`, `assign_owned`, `concurrent`, `delete_owned`, `destroy_locals`, `dup_owned`, `effects_edges`, `eq_kinds`, `f32_far_field`, `float_math`, `forward_own_base`, `generic_copies`, `generic_static`, `generic_sync`, `long_names`, `lock_free_queue`, `loops`, `member_keywords`, `nullable`, `object_model`, `optional_values`, `owned_buffers`, `owning_values`, `parallel_classes`, `param_facts`, `stack_args`, `strings`, `synchronized`, `transient`. `deserialize` through the collections |
| Same type | `callbacks`: qsort hands back `*byte` to `i32` elements, read as `*i32` |
| Integers | none. The rule keeps C's rule for a binding |
| Bytes | `arith_facts` views a `Pair` as `*byte` and back. `callbacks`, `owning_destruct` and `owned_buffers` pass memory as `*byte` to C |
| Pointers | `regex_compile` and `pattern_literals` read the code pointer that `struct anti_pattern` holds first as `?*byte` |
| Structs and classes | `object_model`, `interfaces`, `class_is`, `dup_owned`, `delete_owned`, `deserialize`, `root_object`, `long_names`, `eq_default`, `trace_handlers`, `regex_compile` and `pattern_bytes` reach a base or an interface. No program views a value as one of its parts |
| Arrays and slices | `callbacks`: an element pointer of `[5]i32` comes back from qsort |
| Simd structs | `simd`: `as` between `Vec4`, `Vector4`, `[4]f32` and `[16]byte`, all value copies. No program views a simd struct through a pointer |
| Unions and variants | `eq_hash_agree` reads `Word` through its union. `eq_parts`, `generic_copies`, `generic_nested`, `generic_variants`, `hash_builtin`, `variant_owning` and `variants` hold variants |
| A view where it stands | `arith_facts` reads `*Wide` past a `Pair`. `half` reads `f16` as `u16`, `u32` as `f32` and `f32` as `u32`. `callbacks`, `regex_compile` and `pattern_literals` read through a local view |
| A view passed on | none in `tests/programs`. `tests/std/destroy_from.anti` and the collections of `src/std/` keep allocator memory as `*T` in a field |
| Class pointers | the programs of "Structs and classes" |
| Functions of C | `callbacks` with qsort, `owning_destruct` and `owned_buffers` with `anti_lang_Object_copy`, `regex_compile` and `pattern_literals` with PCRE2, and every program through the runtime |

Every view of `src/std/` was read against the rules. Each one views memory as
bytes for C, uses a local view, converts between classes, or gives memory
from an allocator the one type it holds for its whole life. The room of a
collection, the parts of one block in `map`, `sorted` and `pool`, and the
`Block` of `ArenaAllocator` are of that last kind.

## Measurement

No file under `src/` changed, so the LLVM text of every program is that of the
step `arith`. `tests/bench/ablate/run.py <program>`, 15 runs each, logs
`build/drive/logs/tbaa-rules-bench-<program>.log`.

Before, the "After" table of `docs/reports/2026-10-05-llvm-opt-arith.md`, the
same compiler:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 297.5 ms | 292.9 ms | 1.02 | 1,896 B | 40.8 ms |
| `objects` | 141.7 ms | 105.6 ms | 1.34 | 13,448 B | 51.8 ms |
| `builder` | 262.7 ms | 45.6 ms | 5.76 | 15,328 B | 67.3 ms |
| `simd_loop` | 129.1 ms | 125.2 ms | 1.03 | 2,016 B | 40.9 ms |
| `map_work` | 181.8 ms | 137.3 ms | 1.32 | 108,608 B | 310.2 ms |
| `mixed_work` | 191.3 ms | none | | 668,960 B | 1996.9 ms |

After, this step:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 297.3 ms | 292.8 ms | 1.02 | 1,896 B | 41.4 ms |
| `objects` | 143.2 ms | 106.0 ms | 1.35 | 13,448 B | 51.3 ms |
| `builder` | 266.6 ms | 45.9 ms | 5.80 | 15,328 B | 78.2 ms |
| `simd_loop` | 129.3 ms | 125.2 ms | 1.03 | 2,016 B | 40.8 ms |
| `map_work` | 181.1 ms | 136.8 ms | 1.32 | 108,608 B | 304.4 ms |
| `mixed_work` | 191.8 ms | none | | 668,960 B | 1992.4 ms |

Every build printed the line of its C twin, and `mixed_work` its baseline.
Every median lies within 2 percent of the one before it.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after those of `arith`:

- The aliasing rules of views by `as` stand in the additions. Where the
  documents were silent they take those of C. Every pointer is one type and a
  simd struct is its lanes. A view reaches any memory along the paths of the
  entry on `inbounds`.

## Questions

- A view passed on to a type its memory does not hold breaks a rule, and
  nothing reports it. Should the checker warn where a view leaves along a
  path that the rules no longer cover? The arith report asked whether the
  view belongs to the type, which would answer this too.
- Bytes are `u8` and `i8`, as in C, so every `u8` field may refer to any
  memory. A narrower rule, views as `*byte` alone, gives more facts but needs
  the view to travel with its pointer.
- Every pointer is one type. Clang now tells pointer types apart. Anti's
  allocators and collections convert pointers freely, so the step keeps them
  together.
- A pointer to one field of a union reads the field the last store wrote.
  Should `&u.f` be refused instead?

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/tbaa-rules-{host,asan,ubsan}-build.log`.
- Host suite: 1569 of 1569, 213 s, `build/drive/logs/tbaa-rules-host-suite.log`.
- ASan suite: 1568 of 1568, 421 s, `build/drive/logs/tbaa-rules-asan-suite.log`.
- UBSan suite: 1568 of 1568, 294 s, `build/drive/logs/tbaa-rules-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite. No pin changed.
- The docs-style checker reports nothing on the three documents and on this
  report, `build/drive/logs/tbaa-rules-docs.log`.

State after the push of the rules, before this report:

```console
$ git log --oneline -3
abe03bec Write the aliasing rules of views by as
9b122b12 Report step arith of the optimization facts
2bcc49e3 Leave nsw off a step of half the range of a signed type
$ git status --short
$ git rev-parse HEAD origin/main
abe03bec15bc5cf0ba1f616ae58ebc16ca2e8fb6
abe03bec15bc5cf0ba1f616ae58ebc16ca2e8fb6
```
