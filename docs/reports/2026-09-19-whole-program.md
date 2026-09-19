# The whole-program pass

The first compiler item of the handover: one pass over the IR of the whole program,
then four things on it. All five are done, each with its tests. 428 tests pass on the
development Mac, and the ASan and UBSan builds pass 427 each, without `no_paths`. The
build has zero warnings.

## Toolchain

A configure of a fresh copy of the tree downloaded `llvm-tools` and `clang` of release
`23.1.1-anti.3` from `anti-lang/llvm-tools` and checked both against the signed digests.
It cached `CMAKE_C_COMPILER` as the downloaded clang. That tree has no sysroot and no raylib,
which the README installs in two more steps, and 129 of its 414 tests failed for that
reason alone. With the sysroot and raylib of `build/`, it passed 416 of 416. `build/`
passed 416, and ASan and UBSan 415 each.

## What was built

1. The pass, `src/whole.c`. It runs after lowering and before the optimizer, in every
   release build, and in dev mode for the module that links. Lowering now writes a class
   record per class with its descriptor, base, tables, init function and `mutable`
   fields. A `worker fn` keeps its mark, and a call through a table names its class and
   slot. Library files carry all of it, format version 23. The class model lists the
   functions a table call may reach, across modules and through interface sub-objects.
2. Release devirtualisation. A table call becomes direct when every table it may read
   holds one function at its slot. Dev mode keeps it.
3. The registry, `reflect.new` and `Object.deserialize`. The registry is written when the
   program reaches one of its readers, `rt/registry.c`. `deserialize` reads the JSON of
   the default `serialize` back, `own` objects included.
4. The singleton check over the IR. It follows a worker into other modules and through
   tables, which the checker of one module cannot see. The checker keeps its own walk,
   which names lines.
5. Used-slot bitmaps, `anti_rt_slots`, for every abstract class a call reaches.

`docs/notes/whole-program.md` holds the choices inside the pass.

## Provisional decisions for review

New or changed in `docs/decisions.md`, in order:

- The entry on the checker's report walk now says the IR pass holds the singleton check
  as well. The `delete` in a worker and the abstract class report stay in the checker.
- The entry on direct calls now covers lowering alone. The release form is a new entry.
- `anti.reflect` has `new`, and still no `call` and no `Value`.
- The registry exists only when the program reaches a reader of it. A bundled library
  gets an empty one.
- `reflect.new` takes `Class` or `module.Class`, and gives null for an abstract class, a
  singleton, a `construct` with arguments and a name two modules share.
- `Object.deserialize` gives null rather than an error, because `anti.rt` cannot name
  `anti.error.Error`. It does not run a `construct` with arguments.
- Every abstract class counts as an injectable interface for the bitmaps until `inject`
  exists. A program with no slot reached carries no table.

## Defects found and not fixed

- In dev mode, `p is *m.Point` gives false for an object made in module `m`. The object
  of the module that links holds its own copy of every descriptor and table of other
  modules, as local symbols. Release mode gives true. Reproduce: a library with
  `pub fn make() -> *Point`, a program that tests `make() as *Object is *m.Point`, built
  with `--dev` for each module. The design comment in `class_global` says one descriptor
  per class. The same copies sit behind the registry in dev mode, so a registry entry
  names the copy.
- `inherits` takes one name, so a class cannot inherit a class of another module.
  `implements m.Interface` works.

## Also changed

- `--dev --dump-opt` of a module without `main` no longer runs the pass, as its build
  does not.
- A class record's sub-object tables were built by two calls in one argument list. They
  are bound to locals first, per the rule of `CLAUDE.md`.
- The emit identity manifest follows the new assembly, and `anti.error` numbers its
  literals one higher, from the init function of `Error`.

## Not done

- The Linux and Windows VMs did not run this work. `emit_identity` on Windows is the test
  that would catch an argument-order difference.
- `reflect.call` and `Value`, and with them the bitmap rule for `reflect.call`.
- A reader of `anti_rt_slots`, which is the plugin loader. Once plugins exist,
  devirtualisation must keep a call through an injectable interface indirect.

## Questions

- Should dev mode keep one copy of each descriptor and table, as release mode does? The
  fix would make them global hidden symbols of the module that declares them.
- Should `inherits` accept `module.Class`?
