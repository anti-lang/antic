# SQLite and Mbed TLS in src/native

The step builds SQLite and Mbed TLS in `src/native/` for all six targets, pinned,
downloaded and verified as PCRE2 is. A C probe links each library for every target.

## What was done

- `tools/sqlite-pin` pins SQLite 3.53.4 by version, the year of its directory on
  sqlite.org and the SHA3-256 of the amalgamation that sqlite.org publishes.
  `tools/mbedtls-pin` pins Mbed TLS 3.6.7 by the SHA-256 of its release page.
  `src/native/get-sqlite.cmake` and `src/native/get-mbedtls.cmake` download over HTTPS
  into `build/deps/`, check the digest and unpack.
- `src/native/sqlite.cmake` builds `libsqlite3.a`, or `sqlite3.lib`, from `sqlite3.c`
  with the defaults of the release. `src/native/mbedtls.cmake` builds `libmbedtls.a`, or
  `mbedtls.lib`, from the 107 sources of the three libraries of the release with its
  default configuration. Both land in `lib/<target>/` and copy a licence file to
  `licenses/`. `src/native/CMakeLists.txt` includes them after PCRE2, SQLite first.
- `sqlite_link_<target>` and `mbedtls_link_<target>` link a probe for each target through
  antic. `sqlite_run` queries an in-memory database on the host, and `mbedtls_run` sets up
  a TLS client context. `sqlite_pin` and `mbedtls_pin` check the pins.
- The decisions are in `docs/decisions-native.md`, "SQLite and Mbed TLS".

## What failed and how it was fixed

- SQLite on Windows raised `-Wlanguage-extension-token`, `-Wsign-compare`,
  `-Wunused-variable` and then `-Wunused-function`, all in code that SQLite writes for
  `_MSC_VER`. The four are off on Windows alone. Log: `build/drive/logs/build-host.log`.
- `sqlite_pin` refused `get-sqlite.cmake`, whose comment spelled the pinned version. The
  comment now gives another version as its example.
- `fmt_canonical` refused `tests/abi/sqlite_link.anti`. `anti fmt` rewrapped its comment.
- A link without the library fails on `mbedtls_version_get_number` for windows-x86_64 and
  linux-arm64, so the link tests reach the library. Logs:
  `build/drive/logs/neg-mbedtls-*.log`.

## Gates

- Zero warnings in the host, ASan and UBSan builds: `build/drive/logs/build-host-final.log`,
  `build-asan.log`, `build-ubsan.log`.
- Host: 828 of 828 passed, `build/drive/logs/test-host-final.log`. ASan: 827 of 827,
  `test-asan.log`. UBSan: 827 of 827, `test-ubsan.log`. The sanitizer trees leave out
  `no_paths` as before.
- The docs-style checker reports nothing on every touched file. It reads
  `src/native/CMakeLists.txt` as Markdown, so a copy named `.cmake` was checked, as in the
  step before.
- The syntax overview and the specifications name no status of the native libraries
  themselves. `anti.db`, `anti.net` and `anti.crypto` stay unbuilt, as they say.

## Provisional entries added

All are in `docs/decisions-native.md`, "SQLite and Mbed TLS".

- The 3.6 LTS line of Mbed TLS, and one archive for its three libraries.
- The release configuration of Mbed TLS, and the defaults and the name of SQLite.
- The SHA3-256 digest of SQLite.
- The warnings, with the four Windows exceptions of SQLite.
- Both Linux libraries against musl.
- The SQLite licence file written from `sqlite3.h`.
- Where the tests are registered.

## Questions for Eddie

1. "Runtime archive" offers 4.x with TF-PSA-Crypto or the 3.6 LTS line. The step took
   3.6.7, the smaller option. The line is supported until at least March 2027. Keep it, or
   move to 4.2.0 as two libraries?
2. `LICENSES/` needs the Apache 2.0 text of Mbed TLS. Which step adds it?
3. Mozilla's CA bundle, `lib/cacert.pem`, was outside this step. Should the next step of
   `src/native/` pin it?
