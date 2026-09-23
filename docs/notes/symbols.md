# The symbols command

The choices inside `tools/anti/syms.c`. The rules are in "Symbols
tooling" of `docs/anti-language-additions.md`, and the settled points are
in `docs/decisions.md` under "The symbols command".

## Finding the binaries

The runtime configuration is read the way `rt/conf.c` reads it, with
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

## The zip reader

The reader finds the end record from the back of the file and walks the
central directory. The data of each entry follows its local header. Deflate is decoded the way zlib's `puff` does. The decoder
walks a table of counts per code length one bit at a time. Every entry is checked
against its CRC.
