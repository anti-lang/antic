# The source tree

This note lists every file of the root, `src/`, `tests/unit/`, `tools/` and
`tests/runners/`, every file and `.cmake` script of `tests/` and every
directory of `tests/`, one row each, with the purpose its code shows. The
test `source_tree` refuses a file without a row, a path with two rows and a
row of a path that is gone. A commit that adds, moves or removes a file
changes its row in the same commit.

The choices that split a part of antic into its files stand under "Files of
the parser" and the sections after it in `docs/decisions.md`. The third
audit wrote the first form of these sentences, under "The sentence of each
file" of `docs/audit/summary.md`, beside what each header said at the time.

## The root

The files that the top level holds by "Repository layout" in `CLAUDE.md`.

| File | Purpose |
|---|---|
| `.editorconfig` | The whitespace, the line ends and the indent of every file for an editor. |
| `.gitattributes` | Checks every file out with line feeds, since a test compares the bytes of an output with its expected file. |
| `.gitignore` | Keeps `build/`, the private keys and the files a build or an editor writes out of git. |
| `CHANGELOG.md` | One entry per released version, the newest first, which the release script reads. |
| `CLAUDE.md` | The handover of the repository: the order of authority, the rules, the build, the layout, the tests and the state. |
| `CMakeLists.txt` | Builds antic, `anti`, the runtime of every target and level and the standard library, and lays out the runtime archive with its `bin/`. |
| `CMakePresets.json` | The five build trees `host`, `release`, `asan`, `ubsan` and `coverage`, with the test presets of each. |
| `LICENSE` | The MIT licence of the compiler and the tools. |
| `README.md` | What Anti is, how to install it and how to build this tree. |
| `c` | The link `./c` to `tools/compile.sh`. |
| `r` | The link `./r` to `tools/release.sh`. |

## The compiler, `src/antic/`

antic reads Anti sources and library files and writes LLVM IR text. opt
and llc of the pinned release turn it into objects, which lld links. A `.h` file whose row says "The interface of" declares what the `.c`
file of its name defines.

### The front end

The lexer, the parser, the syntax tree, the checked types and the checker.

| File | Purpose |
|---|---|
| `src/antic/ast.h` | The syntax tree of one module, the fields the checker adds to it, and the entry points of the dump and the walk. |
| `src/antic/ast_dump.c` | Writes the tree, untyped or with the checked types, one node per line for `--dump-ast` and `--dump-types`. |
| `src/antic/ast_walk.c` | The one walk over a body that every analysis searching a whole body or expression for a kind of node uses. |
| `src/antic/diagnostic.c` | Appends a formatted error, warning, safety check or doc warning to a growable list. |
| `src/antic/diagnostic.h` | The record of one diagnostic with its stable name, and the list of one compilation. |
| `src/antic/lexer.c` | Splits a validated UTF-8 source into tokens under caps on size, nesting and errors, decodes literals and doc comments, and lexes each `{expr}` with a nested lexer. |
| `src/antic/lexer.h` | The token kinds, the token and format-piece records, the entry points of the lexer, its three caps and the depth limit of the parser. |
| `src/antic/modpath.c` | Derives a module path and a root-relative file name from a source path, and reads the segments of a module path. |
| `src/antic/modpath.h` | The interface of `modpath.c`. |
| `src/antic/parser.c` | The helpers every parser file shares, and `parser_parse`, which reads the lines at module level. |
| `src/antic/parser.h` | The one entry point of the parser. |
| `src/antic/parser_clause.c` | Reads `allow` and `unchecked` and records the source each covers. |
| `src/antic/parser_decl.c` | The parts of a signature and of a body: marks, parameters, result, `may fail`, type parameters, members, nested types, and the guard and clauses of a field. |
| `src/antic/parser_expr.c` | Expressions, from the primary forms through postfix, unary, casts and precedence climbing, with `f"..."` and anonymous functions. |
| `src/antic/parser_item.c` | The items of a module, a class and a variant with their bodies and header clauses. |
| `src/antic/parser_parser.h` | The state of the parser and the functions its files share, each under the file that defines it. |
| `src/antic/parser_stmt.c` | Statements and blocks, the handlers of `catch` included. |
| `src/antic/parser_type.c` | Types, and the scan that tells type arguments after `<` from a comparison. |
| `src/antic/pattern.c` | Checks a pattern literal: compiles it through the PCRE2 wrapper of the runtime, numbers its groups, and reads it into a tree for `exponential-pattern` and the least match length. Reads the pieces of a replacement template for the checker. |
| `src/antic/pattern.h` | The interface of `pattern.c`. |
| `src/antic/sema.c` | The shared helpers of the checker, the resolution of types and signatures, the passes over the function bodies and over what crosses to C, and `sema_check`, which runs every pass in order. |
| `src/antic/sema.h` | The interface of the checker to the driver, lowering and the header: symbols, constants, the interface of a module, `sema_check` and the queries of the later passes. |
| `src/antic/sema_call.c` | Checks calls and members: fields and their visibility, the functions of a class, `construct`, errors and their handlers, `mul_high` and the calls of a plugin library. |
| `src/antic/sema_chain.c` | Takes a chain of declarations that name each other apart and resolves them leaves first. |
| `src/antic/sema_checker.h` | The state of the checker and the functions its files share, each under the file that defines it. |
| `src/antic/sema_class.c` | Checks the class model of every type with fields once the module is declared, with the fields a `construct` sets, and gives lowering and the C header the primary table. |
| `src/antic/sema_closure.c` | Checks an anonymous function as a function of its own, with its type from the target, what it captures, the writes and calls through a capture, a snapshot and a closure that would outlive a capture. |
| `src/antic/sema_const.c` | Evaluates constant expressions and gives each constant symbol its value once, in dependency order. |
| `src/antic/sema_copies.c` | The compiled copies: clones the checked tree of a generic per use, rechecks the open nodes, and rewrites the calls that name the copies. |
| `src/antic/sema_declare.c` | Every pass that declares the items of a module, with the checks of `provides`, `link` and a simd struct. |
| `src/antic/sema_doc.c` | The doc warnings of `anti check`, reading the module alone. |
| `src/antic/sema_export.c` | The checks of what crosses to C, export items and the pointers of an `extern fn`, and the interface of a checked module. |
| `src/antic/sema_expr.c` | Checks expressions and the conversions a value makes where it stands, and dispatches over the kinds of expression. |
| `src/antic/sema_generic.c` | The generics of the checker: type parameters, constraints, aliases, the copies of generic types, substitution and inference, and the strip of generics before the later passes. |
| `src/antic/sema_hash.c` | The checker's part of hashing and of the default `==`, and the default `equals` and `hash` of each class. |
| `src/antic/sema_operator.c` | Checks the binary operators, `in` and `??` through the operator table, and finds the `operator fn` functions and language hooks of a type. |
| `src/antic/sema_pattern.c` | The methods of `str`, `[]byte` and a match that become calls of `anti.regex` and `anti.text`, the groups of a literal as fields, the pattern literal with its checks, and `Regex.compile`. |
| `src/antic/sema_safety.c` | The checks of the thread-safe classes: the kind of a class, its base, the hidden lock, the guards and the reach of a field. |
| `src/antic/sema_simd.c` | The checks of simd structs: operators lane by lane, `as` with the C layout it compares, and the built-ins. |
| `src/antic/sema_stmt.c` | Checks statements and function bodies, and walks the functions a worker reaches. |
| `src/antic/sema_thread.c` | The atomic operations, `Mutex.new` and `destroy`, the channel operations, and the workers of `parallel` and `dispatch` with `join`. |
| `src/antic/sema_value.c` | The value rules: what owns something, what moves, and the copies that `=`, `let`, a literal and a call refuse. |
| `src/antic/types.c` | Builds and interns the checked types of a compilation, answers the predicates on them, declares the types of `anti.lang` the compiler knows, names types and measures how deep values nest. |
| `src/antic/types.h` | The interface of `types.c`, with the names of the library modules the compiler knows. |
| `src/antic/warnings.c` | The one table of warning and safety-check names, the application of `allow` and `unchecked`, and the promotion of warnings to errors. |
| `src/antic/warnings.h` | The interface of `warnings.c`. |

### The middle

The IR, lowering from the checked tree into it, the optimizer, the layout
and the pass over the whole program.

| File | Purpose |
|---|---|
| `src/antic/arith.c` | The arithmetic on constants of n bits that the checker, the optimizer and the layout share. |
| `src/antic/arith.h` | The interface of `arith.c`. |
| `src/antic/ir.c` | The construction of a module of the IR, its tables, and the appenders of instructions. |
| `src/antic/ir.h` | The typed three-address IR: its types, instructions, functions, globals and class records, and the builders that append to it. |
| `src/antic/ir_fold.c` | The one rule that folds an integer operation on constants, and the three cases that have no value. |
| `src/antic/ir_fold.h` | The interface of `ir_fold.c`. |
| `src/antic/ir_print.c` | The text form of a module of the IR, which `--dump-ir`, `--dump-opt` and the tests read. |
| `src/antic/ir_verify.c` | The check of the structural and type rules of a module, with definite assignment, the facts and the class records. |
| `src/antic/layout.c` | The layout of the aggregates of the IR for one target and the folding of its symbolic values, with the units of bitfields, the resolution of a function and the bytes of a constant global. |
| `src/antic/layout.h` | The interface of `layout.c`. |
| `src/antic/lower.c` | The helpers every file of lowering uses, the declarations of C and runtime functions, and `lower_module`. |
| `src/antic/lower.h` | The entry of lowering, a checked module into a module of the IR, and its options. |
| `src/antic/lower_check.c` | The text of a failed check, the checks of the integer operators and the check of an index against its length. |
| `src/antic/lower_class.c` | The functions written for a class: its init, the `construct` for C, the `get` of a singleton, the thunks and the tables of a new object. |
| `src/antic/lower_desc.c` | The descriptors, field and function records, tables and class records of classes, structs, variants, tuples, arrays and `?T`. |
| `src/antic/lower_eq.c` | The default `==` of a value and the `equals` of a class. |
| `src/antic/lower_error.c` | The error channel: a handled call, its handler, the guards of a `let` and `fail`. |
| `src/antic/lower_expr.c` | Expressions: a literal built in place, the operators, the conversions with their checks, and calls with their targets. |
| `src/antic/lower_function.c` | A function body with its parameters, hooks and lock, an anonymous function, and a closure with its context or snapshot. |
| `src/antic/lower_hash.c` | The default hash of a value and the `hash` of a class. |
| `src/antic/lower_hook.c` | The sites of the hooks, and which classes are traced. |
| `src/antic/lower_lowerer.h` | The state of lowering and the functions its files share, each under the file that defines it. |
| `src/antic/lower_owning.c` | The one teardown, copy and clear of a value, the destroy and copy of a class, and the preparation of a new object. |
| `src/antic/lower_pattern.c` | Pattern literals and the constructor that compiles them. |
| `src/antic/lower_place.c` | Places and addresses, the type an access reads memory as, constant data and `here`. |
| `src/antic/lower_simd.c` | The operations of simd structs, as one instruction, as one operation per lane or as a loop over the lanes. |
| `src/antic/lower_stmt.c` | Statements, loops, `switch`, the exit actions of a block and the slots of a function. |
| `src/antic/lower_sync.c` | Locks, `sync`, `select` and the operations of a channel and a Mutex. |
| `src/antic/lower_worker.c` | `parallel` and `dispatch` with their thunks and contexts, and `join`. |
| `src/antic/memcheck.c` | The checks of AddressSanitizer that `--memory-checks` puts before every load and store, their declarations and their insertion. |
| `src/antic/memcheck.h` | The interface of `memcheck.c`. |
| `src/antic/optimize.c` | The passes over one function, and the removal of what the entry cannot reach. |
| `src/antic/optimize.h` | The optimizer of the IR over a program, a module or one function, and the drop of the failure arms. |
| `src/antic/reach.c` | What the entries of a program reach, by one rule, and the walk that computes it. |
| `src/antic/reach.h` | The interface of `reach.c`. |
| `src/antic/rt_abi.c` | The tables behind `rt_abi.h`: the functions of the runtime that generated code calls and the records it writes. |
| `src/antic/rt_abi.h` | The contract with the runtime: the functions generated code calls and the records it writes. |
| `src/antic/whole.c` | The class model, devirtualisation, the merge of copies, the singleton check, the table facts and `whole_program`, which runs the passes. |
| `src/antic/whole.h` | The passes over the IR of the whole program and the class model they share. |
| `src/antic/whole_parts.h` | What the two files of the pass over the whole program share, each under the file that defines it. |
| `src/antic/whole_tables.c` | The tables the runtime reads: the registry, the default of the backtraces, the trampolines, the slots, the injections and the provides table. |

### The back end and the driver

The LLVM text, the runs of opt and llc, the links, the targets, the C header
and the driver that runs a compilation.

| File | Purpose |
|---|---|
| `src/antic/abi.c` | The classification of the parameters and the result of a function for the C convention of a target. |
| `src/antic/abi.h` | The interface of `abi.c`. |
| `src/antic/antic.c` | The walks over a source and a library file that `anti` asks for through `antic.h`. |
| `src/antic/antic.h` | What the `anti` tool may use of antic. |
| `src/antic/applesdk.c` | The newest SDK of the Command Line Tools that ld64.lld reads. |
| `src/antic/applesdk.h` | The interface of `applesdk.c`. |
| `src/antic/attributes.h` | The format attribute of antic, so that clang checks the arguments of every function that formats like printf. |
| `src/antic/coff.c` | The join of COFF objects into one relocatable object, and the names the linked members of a COFF archive define. |
| `src/antic/coff.h` | The interface of `coff.c`. |
| `src/antic/cpu.c` | The table of the processor levels, their features, their clang arch and the default of each target. |
| `src/antic/cpu.h` | The interface of `cpu.c`. |
| `src/antic/docpage.c` | The records of the page of `anti doc`, built from the interface or the syntax tree of a module. |
| `src/antic/driver.c` | The files of a compilation and the run of it through the front end, lowering, the passes and the back end, ending with the text, the library file or the header. |
| `src/antic/driver.h` | The options and the entry points of the driver, which `anti` compiles through. |
| `src/antic/driver_index.c` | The plugin index beside a plugin, written with the entries of the other libraries kept. |
| `src/antic/driver_library.c` | A library for C or a plugin: the archive with its package header and bundled runtime, or the shared library with its exports. |
| `src/antic/driver_link.c` | The link of a program: its facts, inputs, Windows paths, host exports and native libraries. |
| `src/antic/driver_parts.h` | What the files of the driver share, each under the file that defines it. |
| `src/antic/driver_search.c` | The library files of a compilation, found under the roots and loaded in dependency order, with the link lines they carry. |
| `src/antic/header.c` | The C header of a library for C, written from the interfaces of the module and its libraries. |
| `src/antic/header.h` | The interface of `header.c`. |
| `src/antic/linker.c` | The command lines of lld, the platform linkers and llvm-ar for a program, a shared library, an archive and a relocatable object of each target. |
| `src/antic/linker.h` | The interface of `linker.c`. |
| `src/antic/llvm_debug.c` | The debug metadata of the LLVM text: the compile unit, the files, the subprograms and the locations, and the spans the build id leaves out. |
| `src/antic/llvm_debug.h` | The interface of `llvm_debug.c`. |
| `src/antic/llvm_emit.c` | The module of LLVM IR text: the names of the symbols, the globals, the imports of a COFF plugin, the declarations, the metadata, the roots and the constructors, with the small texts of a package header, a bundle marker, a notice and the names of a `.def` file. |
| `src/antic/llvm_emit.h` | The interface of the translation of a laid-out module of the IR into LLVM IR text, which `llvm_emit.c` and its three files beside it define. |
| `src/antic/llvm_emit_call.c` | The C convention of the LLVM text: the classification, the parameters and the prologue of a function, the arguments and the result of a call, the atomic calls, the signature and the linkage. |
| `src/antic/llvm_emit_facts.c` | The facts for opt: the ranges of the loads, the `!tbaa` tree, the facts of the tables, the weights of a cold branch and the functions whose frame a stack trace keeps. |
| `src/antic/llvm_emit_function.c` | The text of a function: the operands and constants, the scalar, memory, wide and simd operations, the flags, each instruction and the definition. |
| `src/antic/llvm_emit_parts.h` | What the files of the LLVM text share, `struct emitter` and the functions of more than one file, each under the file that defines it. |
| `src/antic/llvm_run.c` | The runs of opt and llc on the text, with the options of each mode. |
| `src/antic/llvm_run.h` | The interface of `llvm_run.c`. |
| `src/antic/llvm_target.c` | The triple, the data layout, the relocation model and the processor attributes the text carries per target and level. |
| `src/antic/llvm_target.h` | The interface of `llvm_target.c`. |
| `src/antic/main.c` | The command line of antic and the run it asks the driver for, on a thread with a stack of its own. |
| `src/antic/notice.c` | The licence notice of a set of packages, and the identifier of a component of the runtime archive. |
| `src/antic/notice.h` | The interface of `notice.c`. |
| `src/antic/platform.c` | The platform layer of antic and `anti`: files, environment, arguments, paths, processes and the work thread, once per host. |
| `src/antic/platform.h` | The interface of `platform.c`. |
| `src/antic/target.c` | The table of the six targets, their names and the symbols of functions and of C names per object format. |
| `src/antic/target.h` | The interface of `target.c`. |
| `src/antic/userdirs.c` | The directories of a user-local install on each platform, and the runtime archive of the machine. |
| `src/antic/userdirs.h` | The interface of `userdirs.c`. |

### The library file and the leaf code

The library file, and the code that every part of antic and `anti` uses.

| File | Purpose |
|---|---|
| `src/antic/alloc.c` | The checked allocation of antic, which ends the run on a failure or an overflow. |
| `src/antic/alloc.h` | The interface of `alloc.c`. |
| `src/antic/antl.c` | The magic, the checks of the enums the tables store, two shared helpers and the top of the reader, `antl_header` and `antl_read`. |
| `src/antic/antl.h` | The public face of the library file: its version, the depth limit of a tree and the five entry points. |
| `src/antic/antl_io.c` | The primitives that write and read an integer, a count, a name and a C string, each with its check. |
| `src/antic/antl_io.h` | The inside of the library file: the writer, the reader and the functions its files share, each under the file that defines it. |
| `src/antic/antl_read.c` | The reader of the type table, the constants and the items, with the checks of each record. |
| `src/antic/antl_read_ir.c` | The reader of the IR, which moves each record to the indices of the program and merges the copies it has. |
| `src/antic/antl_tree.c` | The section of the generics, written, collected and read by one walker per kind of record. |
| `src/antic/antl_verify.c` | The rules of a tree the reader built, with the shape of a simd struct and the type of an IR constant. |
| `src/antic/antl_write.c` | The writer of the header, the type table, the constants, the items and the IR. |
| `src/antic/arena.c` | Memory handed out from blocks and freed all at once. |
| `src/antic/arena.h` | The interface of `arena.c`. |
| `src/antic/ptrset.c` | A set of pointers, a map from a pointer to a pointer and one to an index, over one table. |
| `src/antic/ptrset.h` | The interface of `ptrset.c`. |
| `src/antic/shared.h` | The names and the records that more than one part of antic shares, and `anti` through `antic.h`: the suffixes of its files, the layout of the runtime archive, the linker, the mode of LTO, the package record and the paths of the modules the compiler names. |
| `src/antic/say.c` | The one writer per tool of a message about a file, a tool or an option, with the name of the tool before it. |
| `src/antic/say.h` | The interface of `say.c`. |
| `src/antic/sha256.c` | The SHA-256 digest of a file, by the hash of the runtime. |
| `src/antic/sha256.h` | The interface of `sha256.c`. |
| `src/antic/text.c` | A byte buffer that grows and stays NUL-terminated, and the formatting of a message into a fixed buffer. |
| `src/antic/text.h` | The interface of `text.c`. |

## The `anti` tool, `src/anti/`

One file per command, and the code the commands share. Every file but
`main.c` is the static library `anti_core`, which the unit tests link.

| File | Purpose |
|---|---|
| `src/anti/bind.c` | `anti bind` and `anti bind --header`. |
| `src/anti/bind.h` | The interface of `bind.c`. |
| `src/anti/bindapi.c` | The reader of an API description into the model. |
| `src/anti/bindclang.c` | The run of clang on a header, and the reader of its AST and its preprocessed text into the model. |
| `src/anti/bindexpr.c` | The evaluator of a constant expression of C, and the constant it adds to the model. |
| `src/anti/bindexpr.h` | The interface of `bindexpr.c`. |
| `src/anti/bindmodel.c` | The model of a binding, with its memory, its warnings, the keyword test and the tables of what a bundled library links. |
| `src/anti/bindmodel.h` | The model of a binding, and the functions the files of `anti bind` give each other. |
| `src/anti/bindtype.c` | The fixed C types and the parser of a C type spelling. |
| `src/anti/bindwrite.c` | The choice of what a module holds, and the writer of the module, the shim and the two probes. |
| `src/anti/build.c` | `anti build` and `anti run`: a manifest into the files of `build/` and the deliverable of `dist/`. |
| `src/anti/build.h` | The interface of `build.c`. |
| `src/anti/check.c` | `anti check`, the four classes that write no artifact of a program. |
| `src/anti/check.h` | The interface of `check.c`. |
| `src/anti/cursor.c` | The cursor over one line of text from another machine. |
| `src/anti/cursor.h` | The interface of `cursor.c`. |
| `src/anti/deps.c` | The dependency graph of a project: the lock file, the index, the walk and the comparison of versions. |
| `src/anti/deps.h` | The interface of `deps.c`. |
| `src/anti/doc.c` | `anti doc`, the pages of HTML and Markdown from the records antic fills. |
| `src/anti/doc.h` | The interface of `doc.c`. |
| `src/anti/files.c` | The allocation helpers and the file helpers of the tool. |
| `src/anti/files.h` | The interface of `files.c`. |
| `src/anti/fmt.c` | `anti fmt`, the canonical form of a source and the run over the files. |
| `src/anti/fmt.h` | The interface of `fmt.c`. |
| `src/anti/jsontree.c` | A tree of a JSON document in a memory pool, built over the scanner of the runtime. |
| `src/anti/jsontree.h` | The interface of `jsontree.c`. |
| `src/anti/licensing.c` | `anti license` over the runtime archive, a project and a static archive, with the reader of an `ar` archive. |
| `src/anti/licensing.h` | The interface of `licensing.c`. |
| `src/anti/main.c` | The command line of `anti`, its usage text and one function per command. |
| `src/anti/manifest.c` | The reader of `anti.toml`, and the grammar of a name, a version and a digest. |
| `src/anti/manifest.h` | The interface of `manifest.c`. |
| `src/anti/memreport.c` | The run of a `--memory-checks` program whose report names its frames through Anti's symbolizer. |
| `src/anti/memreport.h` | The interface of `memreport.c`. |
| `src/anti/platform.c` | The calls that differ by host and that antic does not make. |
| `src/anti/platform.h` | The interface of `platform.c`. |
| `src/anti/project.c` | `anti new`, a project of the default layout. |
| `src/anti/project.h` | The interface of `project.c`. |
| `src/anti/repo.c` | The fetch from a repository into the cache of the user. |
| `src/anti/repo.h` | The interface of `repo.c`. |
| `src/anti/sdk.c` | `anti sdk export` and `import`: the stubs of Apple's SDK into a bundle and out of one. |
| `src/anti/sdk.h` | The interface of `sdk.c`. |
| `src/anti/symmap.c` | The text map of a symbols archive, written with the readers of `src/rt/symbols.c`, and the build id of a program. |
| `src/anti/symmap.h` | The interface of `symmap.c`. |
| `src/anti/syms.c` | `anti symbols inventory`, `check` and `resolve`. |
| `src/anti/syms.h` | The interface of `syms.c`. |
| `src/anti/test.c` | `anti test`, the runner of the `tests` and `fixtures` blocks of each module. |
| `src/anti/test.h` | The interface of `test.c`. |
| `src/anti/units.c` | The modules of a run in the order of their imports, the options every compile starts from, the reader of a library header and the links of the library files. |
| `src/anti/units.h` | The interface of `units.c`. |
| `src/anti/zip.c` | The writer and the reader of a zip archive, with a decoder of deflate. |
| `src/anti/zip.h` | The interface of `zip.c`. |

## The runtime, `src/rt/`

anti_rt, the runtime every program links. A header that antic or `anti`
reads as well says so in its row.

| File | Purpose |
|---|---|
| `src/rt/LICENSE` | The 0BSD text of the runtime, which the build copies to `licenses/anti_rt.txt` of the runtime archive. |
| `src/rt/assert.c` | The failure routine: one line to standard error, then exit, abort or return, and the failed assertion of a test. |
| `src/rt/atomic.c` | The atomic operations at a width that comes with the address, by the builtins of the compiler. |
| `src/rt/atomic.h` | The calls of `atomic.c`. |
| `src/rt/backtrace.c` | Whether `fail` captures frames, alone so a program that never fails links no default. |
| `src/rt/call.c` | `reflect.call` over the trampolines of a program. |
| `src/rt/cast.c` | The traps of a failed checked cast and of an object whose table is not set. |
| `src/rt/check.c` | The traps of a failed dev-mode check and of a collection changed during a walk. |
| `src/rt/conf.c` | The keys, the layers, the options and the file of the runtime configuration, which hands each line of `[injections]` to `inject.c`. |
| `src/rt/conf.h` | The keys of `[runtime]`, the layer each value came from and the options under `--anti.` that name them. |
| `src/rt/conf_include.h` | The bounds and the messages of the include walk, shared with `anti symbols`. |
| `src/rt/cpu.c` | The table of processor levels, and the check of the machine against the level of the runtime. |
| `src/rt/cpu_level.h` | The processor levels, shared by antic and the runtime. |
| `src/rt/digest.c` | SHA-256 for the runtime, antic and `anti`. |
| `src/rt/digest.h` | The calls of `digest.c`, in pieces and over a stream. |
| `src/rt/errno.c` | The value of `errno` for an `extern fn`. |
| `src/rt/f16.c` | The two `f16` conversions a program calls where the processor has no instruction. |
| `src/rt/f16.h` | The conversions between `f16` and `f32`, shared with antic. |
| `src/rt/fail.h` | The failure routines: the one form of every failure the runtime reports, and the failed assertions, checks and casts of a program. |
| `src/rt/fs.c` | The C side of `anti.fs`: open, size, read whole, list, remove and rename. |
| `src/rt/grow.h` | The one growth rule of a block of the C library. |
| `src/rt/hash.c` | The hash of bytes and the seed of the hashing collections. |
| `src/rt/hash.h` | The constants and the last step of a hash, shared with antic. |
| `src/rt/hooks.c` | What a hook site does: the installed handler, then the object's own hook. |
| `src/rt/hooks.h` | The calls of the nine hooks and of `Trace.install`. |
| `src/rt/init.c` | The start flag of the runtime and the option of the backtrace. |
| `src/rt/inject.c` | The start of the injections: the lines that name a library for an interface, and the providers the loader takes from those libraries before `main`. |
| `src/rt/inject.h` | The interface of `inject.c`, with the interfaces a program injects. |
| `src/rt/io.c` | The write and the exit of `anti.io` through the C streams. |
| `src/rt/json.c` | A scanner of JSON text at the level of tokens, which builds nothing. |
| `src/rt/json.h` | A scanner of JSON text at the level of tokens and positions. |
| `src/rt/license.c` | The licence text and the runtime version, read from the notice of the program. |
| `src/rt/license.h` | The markers of the notice and its build id line, shared with antic and `anti`. |
| `src/rt/license_stub.c` | The empty licence text and version of a bundled static library. |
| `src/rt/loaded.c` | The slots of the open libraries, their lock and the count of live objects of each. |
| `src/rt/lock.c` | The Mutex, the hidden lock of a synchronized object and the lock orders of a dev build. |
| `src/rt/mem.c` | Aligned memory for `anti.mem`, the out-of-memory end and `anti_rt_grow`. |
| `src/rt/object.c` | The C behind `anti.lang.Object`: the functions of the root, the walk that writes JSON and text, and delete, destroy, dup and give. |
| `src/rt/object.h` | The layout of descriptors, field records and tables, and the calls of `object.c` and `snapshot.c`. |
| `src/rt/patterns.c` | The run-time side of `anti.regex`: literals, messages, searches, groups, replace, patch and stops. |
| `src/rt/platform.h` | What the runtime asks of the system, apart from the clocks, the error texts and the kept block. |
| `src/rt/platform_entry.c` | The entry point of a Windows program, in an object only the link of a program pulls in. |
| `src/rt/platform_posix.c` | The platform layer on macOS and Linux. |
| `src/rt/platform_stdio.c` | The printf and scanf families and `atexit` of a Windows target, weak and never bitcode. |
| `src/rt/platform_windows.c` | The platform layer on Windows, and the static part of the C runtime. |
| `src/rt/plugin.c` | Loading a library that provides interfaces: the checks, the objects and discovery through the index. |
| `src/rt/plugin.h` | The table a plugin carries, the slots of the host and the calls of the loader. |
| `src/rt/plugin_index.h` | The file name of the index of plugins, shared with antic and `anti`. |
| `src/rt/reflect.c` | The walks of a descriptor behind `anti.reflect`, with `get` and `set` of a field. |
| `src/rt/reflect.h` | The layout of `Value`, the reasons, the calls of `reflect.c` and the trampoline table. |
| `src/rt/regex.c` | The compile of a text and a byte pattern with PCRE2, the rewrite of a byte pattern, and the groups and template pieces. |
| `src/rt/regex.h` | What `regex.c` and `patterns.c` share with antic: the compile, the groups, the template pieces, the literals, the layouts of `Match` and of `anti.regex` and the codes of a search, each group under the file that defines it. |
| `src/rt/registry.c` | `reflect.new` and `Object.deserialize` over the registry of classes. |
| `src/rt/registry.h` | The records of the registry and its two calls. |
| `src/rt/rt.h` | The start of the runtime in `init.c`, and the option of the backtrace that the configuration sets. |
| `src/rt/signal.c` | The functions a program registers for signals, and their delivery. |
| `src/rt/signals.h` | The two console signals and the two calls of `signal.c`. |
| `src/rt/snapshot.c` | The heap snapshots of `snapshot fn` and their count. |
| `src/rt/snapshot.h` | The calls of `snapshot.c`. |
| `src/rt/start.c` | The C `main`: the processor check, init, the arguments, the option pass, the program and its status. |
| `src/rt/std.h` | The C functions behind `anti.io`, `anti.text`, `anti.license`, `anti.mem` and `anti.fs`, with `struct anti_text` and the value of `errno`. |
| `src/rt/str.h` | The layout of a `str` as a function of the runtime takes and gives it: the bytes and their count. |
| `src/rt/symbols.c` | Bounds-checked readers of ELF and Mach-O symbol tables, the debug map and DWARF line tables. |
| `src/rt/symbols.h` | The calls of `symbols.c`. |
| `src/rt/sync.c` | The channels of `chan T` and `select`. |
| `src/rt/sync.h` | The calls of locking and channels that generated code makes. |
| `src/rt/text.c` | The C behind `anti.text`: text helpers, the buffer of a Builder, floats to text and text to floats. |
| `src/rt/threads.c` | The worker pool behind `parallel`, `dispatch` and `join`. |
| `src/rt/threads.h` | The calls of `parallel`, `dispatch` and `join`. |
| `src/rt/toml.c` | One pass over a subset of TOML that gives a flat list of dotted keys. |
| `src/rt/toml.h` | A read of the TOML subset every TOML file of Anti needs. |
| `src/rt/trace.c` | The module and the build id of a frame, the text of a trace and the choice of a symbol reader. |
| `src/rt/trace.h` | The layouts and the calls behind `anti.lang.StackTrace`. |
| `src/rt/utf.c` | UTF-8 and UTF-16 conversion and the split of a Windows command line. |
| `src/rt/utf.h` | The calls of `utf.c`. |

## The standard library, `src/std/`

The modules of the standard library, in Anti. The header of a module is its
`//!` comment.

| File | Purpose |
|---|---|
| `src/std/LICENSE` | The 0BSD text of the library. |
| `src/std/anti/args.anti` | A command line declared once, parsed and read by name. |
| `src/std/anti/collection.anti` | The interfaces of walking, the bases `Collection<T>` and `Indexed<T>`, the failures `NotOne` and `Changed`, `Snapshot<T>`, `Copies<T>`, the hashes, the writers of a map entry and the stable sort. |
| `src/std/anti/collection/concurrent.anti` | `ConcurrentMap` and `SpscRing`. |
| `src/std/anti/collection/deque.anti` | `Deque<T>`, a ring that grows at both ends. |
| `src/std/anti/collection/grid.anti` | `Grid<T>`, a rectangle of cells that never changes its size. |
| `src/std/anti/collection/list.anti` | `List<T>`, with `sort`, `sort_by` and `map`. |
| `src/std/anti/collection/map.anti` | `Map` and `HashMap`, their two tables for any element, their walks and the sort of serialized texts. |
| `src/std/anti/collection/pool.anti` | `Slots<T>`, `Handle<T>`, `Pool<T>` and the walk `SlotsWalk<T>`. |
| `src/std/anti/collection/queue.anti` | `PriorityQueue<T>`, a binary heap, and its walk. |
| `src/std/anti/collection/ring.anti` | `Ring<T, N>`, a ring of fixed room. |
| `src/std/anti/collection/set.anti` | `Set` and `HashSet` on the tables of the maps, and `BitSet`. |
| `src/std/anti/collection/sorted.anti` | `SortedMap` and `SortedSet` over one B-tree in one block. |
| `src/std/anti/collection/synchronized.anti` | Four synchronized classes around a plain collection each, with a version per element. |
| `src/std/anti/collection/tree.anti` | `Tree<T>`, a hierarchy on the slots of the pool. |
| `src/std/anti/config.anti` | The settings of the program: the interface `Config` and `FileConfig`. |
| `src/std/anti/debug.anti` | `backtrace`, the short form of `StackTrace.capture`. |
| `src/std/anti/error.anti` | `SystemError`, `on_fatal` and `check`, beside the `Error` of `anti.lang`. |
| `src/std/anti/fs.anti` | Files by handle and by whole file, and the interface `FileSystem` with its default. |
| `src/std/anti/io.anti` | Console output and the exit of a program. |
| `src/std/anti/json.anti` | Writing and reading the members of the JSON that `serialize` writes. |
| `src/std/anti/lang.anti` | The root that imports nothing: the types the compiler knows by name, `Error` with its text, the trace of a stack, the hook handler and the hash seed. |
| `src/std/anti/license.anti` | The licence notice of the program. |
| `src/std/anti/log.anti` | Levelled logging: sinks, the interface `Logger`, the logger of the program and the file that configures it. |
| `src/std/anti/mem.anti` | The interface `Allocator`, its two implementations, and `Shared<T>`. |
| `src/std/anti/os.anti` | Signals, and the three per-user directories of the platform. |
| `src/std/anti/plugin.anti` | Loading a shared library and asking it for the class it provides. |
| `src/std/anti/random.anti` | The generator `Random`, the interface `Source` and the shared default. |
| `src/std/anti/reflect.anti` | Reading the descriptor of a class: fields, functions, `get`, `set`, `new`, `call` and `Value`. |
| `src/std/anti/regex.anti` | The run-time half of patterns: the errors, `compile`, the iterators, every function the pattern methods call, and `patch`. |
| `src/std/anti/runtime.anti` | `configure` and `get` of the runtime configuration. |
| `src/std/anti/simd.anti` | A header alone: the three mask functions the compiler knows by name. |
| `src/std/anti/text.anti` | Functions on `str`, and `Builder`, which collects bytes and formats values. |
| `src/std/anti/time.anti` | `Duration`, the two clocks, and the interface `Clock` with its default. |
| `src/std/anti/toml.anti` | Reading the TOML subset into a flat `Document`. |
| `src/std/anti/trace.anti` | The seven stock handlers of the hooks, `install`, `start` and their tests. |

## The recipes of the native libraries, `src/native/`

The recipes that build the third-party C libraries of the runtime archive.
The sources are downloaded under `build/deps/` and never tracked.

| File | Purpose |
|---|---|
| `src/native/CMakeLists.txt` | Builds the third-party libraries of the runtime archive for every target from the pinned sources with the pinned clang, one recipe after another. |
| `src/native/get-source.cmake` | `antic_get_source`, the fetch of a pinned source of the runtime archive, checked against the digest of its pin and unpacked. |
| `src/native/mbedtls.cmake` | Builds Mbed TLS for every target, with its tests. |
| `src/native/media.cmake` | Holds the targets, the flags and the glibc link test of miniaudio and raylib. |
| `src/native/mimalloc.cmake` | Builds mimalloc for both musl targets at every level. |
| `src/native/miniaudio.cmake` | Builds miniaudio for every media target, with its tests. |
| `src/native/pcre2-files.cmake` | Lists the sources and the definitions of PCRE2 for the build and the packer. |
| `src/native/pcre2-source.cmake` | Fetches PCRE2, writes its generated files and builds the copy antic links. |
| `src/native/pcre2.cmake` | Builds PCRE2 and the regex code of the runtime for every target, with their tests. |
| `src/native/raylib-source.cmake` | Fetches the pinned raylib source as `ANTIC_RAYLIB_SOURCE` before `tests/` and `src/native/` read it. |
| `src/native/raylib.cmake` | Builds raylib for every media target, with its tests and the audio link test. |
| `src/native/sqlite.cmake` | Builds SQLite for every target, with its tests. |
| `src/native/warnings.cmake` | Holds the warning flags of each third-party library. |

## The tests, `tests/`

`CLAUDE.md` says under "Tests" which directory a new test goes in.

### The shared files

The files directly under `tests/`.

| File | Purpose |
|---|---|
| `tests/CMakeLists.txt` | Builds the C programs of the tests and registers every test of the suite but those of `src/native/`, in sections by kind after the helpers they share. |
| `tests/anti_messages.cmake` | The cases of the messages of `anti new`, `anti license` and `anti sdk`, which have no directory under `tests/`. |
| `tests/antic_messages.cmake` | The cases of the messages of the driver of antic that stand at no position of a source. |
| `tests/binary_stdio.h` | Puts the standard output and error of a C test program into binary mode on Windows before `main`. |
| `tests/build_id.cmake` | `read_build_id`, the build id of a program or of its debug link, from the line `build` of its licence notice. |
| `tests/diagnostics-open.txt` | The messages of antic and `anti` that no test expects, each with the reason it stays, which `diagnostics_covered` reads. |
| `tests/known-failures.txt` | The tests the suite expects to fail, each with the id of its finding and one sentence. |
| `tests/known_failures.cmake` | The reader of `tests/known-failures.txt` that `tests/CMakeLists.txt` and `run_known_failures.cmake` share. |
| `tests/options-open.txt` | The options of antic and the commands and options of `anti` that no test passes, which `options_covered` reads. It holds no line. |
| `tests/package_tree.cmake` | The package of the host that `package_pack` unpacked, and the runs of its programs alone, for the runners `run_package_*.cmake`. |
| `tests/pinned_clang.cmake` | Puts the pinned clang of the build first on `PATH` for a runner whose cases run `anti bind --clang`. |
| `tests/program_output.cmake` | The functions a runner includes to run antic, `anti` and a program with the output in files, to compare the status and the bytes, and to fake a release. |
| `tests/release_copy.cmake` | The clone of the tree with a bare origin and a bumped version that the tests `release_*` run `./r` on, and its first `./r --dry-run`. |

### The directories

The data files of a directory have no row of their own.

| Directory | Purpose |
|---|---|
| `tests/abi/` | C sources, Anti programs and expected outputs that compare the layouts and the calls of C with those of Anti, and the link probes of the native libraries. |
| `tests/anti-build/` | The projects that `anti build`, `anti run` and `anti license` run over. |
| `tests/anti-symbols/` | The project `tracer` of `anti symbols`. |
| `tests/anti-test/` | The modules that `anti test` runs. |
| `tests/bench/` | The benchmark programs with their C twins, outside the suite, and `ablate/`, the measurement of `docs/work-order-llvm-optimization.md`. |
| `tests/bind/` | C headers, `raylib_api.json` and the expected binding modules of `anti bind`. |
| `tests/check/` | The projects of `anti check`. |
| `tests/clib/` | Libraries for C, the C and C++ programs that link them and their expected outputs. |
| `tests/conf/` | Runtime configuration files, the programs that read them and the patterns their output must match. |
| `tests/doc/` | The fixture of `anti doc`, its expected pages and `antl_rename.c`. |
| `tests/dump/` | Sources and the expected dumps of the stages of antic, the expected C headers, the unwind data and the list of targets. |
| `tests/emit-identity/` | The manifest of the digests that `emit_identity` compares. |
| `tests/errors/` | Programs antic refuses, each with its expected messages. |
| `tests/fmt/` | Sources and their canonical form for `anti fmt`. |
| `tests/framework/` | One program that links CoreFoundation. |
| `tests/inject/` | The program, the projects and the patterns of the injection tests. |
| `tests/link-identity/` | The digest of one linked program and the programs of the address and dead-code tests. |
| `tests/matrix/` | The matrix of constructs and contexts: a program with its expected output or a refusal with its messages, each registered by its file name. |
| `tests/modules/` | Programs of more than one module, two library files as hex and `poke_byte.c`. |
| `tests/opt/` | Sources with their expected IR before and after the optimizer. |
| `tests/plugin/` | The hosts and the libraries of the plugin tests, with the versions of one interface. |
| `tests/programs/` | Programs whose subject is one feature, with their expected exit status and output. |
| `tests/raw/` | A C program and an Anti program that write a CR, an LF and a NUL. |
| `tests/std/` | Programs whose subject is a module of the standard library, with their expected output. |
| `tests/trace/` | Programs of hooks, tracing and stack traces with the patterns their output must match. |
| `tests/traps/` | Programs that must stop: the dev-mode checks and the programs of `--memory-checks`. |

### The tables of messages

Each holds one case per message of a command, which
`tests/runners/run_anti_messages.cmake` runs.

| File | Purpose |
|---|---|
| `tests/anti-build/messages.cmake` | The cases of the messages of `anti build` and `anti run`. |
| `tests/anti-symbols/messages.cmake` | The cases of the messages of `anti symbols`. |
| `tests/anti-test/messages.cmake` | The cases of the messages of `anti test`. |
| `tests/bind/messages.cmake` | The cases of the messages of `anti bind`. |
| `tests/check/messages.cmake` | The cases of the messages of `anti check`. |
| `tests/doc/messages.cmake` | The cases of the messages of `anti doc`. |
| `tests/fmt/messages.cmake` | The cases of the messages of `anti fmt`. |

### The unit tests, `tests/unit/`

`unit_<file>` runs the tests of `test_<file>.c`.

| File | Purpose |
|---|---|
| `tests/unit/` | The C unit tests that link the code of antic, `anti` and the runtime. |
| `tests/unit/check.h` | The two assertion macros, which count a failure and go on. |
| `tests/unit/path_rules.h` | The cases of the path rules that both platform layers are run over. |
| `tests/unit/pipeline.c` | The stages that `pipeline.h` declares. |
| `tests/unit/pipeline.h` | The stages the unit tests run a source through, for one module or a compilation of several, and the IR print without the types of the runtime. |
| `tests/unit/plugin_bad.c` | A shared library with a writable plugin table, which `test_plugin_table.c` damages and loads. |
| `tests/unit/plugin_table.c` | A shared library with an empty plugin table, which `test_plugin_race.c` and `test_platform.c` load. |
| `tests/unit/symbols_probe.c` | A program that looks a function up in a binary of another target with the readers of `src/rt/symbols.c`. |
| `tests/unit/syms.err` | The standard error that the group `syms` of `test_syms.c` writes, which `unit_syms_messages` compares. |
| `tests/unit/test_abi.c` | `abi_classify`, one case per rule of the calling conventions. |
| `tests/unit/test_alloc_oom.c` | A program that asks one allocation helper of antic for more memory than a host gives, for `run_alloc_oom.cmake`. |
| `tests/unit/test_arith.c` | The upper half of a product and the saturating operations of `arith.c` at every width. |
| `tests/unit/test_bind.c` | The parser of C type spellings and the evaluator of constant expressions of `anti bind`, and a binding written from a description of rlparser. |
| `tests/unit/test_coff.c` | The join of COFF objects and the exports of an archive. |
| `tests/unit/test_commands.c` | `anti check`, `anti doc`, `anti build`, `anti run` and `anti test` in the process on the project of `anti new`, and a plugin built through `driver_run`. |
| `tests/unit/test_conf_race.c` | `rt.configure` replacing a value while another thread reads it. |
| `tests/unit/test_contract.c` | The layouts and the numbers that a module of the standard library and the runtime both write, read from both sides. |
| `tests/unit/test_cpu.c` | The level table of antic, the names of `--cpu` and the start-up check of the runtime. |
| `tests/unit/test_deps.c` | The manifest, the version constraints and the repository URLs of `anti build`. |
| `tests/unit/test_deserialize.c` | `Object.deserialize` of `src/rt/registry.c` against malformed and cut-off text. |
| `tests/unit/test_f16.c` | The conversions between `f16` and `f32`. |
| `tests/unit/test_files.c` | The file helpers of `anti` and the walk over a directory tree. |
| `tests/unit/test_files_oom.c` | A program that asks one allocation helper of `anti` for more memory than a host gives, for `run_files_oom.cmake`. |
| `tests/unit/test_float_read.c` | The reader and the writer of the text of a float in `src/rt/text.c`. |
| `tests/unit/test_fmt.c` | `anti fmt` over two sources the parser would refuse. |
| `tests/unit/test_fs.c` | `anti_rt_fs_read`. |
| `tests/unit/test_hash_draw.c` | The number of the walk order of a hashed collection and the seed of the hashes when the system gives no random bytes. |
| `tests/unit/test_header.c` | The C header antic writes for a library, for owning structs and simd lanes. |
| `tests/unit/test_interface.c` | The walks of `antic.h` that the commands of `anti` call in place of the lexer, the parser and the library reader, and the doc page of `docpage.c`. |
| `tests/unit/test_ir.c` | The IR built by hand, its printed form and its verifier. |
| `tests/unit/test_json.c` | The JSON scanner on malformed input. |
| `tests/unit/test_layout.c` | The sizes, alignments, bitfields and refusals of `layout.c`. |
| `tests/unit/test_lexer.c` | The tokens, the literals and the errors of the lexer. |
| `tests/unit/test_licensing.c` | The readers of the licence notice in a binary and of the record of upstream sources, of `src/anti/licensing.c`. |
| `tests/unit/test_link.c` | The command lines of the linker per target and mode, the names of an SDK, the markers of the notice and the notice between them. |
| `tests/unit/test_llvm_emit.c` | `llvm_emit_module`, by the sections of the work order of the LLVM back end. |
| `tests/unit/test_llvm_run.c` | The run of opt and llc of the runtime archive by `llvm_run.c`, in dev and release mode and on text they refuse. |
| `tests/unit/test_llvm_target.c` | The triple, the relocation model and the function attributes per target and level. |
| `tests/unit/test_lower.c` | The IR that lowering writes for a source, and the table of the runtime functions against the prototypes of `src/rt/`. |
| `tests/unit/test_main.c` | The table of the entry functions of the unit tests by file, and the `main` that runs the files one argument names. |
| `tests/unit/test_memcheck.c` | The checks of AddressSanitizer that `--memory-checks` puts before a load and a store, and the declarations they need. |
| `tests/unit/test_modules.c` | The library file written, read back and damaged, the imports across modules and the module paths. |
| `tests/unit/test_nullable.c` | The rules of nullable pointers in the checker. |
| `tests/unit/test_optimize.c` | The passes of `optimize.c` over the IR of a source. |
| `tests/unit/test_parser.c` | The tree the parser builds, its errors and its depth limit. |
| `tests/unit/test_pattern.c` | The reader of a pattern literal of `src/antic/pattern.c`: the fewest bytes a match or a group can take. |
| `tests/unit/test_platform.c` | The platform layer of the runtime on the host. |
| `tests/unit/test_plugin_race.c` | The table of open libraries under threads that load and unload at once. |
| `tests/unit/test_plugin_table.c` | The loader of the runtime against a damaged plugin table and a damaged index. |
| `tests/unit/test_project.c` | `anti new` and the sources of a project, of `src/anti/project.c`. |
| `tests/unit/test_regex.c` | The rewriter of a byte pattern of `src/rt/regex.c` on every prefix of one pattern, and a group named by a text that holds a NUL. |
| `tests/unit/test_rt_bounds.c` | The growth of a block, the size of a text builder and atomic subtraction at the minimum of each width. |
| `tests/unit/test_rt_oom.c` | A program that runs the text builder, the copy of an object and the copy of an owned buffer out of memory, for `run_rt_oom.cmake`. |
| `tests/unit/test_sema.c` | What the checker accepts, refuses and warns of. |
| `tests/unit/test_sha256.c` | The SHA-256 of `src/rt/digest.c` over inputs given in pieces, and the digest of a file. |
| `tests/unit/test_signal.c` | The signal table of the runtime under threads that register at once. |
| `tests/unit/test_symbols.c` | The readers of `src/rt/symbols.c`: the names of COFF, the sections and line tables of ELF and the symbols of Mach-O. |
| `tests/unit/test_syms.c` | `anti symbols` over archives and programs that name nothing. |
| `tests/unit/test_sync.c` | The rules of locking and channels in the checker. |
| `tests/unit/test_target.c` | The mangled names, the C names and the block labels of `target.c`, and the target of the host. |
| `tests/unit/test_text.c` | The text buffer of antic and the cut of a long line of a message. |
| `tests/unit/test_toml.c` | The TOML subset of `src/rt/toml.c`. |
| `tests/unit/test_tool_platform.c` | The platform layer of antic and `anti` on the host. |
| `tests/unit/test_types.c` | The type table and the pointer sets of `ptrset.c`. |
| `tests/unit/test_units.c` | The modules of a run of `src/anti/units.c`: the options, the file of a module, one module read and the order of the imports. |
| `tests/unit/test_userdirs.c` | The three user directories under an environment the test gives. |
| `tests/unit/test_utf.c` | The UTF-8 of `src/rt/utf.c`: the repair of bytes, the conversion from UTF-16, a Windows file name, the split of a command line and a scalar value encoded and decoded. |
| `tests/unit/test_variant.c` | The rules of sum types in the checker. |
| `tests/unit/test_whole.c` | The pass over the whole program. |
| `tests/unit/test_zip.c` | The zip reader of `anti symbols` on damaged archives. |
| `tests/unit/zip.err` | The standard error that the group `zip` of `test_zip.c` writes, which `unit_zip_messages` compares. |

### The runners, `tests/runners/`

Each runner runs as `cmake -P`, and its header names the values it takes.

| File | Purpose |
|---|---|
| `tests/runners/` | The runners `run_*.cmake` of the tests. |
| `tests/runners/run_address_tables.cmake` | Checks that an object and the runtime archives of a target carry the address-significance table. |
| `tests/runners/run_alloc_oom.cmake` | Runs the allocation helpers out of memory and expects status 70. |
| `tests/runners/run_anti_bind.cmake` | Runs one case of `anti bind`. |
| `tests/runners/run_anti_build.cmake` | Drives `anti build`, `anti run` and `anti new` over a project of two modules. |
| `tests/runners/run_anti_build_cases.cmake` | Drives `anti build` over one case of what it delivers and caches. |
| `tests/runners/run_anti_build_deps.cmake` | Drives `anti build` over a path dependency and a `file://` repository. |
| `tests/runners/run_anti_build_framework.cmake` | Builds a project that names a framework and checks the program loads it. |
| `tests/runners/run_anti_build_inputs.cmake` | Feeds `anti build` a malformed index and lock file. |
| `tests/runners/run_anti_build_rerun.cmake` | Runs and rebuilds a program in turns on a Windows host. |
| `tests/runners/run_anti_build_resolve.cmake` | Checks the resolver and the search over more than one repository. |
| `tests/runners/run_anti_build_running.cmake` | Builds a changed program while the old one runs. |
| `tests/runners/run_anti_check.cmake` | Drives `anti check` over fixture projects and the standard library. |
| `tests/runners/run_anti_doc.cmake` | Drives `anti doc` over a fixture in its forms. |
| `tests/runners/run_anti_doc_page.cmake` | Writes the page of one module with `anti doc` and compares it with a file. |
| `tests/runners/run_anti_fmt.cmake` | Checks the canonical form of one file and `--check`. |
| `tests/runners/run_anti_fmt_write.cmake` | Checks that `anti fmt` leaves a source whole when a write fails. |
| `tests/runners/run_anti_interface.cmake` | Checks by its includes that anti reaches antic through the interface alone. |
| `tests/runners/run_anti_internal.cmake` | Runs check, build, test and doc over a project with an internal item. |
| `tests/runners/run_anti_memory_checks.cmake` | Drives `anti run` and `anti test` with `--memory-checks`. |
| `tests/runners/run_anti_memory_dll.cmake` | Cross builds for windows-x86_64 with memory checks and looks for the DLL. |
| `tests/runners/run_anti_messages.cmake` | Runs one case of a table of messages of `anti` or of the driver of antic, and compares the whole standard error. |
| `tests/runners/run_anti_options.cmake` | Runs `anti` with one option beside the run without it, and names what differs. |
| `tests/runners/run_anti_sdk.cmake` | Checks `anti sdk export` and `anti sdk import`. |
| `tests/runners/run_anti_symbols.cmake` | Drives `anti symbols inventory`, `check` and `resolve`. |
| `tests/runners/run_anti_symbols_input.cmake` | Checks that `anti symbols resolve` refuses numbers beyond 64 bits. |
| `tests/runners/run_anti_test.cmake` | Drives `anti test` over fixture modules. |
| `tests/runners/run_anti_test_module.cmake` | Runs `anti test` over one module of `tests/anti-test/` in dev mode. |
| `tests/runners/run_antic_calls.cmake` | Refuses a runner that runs antic in an `execute_process` of its own instead of `antic_run` or `antic_build`. |
| `tests/runners/run_antic_options.cmake` | Runs antic with one option beside the run without it, and names what differs. |
| `tests/runners/run_antic_stack.cmake` | Compiles the deepest accepted expression on a small stack. |
| `tests/runners/run_antl.cmake` | Compares the bytes of a library file with a hex listing. |
| `tests/runners/run_antl_verify.cmake` | Changes one byte of a library file and expects the refusal of the verifier. |
| `tests/runners/run_asm.cmake` | Writes assembly with `-S` and assembles it with llvm-mc. |
| `tests/runners/run_asserts.cmake` | Checks `assert` in each build mode and through a library file. |
| `tests/runners/run_atomic_load.cmake` | Checks the instruction of the atomic load in an ARM64 runtime. |
| `tests/runners/run_banned_calls.cmake` | Refuses a call of a function that rule 9 of `docs/c-guidelines.md` bans in a file of antic, of the runtime or of anti. |
| `tests/runners/run_bench.cmake` | Builds and times the benchmark programs and their C twins. |
| `tests/runners/run_build_id.cmake` | Checks that links of one program share a build id. |
| `tests/runners/run_build_id_reader.cmake` | Tests `read_build_id` of `tests/build_id.cmake` on a file with one build line and on files without one. |
| `tests/runners/run_build_tool_page.cmake` | Compares the commands and options that the site page of the build tool names with the usage of anti and of antic. |
| `tests/runners/run_check.cmake` | Builds a check program in both modes and expects the dev build to stop. |
| `tests/runners/run_check_cpu.cmake` | Tests that `tools/check-cpu.cmake` refuses a package that lacks a level of `tools/cpu-levels`. |
| `tests/runners/run_checks.cmake` | Checks `--checks`, `--no-checks` and a library whose checks follow the build. |
| `tests/runners/run_classlib.cmake` | Compiles a library with a class and a client of it. |
| `tests/runners/run_clib.cmake` | Runs one case of the libraries for C: a library built with `antic --lib` and the C programs that link it. |
| `tests/runners/run_codeview_empty.cmake` | Checks that a module without a function writes no empty CodeView subsection of symbols. |
| `tests/runners/run_compare_self.cmake` | Tests that `expect_same_bytes` of `tests/program_output.cmake` names the first line where two texts differ. |
| `tests/runners/run_conf_include_depth.cmake` | Checks that the runtime and `anti symbols` stop at the same include depth. |
| `tests/runners/run_coverage_report.cmake` | Tests the coverage report on a program of three files that two runs reach. |
| `tests/runners/run_cpu_archive.cmake` | Checks that each target of the archive has a runtime per level. |
| `tests/runners/run_cpu_check.cmake` | Runs a program above the level of the machine and expects the refusal. |
| `tests/runners/run_cpu_level.cmake` | Checks the dev assembly of one level for patterns. |
| `tests/runners/run_cpu_pin.cmake` | Compares `antic --print-cpu-levels` with `tools/cpu-levels`. |
| `tests/runners/run_cpu_runtime.cmake` | Checks the instructions of one runtime library. |
| `tests/runners/run_cross_link.cmake` | Links a program for a target and checks the format of the file. |
| `tests/runners/run_dead_code.cmake` | Checks which functions a link keeps and drops. |
| `tests/runners/run_debug.cmake` | Checks the line table of `-g` and three debugger sessions. |
| `tests/runners/run_deps_dir.cmake` | Configures a copy of the tree with `ANTI_DEPS_DIR` and checks nothing is downloaded. |
| `tests/runners/run_dev.cmake` | Builds a program in dev mode from sources and from library files and compares both. |
| `tests/runners/run_dev_report.cmake` | Checks what a dev build reports on standard error while a program runs, and that a release build reports nothing. |
| `tests/runners/run_diagnostics_covered.cmake` | Refuses a message of antic or `anti` that no test expects and `tests/diagnostics-open.txt` does not list. |
| `tests/runners/run_diagnostics_covered_self.cmake` | Tests `run_diagnostics_covered.cmake` on made-up trees. |
| `tests/runners/run_direct_calls.cmake` | Refuses an indirect call in one function of a release build. |
| `tests/runners/run_distinct_addresses.cmake` | Checks that distinct functions keep distinct addresses under a link mode. |
| `tests/runners/run_document_examples.cmake` | Compiles every `anti` block of a document through the front end of antic. |
| `tests/runners/run_downloads_once.cmake` | Refuses a download of the build that does not go through `antic_download` of `tools/download.cmake`. |
| `tests/runners/run_dump.cmake` | Compares the output of antic with an expected file. |
| `tests/runners/run_emit_identity.cmake` | Compares the digests of the LLVM text of every program with those of the Mac. |
| `tests/runners/run_emit_identity_self.cmake` | Tests that `run_emit_identity.cmake` keeps the texts of its manifest and names the first line that moved. |
| `tests/runners/run_errors_registered.cmake` | Checks that every error program is named by a test. |
| `tests/runners/run_expect.cmake` | Runs a command and checks its status and output. |
| `tests/runners/run_expect_self.cmake` | Tests `run_expect.cmake` and refuses `PASS_REGULAR_EXPRESSION`. |
| `tests/runners/run_file_headers.cmake` | Refuses a file of antic or of the runtime that opens without the comment of its purpose, and a function of a public header without a comment of its own. |
| `tests/runners/run_file_headers_self.cmake` | Tests `run_file_headers.cmake` on made-up trees. |
| `tests/runners/run_files_oom.cmake` | Runs the file helpers out of memory and expects status 70. |
| `tests/runners/run_find_order.cmake` | Refuses a variable read above the `find_program` that sets it. |
| `tests/runners/run_first_build.cmake` | Checks that the configure step of a fresh checkout installs the sysroot of every target and the raylib source. |
| `tests/runners/run_float_digits.cmake` | Checks that three runtime members name no printf or scanf function. |
| `tests/runners/run_fmt_canonical.cmake` | Runs `anti fmt --check` over the sources of the tree. |
| `tests/runners/run_fmt_canonical_self.cmake` | Tests that `run_fmt_canonical.cmake` fails when `anti fmt --check` ends by a signal. |
| `tests/runners/run_framework.cmake` | Links a program with a framework, or expects the refusal without an SDK. |
| `tests/runners/run_front_end.cmake` | Runs `antic --front-end` on a good and a bad program. |
| `tests/runners/run_function_alignment.cmake` | Checks the 16-byte alignment of ARM64 functions. |
| `tests/runners/run_glibc_link.cmake` | Links a C probe of a native library against the glibc sysroot. |
| `tests/runners/run_glibc_sysroot.cmake` | Checks the glibc pins and runs the sysroot script on stand-in packages. |
| `tests/runners/run_hash_seed.cmake` | Runs a program twice and expects two seeds. |
| `tests/runners/run_header_names.cmake` | Refuses one of our headers included with angle brackets. |
| `tests/runners/run_header_sections.cmake` | Checks that each function under a section of a shared header is defined in that file, and that no file outside its family includes the header. |
| `tests/runners/run_host_names.cmake` | Refuses the host of the machine worked out outside `tools/host.cmake`. |
| `tests/runners/run_host_sources.cmake` | Compiles every source of antic and anti for one host under the warning set. |
| `tests/runners/run_installer_alone.cmake` | Runs the installer with a failing cmake on the path and refuses words of removed steps. |
| `tests/runners/run_installer_github.cmake` | Pins the addresses in both installers and runs one against a fake release. |
| `tests/runners/run_installer_options.cmake` | Pins the options, the install roots and the marker across the four install scripts. |
| `tests/runners/run_internal.cmake` | Checks that an internal item is refused from another package. |
| `tests/runners/run_ir_line.cmake` | Requires one line in the IR dump. |
| `tests/runners/run_known_failures.cmake` | Compares `tests/known-failures.txt` with the table of `docs/audit/tests-findings.md` and with the build tree. |
| `tests/runners/run_known_failures_self.cmake` | Tests `run_known_failures.cmake` on made-up lists. |
| `tests/runners/run_license_forms.cmake` | Checks one form of `anti license`. |
| `tests/runners/run_license_notice.cmake` | Checks the notice of three programs against `NOTICE.txt`. |
| `tests/runners/run_licenses.cmake` | Finds the licence notice in the bytes of a program. |
| `tests/runners/run_line_endings.cmake` | Requires LF in the programs and expected files. |
| `tests/runners/run_link_identity.cmake` | Compares the digest of a linked program with a stored one. |
| `tests/runners/run_linux_libc.cmake` | Links a static stand-in for both Linux hosts and runs `check-libc.cmake` on it. |
| `tests/runners/run_linux_link_recipe.cmake` | Checks that the link of a Linux program of a release stands in `antic_host_linux_link` of `tools/host-compile.cmake` alone. |
| `tests/runners/run_linux_modes.cmake` | Builds a static and a dynamic Linux program and reads their ELF headers. |
| `tests/runners/run_llvm_atomics.cmake` | Requires atomic instructions in the dev LLVM text of six targets. |
| `tests/runners/run_llvm_coff_plugin.cmake` | Checks the imports and exports of a Windows plugin and its host. |
| `tests/runners/run_llvm_datalayout_pin.cmake` | Compares the data layout and the features antic prints with the pinned clang. |
| `tests/runners/run_llvm_inline_lines.cmake` | Checks that an inlined callee keeps its line under `-g`. |
| `tests/runners/run_llvm_names.cmake` | Checks that `get-llvm.cmake` does not rewrite an lld name in place. |
| `tests/runners/run_llvm_run.cmake` | Checks `--keep-llvm`, the kept files of each mode and `-S`. |
| `tests/runners/run_llvm_tool_list.cmake` | Refuses a name of an LLVM tool or of lld spelled outside the lists of `tools/pinned-tools.cmake`. |
| `tests/runners/run_llvm_tools_copy.cmake` | Tests that `tools/copy-llvm.cmake` lays out `bin/` of the runtime archive with the tools it names. |
| `tests/runners/run_llvm_tools_root.cmake` | Checks that the root `CMakeLists.txt` alone builds `bin/` of the runtime archive. |
| `tests/runners/run_llvm_verify.cmake` | Runs the verifier of opt over the LLVM text of every program. |
| `tests/runners/run_llvm_verify_self.cmake` | Tests that `run_llvm_verify.cmake` fails for an antic that a signal ends and for a refused file it does not name. |
| `tests/runners/run_lock_order.cmake` | Requires the lock-order reports of a dev build. |
| `tests/runners/run_lto_default.cmake` | Checks which link takes bitcode and which objects. |
| `tests/runners/run_macos_sdk.cmake` | Checks that the SDK script finds the pinned SDK and refuses a changed one. |
| `tests/runners/run_macos_sdk_self.cmake` | Tests that `run_macos_sdk.cmake` skips without the pinned SDK and fails for one that changed. |
| `tests/runners/run_macos_sysroot.cmake` | Checks the Zig pin and runs the sysroot script on stand-ins. |
| `tests/runners/run_manifest_signature.cmake` | Runs the installer against a signed, a wrong, an unsigned and a staging manifest. |
| `tests/runners/run_matrix_files.cmake` | Refuses a file of `tests/matrix/` that registers no test. |
| `tests/runners/run_may_fail_only.cmake` | Refuses a function written by hand with an error result. |
| `tests/runners/run_memory_checks.cmake` | Checks what `--memory-checks` adds and reports per target. |
| `tests/runners/run_memory_checks_bitfield.cmake` | Checks that `--memory-checks` finds a read of a bitfield after free. |
| `tests/runners/run_memory_checks_list.cmake` | Checks the report of a pointer kept across a push. |
| `tests/runners/run_message_forms.cmake` | Checks the forms of a message of antic, at a position of a source and at none. |
| `tests/runners/run_mimalloc_environment.cmake` | Runs a musl program with two variables of mimalloc. |
| `tests/runners/run_modules.cmake` | Builds library modules and a program in one mode and compares the output. |
| `tests/runners/run_musl_allocator.cmake` | Reads the symbols of a musl program for the allocator. |
| `tests/runners/run_mutex_size.cmake` | Reads the size of a Mutex for six targets. |
| `tests/runners/run_names_once.cmake` | Refuses four shared names outside their header. |
| `tests/runners/run_native_back_end_gone.cmake` | Refuses a comment of antic that gives a reason of the native back end, and the loop depth of a block of the IR. |
| `tests/runners/run_native_hosts.cmake` | Refuses a call of a helper of the outline atomics in the native libraries of linux-arm64. |
| `tests/runners/run_native_licenses.cmake` | Compares the licence texts of the repository with those of the runtime tree. |
| `tests/runners/run_native_link.cmake` | Links a test program against a native library for one target. |
| `tests/runners/run_native_pin.cmake` | Checks the form of a pin and that its version and digest stand nowhere else. |
| `tests/runners/run_native_pin_self.cmake` | Tests `run_native_pin.cmake` on a made-up tree. |
| `tests/runners/run_no_paths.cmake` | Refuses the path of the checkout in the runtime libraries and in antic. |
| `tests/runners/run_no_sdk_fetcher.cmake` | Refuses the name of the removed SDK fetcher. |
| `tests/runners/run_no_tool_packed.cmake` | Refuses a tool of the build that no package carries in a directory the packer copies. |
| `tests/runners/run_note_claims.cmake` | Refuses a claim of `docs/notes/` or `docs/decisions.md` that names a C file as the place of a function the file does not hold. |
| `tests/runners/run_note_claims_self.cmake` | Tests `run_note_claims.cmake` on made-up trees. |
| `tests/runners/run_no_workflows.cmake` | Refuses a file under `.github/workflows/`. |
| `tests/runners/run_one_allocator.cmake` | Refuses an allocation outside `src/antic/alloc.c`. |
| `tests/runners/run_options_covered.cmake` | Refuses an option of antic or a command or option of `anti` that no test passes and `tests/options-open.txt` does not list. |
| `tests/runners/run_options_covered_self.cmake` | Tests `run_options_covered.cmake` on made-up trees. |
| `tests/runners/run_own_tools.cmake` | Shows that antic takes no tool or sysroot from the machine. |
| `tests/runners/run_package.cmake` | Packs the package of the host from the build, lists it and unpacks it for the tests `package_*`. |
| `tests/runners/run_package_bind.cmake` | Runs `anti bind` and `anti bind --clang` of the package from the package alone. |
| `tests/runners/run_package_build.cmake` | Builds a project with `anti build --release` of the package and finds its symbols with `anti symbols inventory`. |
| `tests/runners/run_package_contents.cmake` | Checks that the package of the host holds what a package holds. |
| `tests/runners/run_package_cross.cmake` | Checks the linux-arm64 package that the packer packs on a host that is not Linux. |
| `tests/runners/run_package_glibc.cmake` | Links a Linux program against glibc from the package alone. |
| `tests/runners/run_package_keys.cmake` | Refuses a key or a file of `tools/keys/` in the package of the host. |
| `tests/runners/run_package_license.cmake` | Runs `anti license` of the package from the package alone. |
| `tests/runners/run_package_offline.cmake` | Runs `tools/check-offline.cmake` on the package of the host and on a directory that holds no antic. |
| `tests/runners/run_package_sdk_pin.cmake` | Checks that the packer links macOS against the Apple SDK that `tools/macos-sdk-pin` names. |
| `tests/runners/run_package_system_compiler.cmake` | Checks that the packer refuses a build that a compiler other than the pinned clang compiled. |
| `tests/runners/run_package_targets.cmake` | Builds and runs a hello program with `bin/` of the package as the whole `PATH`, and links one for every target. |
| `tests/runners/run_package_version.cmake` | Checks that antic and `anti` of the package print the version of `tools/version`. |
| `tests/runners/run_pdb_guid.cmake` | Runs `check-pdb.cmake` on a linked Windows program. |
| `tests/runners/run_pdb_names.cmake` | Checks the names in a PDB and that it holds no build path. |
| `tests/runners/run_pin_readers.cmake` | Refuses a pin read outside `tools/pins.cmake` and `tools/cpu-levels` read outside `tools/cpu-levels.cmake`. |
| `tests/runners/run_pinned_compiler.cmake` | Checks that the cache names the pinned clang and no runner a compiler or a binary tool of the host. |
| `tests/runners/run_pinned_tools.cmake` | Runs the tool check of the configure step on the cache and on changed copies. |
| `tests/runners/run_platform_layer.cmake` | Refuses a host `#if` outside the platform files. |
| `tests/runners/run_plugin.cmake` | Builds a plugin and its hosts and checks loading and injection. |
| `tests/runners/run_plugin_kinds.cmake` | Builds a host and a plugin whose interface takes and gives every kind of value, and runs the host. |
| `tests/runners/run_plugin_versions.cmake` | Loads libraries of one interface, each built against another version of it, into hosts of other versions, to check the version rules. |
| `tests/runners/run_pre_commit.cmake` | Drives the pre-commit hook in a scratch repository. |
| `tests/runners/run_probe.cmake` | Compares the ABI probe in C with the one in Anti. |
| `tests/runners/run_probe_self.cmake` | Tests that `run_probe.cmake` fails for two probes that print the same and end with a status other than 0. |
| `tests/runners/run_profile.cmake` | Builds with a profile, merges it and builds again. |
| `tests/runners/run_program.cmake` | Compiles one program, runs it and compares with the expected file. |
| `tests/runners/run_raw_output.cmake` | Checks the raw bytes rule and scans every runner for it. |
| `tests/runners/run_raylib_miniaudio_pin.cmake` | Checks that the miniaudio raylib bundles is the pinned one. |
| `tests/runners/run_release_dry_run.cmake` | Runs `./r --dry-run` on the copy of `tests/release_copy.cmake` with stand-in tools and checks what each step wrote. |
| `tests/runners/run_release_dry_run_self.cmake` | Tests that `run_release_dry_run.cmake` fails and prints the output when a fixture does not compile or link. |
| `tests/runners/run_release_key.cmake` | Checks that both installers hold the public key and that a package carries none. |
| `tests/runners/run_release_manifest.cmake` | Tests `tools/check-release.cmake` on the directory of a release and on changed copies of it. |
| `tests/runners/run_release_pins.cmake` | Checks the form of the two LLVM pins and that no script spells them. |
| `tests/runners/run_release_preflight.cmake` | Checks that the preflight of `./r --dry-run` refuses an uncommitted change and a version without its entry. |
| `tests/runners/run_release_signing_key.cmake` | Checks which signing keys `./r --dry-run` takes and which it refuses. |
| `tests/runners/run_release_site.cmake` | Checks the order of the setgid step and the mode of every rsync of step 9. |
| `tests/runners/run_release_tags.cmake` | Checks that `./r --dry-run` warns on a version that is a tag already. |
| `tests/runners/run_release_vm_commands.cmake` | Checks that the commands that take a VM off the network and back stand in `tools/release.sh`, as `docs/vm-setup.md` shows them. |
| `tests/runners/run_release_vm_failures.cmake` | Checks that step 5 of `./r --dry-run` stops when the offline check of a VM fails or its network does not come back. |
| `tests/runners/run_repo_layout.cmake` | Checks every tracked path against the allowed entries. |
| `tests/runners/run_repo_layout_self.cmake` | Tests `run_repo_layout.cmake` on made-up trees. |
| `tests/runners/run_rt_extensions.cmake` | Refuses a compiler extension in the runtime outside the files that exist to hold one. |
| `tests/runners/run_rt_format.cmake` | Compiles every file of the runtime with the warning of a missing format attribute and refuses a warning. |
| `tests/runners/run_rt_headers.cmake` | Checks that each header of the runtime with one interface opens with its purpose and alone declares what its file defines. |
| `tests/runners/run_rt_names.cmake` | Refuses an exported name of the runtime outside its prefixes. |
| `tests/runners/run_rt_names_self.cmake` | Tests `run_rt_names.cmake` on made-up runtime archives. |
| `tests/runners/run_rt_oom.cmake` | Runs each case of `test_rt_oom.c` and expects the stop through `anti_rt_out_of_memory`. |
| `tests/runners/run_rt_wide_text.cmake` | Refuses a call of an ANSI entry point of Windows in the runtime. |
| `tests/runners/run_runtime_frames.cmake` | Requires the frame record in `main` of every Linux and macOS runtime. |
| `tests/runners/run_runtime_from_data_dir.cmake` | Checks that antic finds the archive of the data directory. |
| `tests/runners/run_runtime_hosts.cmake` | Refuses outline atomics in the linux-arm64 runtime. |
| `tests/runners/run_script_policies.cmake` | Requires `cmake_minimum_required` in the scripts two list files name. |
| `tests/runners/run_script_policies_self.cmake` | Tests `run_script_policies.cmake` on scripts with and without `cmake_minimum_required`. |
| `tests/runners/run_shared_inputs.cmake` | Builds shared libraries with `--framework`, `--linux-lib` and `--memory-checks` and reads what each links. |
| `tests/runners/run_simd_width.cmake` | Counts the lines of one instruction in the assembly of a program. |
| `tests/runners/run_skips_registered.cmake` | Refuses a test whose runner can print `SKIP:` without `SKIP_REGULAR_EXPRESSION`. |
| `tests/runners/run_skips_registered_self.cmake` | Tests `run_skips_registered.cmake` on made-up lists of tests. |
| `tests/runners/run_source_cap.cmake` | Checks the size limit of a source. |
| `tests/runners/run_source_nul.cmake` | Checks the refusal of a NUL byte outside a literal at each place the driver reads, and a NUL inside a bytes literal. |
| `tests/runners/run_source_tree.cmake` | Refuses a file or a directory without a row in `docs/notes/source-tree.md`, and a row of nothing. |
| `tests/runners/run_source_tree_self.cmake` | Tests `run_source_tree.cmake` on made-up trees. |
| `tests/runners/run_std_collection_modules.cmake` | Tests the ordering function of `tools/std-modules.cmake`. |
| `tests/runners/run_std_out_of_memory.cmake` | Checks that memory that runs out in the standard library stops the program with a message. |
| `tests/runners/run_std_tests.cmake` | Runs `anti test` on one module in both modes. |
| `tests/runners/run_symbols.cmake` | Looks up two functions in a program built with `-g`. |
| `tests/runners/run_symbols_layout.cmake` | Compares the sections of a shipped program with those of its debug link. |
| `tests/runners/run_sysroot_links.cmake` | Checks that the sysroot script turns an absolute link relative. |
| `tests/runners/run_target_lists.cmake` | Refuses the six targets spelled outside `tools/cpu-levels`. |
| `tests/runners/run_target_triples.cmake` | Refuses a triple of the C of a target outside `antic_target_triple` of `tools/target-compile.cmake`. |
| `tests/runners/run_trace.cmake` | Runs a program and matches its output against pattern files. |
| `tests/runners/run_trap.cmake` | Builds a program that must stop and compares its message. |
| `tests/runners/run_trap_table.cmake` | Checks the trap of an unset table entry per build mode. |
| `tests/runners/run_unicode_paths.cmake` | Runs antic and anti with names outside ASCII. |
| `tests/runners/run_unit_messages.cmake` | Runs one group of the unit tests and compares its standard error with a file. |
| `tests/runners/run_unwind.cmake` | Compares the Windows unwind data of a program with an expected file. |
| `tests/runners/run_upstream_sources.cmake` | Checks `licenses/sources.txt` against the pins and the licence texts. |
| `tests/runners/run_usage.cmake` | Checks the usage of a tool: the option that asks for it, and a command line the tool refuses with it. |
| `tests/runners/run_variant_wide_tag.cmake` | Compiles and runs a program whose variant has 65536 cases, so that its tag is a `u32`. |
| `tests/runners/run_warning_names.cmake` | Holds the warning table of antic equal to the note. |
| `tests/runners/run_warning_set.cmake` | Refuses a warning flag outside the two warning files. |
| `tests/runners/run_warning_set_self.cmake` | Tests `run_warning_set.cmake` on made-up trees. |
| `tests/runners/run_warnings.cmake` | Runs antic and requires or forbids texts in its messages. |
| `tests/runners/run_windows_addresses.cmake` | Refuses 32-bit address relocations in a windows-x86_64 object. |
| `tests/runners/run_windows_sysroot.cmake` | Checks the mingw-w64 pins and runs the sysroot script on a stand-in release. |

## The tools, `tools/`

The scripts of the build and of a release, the pins of every download and
the data the scripts read.

### The scripts

| File | Purpose |
|---|---|
| `tools/check-cpu.cmake` | Refuses a package whose runtime levels are not those of `tools/cpu-levels`. |
| `tools/check-libc.cmake` | Refuses a Linux program that needs a glibc above 2.35. |
| `tools/check-llvm.cmake` | Checks that the LLVM tools of a directory are the release of `tools/llvm-version`. |
| `tools/check-offline.cmake` | Builds a raylib program, a plugin host and five cross links from an install alone. |
| `tools/check-pdb.cmake` | Checks the CodeView record of a Windows executable against the GUID of its PDB. |
| `tools/check-release.cmake` | Checks the directory of a release: the files its signed manifest names, with their digests, and nothing else. |
| `tools/clang-release.cmake` | Gives the version, the tag and the release page of the pinned clang to the build of `anti`. |
| `tools/compile.sh` | `./c`: configures, builds and tests one of the five presets, and runs the coverage measurement. |
| `tools/copy-llvm.cmake` | Lays out `bin/` of the runtime archive: the tools `antic_llvm_archive_tools` names, with `llvm-version` beside them. |
| `tools/coverage-report.cmake` | Merges the raw profiles of a coverage tree and writes its summary and its annotated sources. |
| `tools/coverage.cmake` | The flags of the coverage measurement, the directory of its raw profiles and the instrumented runtime of a coverage tree. |
| `tools/deps-dir.cmake` | Names the directory of the pinned downloads. |
| `tools/docs-style/banned-terms.md` | Lists the banned terms for a reader. |
| `tools/docs-style/check_docs.py` | Checks `.md` files against the docs-style rules. |
| `tools/docs-style/rules.md` | States the docs-style rules for a comment and a `.md` file. |
| `tools/download.cmake` | `antic_download`, the one download of the build, with the check of its digest. |
| `tools/downloads.html.in` | The template of the downloads page that step 9 of a release fills. |
| `tools/fetch-release.cmake` | Downloads one archive of the LLVM release and checks it against the pin, the manifest and the signature. |
| `tools/get-clang.cmake` | Installs the pinned clang of the host. |
| `tools/get-llvm.cmake` | Installs the pinned LLVM tools of the host, or lays out those of another host for the packer. |
| `tools/get-sysroot.cmake` | Installs the sysroot of each named target and the licences of its components. |
| `tools/git-hooks.cmake` | Points `core.hooksPath` of the checkout at `tools/hooks`. |
| `tools/hooks/pre-commit` | Refuses a commit whose staged `.md` files fail the checker. |
| `tools/host-compile.cmake` | Gives the options of the pinned clang that compile antic or `anti` for one host. |
| `tools/host.cmake` | Names the host of the machine, the suffix of an executable and the file of a static library of a target. |
| `tools/install.ps1` | Installs the package of a Windows host after checking the signature and the digest. |
| `tools/install.sh` | Installs the package of a macOS or Linux host after checking the signature and the digest. |
| `tools/libc-versions.cmake` | Gives the versions of musl and mimalloc to the build of antic. |
| `tools/macos-sdk.cmake` | Finds the pinned Apple SDK and checks the digest of its `libSystem.tbd`. |
| `tools/pack-anti.cmake` | Compiles antic and `anti` for each named host, and writes its package and the manifest. |
| `tools/pinned-compiler.cmake` | Fetches the pinned tools and sets the compiler, the archiver and the linker of a build. |
| `tools/pinned-tools.cmake` | Holds the list of the LLVM tools and the checks that refuse a tool of the host. |
| `tools/pins.cmake` | Reads the pins of `tools/` and builds the URL of a pinned source. |
| `tools/release.sh` | Makes a release in ten steps, from the preflight to the report. |
| `tools/sources.cmake` | Lists the C sources of antic and `anti` for the build and the packer. |
| `tools/std-modules.cmake` | Orders the collection modules of the standard library by their imports. |
| `tools/target-compile.cmake` | The compile of C for every target: the triple clang compiles it for and the headers of its sysroot. |
| `tools/uninstall.ps1` | Removes an install of Windows that carries the marker, with the two executables the installer put on the path. |
| `tools/uninstall.sh` | Removes an install that carries the marker, with its executables and its profile line. |
| `tools/upstream-sources.cmake` | Writes `licenses/sources.txt` from the pins. |
| `tools/warnings.cmake` | Holds the warning set of our own C. |
| `tools/windows-compile.cmake` | Holds the triples, the compile options and the link options of a Windows target. |

### The pins and data

A pin or a data file has no code. Its row says what it holds.

| File | Purpose |
|---|---|
| `tools/clang-pin` | The release, the asset names and the digests of the pinned clang of every host. |
| `tools/cpu-levels` | The processor levels and the default level of each target. |
| `tools/cpu-levels.cmake` | Reads `tools/cpu-levels` into the targets and the levels of each. |
| `tools/keys/release.pem` | The public key that checks the signature of the LLVM tools. |
| `tools/llvm-pin` | The release, the asset names and the digests of the pinned LLVM tools of every host. |
| `tools/llvm-version` | The LLVM version, one line. |
| `tools/macos-sdk-pin` | The version, the name and the digest of the Apple SDK. |
| `tools/mbedtls-pin` | The pin of Mbed TLS. |
| `tools/mimalloc-pin` | The pin of mimalloc. |
| `tools/miniaudio-pin` | The pin of miniaudio. |
| `tools/pcre2-pin` | The pin of PCRE2. |
| `tools/raylib-pin` | The pin of raylib and the miniaudio version it bundles. |
| `tools/release-base` | The addresses of the GitHub release. |
| `tools/site-base` | The addresses of the site, the signature and the key. |
| `tools/sqlite-pin` | The pin of SQLite. |
| `tools/sysroot-pins` | The pins of musl, the glibc packages and mingw-w64. |
| `tools/version` | The version of Anti, one line. |
| `tools/zig-stubs-pin` | The pin of the Zig release and of the APSL text. |
