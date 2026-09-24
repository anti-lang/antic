# Decisions of the minor runtime fixes

Decisions of fix step 20 for `src/rt/`. A later step folds them into
`docs/decisions.md`.

## The minor findings of the runtime, step 20

- [provisional] `anti_rt_entry_body` and `anti_rt_body_entry` of
  `src/rt/object.c` are the one place the runtime converts a table entry
  between an object pointer and a function pointer, through a union.
  Every other file calls them. Reason: rule 1 keeps the extension in one
  file, and `object.c` owns the tables. A file of its own would change
  the list of runtime sources outside `src/rt/`.
- [provisional] The failure routine of the runtime is
  `anti_rt_fail_exit(status, format, ...)` and
  `anti_rt_fail_abort(format, ...)` in `src/rt/assert.c`, declared in
  `std.h`. Each flushes standard output, writes the text and a newline
  to standard error and ends the program. `anti_rt_note` writes a line
  in the same form and goes on, for the library that discovery passes
  over. Every message keeps the text it had, `anti: ` included where it
  stood before. Reason: rule 13 asks for one routine with an exit and an
  abort form, and the decision on `assert` names the file.
- [provisional] A value of the configuration file with a NUL byte inside
  it ends the program with status 70 and the position of the value.
  `rt.configure` refuses a path with a NUL byte inside it the same way.
  Reason: every value reaches a C string, and rule 14 refuses input the
  runtime does not understand. A cut value would name another file.
- [provisional] The reason of a failed plugin load, and the name of a
  function in it, end in `...` when they do not fit their buffer. Reason:
  rule 9 asks for a check, and a reason cut short without a mark reads
  as whole.
- [provisional] The start flag of `init.c` is atomic, since a static
  library for C sets it on first use from any thread of the host.
  Reason: rule 23.
