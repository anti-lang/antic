# Interface versioning

The choices inside the parser, the checker, lowering, the pass over the
whole program and the loader that carry a version of an interface. The
rules are in "Versions" of `docs/anti-language-additions.md`. The
settled points are in `docs/decisions.md` under the same name. `docs/notes/plugins.md` holds
the rest of the loader.

## What a descriptor carries

`descriptor_agg` of `src/antic/lower.c` gives every class descriptor three
fields after the function list. They are the bytes of the version of the
package that declared the class, its length, and a record. The record
belongs to an abstract class and is NULL for every other. The record is `anti.rt.Versions`, with the
chain, its length, the floor and its length.

The module that declares a class writes its descriptor, and every other
module refers to the one it wrote. The version of the build is therefore
the class's own, and `version_global` holds it once per module. That
global has a name rather than a number, so adding it moves no literal.

A struct descriptor carries the version as well and points at no record.
The root `anti.lang.Object` is the runtime's own literal in
`src/rt/object.c`, with no version and no record.

## The chain

`class_chain` walks `table_of`, which gives the entries of the table in
order: the sixteen of the root first, then the public functions of the
chain. It rolls an FNV-1a over each entry's name, a colon, its signature
and a semicolon. An entry whose signature no `reflect.Value` carries
adds the count of its parameters in place of the text. `signature_code`
builds that text, and `signature_text` interns it as before.

The hash after k entries is entry k of the chain, so the array is one
hash per prefix and holds the empty prefix first. It is an array of
`i64` constants and not bytes, so no target's byte order reaches it.

## `compatible`

`compatible_line` of `src/antic/parser.c` reads the contextual word where a
field of a class body stands. The version is the source span of the
number the lexer read, with any further `.<integer>` parts after it,
because `1.1.0` is no number of Anti. The checker refuses the line on a
class that is not abstract, `struct type` carries it, and the library
file carries it from format version 50, so every module writes the same
descriptor.

## What a plugin records

`write_provides` of `src/antic/whole.c` reads the descriptor of each provided
interface out of the IR, which a library file brought. It copies four
things into the library's own image: the chain, the field count, the
size and the version of the interface's package. A reference to the
interface's own globals would resolve against the host at load. It would
then give the host's numbers, which are what the check compares against.
The size stays a symbolic `size_of`, so the back end lays it out per
target.

## The bitmaps

`write_slots` keeps the slots the program's calls reach and adds the flag
`reflect` to `anti_rt_slots` for a program that calls `reflect.call`. The
two kinds of reach are apart because a reached slot the library lacks is
a refusal and a slot only reflection may reach is a stub. Every program
carries the table, empty where its calls reach none. The reader of the
configuration file calls the loader, so every link holds it.

## The checks

`checked` of `src/rt/plugin.c` runs per provided interface, after the runtime
version and the image of the interface. It compares the two chains at the
length of the shorter. It refuses a version below the floor, and a field
count or a size that differs. It then walks the bitmap for a slot
past the last the library carries. The message of that one names the
function, which the function list of the host's descriptor gives by
slot.

`stubs_of` writes a table for the sub-object of an object the loader
builds. The library's entries come first and a stub fills every slot
above them. `build` copies the library's entries into it on the first
object and points the sub-object at it. The stub reads the table back
through the object it was called with. It steps to the header before the
table and names the class the library provides and its version.
