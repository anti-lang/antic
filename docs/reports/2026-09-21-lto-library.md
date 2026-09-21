# The `-lto_library` warning of the macOS host links

The task: find where the link line adds `-lto_library`, stop passing it, and
confirm the warning is gone from a fresh build. The flag does not come from
antic, and Eddie chose to keep it and document it.

## Findings

- `src/linker.c` never passes `-lto_library`. antic calls `ld64.lld` for a
  Mach-O program and `ld -r` for a join, and no ctest log holds the warning.
- The pinned clang driver adds it. `clang/lib/Driver/ToolChains/Darwin.cpp`
  line 284 appends `-lto_library <clang>/lib/libLTO.dylib` to every link at
  ld64 version 133 or above, unless `-fuse-ld=lld` names the linker.
- The warning comes from the CMake build. A relink of `antic` printed it once.
  Apple's ld64 is `ld-27037.1`.
- The packer links with `--ld-path` to `ld64.lld` and no `-fuse-ld=lld`. The
  driver passes the flag there too, and `ld64.lld` ignores it without a word.

## The routes to remove it

Each route changes how the host links, so Eddie chose.

- Patch the driver in `anti-lang/llvm-tools`, then rebuild all six hosts and
  bump the pin.
- Link the host with `-fuse-ld=lld`. It fails against the SDK of Xcode:
  `libSystem.tbd` names the target `arm64e.x1`, which the pinned lld rejects.
- Append `-Wl,-lto_library,` with the `libLTO.dylib` of Xcode. ld64 takes the
  last one, and the warning went away. The build would name a path in Xcode.
- Keep the warning and document it. Eddie chose this route.

## What changed

- `docs/notes/linker.md` is new. It holds one entry: where the warning comes
  from, why no flag removes it, and why it does no harm.

## Tests

No source file changed, so the full suite and the sanitizer suites did not
run. The docs-style checker passes both files. The warning stays in a fresh
build by the decision above.
