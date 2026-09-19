# The six Windows tests

The Windows VM passes 351 of 351. The six tests that failed there alone had five causes.
One was in the test scripts, one in the runtime, one in `anti.log`, and two in the tests.

## Causes and fixes

| Test | Cause | Fix |
|---|---|---|
| `program_args`, `std_text` | CMake decoded the output through the console code page | The raw-bytes rule, below |
| `program_abi_wchar` | The expected values assumed a `wchar_t` of 32 bits | Expected files per type of `wchar_t` |
| `std_error` | The test expected code 0 from `from_win32` on Windows | `error.win32.expected` holds code 3 |
| `std_log` | The test called `setenv`, which the C runtime of Windows lacks | The test names `ANTI_LOGGER` in its environment |
| `std_log` | The file sink opened its file in text mode and wrote CRLF | The file sink opens it with `"ab"` |
| `std_signals` | `raise` reaches the table of the C runtime, whose default exits with 3 | A C runtime handler beside the console handler |

## The raw-bytes rule

- `rt/start.c` puts stdout and stderr into binary mode on Windows before `main`. Every
  program there wrote CRLF for each LF until now.
- `tests/program_output.cmake` reads the output of a program through `OUTPUT_FILE`.
  `run_program.cmake` and `run_classlib.cmake` compare those bytes. An `OUTPUT_VARIABLE` of
  CMake drops the CR of each CRLF and every NUL on every host, which hid the CRLF.
- Every other call that captures output names `ENCODING NONE`.
- `raw_output` runs `cmake -E cat` and `tests/raw/raw_bytes.anti` through the scripts. Both
  write a CR, an LF and a NUL. On Windows without the runtime fix it read `0d0d0a`.
- `decisions.md` records the rule without a tag. The two lines of the logger and the
  signal handler lost theirs.

## Commits

`b367639` adds the context rules to `CLAUDE.md`. `f54a871`, `0b7c5d7`, `3797312`,
`907f636` and `a45cd93` are the fixes. Each passed the whole suite on the Mac before it was
committed.

## Results

| Host | Suite | ASan | UBSan |
|---|---|---|---|
| Mac | 416 of 416 | 415 of 415 | 415 of 415 |
| Linux VM | 366 of 366 | 365 of 365 | 365 of 365 |
| Windows VM | 351 of 351 | none | none |

## Findings

- A tree extracted on the VM keeps the times of the Mac. Ninja kept objects of an older
  `lower.c`, and four tests failed until `tar -xmf` forced a new build.
- My first commit of the context rules also took two renames that `git mv` had staged.
  It was undone before any push and committed again alone.

## Questions

- `run_probe` and `run_clib` still read through `OUTPUT_VARIABLE`. They compare the output
  of C programs, whose stdout stays in text mode on Windows. Do they follow the rule too,
  with binary mode in each C file?
