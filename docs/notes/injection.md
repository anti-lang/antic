# Injection

The choices inside the parser, the checker, lowering and the pass over
the whole program that carry `inject`. The rules are in "Injection" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md` under the same name.

## The field

`inject` and `inject final` stand before the name of a field of a class
body, and `src/parser.c` reads them as contextual words. The `final`
form is read first, because its second word is a name as well: the field
`inject final: *L` is called `final`, and the field `inject: int` is
called `inject`.

The checker refuses a type that is no `*Interface` of an abstract class.
It refuses a default beside the marker, `own` on the field and a literal
that names it. A literal that leaves the field out is complete. `struct
struct_field` carries the two marks, and the library file carries them
from version 48. A module that builds a class of another module then
fills the field the same way.

## The slot

One global per interface holds the provider. It belongs to the runtime
module and is named `inject.` and the path of the interface. The symbol
of the slot of `com.example.app.Logger` is therefore
`anti.rt.inject.com.example.app.Logger`. The module matters. A dev build
drops the data of every module but the one it compiles and the runtime
module. The pass writes the slot into the object that links.

Lowering writes the slot as an extern global wherever a module injects.
It fills the field where it writes the defaults, in a literal and in the
init function of the class. The code is one load of the slot and one
indirect call of `fn() -> *Interface`.

## The pass

`write_injections` of `src/whole.c` runs where the program is whole.
That is every release build, and in dev mode the compilation of the
module that links. The program holds the IR of every module in both
modes, so the class records of every module are there. Each carries the
`inject` fields its class declares.

The pass collects one entry per interface, finds the slot, resolves the
provider and writes the value of the slot. `--inject Interface=Provider`
carries the table of the build. The provider's path is split at each
dot, and the module and the name that answer name a function of the
program. A path that names no function names a class, and `get` is
appended to it, which is the second form of the specification. Where the
name is `C.f` and the module holds a class `C`, the class must be the
interface, inherit it or implement it. An interface it
implements is a sub-object at a field, so the pass writes a thunk that
calls the provider and moves the pointer by the offset of that field.
`struct ir_subtable` carries the aggregate and the index of the field
for it, which is why the IR needs neither the types nor a size.

The pass reports the link-time errors of the specification. An interface
with no provider names the class and the field that need one. A provider
that names no function of the program is refused, and so is one that two
functions answer. A provider that takes arguments or gives no pointer is
refused. So is a provider whose class is no such interface.

## The default provider

An interface carries its own provider in a static function `default` of
it. `resolve_provider` reads the build's table first, and where that
names none it resolves `<interface>.default` instead. The name is a path
like any other, so the same split at each dot finds the static function
of the class. The message of an interface with neither names both ways
to give one.

The six standard interfaces are written that way. `anti.mem.Allocator`
gives `LibcAllocator.get()`, `anti.log.Logger` the logger of the
program, `anti.time.Clock` the `SystemClock`, `anti.random.Source` the
`SharedRandom`, `anti.fs.FileSystem` the `SystemFileSystem` and
`anti.config.Config` the `FileConfig`. A library under its own root does
the same.

The provider graph follows the default as it follows any other provider.
A default that reads another interface's slot makes an edge, and a cycle
through the defaults is refused.

## The cycle

The provider graph has one node per interface. The pass walks the
functions a provider may run, following the direct calls alone, and
marks every global those functions read. An edge stands from one
interface to another where the walk from the first reads the slot of the
second. A depth-first search over the graph reports the first cycle it
closes, and the message names the interfaces from where the cycle begins.

A call through a table names no function in the IR and is not followed.
The graph therefore misses a cycle that runs through a dispatch, and it
never refuses a program that has none.

## What the runtime reads

`anti_rt_injectable` names every interface the program injects, with the
class and the field that need it and whether it is final. Every program
carries it, empty where nothing injects, because `rt/conf.c` reads it
before `main`. A `[injections]` line and a `--anti.inject` that name an
interface the program has not are a startup error naming the ones it
does have. One that names an `inject final` field says so. One that
passes both is a startup error while plugins are not built, because the
value is the path of a library that nothing can load.

`--anti.inspect` prints one line per interface, its path and the class,
the field and `final` in brackets.

## The manifest

`anti test` reads `anti.toml` of the directory it runs in with the
runtime's TOML reader, the one parser `anti.toml`, the logger and the
runtime configuration share. It takes the `[inject]` table and lays
`[inject.test]` over it, per interface, and passes each entry to every
compilation of the run. `tools/anti/manifest.c` holds it. The reader
gives a flat list of key paths, so `[inject.test]` arrives as keys under
`inject.test.`, which is what TOML makes of the nested table.
