# The out slot of a catch, tests and fixtures, and the two VMs

Three items: the `std_toml` segmentation fault of the Linux VM, the `tests` and
`fixtures` blocks with the runner of `anti test`, and a run of the whole suite on
the Windows VM with a check of its CodeView line table.

## The segmentation fault

It was never the error path. antic supplies the out pointer of a `catch` binding
over stack storage, and it never zeroed it. The callee writes its result
with `=`, which destroys what the place held and reads the table pointer to learn
whether it held anything. The bytes an earlier call left in the frame were that
table. `Document.read` then tore down whatever the frame carried, on the first and
successful read. The Mac's frame was zero at that depth and the Linux VM's was not.

`docs/anti-object-model.md` already gives a zero table to an unfilled element of
`alloc(T, n)`. The fix gives the same to the storage the compiler supplies, which is
one store before the call. Both specifications and the overview now say so.

The same three lines held a second defect. The binding of a `catch` was the one local
of class type that no teardown reached, against the rule at line 183 of the object
model. It is registered after the handler, where every path that arrives has a value in the
slot. A handler that leaves the block passes over storage the call never wrote, so its
zero table reaches no teardown, which traps on one.

No program under `tests/programs` reached that path, which is why it survived. The
emit manifest gained six digests and changed none. `program_out_slot` dirties a frame
and takes both paths, and it segmentation-faulted on the Mac before the fix.

## Tests and fixtures

`tests { }` and `fixtures { }` are module-level blocks of functions. The parser turns
each function into an ordinary module function carrying the block it came from, so no
pass after it learns the syntax. The driver drops them after parsing unless `--tests`
asked for them. A dev build, a release build and a `.antl` hold none of it by
construction rather than by a filter in each writer.

`anti test` is the third command of the tool. It reads each module with the compiler's
own lexer and parser, compiles it with its blocks, writes an Anti runner at
`anti/test/` of the work directory, links it and runs it, calling `driver_run` in its
own process as the sdk commands call their own code. The runner names each test to the runtime before it
calls it. A failed assertion then reports `FAIL module.test`, and after it the file
and the line the compiler wrote. `--release` runs the same tests as a whole
program with the assertions and the dev-mode checks off.

The two new tokens move the token enum, so the library format rose to 29 and its two
fixtures followed.

Eight entries went into `docs/decisions.md` under "Tests and fixtures", seven of them
`[provisional]`. The specification gives the blocks and the report line, and says
nothing about who writes the runner or where it lives.

## The hosts

Linux passed 407 of 407 at `928baa3`, `std_toml` and `emit_identity` among them.
Windows passed 386 of 387, skipping `sysroot_digest` as a Windows host always does.
The Mac passes 467, and both sanitizer suites pass 466.

`anti test` failed on Windows at first, because the runner spelled the object of a dev
build `.o` and a Windows target writes `.obj`. That is fixed and the suffix now comes
from the target.

The CodeView line table of a `-g` build was read with `llvm-readobj --codeview` of the
pinned tools. It names both sources in its `FileChecksums` and holds one
`FunctionLineTable` per function with the statement lines the DWARF test expects: 1, 3
and 4 for `step`, 3 and 5 for `helper`, 8, 10 and 11 for `main`.

## Questions

- `table_unset` fails on Windows and nowhere else, and it is not from this session. Its
  release check reads the assembly between `table.run:` and `table.main:`, which a
  Windows host mangles to `_A5table_run:` and `_A5table_main:`. The match is empty, so
  the test reports that release mode checks a table in a dispatch. The check is newer
  than `03d1064`, the last Windows run before this one, so it has never passed there.
  The fix is a pattern that takes both spellings. Shall I make it?
- `cmake --preset asan` on the Linux VM builds a runtime with no sysroot, and 102
  programs then fail to link. The presets read the sysroot from `build/sysroot`, which
  the Mac has and that VM does not, because its own build installed the sysroot into
  `build/runtime/sysroot`. The plain suite covered the host instead. This is build
  configuration, so it is yours to decide.
- A failed assertion aborts, so the run of a module stops at its first one and the
  tests after it do not run. The specification names the first failed assert, which is
  what this does. Running the rest would need a jump out of a test. Is stopping right?
- `anti test` takes `.anti` files and `-I` roots, since no manifest and no package
  layout exist. `docs/tooling.md` describes an `anti test` that also runs the programs
  under `test/`. That half is still unwritten.
