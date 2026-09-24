# Plugins

The choices inside the parser, the checker, the pass over the whole
program, the linker and the runtime that carry a loaded library. The
rules are in "Plugins" of `docs/anti-language-additions.md`, and the
settled points are in `docs/decisions.md` under the same name.

## The line

`provides Interface as Class;` stands at module level. `src/antic/parser.c`
reads `provides` as a keyword and the interface as one dotted path. The
last name is the class and the names before it are its module, written
as the alias an `import` declared or as the whole module path.
`sema_interface_named` of `src/antic/sema_call.c` resolves both
spellings, and `lib.instance(I)` and `lib.supports(I, "f")` read the
same form through `name_path`.

The checker refuses an interface that is not abstract and a class the
module does not declare. Refused as well are an abstract class, a
singleton, a class that neither inherits the interface nor implements
it, and two lines for one interface. `class_record` of `src/antic/lower.c`
writes one entry per line into the record of the class, and the library
file carries them from format version 49, and `compatible` from 50.

## The table

`write_provides` of `src/antic/whole.c` runs where the build writes a plugin,
which is `--lib shared --no-runtime`. It writes `anti_rt_provides`, an
entry per `provides` line with the path of the interface, the
descriptor of the interface and of the class, the function that
prepares an object and the offset of the interface sub-object. The
offset is the symbolic `offset_of` that the injection pass already
computes, so the IR holds no size. The table carries the version of the
runtime the library was built against and the classes of the library.

The pass writes nothing else for a plugin. The registry, the default of
the backtraces and the slots of the injectable interfaces belong to the
host.

## The build of a library

`--no-runtime` makes the driver emit the object of the module alone, as
a dev build does, and `ir_optimize_module` drops the bodies and the data
of every other module. The link passes no runtime library and leaves
every undefined name to the loader. The `provides` lines reach the index
through `struct extras`, because the optimizer drops the class records
that carry them.

`write_plugin_index` writes `anti-plugins.toml` beside the library. It
keeps the `[[library]]` entries of the other libraries of the directory
and replaces the one of the library it wrote.

The driver sets `plugin` of the IR module. On ELF and COFF
`select_uses_got` then gives every function and every datum of another
module the address in an entry: the GOT on ELF, and on COFF the
`__imp_` pointer the import library of the host defines. ld.lld refuses
a pc-relative reference to a name a shared object leaves undefined, and
a COFF image has no GOT. Mach-O keeps the direct references that
`-undefined dynamic_lookup` binds.

The data of a COFF plugin cannot load an entry. An address of a name of
the host in it holds the address of the `__imp_` entry instead, and
`emit_imports` lists each such place in `anti_rt_imports`. The loader
of `src/rt/platform_windows.c` replaces each with the address its entry
holds before `plugin.c` reads the table. A state word keeps a second
open of the same library from doing it twice. The `.def` file of a
Windows plugin exports `anti_rt_provides` and `anti_rt_imports`, and the
link takes `/NOENTRY`, the import library of the host, the vcruntime
and the UCRT, whose heap the host shares.

## The host

A program that injects an interface or calls `anti_rt_plugin_load` is a
host. `whole_hosts_plugins` says so, and the driver reads it before the
optimizer, for the same reason. The emitter then makes every function
and every datum global and marks none hidden. The link passes
`-export_dynamic` on macOS. On Linux it links in the glibc mode with
`--export-dynamic`, since a static program of musl has no loader.
`--closed` turns all of it off.

A Windows host writes `<program>.def` and the link writes the import
library `<program>.lib` beside it. `emit_names` lists the names of the
object, a datum with `DATA`. `coff_archive_exports` of
`src/antic/coff.c` adds the names of the runtime members the link takes.
It reads the symbol index of the archive and follows the undefined names
of the object and `main` through the members, as the linker does.

The runtime is compiled without hidden visibility, so its symbols are
there to resolve as well. A shared library for C keeps the surface that
`docs/libraries-for-c.md` gives through the export list of its link.

## The two calls

`lib.instance(I)` and `lib.supports(I, "f")` name an interface where a
value stands. `sema_check_call` of `src/antic/sema_call.c` sees a
receiver of type `*anti.plugin.Library`. It resolves the path of the
first argument to an abstract class and replaces that argument with an
`EXPR_DESCRIPTOR` node. It renames the call to `instance_at` or
`supports_at`, which are ordinary functions of the class. The method
call carries it from there. `instance` has the type `?*I`, which
`catch fatal` narrows.

`EXPR_DESCRIPTOR` lowers to the address of the class descriptor, and no
source text writes one.

## The loader

`src/rt/plugin.c` opens the library, reads `anti_rt_provides` and checks it.
The runtime version must match the host's exactly. Every interface
descriptor must lie in the host's image. That proves the library was
bound against the host rather than carrying an interface of its own.
The version checks of each interface follow, in `docs/notes/versions.md`.
The classes of the library then join the host's registry, so
`reflect.new` finds one by name.

`instance` allocates the size the class descriptor gives, calls the
function that prepares an object and moves the pointer by the offset of
the sub-object. A class whose `construct` takes arguments is refused
there, because the host has none to give.

`src/rt/loaded.c` holds the slots of the open libraries, the count of them
and the two hooks that count their objects. It stands apart from the
loader, because every hook site of every program reaches it. The owner
of an object is the image the table of its class lies in. `dladdr` and
`GetModuleHandleEx` give it.

## Discovery and the providers

`plugin:<path>` and `discover` in the `[inject]` table of the manifest
name a library rather than a function of the program. The slot is empty
at the link, and the runtime fills it before `main`.

A slot holds a function and a provider from a library is an object.
Every injectable interface therefore carries one holder and one thunk
that gives what the holder holds. The runtime stores the object in the
holder and puts the thunk in the slot. `[injections]` of the configuration file and
`--anti.inject` replace a provider the same way, the command line over
the file and both over the build.

Discovery reads the `anti-plugins.toml` of each directory of the
`plugins` key, in order. It opens no library to find out what is inside
one. A library built for another runtime is logged and passed over. One
whose bytes do not match the digest of the index is refused. Two
libraries that provide one interface are an error naming both.
