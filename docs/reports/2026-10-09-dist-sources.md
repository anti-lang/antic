# The record of upstream sources

Step `sources` of `docs/work-order-distribution.md`, 2026-10-09. Every runtime archive and
every package carries `licenses/sources.txt`, the record of the exact upstream source of
each pinned component, and `anti license` prints the line of a component after its text.

## What the step built

- `tools/upstream-sources.cmake` writes the record at configure, from `tools/sysroot-pins`,
  `tools/zig-stubs-pin` and the pins of the six native libraries. One line per component:
  the name of its licence text in `licenses/`, its version and the URL of the exact source
  package. For glibc, the kernel headers and the X11 and OpenGL packages that is the
  `.dsc` of the Ubuntu source package in the pool, for musl, Zig and each native library
  the release archive. The two processors must agree on the version of each Ubuntu
  package, or the configure step stops. The file ends its lines in LF on every host.
- The URL of each native library moved from its download script into its pin as
  `<NAME>_URL`, with `@VERSION@`, `@YEAR@` and `@NUMBER@` for the parts the pin fills.
  `antic_pin_url` of the module fills them for the download scripts and for the record,
  so each pin holds the URL once. `run_native_pin.cmake` requires an HTTPS URL in the pin.
- `anti license --from <binary> [--runtime <dir>]` follows the text of each component
  with its line as `source <name> <version> <url>`, read from the archive `--runtime`
  names or the one anti finds beside itself. `NOTICE.txt` of `anti build` holds the same.
  `RUNTIME_SOURCES_FILE` of `src/antic/antic.h` names the file.
- `tools/pack-anti.cmake` copies the record with the licence texts, as before, and its
  comment says so. "Binary distribution" in `docs/decisions.md`, "Obligations of a shipped
  program" and "License command" in `docs/distribution.md`, the two site pages and item
  30 of `CLAUDE.md` describe the record.

## What it tested

Test first, `build/drive/logs/sources-red.log` and `sources-red-detail.log`: `runtime_sources`
failed on the missing file, the six `<name>_pin` tests on the pins without a URL, and
`license_notice` on a notice without the lines.

- `runtime_sources`, `tests/run_sources.cmake`, reads the record of the build tree. Every
  line has three fields and an HTTPS URL. Every name has a licence text beside the file
  and appears once. No byte is a carriage return. The URLs of glibc, the kernel
  headers and musl name the versions of `tools/sysroot-pins`. Zig and the six libraries
  carry the version of their pins.
- `package_keys` requires `anti/licenses/sources.txt` in the archive, runs the same checks
  on the file of the unpacked package, and runs the packed `anti license --from` on the
  linux-arm64 program it linked, with the package's `bin/` alone on the `PATH`. The notice
  holds the lines of musl and mimalloc of the package's record.
- `license_notice` passes `--runtime` to `anti license --from`, finds `source <line>`
  after the text of musl and of mimalloc in the notice of a program of musl, and no
  `source` line in the notice of a program of glibc or of macOS.
- `native_pin_self` refuses a pin whose URL is plain HTTP or missing.

The Windows VM failed `package_keys` and `license_notice` at the first run,
`build/drive/logs/sources-vm-windows-suite-1.log`. `file(WRITE)` ends a line as the host does,
so the record held CRLF there. The `source` line of a notice then carried a carriage
return, `sources-vm-windows-NOTICE.txt`. `run_sources.cmake` now reads the bytes as hex and refuses a
carriage return, which failed on the VM, `sources-vm-windows-red-cr.log`, and
`file(CONFIGURE ... NEWLINE_STYLE LF)` writes the record, after which the record of the VM
equals the Mac's byte for byte and the three tests pass, `sources-vm-windows-green-*.log`.

## Sizes

The record of the build tree is 3,361 bytes and 32 lines of components. The macos-arm64
package packed from the tree, `build/drive/logs/sources-pack-macos-arm64.log`, is
142,222,704 bytes, against 142,227,872 in the report of the step `glibc`. The record
costs less than the variation of the archive. The record in that package equals the one
of the runtime archive.

## Decisions

Three `[provisional]` entries under "Binary distribution" in `docs/decisions.md`:

- which components have a line, and that the runtime, the compiler-rt builtins, the LLVM
  tools, Apple's SDK stubs and the xwin trees have none;
- the URL of each native library in its pin as `<NAME>_URL`, with the names the pin fills;
- `anti license --from` takes `--runtime` and refuses to run without an archive or
  without the record in it.

The entry on `anti_licenses` under "Build tool and distribution" says the `source` line is
not in the binary. No sentence of the work order contradicted the code.

## Gates

| Suite | Result | Time and log |
|---|---|---|
| Mac host | 1694 of 1694, `sysroot_build_tools` and the two `mimalloc_environment` skipped | 313 s, `build/drive/logs/sources-host-suite-2.log` |
| Mac ASan | 1693 of 1693, the same three skipped | 463 s, `sources-asan-suite-2.log` |
| Mac UBSan | 1693 of 1693, the same three skipped | 421 s, `sources-ubsan-suite-2.log` |
| anti-linux | 1635 of 1635 in two parts of `ctest -I`, six skipped | 263 s and 57 s, `sources-vm-linux-suite-3.log` and `-4.log` |
| anti-windows | 1612 of 1612 in three parts, fourteen skipped | 626 s, 367 s and 29 s, `sources-vm-windows-suite-final-1.log`, `-2a.log` and `-2b.log` |

Every suite ran over the final tree, after the LF fix. The earlier runs over the first
commit are `sources-host-suite.log`, `sources-asan-suite.log`, `sources-ubsan-suite.log`
and `sources-vm-linux-suite-1.log` and `-2.log`. `package_keys` took 312 s on the Mac,
263 s on anti-linux and 342 s on anti-windows. The VMs took the changed files with
`tar -xmf` after a digest of every tracked file showed the rest equalled `HEAD`,
`sources-vm-linux-digest.log` and `sources-vm-windows-digest.log`. `emit_identity` and
`link_identity_macos-arm64` passed unchanged in the three Mac suites.

`tools/pack-anti.cmake` changed in a comment alone. `release_dry_run` passed in the host
suite. The real `./r --dry-run` is Eddie's and did not run here.

## State

Before the commit of this report, which follows it and is pushed with it.

```text
$ git log --oneline -3
fde2028d Record the upstream source of every pinned component in the archive
aa458387 Report the step glibc of the binary distribution
0b55d4f2 Put the two glibc sysroots and the glibc runtime into every package
$ git status --short
$ git rev-parse HEAD origin/main
fde2028dae14b5d3141f4aead2973730f095ec78
aa4583875cd6621896f1ef7f1c1fed05a031c0cf
```
