# The runtime configuration

The choices inside `src/rt/conf.c`, the pass over the arguments in
`src/rt/start.c` and the readers of the keys. The rules are in "Runtime
configuration" of `docs/anti-language-additions.md`, and the settled
points are in `docs/decisions.md` under the same name.

## The layers

One table of keys lives in `src/rt/conf.c`, one entry per key of `[runtime]`:
`backtrace`, `logger`, `plugins`, `threads` and `trace`. Each entry holds
the value, the layer that set it and the file of a value of the file
layer. The layers are `build`, `file` and `command`, in that order, and
a write is dropped when the entry stands at a higher layer. Two writes of
the file layer overwrite, so a later include wins over an earlier one.
The including file wins over both, because its own keys are applied
after its includes.

A key no layer set holds no value. `anti_rt_conf_get` then gives an empty
text, and the reader of the key keeps what the build gave it. The pool
counts the processors, and `anti.log` builds the default logger.

## The pass over the arguments

`src/rt/start.c` walks the arguments once. An argument that does not start
with `--anti.` moves down into the slice that `main` receives, and the
order of those is the order they came in. Everything else goes to
`anti_rt_conf_option` with the name after the prefix and the value after
the first `=`, or no value. Three names are the pass itself rather than a
key: `conf` remembers the path, `inspect` and `help` set a flag. `inject`
carries an interface and a path and ends the program, because no program
has an injectable interface yet.

`anti_rt_conf_start` runs after the pass. It prints the options and ends
for `--anti.help`, reads the file of `--anti.conf` or of `ANTI_CONF`, and
prints the effective configuration and ends for `--anti.inspect`. The
help comes before the file so that a program with a broken file still
answers it, and the inspection after it so that it shows what the file
gave.

## The file

`anti_rt_toml_read` of `src/rt/toml.c` reads the file, which is why arrays
and quoted keys are in that subset. The reader walks the flat list twice:
once for `include` and `include.<n>`, which it resolves against the
directory of the file that names them and reads depth first, and once for
the keys. A key outside `include`, `runtime.` and `injections.` is a
startup error, as is a key of `[runtime]` that no entry of the table
names.

A cycle is found by the paths on the stack of files being read, compared
as text. Two spellings of one path escape that comparison, and the depth
limit of thirty-two catches them with the same message.

The file is opened through `anti_rt_fs_open` of `src/rt/fs.c`, so the path of
Windows goes through UTF-16 as every other path of the runtime does.

## Who reads a key

`backtrace` is the one key the configuration applies itself: it writes
`anti_rt_option_backtrace`, which `anti_rt_backtrace_on` already read for
`--anti.backtrace`. `threads` is read by `worker_count` of
`src/rt/threads.c`, and `logger` by `from_configuration` of
`src/std/anti/log.anti` through `anti_rt_conf_get`. `trace` is read the same
way by `start` of `src/std/anti/trace.anti`, which the program calls.
Nothing reads `plugins` yet.

The version that `--anti.inspect` prints comes from the notice of the
program, through `anti_rt_runtime_version` of `src/rt/license.c`. That object
therefore reaches every program, and the stub of a bundled archive
answers for it as it answers for the licence text.
