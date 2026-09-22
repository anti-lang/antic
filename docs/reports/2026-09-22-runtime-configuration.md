# Runtime configuration

The step built "Runtime configuration" of `docs/anti-language-additions.md`:
`--anti.conf` and `ANTI_CONF`, `rt.configure`, the TOML file with
`include`, `[runtime]` and `[injections]`, the options of every key,
`--anti.inspect`, `--anti.help`, the precedence of the layers and the key
`backtrace`, which the error-origin step deferred.

## What it holds

- `rt/conf.c` holds the five keys of `[runtime]`, the layer of each value
  and the file it came from. `rt/start.c` hands every argument under
  `--anti.` to it in one pass and then reads the file the command line or
  the environment named. `docs/notes/runtime-conf.md` holds the choices.
- The reader of the TOML subset took arrays and quoted keys, which the
  file needs for `include` and for an interface name. `anti.toml` gives
  the elements of an array as `hosts.0` and `hosts.1`, as it numbers the
  entries of a repeated table.
- `std/anti/runtime.anti` is the module of `configure`. A program writes
  `import anti.runtime as rt;` and calls `rt.configure(path)`, the form
  the specification gives.
- The pool reads the `threads` key and `anti.log` the `logger` key, so
  `ANTI_THREADS` and `ANTI_LOGGER` are gone. The runtime reads one
  environment variable, `ANTI_CONF`.
- Twenty-one cases in `tests/conf` cover every option and the file of
  each of the three sources. They cover the order of the includes, an
  array value, a missing include and a cycle. They cover an unknown
  option, an unknown key, a value a key does not take and a line of
  `[injections]`.

## What failed and how

- `clib_bundle`: a static library for C with a bundled runtime left
  `anti_rt_runtime_version` undefined, because the bundle takes the stub
  of `rt/license_stub.c` instead of `rt/license.c`. The stub answers for
  the version as well now, with an empty text, as it does for the licence.
- `program_build_id`: the test read every line of a binary that starts
  with `build `, and `rt/license.c` holds that constant to find the id
  with. Its object reaches every program through `--anti.inspect` now. The
  search takes the whole form of the line, `build [0-9a-f]+`, which is
  what the test documents.
- `link_identity_macos-arm64`: the runtime gained `rt/conf.c`, so the
  digest of the linked executable changed. This Mac wrote the new one.
- The file of the specification writes `plugins = ['/opt/some_program/lib']`.
  The key then stood as `plugins.0`, which no key of the table matched.
  A key whose elements are numbered takes them joined with `:`, the
  separator of `--anti.plugins=dir:dir`. `conf_array` pins it.

Logs: `build/drive/logs/build.log`, `test.log`, `asan-test.log`,
`ubsan-test.log`.

## Provisional entries

`docs/decisions.md`, under "Runtime configuration":

- `rt.configure` is `configure` of `anti.runtime`, imported as `rt`.
- The file of the command line wins over the one of the environment, and
  both over the one of `rt.configure`.
- An unknown key and a value a key does not take are startup errors that
  name the file and the line. So is a key outside the three names.
- Every line of `[injections]` and every `--anti.inject` is a startup
  error while no program carries an injectable interface.
- A cycle is found by the resolved path, and a depth above thirty-two
  counts as one.
- The lines of `--anti.inspect`, and the empty value of a key no layer
  set.
- `--anti.help` answers before the file is read and `--anti.inspect`
  after it.
- The array of `plugins` is joined with `:`.

One line of `docs/anti-object-model.md` said `dispatch` takes the same
`ANTI_THREADS` as `parallel`. It names the `threads` key now, which the
overview and "Runtime configuration" of the additions had already
settled.

## Question for Eddie

`--anti.plugins=dir:dir` writes the separator of Unix, and the file joins
its array the same way. A Windows path holds a colon after its drive
letter. The separator wants to be `;` there, or the option wants another
form. Nothing reads the value yet, so the answer can wait for plugins.

## Proof

```text
$ git log --oneline -3
876d36f Take an array as the value of a key of the table runtime
e03a0ad Record the runtime configuration in the documents
b6861b4 Take the logger and the pool from the configuration

$ git status --short

$ git rev-parse HEAD origin/main
876d36f33e56ec33125739eac946453b1a92ae73
876d36f33e56ec33125739eac946453b1a92ae73
```

The suites: 723 of 723 on the host, 722 of 722 under AddressSanitizer and
722 of 722 under UndefinedBehaviorSanitizer, each without a skip. The
sanitizer builds leave out `no_paths`, which needs a build that no
sanitizer wrote paths into.
