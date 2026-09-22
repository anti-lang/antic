# The Windows answers

The four answers to `docs/reports/2026-09-22-vm-answers.md` are built. The Mac, the
Windows VM and the Linux VM ran the suite from `b0a8071`.

## Counts

| Host | Run | Suite | ASan | UBSan |
|---|---|---|---|---|
| Mac | `b0a8071` | 630 of 630 | 629 of 629 | 629 of 629 |
| Windows VM | `b0a8071` | 524 of 524, eight skipped | none | none |
| Linux VM | `b0a8071` | 541 of 541, three skipped | none | none |

The Mac gained `clib_bundle_windows-x86_64` and `clib_bundle_windows-arm64`. The logs
are `build/drive/logs/test.log`, `test-asan.log`, `test-ubsan.log`, `vm-windows.log`
and `vm-linux.log`.

## What changed

1. `1d9802f`: `--bundle-runtime` joins the runtime into the library's one object on
   Windows too. No linker of COFF writes a relocatable object, so `src/coff.c` joins
   COFF objects. `clib_bundle` requires a duplicate that the runtime library defines,
   read with `llvm-objdump`, on every host. The two new tests run the case for both
   Windows targets on any host. The unit test `test_coff` joins objects built by hand.
2. `3fb5f2b`: link.exe under `--linker platform` no longer gets `/pdbsourcepath:.`.
3. `3fb5f2b`: every Windows link of antic runs in the directory of its output. The
   objects, the PDB, `/OUT:`, the `.def` file and the extra inputs are relative to it,
   and `/PDB:` is given. A runtime archive, its sysroot and its linker stay as given.
   The packer links a Windows program the same way. `pdb_names` links each mode with
   the runtime absolute and relative. It fails on a path of the build in the PDB or the
   executable, and with the relative runtime on any path of the checkout.
4. `f1639eb`: the symbol record named a function `_A11stack_trace_inner`, and the ELF
   and Mach-O readers give `stack_trace.inner`. The record now carries that form, or
   the C name of an export fn, and the entry is no longer provisional.

## What failed and how it was fixed

- The first Windows run failed `pdb_names_windows-arm64` and `clib_bundle`. The VM
  builds its host runtime in Debug, with CodeView. Each object named itself by its
  absolute path in `S_OBJNAME` and recorded the clang command line in `LF_BUILDINFO`,
  which `-ffile-prefix-map` reaches neither of. The join refused the bundle, because
  every member carried lines and types. `ef2921e` compiles for a Windows host with
  `-fdebug-compilation-dir=.` and `-gno-codeview-command-line`. The first object with
  CodeView lines or types keeps them in a join, and a later one loses its own.
- The Linux VM did not compile `selfpath.c`: glibc declares `realpath` for X/Open alone.
  `clib_bundle` there expected `-lpthread -lm` on the line of a Windows target.
  `b0a8071` fixes both.

## Entries

- New `[provisional]`: the rules of the COFF join, under "Libraries for C".
- New `[provisional]`: the two CodeView flags of a Windows host, beside the entry on
  `-ffile-prefix-map`.
- The entry on the name of the symbol record lost its tag.

## Findings

- link.exe makes every path of its PDB absolute. That is the module of each object,
  the runtime library, the PDB and the directory of the link. A link of `return42.anti`
  on the VM ran in its output directory, printed no LNK4044 and returned 42. Its PDB
  held `C:\Users\Eddie\antic-check\build\lnk` four ways. It also printed LNK4098,
  because the Debug host runtime asks for `msvcrtd.lib`.
- The PDBs of the packer still name the sysroot, the LLVM tools and the resource
  directory of clang absolutely. The release passes them so, and all lie under
  `build/` of the checkout. The objects, the PDB and the output are relative.
- `work/<host>/anti` is the tree of a package, and the packer wrote the objects of
  `anti` there, so a Linux package carried them. They now stand in
  `work/<host>/objects`.
- The Mac's linker prints that it ignores `-lto_library`, because the pinned clang
  names a `libLTO.dylib` that its archive lacks. Logs of earlier sessions show it too.

## Questions

1. link.exe records the paths of the build in its PDB whatever the command line gives.
   Shall `--linker platform` keep `/DEBUG` with it?
2. A release PDB names the checkout through the toolchain of the packer. Shall the
   packer call lld-link itself, with its sysroot and LLVM tools relative to the output?
