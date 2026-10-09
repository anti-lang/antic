# The headers of the native libraries into the archive and the package

Step `headers` of `docs/work-order-distribution.md`, 2026-10-09. The headers of PCRE2,
SQLite, Mbed TLS, miniaudio and raylib stand in `include/<library>/` of the runtime
archive and of every package.

## What the step built

- `antic_native_headers` of `src/native/CMakeLists.txt` copies the headers at configure,
  beside the licence texts. Each library gives the headers its own build installs:
  `pcre2.h`, `sqlite3.h` and `sqlite3ext.h`, every header of `include/mbedtls/` and
  `include/psa/` of Mbed TLS, `miniaudio.h`, and `raylib.h`, `rcamera.h`, `rlgl.h` and
  `raymath.h`. Each directory is the one a C compile names with `-I`, so Mbed TLS keeps
  `mbedtls/` and `psa/` below `include/mbedtls/`.
- The C probes of the native tests compile against these copies for all six targets,
  in place of the source trees.
- `tools/pack-anti.cmake` copies `include/` of the runtime archive as it stands.
- `lib/cacert.pem`: there is none. The release of Mbed TLS 3.6.7 carries no CA bundle,
  only test certificates under `framework/data_files/`. The bundle comes from curl's
  extract with a pin of its own, as "CA bundle" in `docs/distribution.md` says, and that
  pin is not built.
- Items 16 and 31 of "First sessions" in `CLAUDE.md` read Done.

## What it tested

Test first. `package_keys` failed on the old packer with `lacks
anti/include/pcre2/pcre2.h`, `build/drive/logs/headers-red.log`. It now requires eleven
named headers in the package and every file of `include/` of the runtime tree. The packed
`anti bind --clang`, with `bin/` of the package and the pinned clang alone on the `PATH`,
binds `include/raylib/raylib.h` of the unpacked package. Its output equals what the `anti`
of the tree writes from `raylib.h` of the pinned raylib source, file for file.

On Windows the bind first failed, since the package holds no Windows sysroot until the
step `mingw` and clang reads the host's headers,
`build/drive/logs/headers-vm-windows-suite-4.log`. The bind moved after the stand-in of
the Windows sysroots that the test already makes for its links.

## Correction of the work order

The step compared the module with `anti.raylib` of the tree. The tree holds no such file,
since "Bindings" in `docs/decisions.md` has `anti bind` write it. The sentence now names
the module the `anti` of the tree writes from the pinned `raylib.h`, which
`anti_bind_raylib` compiles.

## Sizes

`include/` of the runtime archive is 7,724 KiB unpacked: Mbed TLS 2,440, miniaudio
4,012, SQLite 716, raylib 504 and PCRE2 52. The macos-arm64 package went from 142,258,212
to 143,191,944 bytes after xz, 933,732 more, `headers-red.log` and `headers-green.log`.

## Decisions

One `[provisional]` entry under "Binary distribution" in `docs/decisions.md`: which
headers go in, the directory of each as the root of `-I`, the probes against the copies,
and no `lib/cacert.pem`. The entries on raylib, miniaudio, SQLite and Mbed TLS that said
the headers stay in the source trees now name `include/`.

## Gates

| Suite | Result | Time and log |
|---|---|---|
| Mac host | 1694 of 1694, `sysroot_build_tools` and the two `mimalloc_environment` skipped | 311 s, `build/drive/logs/headers-host-suite-2.log` |
| Mac ASan | 1693 of 1693, the same three skipped | 460 s, `headers-asan-suite-2.log` |
| Mac UBSan | 1693 of 1693, the same three skipped | 390 s, `headers-ubsan-suite-2.log` |
| anti-linux | 1624 of 1624 in two parts of `ctest -I`, six skipped | 142 s and 189 s, `headers-vm-linux-suite-1.log` and `-2.log` |
| anti-windows | 1612 of 1612 in five parts, fourteen skipped | 252 s, 143 s, 374 s, 324 s and 154 s, `headers-vm-windows-suite-1.log` to `-3.log`, `-4b.log`, `-5.log` |

The move of the bind changed `tests/run_package.cmake` after the first Mac suites and the
two Linux parts. The three Mac suites ran again on the final commit, and `package_keys`
and `raw_output` ran again on anti-linux, `headers-vm-linux-suite-3.log`. The VMs took
the changed files with `tar -xmf` after a digest of every tracked file outside `docs/`
showed that the rest equalled the commit before. `emit_identity` and
`link_identity_macos-arm64` passed unchanged in the three Mac suites. The builds had no
warning.

`tools/pack-anti.cmake` changed. `release_dry_run` passed in the host suite, and its
stand-in packer does not read the real one. The real `./r --dry-run` is Eddie's and did
not run here.

## State

Before the commit of this report, which follows it and is pushed with it.

```text
$ git log --oneline -3
16464f7d Put the headers of the native libraries into the archive and the package
cc4518fd Report the step sources of the binary distribution
fde2028d Record the upstream source of every pinned component in the archive
$ git status --short
?? docs/reports/2026-10-09-dist-headers.md
$ git rev-parse HEAD origin/main
16464f7d99f8be5031ad383fef3581988e072998
16464f7d99f8be5031ad383fef3581988e072998
```
