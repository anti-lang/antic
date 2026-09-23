# anti symbols

The step builds `anti symbols inventory`, `check` and `resolve` from
"Symbols tooling" of `docs/anti-language-additions.md`.

## What changed

- `tools/anti/syms.c` holds the three commands. They read the runtime
  configuration with its includes, find the program in the directory of
  the file, the libraries that the `anti-plugins.toml` of every `plugins`
  directory lists and the libraries of `[injections]`. Every binary is
  known by the build id of its notice.
- `inventory` folds the archive of each binary into one archive, each
  entry under a directory named after its id, with `index.toml` of
  module, id, version and source. It reports every binary as present,
  stale or missing.
- `check` writes the same three states, against the archive beside each
  binary or against the archives `--symbols` names, and exits with 1
  unless every binary is present.
- `resolve` prints the trace line by line and adds the function and the
  `file:line` to every frame whose id an archive holds. The debug link
  answers first and the map fills the gaps. Every other line stays raw.
- `tools/anti/zip.c` reads an archive as well as writing one, stored or
  deflated, and checks the CRC of every entry.
- `anti build` passes the version of the manifest to every link. The
  notice names the package of the compiled module last. Before, a
  program built by `anti build` named `0.0.0` in its notice and its
  class descriptors.
- The test `anti_symbols` builds `tests/anti-symbols/tracer` in release
  mode and deploys it. On a macOS host it adds the plugin of
  `tests/plugin`, whose archive another tool packed with deflate. The test
  checks the inventory, its index and the check of present symbols. A
  release trace resolves to `com.example.tracer.inner`, `outer` and
  `main` with their lines. The check after a rebuild says stale, and
  the check without an archive says missing. It runs on every host but
  Windows.

## What failed and how it was fixed

- `anti build` wrote `0.0.0` as the version of the program, so the index
  had nothing to name. The link now gets the version of the manifest.
- The first run of the test created no directory for the interface of
  the plugin. `file(COPY)` then kept the old program after a rebuild of
  the same minute. The test makes the directory and uses `COPY_FILE`.
- `raw_output` refused a capture without `ENCODING NONE`, and
  `fmt_canonical` refused the doc comment of the new program. Both
  fixed in the test files.

## Provisional entries

Seven entries stand under "The symbols command" in `docs/decisions.md`.

- Where the program is found, and what a relative path is read against.
- The libraries of a `plugins` directory.
- The name of the archive of a library.
- The layout of the deployment archive and its index.
- The three states and the exit codes.
- The output of `resolve`, with a Windows frame left raw.
- The methods the zip reader takes.

## Gates

| Suite | Result | Log |
|---|---|---|
| host | 782 passed, 0 failed | `build/drive/logs/test.log` |
| asan | 781 passed, 0 failed | `build/drive/logs/asan-test.log` |
| ubsan | 781 passed, 0 failed | `build/drive/logs/ubsan-test.log` |

The builds write no compiler warning: `build/drive/logs/build.log`,
`asan-build.log`, `ubsan-build.log`. The one line they carry is the
known note of the linker about `libLTO.dylib`.
`python3 tools/docs-style/check_docs.py` reports nothing on every file of
this step, apart from `tests/CMakeLists.txt`. The checker reads that
file as Markdown, where every comment line ending in a period is a
heading. The new block adds the two findings that every block of the
file carries, beside its 201 old ones.

## State at the push of the code

```text
$ git log --oneline -3
7fe34d6 Test anti symbols over a program, a plugin and a release trace
4477035 Build anti symbols inventory, check and resolve
fd2480b Add round four of the language additions
$ git status --short
$ git rev-parse HEAD origin/main
7fe34d676d88cfc00518239156637d19dfce4776
7fe34d676d88cfc00518239156637d19dfce4776
```

## Open

- `anti build` writes no symbols archive for a shared library, and
  nothing builds a plugin with one. The overview lists both as not
  built.
- `resolve` names no Windows frame, because the functions stand in the
  PDB.
