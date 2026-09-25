# Value rules, pinned outputs and leak checks

This note lists the steps that keep the pinned outputs true after a change to
lowering, the runtime or the library format. It gives the pattern of a leak
check and the traps of the value rules of locals, structs, tuples and
collections.

## Pinned outputs

Each file below pins what antic writes. A change that alters that output
writes the file again from this Mac, then reads the diff. Only what the change
should alter may move.

- `tests/emit-identity/programs.sha256` holds the assembly of every program of
  `tests/programs/` on the six targets. A new program adds six entries. A
  change to lowering may change existing ones. Write it with:

  ```bash
  cmake -DANTIC=build/host/antic -DRUNTIME=build/host/runtime \
    -DPROGRAMS=$PWD/tests/programs \
    -DMANIFEST=$PWD/tests/emit-identity/programs.sha256 \
    -DWORK=$PWD/build/host/tests/emit-identity -DWRITE=yes \
    -P tests/run_emit_identity.cmake
  ```

- `emit_identity` stops at the first program that does not compile. An
  untracked test that fails there hides the missing entries of every other
  new program. Run it again once the tree compiles.
- `tests/link-identity/return42.macos-arm64.sha256` changes when the runtime
  gains or loses a function. The failure of `link_identity_macos-arm64` prints
  the new digest.
- `ANTL_VERSION` of `src/antic/antl.h` goes up by one when the library file
  changes: a byte of a type form, or a field that `antl_tree.c` writes. The
  same commit changes:
  - the version byte of `scale_antl` in `tests/unit/test_modules.c`, and the
    refused version after it, `copy[4]` and its message;
  - `tests/modules/scale.antl.hex` and `tests/modules/generics/pick.antl.hex`.
    Run `antl_scale` and `antl_generic`, then write the hex of
    `build/host/tests/modules/com/example/scale.antl` and
    `build/host/tests/modules/pick.antl` in the layout of
    `tests/run_antl.cmake`, 16 bytes per line;
  - the format version that `CLAUDE.md` names.
- A version bump alone changes one byte of each hex file. A new field of every
  symbol of a tree adds bytes to `pick.antl.hex`.
- A `.err` file of `tests/errors/` is the output of `antic -S` with the path
  `tests/errors/` removed. Read every line against the intent before writing
  it.
- A changed message also changes `tests/unit/test_sema.c` and every `.err`
  file that holds it.
- New sources in `tests/` go through `anti fmt` before the suite, or
  `fmt_canonical` fails. A reformatted error source moves the lines its `.err`
  names.

## Leak checks

- A leak check counts the objects alive in a static atomic of a class:
  `static atomic live: int = 0;` after the instance fields. The function that
  makes one adds 1, its `destruct` subtracts 1, and the test prints the count
  after each case, which must read 0.
- Count the teardowns of the value that could be torn down twice, beside the
  blocks alive. A second teardown of a class value finds its `own` fields
  cleared, since the first stored zero there. The count of live blocks then
  stays right while `destruct` of the outer class runs twice. Give that class
  a `destruct` that counts its calls.
- A copy of an `own` field inside a class copy runs no constructor and
  dispatches no `copied` hook. A test that copies with `dup` adds the blocks
  it knows the copy made by hand, with a comment that says so.
- Run the new test on the compiler before the change. A test that passes
  there cannot fail, and its count is the wrong one.
- A program that uses `may fail` or `catch` imports `anti.lang`. Its dev-mode
  test then passes `anti_lang_dev.o` in `OBJECTS` and requires the fixture
  `lang_dev_object`. `let p = alloc(T, n) else { }` needs no import.

## Traps

- `sema_needs_teardown` of `sema_stmt.c` is the one rule of what owns
  something. `lower_type_needs_destruct` asks it. A new form of ownership goes
  there, and the refusal of `=`, the moves and the teardown follow it.
- A destructuring `let` holds the value in a hidden symbol and gives each part
  to a name. Only the names are torn down. `lower_let_value` checks
  `name_count` in both of its paths.
- The base of `anti.collection` moves elements as bytes through tuple locals,
  in `take_place` and the destructuring of its callers. A new teardown of
  tuples or structs reaches that code, and `std_collection_parts` finds a
  double teardown there.
- A channel, a `Mutex`, an object lock and a `Job` are `TYPE_STRUCT`. A rule
  that keys on `TYPE_STRUCT`, such as `dup` of a struct, leaves them out.
- A copy of a generic checks again only the nodes whose type holds a type
  parameter. A mark set on the generic node, such as `value` of `dup`, keeps
  its meaning in every copy. The tree of a library file must carry it.
- `execute_process` in a script of `tests/` passes `ENCODING NONE`, or
  `raw_output` fails.
- A file restored with `cp` right after a build may carry the time of its
  object, and `make` then keeps the old object. `touch` it before the build.
- The docs-style checker is for comments, notes and `.md` files. On
  `CMakeLists.txt` it reports old findings that are not errors of the change.
