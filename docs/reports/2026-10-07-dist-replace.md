# Programs in dist/ replaced on every host

Item 2 of the session that closes the move to LLVM. `anti build` now writes
`<program>.new` beside a program in `dist/` on the Mac and on Linux as well, with the
execute bits of the program it copies, and renames it over the program. Windows did so
since `ef715b7c`.

## The test first

`anti_build_running` builds `tests/anti-build/running` with `anti build`, then runs a
pipeline of two commands at once: the program from `dist/`, which writes `started` and
waits for `stop`, and a second run of the script, which waits for `started`, changes the
result of the program from 7 to 8 and builds again. Then it checks four things: the
build passed, the running program returned 7, no `<program>.new` is left, and the
program in `dist/` now returns 8.

Before the change, on anti-linux:

```text
the build while the program ran failed with 1
anti: cannot write ./dist/linux-arm64/dev/running
```

On the Mac the test passed before the change, since macOS lets a program that runs be
written in place. The rename still keeps a failed copy from leaving a broken program,
which no test provokes on the Mac.

## The change

- `files_copy_program` of `src/anti/files.c` takes one path on every host: the copy to
  `<to>.new`, the permissions of the source, then `platform_replace_program`. A copy
  that fails removes its own file and leaves the program whole.
- `platform_program_replaced` is gone. On POSIX `platform_replace_program` is the
  `rename` it was.
- The `EACCES` wait of `platform_open` in `src/antic/platform.c` is unchanged.
- The entry of the copy into `dist/` in `docs/decisions.md` says that the Mac and Linux
  take the same path, which Eddie decided on 2026-10-07.

After the change `anti_build_running`, `anti_build`, `anti_build_rerun` where it runs and
`unit_files` passed on the Mac and on anti-linux.

## Gates

| Machine | Suite | Result | Time |
|---|---|---|---:|
| Mac | host | 1641 of 1641 | 232 s |
| Mac | asan | 1640 of 1640 | 462 s |
| Mac | ubsan | 1640 of 1640 | 312 s |

The three builds print no warning. The item needs no VM suite, and anti-linux ran the six
tests named above.
