# Runtime decisions of the fix steps

Decisions the fix steps of `docs/audit/summary.md` took in the runtime.
A later step folds them into `docs/decisions.md`.

## Symbol and trace readers, step 10

- A section of type NOBITS has no bytes in the file. Every reader of the
  bytes of an ELF section refuses one, whatever its size says. The home
  section of a symbol may be one, since only its flags are read.
- [provisional] A step of the DWARF line register that leaves `int64_t`
  refuses the whole unit, so no row of it answers a lookup. Reason: the
  rows after the step are wrong, and the smallest rule is one answer per
  unit.
- [provisional] A DWARF 5 entry table with a count and no entry formats
  is refused. A unit whose directories are such a table gives no answer,
  and a file table of that form gives no file name. Reason: entries of no
  format take no bytes, so the count is no bound.
- The shift of a LEB128 number stops counting at 64. Bytes past the tenth
  add nothing to the value, as before.
- A build id of a loaded module is read only inside a readable `PT_LOAD`
  segment of the program headers that `dl_iterate_phdr` gives. No byte
  past the end of that segment is read, and outside one the id is empty.
- [provisional] A read error in `load` of `src/rt/trace.c` leaves the
  entry of the file without bytes for the life of the program. A file
  that does not open is kept the same way. Reason: the cache of files already keeps a
  failed open that way, and a second rule for one error is not needed.

## Integer bounds, step 11

- A count of bytes that the size of a text builder cannot hold is
  treated as memory that ran out. The builder keeps what it holds, and a
  pad or an append of that count writes nothing.
- The results of `parallel` take the bound of `anti_rt_chan_new`. A
  count of chunks times the result size past `INT64_MAX / 2` stops the
  program with a message on standard error, before a worker runs.
- [provisional] The `threads` key takes decimal digits naming a count
  from 1 to `INT32_MAX`, from the file, from `--anti.threads` and from
  `rt.configure`. Any other text is a startup error. Reason: the pool
  counts its workers in `int`, and the smallest bound that changes no
  count accepted before is the largest `int` of every target.
- A `[[name]]` table whose name holds the number `INT64_MAX` under it
  already has no next count, and the document is refused.
- A TOML key is at most 319 bytes, the room of a path less its NUL. A
  longer key was refused before by the length of the path it wrote, and
  it is now refused where it is read.
