# Fold of the native lane

The native lane's decisions, licences, miniaudio copy and sysroot pins now stand in the
main tree.

## What changed

1. `d12193c`. Every entry of `docs/decisions-native.md` moved under "Libraries and
   runtime" in `docs/decisions.md`, with its wording and tag, in the subsections
   "PCRE2", "raylib and miniaudio" and "SQLite and Mbed TLS". The file is gone. The
   lines that the lane listed for the fold changed as well: the contents and the order
   of the runtime archive, the refusal under "CPU levels", the frameworks of a binding,
   the open item on publishing, item 16 of `CLAUDE.md` and the CMake block of
   `docs/site/runtime-archive/index.md`.
2. `cb3ddb1`. `LICENSES/` holds `pcre2.txt`, `raylib.txt`, `miniaudio.txt`,
   `mbedtls.txt` and `sqlite.txt`. `antic_native_license` copies each text of a pinned
   release to `licenses/` of the runtime tree. The test `native_licenses` compares the
   two copies byte for byte. `package_keys` checks that the package holds every file of
   `licenses/` of the runtime tree.
3. `bfd7904`. `raudio.c` compiles with `-Dminiaudio_c` and reads the declarations of
   miniaudio alone. `libraylib.a` defines no `ma_` function, and every link of raylib
   takes `libminiaudio.a` after it. `media_audio_link_<target>` links raylib's audio and
   miniaudio in one program for all six targets, and `media_audio_run` runs it.
   `tools/raylib-pin` names the miniaudio version its release bundles, and
   `raylib_miniaudio_pin` compares that version with `tools/miniaudio-pin`. It also
   checks raylib's copy of the header against the pinned one.
4. `026ed4f`. `glibc_sysroot` of `tools/get-sysroot.cmake` unpacks the 22 packages of X11
   and OpenGL after glibc, from one set of `GLIBC_` pins listed in `GLIBC_PACKAGES`. The
   `MEDIA_` block and the four X11 and GL keys of the link step are gone. Every sysroot
   the script builds has its absolute links turned relative. `get-media-sysroot.cmake`
   and `media_sysroot` are gone, and `sysroot_links` checks the rewrite in a musl
   sysroot.

## Failures and fixes

- `media_audio_link_<target>` failed on every target before the fix, with `duplicate
  symbol: ma_version` and the rest (`build/drive/logs/s3-red2.log`). It passes since
  `raudio.c` reads the declarations alone (`build/drive/logs/s3-green.log`).
- `native_licenses` failed with `LICENSES/raylib.txt` removed and passes with it
  (`build/drive/logs/s2-lic-fail.log`, `s2-lic.log`). `raylib_miniaudio_pin` failed
  before the pin line and with a wrong version (`s3-pin-red.log`, `s3-pin-mut.log`).
- `glibc_sysroot` fails without the rewrite, since the copy step no longer follows an
  absolute link (`s4-glibc-mut.log`). `sysroot_links` fails without the rewrite
  (`s4-links-red.log`).
- `fmt_canonical` failed once on `tests/abi/media_audio_link.anti`, and `anti fmt`
  rewrapped its comment (`s4-fmt.log`).

## Provisional entries added

- `LICENSES/<name>.txt` holds the same name and bytes as `licenses/` of the runtime tree.
- `raudio.c` compiles with `-Dminiaudio_c` against raylib's copy of the header.
- The `MA_COINIT_VALUE` of 2 in `raudio.c` no longer applies, so raylib's audio
  initialises COM as a multithreaded apartment on Windows.
- `media_audio_link_<target>` calls miniaudio through a C probe, since `anti.miniaudio`
  is not built.
- A host that cannot write a symbolic link gets a copy of the file instead.
- raylib and miniaudio read the glibc sysroot of the runtime tree, and a sysroot without
  the X11 headers stops the configure.

## Questions

- The lane's entry on `antic_native_target` says that the fold makes it and
  `anti_cross_runtime` read one function. The four steps did not ask for it. It changes
  the Windows flags of anti_rt, where the native libraries put clang's headers first.
- `anti license` is not built. The directory it reads holds the five texts.
- Both VMs keep a glibc sysroot installed before this step, which the configure now
  refuses. `tools/get-sysroot.cmake` for `linux-x86_64-glibc` and `linux-arm64-glibc`
  installs the new one.

## Gates

The logs are `build/drive/logs/host-test.log`, `asan-test.log` and `ubsan-test.log`.
The build of each preset wrote no warning.

- host: 100% tests passed out of 951
- asan: 100% tests passed out of 950
- ubsan: 100% tests passed out of 950

## Proof

The state after the push of the report commit.

```text
$ git log --oneline -3
6e4232b Report the fold of the native lane
026ed4f Move the X11 and OpenGL packages into the glibc sysroot
bfd7904 Build raylib against the miniaudio library of the runtime tree
$ git status --short
$ git rev-parse HEAD origin/main
6e4232b0cc38a7072f1aa04a7fca21659cef1f7b
6e4232b0cc38a7072f1aa04a7fca21659cef1f7b
```
