# The symbols command

The choices inside `src/anti/syms.c`. The rules are in "Symbols
tooling" of `docs/anti-language-additions.md`, and the settled points are
in `docs/decisions.md` under "The symbols command".

## Finding the binaries

The runtime configuration is read the way `src/rt/conf.c` reads it, with
`anti_rt_toml_read`. The includes of a file come before its own keys and
resolve against the directory of the file that names them. A file that
sets `plugins` replaces what an include gave, and a line of
`[injections]` replaces the line of the same interface. A cycle of
includes, or a chain deeper than thirty-two, ends the command.

The program is found in the directory of the configuration. A Mach-O
file says in its header whether it is an executable. ELF writes a static
program of musl as a shared object, so the name decides there: a file
without a suffix is a program. Every binary is known by the build id of
its notice, and a file without one is reported and counted as missing.

## Matching an archive

The id of an archive of one build is the id of its debug link, whose
notice carries the id of the plain link. An archive without a link takes
the id from the `# build` line of its map. An archive of a deployment
names the id of each directory in its index.

## Resolving a frame

A trace gives each frame as an offset into its module. An ELF link
starts at 0, so the offset is the address the tables name. A Mach-O
link starts at the address of `__TEXT`, which the load commands of the
debug link give. The lookup takes the byte before the return address,
which lies in the call and names its line, as `symbolize` does.

## Reading an escaped name

A symbol of a copy of a generic escapes each byte an assembler cannot read
as `$` and two lowercase hex digits, so `app.List<int>.push` is the symbol
`app.List$3cint$3e.push`. `anti_rt_symbol_unescape` of `src/rt/symbols.c`
reads it back, and `symbolize`, `resolve` and the map all call it. It reads
a name back only where the name is an escaped form without doubt:

- every byte is a letter, a digit, `_`, `.` or `$`;
- every `$` stands before two lowercase hex digits;
- the byte those digits give is one the escape writes, never a letter, a
  digit, `_`, `.` or 0;
- the name holds one escape at least, and a `.`.

antic escapes `$` itself as `$24`, so a `$` of an Anti name never stands
bare in a symbol. The escape then maps one name to one symbol, and the rules
above take back exactly those symbols. A C symbol holds no `.`, and one
with a bare `$` breaks the second rule, so either stays as it is.

The map writes the name a person reads, which may hold a blank, as
`Pair<int, str>.swap` does. A location ends a line and holds no blank, in
the form `<file>:<line>`. The reader of a map therefore takes the rest of
the line as the name, less a last word of that form.

## The zip reader

The reader finds the end record from the back of the file and walks the
central directory. The data of each entry follows its local header. Deflate is decoded the way zlib's `puff` does. The decoder
walks a table of counts per code length one bit at a time. Every entry is checked
against its CRC.
