# Decisions of the tool fixes

Decisions made by the fix steps of `docs/audit/summary.md` for the `anti`
tool. A later step folds them into `docs/decisions.md`.

## Step 14, repository inputs

- [provisional] A version of an index, a lock file or a library header is
  one to three parts of decimal digits joined by dots. Each part is at
  most nine digits. Nine digits fit a 32-bit `long`, so every target compares
  alike. A version or a constraint of another form satisfies nothing, and
  the comparison bounds every part, so no digit string overflows.
- [provisional] A package name and a module path that reach a path of
  the cache are lowercase ASCII identifiers joined by single dots. A
  keyword is not refused there, since it is no path. A digest is 64
  lowercase hex digits.
- [provisional] An index with one string not of its form is refused as
  a whole, with a message naming the string. That covers its versions,
  module paths, digests, dependency names and constraints.
- [provisional] A lock file that holds such a string is not read, with a
  message naming it. The build then resolves from the indexes and writes
  the lock again, as it does for a lock that no longer reads.
- [provisional] The TOML of `anti` reads no escape, so the lock file
  writes each value between double quotes as it is. A value that holds a
  double quote or a control byte is refused and no lock is written. A
  backslash stands for itself, so a Windows path reads back as written.
- [provisional] A fetched library file that fails its digest is removed
  from the cache, so the next build fetches it again.
- [provisional] A plain `http://` URL passes the loopback check when its
  authority holds no `@` and is `127.0.0.1`, `localhost` or `[::1]`,
  followed by nothing or by `:` and digits.
- [provisional] `anti sdk export` takes a `Version` of `SDKSettings.json`
  that is digits in parts joined by single dots, and refuses another.
- [provisional] `anti doc` refuses a library file whose module path is not
  lowercase identifiers joined by dots, and escapes every name it writes
  into HTML.

## Step 15, anti bind readers

- [provisional] An API description that names one struct twice is
  refused as a whole, with a message naming the struct.
- [provisional] The evaluator of macros and defines fails an expression
  nested deeper than 128 levels. A level is a prefix operator, a cast, a
  parenthesis or a macro a name leads into, counted across the nested
  evaluations. A failed macro is skipped with a warning, as before.
- [provisional] The C type parser does not parse a spelling nested
  deeper than 256 levels. A level is a pointer, an array, a parameter
  list, a group or a typedef a name leads into, counted across the nested
  parses. The field, parameter or function is then left out with a
  warning, as for another type Anti cannot name.
- [provisional] Both counts are global to their file, since a nested
  evaluation or parse starts in a callback of a reader. `anti bind` runs
  on one thread.
- [provisional] The ABI probe walks at most 64 records deep into a field
  and enters no record it stands in. It enters a record that held no
  value once. A field past that has no value in the probe.
- [provisional] A version of clang that does not fit an `int` reads as
  -1 and is refused as a version not tested. A `#pragma pack` value that
  does not fit an `int` leaves the pack as it was, as clang ignores it.

## Step 16, anti symbols and zip

- [provisional] `zip_read` frees everything it took when it fails and
  leaves the archive empty, so a caller frees nothing on that path. A
  caller that still calls `zip_archive_free` does no harm.
- [provisional] The deflate reader stops at the size the central
  directory declares for the entry, and the entry is broken. A copy may
  not reach back into bytes the output buffer held before the entry.
- [provisional] Each number in a line of a symbols map, and in a frame
  or `module` line of a trace, is digits alone. `0x` may lead a hex number,
  and no sign may. A number above 64 bits refuses the line, which is
  then passed over as another line that is no frame. Blanks may lead a
  line. A name, a location or an id has no length bound, where `sscanf`
  cut them at 511, 1023 and 79 bytes.
- [provisional] A unit of a deployment index with no `id` matches no
  entry of the archive.

## Step 17, anti files and `main`

- [provisional] `src/anti/files.c` holds the one file reader and the one
  writer of `anti`. `read_file` prints nothing, since for some callers a
  missing file is an answer. `read_file_reported` prints that the path
  cannot be read. `write_file` prints that the path cannot be written.
  A caller of either reporting form prints no second message.
- [provisional] A read that fails part of the way fails the whole read,
  and the text is left as it was before the call. A write fails when
  `fclose` fails. A file the writer could not finish stays on disk as
  written. `anti fmt` writes over a source in place, so removing it
  would lose the source.
- [provisional] The directory walk of `list_tree` and `list_dir` follows
  no link to a directory, as `remove_tree` follows none. A link to a
  file is listed as the file, and a link that leads nowhere is passed
  over. On Windows a directory that is a reparse point is not entered.
- [provisional] A directory that does not exist adds nothing to a walk.
  Every other failure to open or read a directory, or to read an entry
  of one, fails the walk and prints the path.
