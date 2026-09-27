# Eddie's answers to the VM check

The step builds the four answers to the questions of
`docs/reports/2026-09-27-vm-check.md` and records each in `docs/decisions.md`.
Status: DONE.

## What was done

- `dec9c18`: the symbols archive of `anti build` holds the PDB of the release
  link, whose GUID the shipped `app.exe` carries. The debug link is named
  `app.debug.exe`, so its own PDB no longer overwrites that one. `anti_build`
  checks the GUIDs with `tools/check-pdb.cmake` on Windows. It failed there
  before the change.
- `0fbc88a`: `deps_dir` checks the downloads the tree was configured with,
  `ANTI_DEPS_DIR` when set and `build/deps` otherwise. A directory the tree
  took from elsewhere, as the four of the VMs, goes on to the copy, and the
  copy's cache must name it. The VMs keep their setup.
- `a09e06c`: the build and test presets `asan` set
  `ASAN_OPTIONS=detect_leaks=1`. LeakSanitizer works on macOS arm64 with the
  pinned clang, so nothing went into the decisions about a Mac without it.
  `CLAUDE.md` names the preset commands of the sanitizer gate.
- `41a8372`: the decisions. `--linker platform` and `program_platform_linker`
  stay. The windows-arm64 sanitizer build goes. It existed as text in the
  decisions alone, and no preset or workflow built it.
  `ANTIC_SYSTEM_COMPILER` stays, since it exists for a reader who builds
  antic from source. `README.md` describes it, no build of ours takes it,
  and the packer refuses it. The windows-arm64 sanitizer build was the one
  build of ours that took it.

## Counts at `41a8372`

| Host | Suite | Passed | Skipped |
|---|---|---|---|
| Mac | host | 1157 of 1157 | 0 |
| Mac | ASan with leak checks, UBSan | 1156 of 1156 each | 0 |
| anti-linux | host | 983 of 983 | 2 |
| anti-linux | ASan, UBSan | 982 of 982 each | 2 |
| anti-windows | host | 955 of 955 | 5 |

The Mac's ASan tree compiled the standard library again under the preset,
so antic ran with leak checks there as well. The two Linux skips are
`release_dry_run` and `macos_sdk`, which run on the Mac that makes a
release. The five Windows skips add `sysroot_digest`, since a Windows host
takes the Build Tools rather than xwin, and `installer_github` and
`manifest_signature`, since `install.sh` needs a shell.
