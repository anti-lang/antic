# xwin leaves the tree

The step `no-xwin` of `docs/work-order-distribution.md`, on 2026-10-10. It
follows the step `mingw`, which built the Windows sysroots from mingw-w64
and already took xwin out of `tools/get-sysroot.cmake`, both installers and
`CMakeLists.txt`. This step removed what that one left.

## What the step built

- `tools/sysroot-pins` lost the eighteen lines of the xwin pins. They were
  `XWIN_VERSION`, three prebuilt archives with their digests and their URL,
  the versions of the CRT and the SDK, and two tree digests. No script
  read them.
- `ANTI_MICROSOFT` left `tools/release.sh`, in the two VM scripts of step 5
  and in step 10, and the tests `manifest_signature` and
  `installer_github`. No installer read it since the step `mingw`.
- Step 10 of `tools/release.sh` fails when an install lacks the sysroot of a
  target. Before, it skipped the target with the line "waits for the C
  runtime of Microsoft". Step 10 of `docs/work-order-release-script.md`
  asks for a link for the other five targets, so the script now does what
  that document says.
- The test `no_sdk_fetcher`, `tests/run_no_sdk_fetcher.cmake`, holds the
  search of the step. It refuses the name xwin, in either case, in `src/`,
  `tests/`, `tools/`, `docs/notes/`, `docs/site/`, the documents directly
  under `docs/` apart from `docs/decisions.md`, and `CLAUDE.md`. It refuses
  `ANTI_MICROSOFT` in `src/`, `tests/` and `tools/`. The script joins both
  words from two halves, so it passes its own check.
- Item 33 of "First sessions" in `CLAUDE.md` reads "Done". Item 32 read
  "Done" already. Item 34 no longer names xwin.

## Corrections of the work order

The step is done when `grep -ri xwin` finds nothing in `docs/*.md` and
`CLAUDE.md`, and the work order is one of those documents. It named xwin in
ten lines, the name of this step among them. So the work order now calls
the tool "the SDK fetcher" and the step `no-sdk-fetcher`, with the note
that `drive-dist.sh` runs it under the name of the tool. The sentence of
the step that excluded "history in `docs/reports/` and
`docs/decisions.md`" says the same in two sentences and names the test.

For the same reason item 33 of `CLAUDE.md` does not spell the path of this
report, whose name the driver fixes. It names the report by its date and
its step. Every other item names its report by path.

## What it tested

`no_sdk_fetcher` failed first, on `tools/sysroot-pins`,
`docs/work-order-distribution.md` and `CLAUDE.md`, in
`build/drive/logs/noxwin-red.log`. A stand-in tree with `ANTI_MICROSOFT` in
a file under `src/` failed the second check, and one with the name in
capitals in a document failed the first while `docs/decisions.md` of that
tree passed.

The literal search of the step finds the history alone:

```text
$ grep -rIil xwin src tests tools docs/notes docs/site docs/*.md CLAUDE.md
docs/decisions.md
```

`release_dry_run` passes on the Mac with the changed `tools/release.sh`. It
runs the first five steps over stand-ins and prints a plan for the rest, so
no test runs the changed branch of step 10. The real `./r --dry-run` is
Eddie's and did not run.

## Sizes

`tools/sysroot-pins` went from 147 lines and 11465 bytes to 129 lines and
10296 bytes. The package carries that file in `tools/` until the step
`installers`, so each package shrinks by 1169 bytes before compression.
Nothing else of a package or of the runtime archive changed.

## Decisions

Two `[provisional]` entries in `docs/decisions.md`:

- Under "Binary distribution": the test `no_sdk_fetcher` and its paths,
  the name "SDK fetcher" in the work order and `CLAUDE.md`, and the report
  that `CLAUDE.md` names by its date.
- Under "The release script": step 10 fails on a missing sysroot of a
  target.

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1694 of 1694 pass, 2 skipped, 343 s | `build/drive/logs/noxwin-host-ctest.log` |
| Mac | `asan` | 1693 of 1693 pass, 2 skipped, in two parts of 437 s and 78 s | `build/drive/logs/noxwin-asan-ctest-1.log`, `-2.log` |
| Mac | `ubsan` | 1693 of 1693 pass, 2 skipped, in two parts of 418 s and 53 s | `build/drive/logs/noxwin-ubsan-ctest-1.log`, `-2.log` |
| anti-linux | the 21 tests the step touches | 21 of 21 pass, 1 skipped, 186 s | `build/drive/logs/noxwin-linux-ctest.log` |
| anti-windows | the 21 tests the step touches | 21 of 21 pass, 3 skipped, 420 s | `build/drive/logs/noxwin-windows-ctest.log` |

The skips on the Mac are the two `mimalloc_environment` tests. Each
sanitizer suite ran as `ctest -I 1,1009` and `ctest -I 1010,1693`. The
second part reports 695 tests, since it runs eleven `dev_object_*` fixtures
of the first part again, and the two logs together hold 1693 distinct
tests.

The step names no VM. It changed `tools/sysroot-pins`, `tools/release.sh`
and two tests of the installers, so each VM built the commit and ran the
tests that read them: `no_sdk_fetcher`, `repo_layout`, `script_policies`,
`pinned_tools`, `runtime_sources`, the four `host_sources_<target>`,
`windows_sysroot`, `macos_sysroot`, `glibc_sysroot`, `sysroot_links`,
`package_keys`, `package_api`, `internal_package`, `own_tools`,
`installer_options`, `installer_github`, `manifest_signature` and
`release_dry_run`. anti-linux skips `release_dry_run`, and anti-windows
skips it and the two tests of `install.sh`, as before. Neither VM ran its
whole suite. A first run on anti-linux failed `repo_layout` on two log
files this session had written into the root of the exported tree. They
moved under `build/` and the run passed.

All three builds on the Mac and both builds on the VMs had no warning.
`emit_identity` and `link_identity_macos-arm64` pass unchanged. The
docs-style checker reports nothing on `CLAUDE.md`, `docs/decisions.md`,
`docs/work-order-distribution.md` and this report.

## Questions

- The work order and `CLAUDE.md` avoid the word xwin, since the search of
  the step covers them. If the work order may keep the name of its own
  step, the test takes one more exception and both documents can say xwin
  again.

## State

The commit of the step is pushed. This report follows it in a commit of
its own.

```text
$ git log --oneline -3
20bcc695 Take the last traces of the SDK fetcher out of the tree
3b17d245 Report the step mingw of the binary distribution
3f111e33 Build the Windows sysroots from mingw-w64 and link against ucrtbase.dll
$ git status --short
?? docs/reports/2026-10-10-dist-no-xwin.md
$ git rev-parse HEAD origin/main
20bcc69571ab2d9dcb400bfef189b1ee82b59f05
20bcc69571ab2d9dcb400bfef189b1ee82b59f05
```
