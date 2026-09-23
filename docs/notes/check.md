# The check command

The choices inside `anti check`, its classes and the formatting rules it
reads. The rules are in "Check command" of `docs/tooling-addendum.md`, and
the settled points are in `docs/decisions.md` under "The check command".
`tools/anti/check.c` holds the classes and `tools/anti/format.c` the
formatting rules.

## The run

`check_run` reads the manifest of the project first, for the source and the
test directory of `[layout]` and the package name of `[package]`. Without a
file argument it walks both directories for `.anti` files, sorted by path,
so a run over a tree is the same run on every machine.

Every source is read once before any class runs. That pass gives the module
path, from `module_path_of_source` over the search roots, and the imports,
from the parser. A file the lexer or the parser refuses reports there and no
class below it sees the file, so no message is printed twice.

The search roots of every compile are the work directory first and then the
roots of `-I`, the source directory and the test directory. The work
directory holds the interface files this run wrote, so a module of the run
wins over one of the runtime archive.

## The front end

`antic --front-end` stops after semantic analysis. `links` of
`src/driver.c` answers false under it, so no link is demanded. A module
that imports nothing of the standard library then needs no runtime
archive.

The class passes `-c` with it, which writes the interface file of the
module into the work directory. `order_units` puts a module after every
module of the run that it imports, so the interface a module needs stands
there when the module is checked. A cycle among the imports keeps the order
the files came in, and the compiler reports the cycle.

`--doc-warnings` rides on the same call, so the doc warnings cost no second
pass over the sources. `report_diagnostics` of `src/driver.c` counts the
errors, the warnings of the checker and the doc warnings into the struct
the caller gives. The check reports one count per class.

`--targets all` adds a pass per target that is not the host. Those passes
pass no `-c`, because a library file is target-independent.

## The doc blocks

`collect_blocks` walks the text of a doc comment for a fence whose tag is
`anti`. `collect_item_blocks` reaches the two comments of every item, of
every field, of every case of a variant and of every member of a class
body.

A block of a user comment becomes a module of its own under
`<work>/check/docs/`, with `import <module>;` before it. A block that
writes that import itself does not get a second one.

A block of a developer comment becomes the module itself. The source of
the module carries the block after it and goes under `<work>/check/dev/`
at the path of the module. The module path of the copy is then the
module's own, and a private item is in reach.

`<work>/check/dev` is the first search root of a block compile. It holds
`.anti` files alone, so an import still finds the interface files of the
work directory.

A block without `fn main` is wrapped. The user block takes `main`. The
developer block takes the module path with `_` for every dot and
`_block_<n>` after it, because the module may define `main` already.

A failed block reports the module the check wrote, with the file and line
of the doc comment it came from. A reader then finds both ends.

## The doc warnings

`sema_doc_warnings` of `src/sema.c` holds them, and `diagnostics_doc`
marks each one, so the check tells them from the warnings of the checker.

A backtick name is one identifier or a path of identifiers, with an
optional `()` at the end. `doc_name_known` resolves it against the module
and its items, the members of those items and the parameters of the
documented item. The imports, the loaded libraries and the names the
compiler declares follow. `lexer_is_keyword` answers for every keyword and
every builtin type name, so `` `int` `` and `` `return` `` are language
words and not names.

The markup rules read one line at a time and skip the lines of a fenced
block. Each kind is reported once per comment. Emphasis needs text after
its opening delimiter and text before its closing one, which keeps a
pointer type and `a * b` in prose out of it.

The class reports and fails nothing. `docs/decisions.md` holds the reason
under "The check command".

## The formatting rules

`format_check` lexes the file and reads seven rules against the tokens and
the bytes.

`mark_code` marks the bytes of every token, so a rule passes over the
inside of a literal and of a doc comment. An ordinary comment is no token
at all. Its bytes are unmarked, so the rules pass over it as well. The
indent of a line is read only where the first byte after the whitespace
begins a token. That leaves the position of a comment alone.

The rules are the indent of tabs alone and no tab past the indent. One
statement stands per line. One tab stands per level, with one more for a
wrapped line. The parentheses around a whole condition are dropped.
`} else {` is one line, and so is the `} while` of a `do` block.

Three of them needed a reading that the rule as written does not give:

- The `;` of `[0; 8]` ends no statement. The rule counts the brackets and
  parentheses around it and reads a `;` at depth zero alone.
- `do {` opens a `do` block where the token before the `do` starts a
  statement, and it stands after the condition in `while cond do {`. Only
  the first takes a `while` on the line of its closing brace.
- The level of a line is the number of open braces before its first
  token. A line that starts with a closing brace stands one level out. A
  wrapped line takes one more tab, so the check takes the level or one
  above it and reports anything else.

The placement of a brace is no rule of the class. The canonical form keeps
the one-line body of `concrete fn joined(self, o: *Object) { }` and of
`pub enum Mode: u8 { Read, Write }`. A token stream does not say which of
the two forms an item asked for.

## What the classes report

The standard library of this checkout passes the front end, the doc blocks
and the formatting, and reports 36 doc warnings. Most of them are a
backtick around a name that no table of a module holds. Those are the name
of an environment variable, of a key of a configuration file or of a
function of the C library.

`tests/programs/simd.anti` aligns its wrapped lines with spaces under the
opening parenthesis, which the formatting class reports and
`tools/scripts/format_anti.py --check` reports as well.
