# Hashing and order

"Hashing and order" of round five is built. `operator fn hash(self) -> u64` is
a hook of the table, every type has `x.hash()`, a struct or class gets a
default over its fields in order, `anti.lang` ships `Ordered`, and the runtime
chooses the seed of the hashing collections at start, which a test fixes.

## What was done

- `hash` is the twentieth name of the operator table. A struct writes
  `operator fn hash(p: Point) -> u64` as a free function. A class replaces the
  `hash` of `Object` with `concrete fn hash`, and `operator fn hash` in a class
  body is refused with the form that replaces it.
- `x.hash()` gives a `u64` on every type. A scalar hashes as its 64 bits
  through the finalizer of MurmurHash3, and zero of either sign hashes as 0. A
  `str` and a run of bytes go through `anti_rt_hash_bytes`, and a pointer hashes
  by address. A struct, a tuple, a variant, a `?T`, an array and a slice take
  the default over their parts in order, and a part with a hash of its own is
  hashed by it, a generic one through its copy. `src/antic/sema_hash.c` holds
  the checker's part and `src/antic/lower_hash.c` writes the default.
  `src/rt/hash.h` holds the constants both sides read.
- The default of a class, the `hash` of `Object`, now takes the fields of the
  root of the chain first.
- `x.hash()` on a value of a type parameter needs `hash` among its
  constraints, and a copy reads the call again with its argument.
- `anti.lang` declares `pub constraint Ordered = eq + lt;`, and a bare
  `Ordered` gets `eq + lt` as a bare `Number` gets its hooks.
- `anti_rt_init` chooses the seed from `arc4random_buf`, `getrandom` or
  `rand_s`. `lang.hash_seed()` gives it and `lang.seeded(h, seed)` mixes it
  into a hash. `--anti.hash_seed=<n>`, the key `hash_seed` and `rt.configure`
  fix it.
- The library file marks a function written `operator fn`, so a module finds
  the `operator fn hash` of a struct of another module. The format is 66.

## Tests

- `programs/hash_builtin.anti`, `hash_default.anti` and `hash_generic.anti` run
  in release, dev and the macOS x86_64 runs. They check that equal values hash
  alike under the default, including `-0.0` and `0.0`, equal text at two
  addresses and structs and objects with equal fields. They pin the values of
  a scalar, a text and a struct.
- `hashing_modules_release` and `hashing_modules_dev` hash the types and the
  generics of a library file inside a struct of the program.
- `listing_error_hashing` holds the refusals of the hook, of a class, of a
  parameter without `hash` and of `Ordered`.
- `conf_hash_seed_option`, `_file`, `_configure`, `_range` and `_text` fix the
  seed or refuse a value, and `conf_hash_seed_random` sees two runs choose two
  seeds. The patterns of `--anti.help`, `--anti.inspect` and the unknown option
  and key name the new key.

## What failed and how it was fixed

- A struct from another module took the default hash where it had its own,
  because a library file did not mark a free `operator fn`. Bit 6 of the flags
  of an item now does.
- The hash of a struct read at `ptradd offset_of` its first field gave a new
  value on every run. `split_slots` of the optimizer read a temporary that
  nothing wrote, since the literal writes the first field at the address of the
  struct. The default now reads it there too. The optimizer pass itself is
  unchanged.
- The x86_64 back end wrote `movzqq` for `zext` of a pointer. A pointer now goes
  through a slot.
- `antl_scale`, `antl_generic`, the unit tests of library files,
  `listing_error_hooks`, `emit_identity`, `link_identity_macos-arm64` and
  `fmt_canonical` needed version 66, the longer list of names, the manifest and
  the digest written again from this Mac and canonical sources.
- The docs-style checker found long sentences in the new comments and the
  decisions, which are split now.
- Logs: `build/drive/logs/hash-ctest1.log` to `hash-ctest3.log`,
  `hash-asan-ctest2.log`, `hash-ubsan-ctest2.log` and `hash-docs6.log`.

## Provisional decisions

All in `docs/decisions.md` under "Generics and collections", and two under
"Files of the checker" and "Files of lowering": the hash function and its
constants, zero of either sign, pointers by address and the pointer to a
struct, arrays, slices, unions, bitfields, `?T` and variants, the hash of a
part that has its own, the replacement in a class and its refusal, the order
of the fields of a class, the hook on a type parameter, the bare `Ordered`, the
choice of the seed with its key and layers, `lang.hash_seed` and `lang.seeded`,
the operator mark of the library file with version 66, and the new files.

## Questions for Eddie

- `split_slots` mistakes a `ptradd` of `offset_of` the first field for another
  field. Nothing else writes that form today. Should it be fixed on its own?
- A class that replaces `equals` alone gets a warning. A struct with
  `operator fn eq` and no `operator fn hash` gets none. Should it?
- The specification names neither `--anti.hash_seed` nor the key `hash_seed`.
  Should it?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1071 of
1071, ASan 1070 of 1070 and UBSan 1070 of 1070. The docs-style checker reports
nothing on every touched file but `CMakeLists.txt` and `tests/CMakeLists.txt`,
whose `#` comments it reads as Markdown headings, as before.

## Proof of the push

After the push of `1fd2d1a`:

```text
$ git log --oneline -3
1fd2d1a Report hashing and order
339ba23 Split the long sentences of the comments of hashing
732824a Record hashing and order in the decisions and the overview
$ git status --short
$ git rev-parse HEAD origin/main
1fd2d1af5715aeae84fc3db295cd470dc91c0b93
1fd2d1af5715aeae84fc3db295cd470dc91c0b93
```
