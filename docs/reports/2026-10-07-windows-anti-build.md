# `anti build` and a program that ran a moment before on Windows

`anti build` on anti-windows failed now and then with `anti: cannot write
./dist/windows-arm64/dev/app.exe` right after the program ran. The cause is Smart App
Control, and the fix is in `ef715b7c`.

## The error

A scratch build of anti logged `errno`, `_doserrno` and `GetLastError` at the failing
`_wfsopen(..., L"wb", _SH_DENYNO)` of `files_copy`, and asked `RmGetList` of the Restart
Manager for the holders of the file.

- The error is `ERROR_USER_MAPPED_FILE`, 1224: the file has a mapped section, and Windows
  refuses to truncate it. The C runtime gives it as `errno` 22, `EINVAL`.
- The write goes through `platform_open`, but its wait never ran. It retries on `EACCES`
  alone, and every failure logged `tries 1`. No `EACCES` was seen in this session.
- The release copy into `dist/windows-arm64/release/` met it once as well.

## Who holds the file, and for how long

`RmGetList` named the same three holders at every failure: `CryptSvc` (Cryptographic
Services), `AppIDSvc` (Application Identity) and the `applockerfltr` driver, pid 0. The VM
has no AppLocker rule. Its Smart App Control is in evaluation mode,
`VerifiedAndReputablePolicyState` 2, which is the default of a new Windows 11 install. It
checks a new program when it first runs. Defender's real-time protection is on and was
never named. Neither was a handle of CTest or of the program.

A scratch build that waited on any error measured the hold from the first failure. In 25
holds the file opened again after 235 to 1563 ms, 22 of them between 1.1 and 1.6 s.

Two more facts came from scratch builds while a holder held the file:

- `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` of a new file over it failed with
  `ERROR_ACCESS_DENIED`.
- A rename with `FILE_RENAME_FLAG_POSIX_SEMANTICS` replaced it at once in most cases. In
  the others it also failed with `ERROR_ACCESS_DENIED`, for 578 ms in the one case timed.
  With that rename alone the hand loop below still failed 33 of 150 turns. In 3 of 3 such
  cases the program could still be renamed aside and deleted. A unit test reproduces that second case with a
  section mapped as an image (`SEC_IMAGE`), and the first one with a section mapped as data.

## The fix

The DESIGN comments of `files_copy_program` in `src/anti/files.c` and of
`platform_replace_program` in `src/anti/platform.c` hold the reasoning.

- On Windows `files_copy_program` writes `<program>.new` beside the program and puts it in
  place with `platform_replace_program`. A copy that fails removes its own file and leaves
  the old program whole, which the copy in place did not guarantee. `copy_into` of
  `src/anti/build.c` still prints `cannot write`.
- `platform_replace_program` renames with the POSIX semantics of NTFS. On
  `ERROR_ACCESS_DENIED` it moves the old program to `<program>.old`, renames the new one
  into place and deletes the old one. A delete that Windows refuses while the image is
  mapped leaves the file until the next copy of the same program removes it. A file system
  without POSIX renames falls back to `platform_replace`, which is unchanged.
- `platform_program_replaced` is false on POSIX, and the Mac and Linux copy in place as
  before.
- The wait of `platform_open` is unchanged. Its DESIGN comment now says that it does not
  cover this case.
- `docs/decisions.md` holds the `[provisional]` entry, and "Windows traps" of
  `docs/notes/hosts-and-harness.md` the error and the holders.

## Tests

- `unit_files` gains three Windows checks. One copies over a program mapped as data. One
  copies over a program mapped as an image, and a second copy then removes `.old`. One
  copies over a program held without shared deletion, which fails and leaves the old
  bytes. The first
  failed before the fix, and the second before the move aside.
- `anti_build_rerun`, on Windows hosts alone, builds into an empty `dist/`, runs the
  program and builds again, fifteen times. It takes 9 to 15 s. Under CTest the hold comes
  about once in 15 to 40 turns, so before the fix it failed in 1 run of 3. It does not
  fail reliably, and `unit_files` is the reliable test.

## Failure rate before and after

| Loop | Before | After |
|---|---|---|
| PowerShell, run and build, by hand | 44 of 140 turns failed | 0 of 300 turns |
| PowerShell, empty `dist/` each turn | not measured | 0 of 150 turns |
| CTest, `anti_build` repeated | failed in run 3 | no failure in 8 runs |
| CTest, `anti_build_rerun` | failed in 1 run of 3 | no failure in 8 runs |
| CTest, both, 8 repeats each, scratch build that waits | 9 holds met | not run |

A failure in the PowerShell loop comes in runs of up to ten turns. A failed build leaves
the program in place, and its next run starts another check. PowerShell starts a hold far
more often than CMake's `execute_process`.

## Suites

| Host | Suite | Result | Time |
|---|---|---|---|
| Mac | host | 1640 of 1640 | 147 s |
| Mac | asan | 1639 of 1639 | 471 s |
| Mac | ubsan | 1639 of 1639 | 348 s |
| anti-linux | host | 1346 of 1348, 2 skipped | 265 s |
| anti-windows | host | 1327 of 1336, 9 skipped | 994 s, `-j4` |

The Mac builds print no warning, and neither does the build of anti-windows. The build of
anti-linux prints the warnings of raylib's `stb_truetype.h` alone. `anti_build`,
`anti_build_rerun` and `unit_files` passed in the full run of anti-windows. The first
ASan run took 577 s while anti-windows ran its suite on the same Mac. Alone it took 471 s.

## For Eddie

1. The Mac and Linux still copy the program in place, as the work order asked. A copy that
   fails partway there leaves a broken program in `dist/`, and Linux refuses to write over
   a program that is still running. The same write and rename would fix both. It is
   yours to decide.
2. `platform_open` waits on `EACCES` on the strength of a measurement that this session
   could not repeat. Every failure seen here was `EINVAL`. The wait stays.
