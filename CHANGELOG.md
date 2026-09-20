# Changelog

One entry per released version, the newest first. The release script reads
the entry of `tools/version` and refuses a release without one. The body of
the entry becomes the text of the GitHub release.

## 0.1.0 - 2026-09-20

The first release of Anti.

- The compiler lexes, parses and type-checks Anti across modules, and its
  IR holds no sizes. The back end lays out types per target and writes
  assembly for the six targets, which llvm-mc assembles and lld links.
- The object model: classes, interfaces as inline sub-objects with thunks,
  four visibility levels, `construct` and `destruct`, operators, singletons,
  the error forms and reflection over descriptors.
- `worker fn`, `parallel` and `dispatch` on all six targets.
- The standard library holds `anti.io`, `anti.text`, `anti.license`,
  `anti.error`, `anti.time`, `anti.os`, `anti.reflect`, `anti.random`,
  `anti.collection`, `anti.toml`, `anti.args`, `anti.json` and `anti.log`.
- `antic -g` writes the line of every statement, and lldb and gdb stop by
  file and line and print a backtrace of Anti function names.
- The processor levels of `--cpu`, with one runtime per level in the
  archive and a check at the start of every program.
- `anti` holds `sdk export`, `sdk import` and `test`.
- Six packages, one per host, with the installers of the two shells. The
  packages, the symbols archives and the signed manifest are assets of the
  GitHub release of the tag. anti-lang.com serves the installers, the
  downloads page and the public key, and no binary.
