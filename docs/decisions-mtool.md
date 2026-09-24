# Decisions of the minor tool fixes

Decisions made by fix step 20 of `docs/audit/summary.md` for the minor
findings of `src/anti/`. A later step folds them into
`docs/decisions.md`.

## Step 20, the anti tool

- [provisional] The functions of `src/anti/files.c` carry the prefix
  `files_`, the name of the module, as `units.c` carries `unit_`. The
  module `jsontree` keeps `json_`, which all its names share.
- [provisional] Every allocation of `anti` goes through `files_array`,
  `files_resize` or `files_grow` of `files.c`. A product that overflows
  and a failed allocation both end the tool through
  `files_out_of_memory` with status 70, as the ten copies did before. A
  command of `main.c` now exits there instead of returning 70. The JSON
  reader of `anti bind` keeps its own growth, since it reports a failed
  allocation as an error of the document.
- [provisional] `files_base_name` takes `/` and `\` as separators on every
  host, as the copy in `syms.c` did. A path that names a Windows
  directory splits the same way on the machine that reads it.
- [provisional] `anti symbols` decides whether a path of a runtime
  configuration is absolute by the rules of the host, through
  `path_is_absolute` of `src/antic/selfpath.c`. A drive path such as
  `C:\x` read on macOS or Linux is relative there.
- [provisional] `anti bind` refuses a literal that no 64-bit integer, no
  double or, with the suffix `f`, no float holds, where it took the
  saturated value. The define or macro is skipped with the usual
  warning. A float literal below the smallest double reads as zero or
  nearly so, as C reads it.
- [provisional] An array length of a C type past `INT64_MAX` does not
  parse, and a field of a `COLOR` define past a `long long` skips the
  define.
- [provisional] A message of the JSON reader of `anti bind` that the
  caller's buffer cannot hold is cut and ends in `...`.
- [provisional] `zip_write` refuses a name above 65535 bytes, a size or
  an offset above 4 GiB and more than 65535 entries, and writes no file.
  The zip64 extension is not written.
