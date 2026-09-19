# The pass over the whole program

Choices made inside the passes over the IR of the whole program, `src/whole.c`. They
describe the inside of the compiler. `docs/decisions.md` holds what a reader of the
language or a user of the tools can observe.

## Where the pass runs

- The driver runs the passes after lowering and before the optimizer. In release mode
  that is every compilation of a program. In dev mode it is the compilation of the module
  that links, which has `main`, and of a library for C. A dev object of any other module
  never links and skips them. `--dump-opt` runs them where the build would, and
  `--dump-ir` shows the IR before them.
- The program holds the IR of every module in both modes. A library file carries the IR
  of its module, and antic loads every library file that the program imports.
- In dev mode the pass reads the tables of other modules before the optimizer turns their
  data into external symbols. The data the pass writes belong to module `anti.rt` or to no
  module, so they stay in the object that links.

## What the IR carries for it

- Lowering writes a class record for each class a module declares. It names the
  descriptor, the base's descriptor, the primary table and the aggregate. It names the
  table of each interface sub-object with the interface's descriptor, and the `mutable`
  fields of a singleton. It carries four flags: abstract, final, singleton, and a
  `construct` with arguments.
- A `worker fn` keeps its mark on its IR function.
- A call through a table names the descriptor of its static class in operand `c` and the
  slot in `field`. The back ends read neither.
- Library files carry all three since version 23.
- The optimizer drops the class records when it removes unused globals, because the
  removal renumbers what they name.

## The class model

- A class is known by the module and the name of its descriptor. A module that names a
  class of another module holds an extern global of its own for that descriptor. An
  index therefore never identifies a class.
- The root `anti.rt.Object` has no record. A call through a `*Object` reaches every table
  of the program.
- A primary table serves its class and every class above it. The table of a sub-object
  serves its interface and every class above that interface. A call at a slot reaches the
  entry at that slot of every table that serves its static class.

## Devirtualisation

- Release mode rewrites a call when every served table holds one function at the slot.
  The call then names that function and loses its signature and its table. The optimizer
  removes the loads that computed the old target.
- Two sub-objects of one interface in two classes give two thunks, even when both reach
  one body. The call stays indirect.
- A call whose static class no concrete class serves keeps its table call.

## The registry

- Lowering gives every complete class that is not a singleton an init function,
  `<Class>.init`. An export class keeps its C name `anti_<Class>_init`. The class record
  names it.
- The pass asks what the entries of the program reach, the way the optimizer's removal of
  unused functions marks them. When `anti_rt_reflect_new` or `anti_rt_Object_deserialize`
  is among them, it writes `anti_rt_registry`. The registry is an exported global with
  a count and an array of `anti.rt.Class` records. Each record holds the descriptor, the
  init function, the module path and a flag for a `construct` with arguments.
- The module paths are byte globals of module `anti.rt`, one per module, named
  `registry.<n>`.
- `rt/registry.c` holds both readers and nothing else. A program that reaches neither does
  not link it, so the registry symbol is never missing. A bundled runtime holds every
  file, so a library for C with the runtime bundled gets an empty registry.
- `Object.deserialize` is the eighth member of the root. It is not `pub`, which keeps it
  out of every table and every header, and its visibility level is `pub`, which lets any
  module call it.

## The singleton check

- Lowering reaches a field at a symbolic offset, `offset_of Agg.field`, and a later field
  of a class always has one, because field 0 is the base. The check matches that offset,
  or a symbolic operation built from it, against the `mutable` fields of the class
  records.
- The walk starts at each function marked `worker` and follows direct calls and table
  calls. A table call reaches every entry the class model lists for its slot.
- The pass runs before the optimizer, whose folding could combine offsets.
- A report is one line per field and worker, printed by the driver as
  `<file>: error: <message>`.

## The used-slot bitmaps

- The pass walks the functions that the entries reach and takes every table call there,
  before devirtualisation. The slot of the call is set in the bitmap of every abstract
  class at or below its static class. A call through the root sets it in all of them.
- A bitmap has bit k of byte k / 8 for slot k, and its length is the highest slot reached,
  plus one, which `slot_count` holds. The table itself is `anti_rt_slots`, a count and an
  array of `anti.rt.Slots` records: descriptor, slot count, bits. Its byte globals are
  `slots.<n>` of module `anti.rt`.
- Nothing reads the table yet. The plugin loader will, and so will a devirtualisation that
  must keep a call through an injectable interface indirect once plugins exist.

## The trampolines of reflect.call

- Lowering gives each record of a function list the text of its signature, a byte global
  of the module. The pass reads the texts from the function lists of the class records,
  so it sees the classes of every module.
- A trampoline is `anti.rt.trampoline.<text>`, and the declaration whose parameters its
  call follows is `anti.rt.signature.<text>`. It checks the count and the kind of every
  `Value`, reads each argument at the type of its parameter, narrowing from the width the
  `Value` holds, calls the entry and widens the result back into a `Value`.
- A `str` argument passes the address of its bytes inside the `Value`, as lowering passes
  an aggregate.
- The layout of a `Value` is the aggregate `anti.reflect.Value`, so the trampolines hold
  symbolic sizes and offsets alone. A program without that aggregate gets an empty table.
- The table is `anti_rt_trampolines`: a count and an array of `anti.rt.Trampoline`
  records, each the text and the trampoline. The texts are byte globals `trampolines.<n>`
  of module `anti.rt`.
- In dev mode the optimizer keeps the functions of module `anti.rt` in the object that
  links, as it keeps that module's data.
