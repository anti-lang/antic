# The pass over the whole program

Choices made inside the passes over the IR of the whole program, `src/whole.c`. They
describe the inside of the compiler. `docs/decisions.md` holds what a reader of the
language or a user of the tools can observe.

## Where the pass runs

- The driver runs the passes after lowering and before the optimizer. In release mode
  that is every compilation of a program. In dev mode it is the compilation of the module
  that links, which has `main`, and of a library for C. A dev object of any other module
  never links and skips them. `--dump-opt` runs them, and `--dump-ir` shows the IR before
  them.
- The program holds the IR of every module in both modes. A library file carries the IR
  of its module, and antic loads every library file that the program imports.

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
