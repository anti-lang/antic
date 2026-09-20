# Windows symbols archives, the install layout and the pinned Apple SDK

Five commits, all pushed: `f3261ef`, `0f40d62`, `960d379`, `a57bc36` and
`baa4622`.

## The PDB of a Windows program

First sessions item 19. Every Windows link now passes `/DEBUG`, with `-g` and
without it, because lld-link writes the debug information to a PDB rather than a
section of the executable. What the executable carries is the CodeView record of
its debug directory, whose GUID is the Windows form of the build id.
`/PDBALTPATH:%_PDB%` keeps that record to the file name of the PDB, so the path
of the machine that linked it does not ship. `/ignore:4099` drops the one
warning that the objects of `libvcruntime.lib` name PDBs no machine here holds,
which was seventeen lines on a link that says nothing about the program.

The packer takes `SYMBOLS`, a directory outside the package, and links each
Windows program with `/PDB:` under it. `./r` names `build/dist/symbols`. Step 4
reads the PDB from there, checks it with `tools/check-pdb.cmake` and puts it in
the symbols archive beside the map of the sections.

`tools/check-pdb.cmake` reads the CodeView record of an executable against the
GUID the PDB carries in its info stream. It parses the MSF to reach it. A record
that names a path rather than a file name is refused, and so is a foreign PDB.
Step 4 runs it before either file goes into an archive. The test `pdb_guid` runs
it over a program antic linked for both Windows targets, with `-g` and without
it. Both pass on the Windows VM, where those programs are native.

The dry run wrote four files into each Windows archive, `antic.exe.syms`,
`anti.exe.syms`, `antic.pdb` and `anti.pdb`. The shipped `antic.exe` names
`antic.pdb` and carries the GUID that PDB carries.

## What removed the setup of the Windows VM

Nothing of ours. `C:\anti` never existed on that machine: its creation time was
the minute of this session's own `mkdir`, and the VM has always built in
`%USERPROFILE%\antic-check` with a self-contained `build/`. `docs/vm-setup.md`
described a `C:\anti` layout the machine never used. The release script's own VM
flow stays under the user profile and touched nothing there.

One real hazard came out of the search. Both uninstallers took whatever
`ANTI_HOME` named and removed it whole, with no check that it was an install of
Anti. They now refuse a directory that carries no `.anti-install`, the marker
the installer writes. They remove the two executables of the bin directory and
nothing else there. Step 5 of a release reads both halves on each VM.

## The install layout

An install is no longer one directory named `~/.anti`. It follows the platform:
`~/.local/bin`, `~/.local/share/anti`, `~/.config/anti` and `~/.cache/anti`,
with `XDG_BIN_HOME`, `XDG_DATA_HOME`, `XDG_CONFIG_HOME` and `XDG_CACHE_HOME`
ahead of each root. Windows keeps the executables in
`%LOCALAPPDATA%\Programs\anti\bin` and the rest in `%LOCALAPPDATA%\anti`, with
`config\` and `cache\` below it. Nothing outside the user's profile is written.
Both installers add the bin directory to the PATH once and say that they did.
`ANTI_HOME` still names one tree that carries everything, which is what step 10
checks a download with.

An installed antic sits on the PATH and the archive does not, so the old rule of
the directory above itself no longer finds it. `runtime_archive` in
`src/userdirs.c` is the one rule antic and the anti tool both ask: the directory
above the running executable when it holds `lib/`, and the user's data directory
otherwise. `anti.os` gives a program the same three directories, and asks
`rt/platform.c` which platform it is compiled for rather than reading
`LOCALAPPDATA` and guessing.

Both test machines keep their own tools in `~/.local/share/anti-vm` and
`%LOCALAPPDATA%\anti-vm`, under a name of their own so an install of Anti stands
beside them untouched. The Windows VM is restored there, with the pinned clang,
the LLVM tools, raylib and the six sysroots.

## The pinned Apple SDK

Xcode brought macOS SDK 27.0 during the session. Its `libSystem.tbd` names the
target `arm64e.x1-macos`, which the pinned ld64.lld reads as malformed. Every
symbol of libSystem was then undefined, and step 3 stopped at "macos-arm64:
antic did not link". The packer had taken the SDK of the machine through a bare `xcrun`.

`tools/macos-sdk-pin` now names the version, the directory name and the SHA-256
of `usr/lib/libSystem.tbd`, today 26.5. `tools/macos-sdk.cmake` resolves it by
version through the Command Line Tools, which keep every SDK they installed. It
checks the digest before anything links. An SDK that is absent is refused,
naming the version to install, and so is one that changed under its own name.
The pin moves only with the pinned LLVM, which `docs/toolchain-later.md`
records.
`APPLE_SDK` and `anti sdk export` are unchanged.

The same update had also left the Xcode licence unaccepted, which failed ten
tests that use the platform toolchain until Eddie agreed to it.

## Suites

The Mac 487, ASan 486, UBSan 486, the Linux VM 426, the Windows VM 406. The dry
run ran steps 1 to 5 and printed the plan for the rest.

## Questions

- The name `anti-vm` for the tools of the two test machines is provisional. It
  keeps them clear of an install of Anti in `~/.local/share/anti`, which step 5
  writes and removes on the same machine.
- `/ignore:4099` on every Windows link is provisional. It drops that warning and
  no other.
- `os.user_config_dir` and its two siblings answer `""` when the environment
  names no home. The error convention would be the other form, and nothing in
  `anti.os` returns an error today.
- `os.site_config_dir` is specified and not built. It is the one directory of
  the three that lies outside the user's profile, and nothing writes there.
- `%USERPROFILE%\test.cmd` on the Windows VM is a file I wrote from
  `docs/vm-setup.md`. The machine had none, so its old contents are unknown.
