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

## Shared state, step 12

- The end of a chunk of `parallel` or of a dispatched job wakes every
  waiter of the pool. Each waiter checks its own condition.
- An atomic load is sequentially consistent on every target. clang,
  which compiles each runtime of the archive, uses the load of the
  builtins on Windows as elsewhere.
- [provisional] MSVC compiles `src/rt/atomic.c` into the tools of a
  Windows host. On ARM64 it loads with a plain load and a full barrier
  after it, as its own C++ library does. Reason: MSVC has no builtin of
  clang, and no MSVC build ran here.
- One lock guards the signal table, the start of the reader and the
  pipe. A reader calls the registered function after it releases the
  lock. A pipe whose reader thread did not start is closed, so a later
  registration starts afresh.
- [provisional] A later layer or include may replace the value of a
  configuration key. The value it replaces is kept until the program
  ends, never freed. Reason: `anti_rt_conf_get` hands out the bytes of
  a value to any thread. Of the two fixes the audit gives, this one
  keeps `rt.configure` usable after threads start.
- A second call of `rt.configure` returns at once while the first one
  reads its file, as it did after the first one ended.
- One lock in `src/rt/loaded.c` guards the slots of the open libraries.
  Load and unload write a slot under it. The two hooks that count the
  objects of a library read the slots under it, and so does the lookup
  of a class by name. The library is opened and closed outside it.
- A lookup by name may find a class of a library. The class stays
  valid while the program keeps that library open, as the handle of
  the library does.
- [provisional] The reason of a failed call of the loader is kept per
  thread. Reason: of the two fixes the audit gives, it changes no
  signature that `anti.plugin` calls.
