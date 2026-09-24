# Decisions of fix step 20, antic front end

Entries a later step folds into `docs/decisions.md`.

- [provisional] The checker, the library reader and the layout pass
  format some messages into fixed buffers. A message that does not fit
  ends in `...` where it was cut. Reason: rule 9 asks for the check of
  every formatted text, and a mark is the smallest change that shows the
  cut without growing any buffer.
- [provisional] `antl_write` and `antl_write_header` return false when a
  count or an index of the library file does not fit in its 32 bits.
  antic then reports `antic: <source> is too large for a library file`
  and writes no file. Reason: rule 17 wants a refusal rather than a
  damaged file, and the format keeps its fixed width.
- [provisional] The reader uses some strings of a library file as C
  strings, such as a module path or a package name. Such a string with a
  NUL inside makes the file damaged. Reason: `strcmp` would compare the
  text before the NUL, so `foo\0x` would match `foo`.
- [provisional] `token_kind_name` gives the bare spelling of a symbol when
  the spelling in backticks does not fit its 16-byte cache entry. No
  spelling is that long today. Reason: a cut spelling would name another
  token.
