# The doc command

The choices inside `anti doc`. The decisions it follows are under "The doc
command" in `docs/decisions.md`, and the specification is "Documentation
generator" in `docs/tooling.md` with "Doc comment forms" and "Doc markup" in
`docs/tooling-addendum.md`.

## One structure, two inputs

`driver_interface` of `src/antic/driver.c` reads a source file or a library file and
gives back one `struct interface`. A source is lexed, parsed and checked, and
`sema_interface` builds the interface of the check. A library file is loaded
with its imports and the interface it carries is taken. The renderer sees the
same structure whichever of the two it was given, so the doc-equivalence test
of `docs/tooling.md` measures the library file and not the renderer.

The names of an interface built from a source point into the source bytes, so
`driver_interface` copies them into the memory pool before the lexer runs. The
pool outlives the call and the buffer does not.

Dev docs and `--private` need the private items and the `//#` notes, which the
interface never carries. They read the syntax tree that the same call gives
back, and they refuse a library file.

## The model between the two

A collector fills a list of `struct entry`, one per item, with the entries of a
body below it. Each entry holds the declaration as text, the anchor, the `///`
text and the `//#` text. Two collectors fill it: one from the interface and one
from the syntax tree. Two renderers read it, one per form, and the signature is
built once per entry.

A function of a class body takes its parameter names from the symbol where a
library file supplied them. A source supplies them on the item. Whether the
function takes `self` is computed and never guessed. The type carries one
parameter more than the declaration wrote, and both inputs carry the count of
the declaration.

## What the library file gained

The file carried the parameter names of the module-level functions alone, so a
page built from one wrote `a0` and `a1` for the members of a class. It now
carries the count the declaration wrote and one name per parameter, after the
doc text of the member. It carries the `worker` mark of a function in the flags
byte of an item. The format version was 51 then. It is 52 since the file
records the `link framework` lines of a module.

## The Markdown subset

The HTML renderer reads the subset of `docs/tooling-addendum.md` line by line.
A fence opens and closes a code block, and a blank line ends a paragraph or a
list. A line that opens with `- ` starts an item, and a line under an open item
continues it. Everything else is a paragraph. Inline, a backtick span becomes
`<code>` and `[text](url)` becomes `<a>`. A form outside the subset is written
as it stands, which is what the specification asks for.

`--markdown` writes the doc text unchanged. The headings, the fenced signatures
and the lists stand around it, so a Hugo site renders the subset with its own
renderer.

## The work directory

A module of a project that imports another needs that module's interface file.
The command writes one per source into the work directory, in the order the
imports ask for. That directory is a search root of the run. The list of
modules and the order are in `src/anti/units.c`, which `anti check` reads for
the same reason.
