# The pass over the whole program

Choices made inside the passes over the IR of the whole program. The file
`src/antic/whole.c` holds the class model and the singleton check. It also holds
devirtualisation, the merge of identical copies and the table facts. The file
`src/antic/whole_tables.c` writes the tables the runtime reads: the registry, the
default of the backtraces, the trampolines and the used-slot bitmaps. It also writes the
slots of injection and the table of what a plugin provides. They describe the inside of
the compiler. `docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

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
  fields of a singleton. It carries five flags: abstract, final, singleton, a
  `construct` with arguments and a required field, an inline class field that no
  default fills. The registry and the table of what a plugin provides write the last
  two as `ANTI_CLASS_ARGS` and `ANTI_CLASS_REQUIRED` of `src/rt/registry.h`.
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
- The root `anti.lang.Object` has no record. A call through a `*Object` reaches every
  table of the program.
- A primary table serves its class and every class above it. The table of a sub-object
  serves its interface and every class above that interface. A call at a slot reaches the
  entry at that slot of every table that serves its static class.

## The registry

- Lowering gives every complete class that is not a singleton an init function,
  `<Class>.init`. An export class keeps its C name `anti_<Class>_init`. The class record
  names it.
- The pass asks what the entries of the program reach, the way the optimizer's removal of
  unused functions marks them. When `anti_rt_reflect_new`,
  `anti_lang_Object_deserialize` or the plugin loader `anti_rt_plugin_load` is among
  them, `whole_reads_registry` says so and the pass writes `anti_rt_registry`. The loader
  adds the classes of a library to the registry. A dev build writes the registry
  whatever the reach says, since its objects carry every function of their modules, and
  so does a library for C with the runtime bundled. A plugin writes none and reads the
  registry of its host. The registry is an exported global with a count and an array of
  `anti.rt.Class` records. Each record holds the descriptor, the init function, the
  module path and the two flags of the class record.
- The module paths are byte globals of module `anti.rt`, one per module, named
  `registry.<n>`.
- `src/rt/registry.c` holds the two readers of reflection and every use of
  `anti_rt_registry`, and reads the classes of a loaded library through
  `anti_rt_plugin_registry` of `src/rt/loaded.c`. A program that reaches none of the
  three readers gets no registry and links no `registry.c`, so the symbol is never
  missing.
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
  plus one, which `slot_count` holds. The table itself is `anti_rt_slots`, an
  `anti.rt.SlotTable` of three items: a count, an array of `anti.rt.Slots` records of
  descriptor, slot count and bits, and the flag `reflect`, set for a program that calls
  `reflect.call`. Its byte globals are `slots.<n>` of module `anti.rt`.
- The plugin loader reads the table. Every program that is no library carries one, empty
  where its calls reach no slot, because the runtime archive puts the loader in every
  program. `docs/notes/versions.md` describes what the loader does with it.

## Devirtualisation

- `devirtualise` runs in release mode alone. A table call becomes direct when every table
  that serves its static class holds one function at the slot. The call then names that
  function and loses its signature and its table, and the optimizer removes the loads
  that computed the old target.
- Two sub-objects of one interface in two classes give two thunks, even when both reach
  one body. The call stays indirect.
- A call whose static class no concrete class serves keeps its table call.
- It does not run for a plugin, nor for a program that injects an interface or calls
  `plugin.load` and is not built with `--closed`. An object there may come from a class
  of a library the IR does not hold, and that class may replace any function of a class
  that is not final. `tests/dump/devirt_plugin.opt` pins the plugin side. The host of
  `tests/plugin/` and the program with an injected provider each hold a class of their
  own that implements `Greeter`, and print the library's words through it.

## The trampolines of reflect.call

- Lowering gives each record of a function list the text of its signature, a byte global
  of the module. The pass reads the texts from the function lists of the class records,
  so it sees the classes of every module.
- A trampoline is `anti.rt.trampoline.<text>`, and the declaration whose parameters its
  call follows is `anti.rt.signature.<text>`. It checks the count and the kind of every
  `Value`, reads each argument at the type of its parameter, narrowing from the width the
  `Value` holds, calls the entry and widens the result back into a `Value`.
- `src/rt/call.c` checks the index, the entry, the signature and the count before it calls the
  trampoline. A trampoline that refuses the call therefore found a wrong kind.
- A `str` argument passes the address of its bytes inside the `Value`, as lowering passes
  an aggregate.
- The layout of a `Value` is the aggregate `anti.reflect.Value`, so the trampolines hold
  symbolic sizes and offsets alone. A program without that aggregate gets an empty table.
- The table is `anti_rt_trampolines`: a count and an array of `anti.rt.Trampoline`
  records, each the text and the trampoline. The texts are byte globals `trampolines.<n>`
  of module `anti.rt`.
- In dev mode the optimizer keeps the functions of module `anti.rt` in the object that
  links, as it keeps that module's data.

## The merge of identical copies

- `merge_copies` runs in release mode, after devirtualisation. It keeps one of each set
  of copies of a generic whose code is identical, as `List<*Person>` and `List<*Order>`
  are. Every call and every constant that named another of the set names the one kept.
- An aggregate compares by its layout and a symbolic value by what it computes. A global
  compares as itself, or by its bytes where it is data without addresses. A call of
  another copy compares by the copy that stands for it, so the merge runs until nothing
  changes. Only copies merge, so a function the program wrote keeps its own address.

## The table facts

- `tables_written_once` decides `table_facts` of the module in release mode. It runs for
  a program that is no library and no plugin. A host of a plugin takes it only when built
  with `--closed`. The facts hold when no function writes tables, no union holds a class
  value and no class outside the standard library inherits `anti.mem.Allocator`. The
  DESIGN above the function gives the reasons, and "Tables and dispatch" in
  `docs/work-order-llvm-optimization.md` what the LLVM text then says.

## The default of the backtraces

- `whole_write_backtrace_default` writes `anti_rt_backtrace_default`, on in dev mode and
  off in release. It goes into a program that reaches the runtime's reader of it, which
  `whole_asks_backtrace` finds. A dev build gets it whatever the reach says, and so does a
  library for C with the runtime bundled. A plugin reads the one of its host. The reach is
  taken again after the slots of injection. A program that reaches the reader through a
  provider alone then gets the default as well.

## Injection and plugins

- `whole_write_injections` writes the slots of injection for every build that is no
  plugin, and `whole_write_provides` the table of what a plugin provides.
  `docs/notes/injection.md` and `docs/notes/plugins.md` describe both, and
  `docs/notes/versions.md` the chains and the slots a plugin records.
