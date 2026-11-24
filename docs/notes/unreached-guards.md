# Guards that no input reaches

The coverage steps of 2026-11-09 and 2026-11-10 sorted the regions that
`./c coverage` showed unreached. Most of what stays unreached is a guard.
A guard tests for a state that the code before it never lets through.
Eddie kept the guards of this note. `stub` stays by Q2 of "Answers
of 2026-11-09, third set" in `docs/decisions.md`, the guards of
`test.c`, `sema_copies.c` and `driver_link.c` and the `EXPR_FREE` arm of
`visit_call` by "Answers of 2026-11-11", and the others by "Answers of
2026-11-10, second set". Those answers keep every guard that no input
reaches, and the guards of `sema_const.c`, `sema_expr.c`,
`sema_generic.c`, `antl_tree.c`, `sema_escape.c`, `whole.c`,
`whole_tables.c`, `lower_place.c`, `coff.c`, `llvm_emit_call.c`,
`build.c`, `sema_lock.c`, `lower_stmt.c`, `lower_expr.c`, `lower_desc.c`,
`platform.c`, `parser_stmt.c`, `types.c`, `patterns.c`, `repo.c`,
`sema_call.c`, `sema_value.c`, `sema_uninit.c`, `registry.c`, `header.c`,
`parser_decl.c`, `lexer.c`, `antl_read_ir.c`, `sema_free.c`,
`sema_hash.c`, `pattern.c`, `optimize.c`, `bindwrite.c`,
`sema_safety.c`, `layout.c`, `lower_simd.c`, `lower_hash.c`, `check.c`,
`cache.c`, `sema_thread.c` and the files of batches 3l, 3m, 3n, 3o, 3p, 3q, 3r, 3s, 3t, 3u, 3v, 3w, 3x and 3y stay by them. A guard stays in the code as it is. A later
coverage step that finds one unreached reads its row here before it asks
again.

Each row names the function, what the guard turns away and why no input
of a test reaches it. The rows were checked against the source and the
coverage run of `docs/reports/2026-11-10-coverage-batch-5.md`, and those
of the third batch against the source of
`docs/reports/2026-11-11-coverage-batch-6.md`. Those of the three files
of the checker were checked against the source of
`docs/reports/2026-11-11-coverage-phase-3a.md`, and those of
`antl_tree.c` against the source of
`docs/reports/2026-11-11-coverage-phase-3b.md`. Those of
`sema_escape.c`, `whole.c` and `whole_tables.c` were checked against the
source of `docs/reports/2026-11-11-coverage-phase-3c.md`, and those of
`lower_place.c`, `coff.c`, `llvm_emit_call.c` and `build.c` against the
source of `docs/reports/2026-11-12-coverage-phase-3d.md`. Those of
`sema_copies.c`, `sema_index.c` and `sema_lock.c` were checked against
the source of `docs/reports/2026-11-13-coverage-phase-3e.md`, and those
of the four files of batch 3f against the source of
`docs/reports/2026-11-13-coverage-phase-3f.md`. Those of the four files
of batch 3g were checked against the source of
`docs/reports/2026-11-13-coverage-phase-3g.md`. Those of the eight files
of batch 3h were checked against the source of
`docs/reports/2026-11-14-coverage-phase-3h.md`, and those of the six
files of batch 3i against the source of
`docs/reports/2026-11-15-coverage-phase-3i.md`. Those of the six files
of batch 3j were checked against the source of
`docs/reports/2026-11-15-coverage-phase-3j.md`, and those of the eight
files of batch 3l against the source of
`docs/reports/2026-11-16-coverage-phase-3l.md`. Those of the seven
files of batch 3m were checked against the source of
`docs/reports/2026-11-16-coverage-phase-3m.md`, and those of the files
of batch 3n against the source of
`docs/reports/2026-11-16-coverage-phase-3n.md`. A guard named "the report
of the first batch" stands in `docs/reports/2026-11-09-coverage-phase-2a.md`,
one of "the report of the second batch" in
`docs/reports/2026-11-10-coverage-phase-2b.md` and one of "the report of
the third batch" in `docs/reports/2026-11-10-coverage-phase-2c.md`.

## `src/antic/llvm_emit_function.c`

Q1 and Q2 of the report of the first batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `llvm_emit_frame_add` | A call after the emitter failed, which returns at once. | Every caller stops at a failure before it adds to the frame. |
| `llvm_emit_frame_add` | An alignment of 0, which it takes as 1. | No layout gives a type the alignment 0. |
| `llvm_emit_definition` | A signature that fails, after which it writes no body. | `llvm_emit_signature` fails only for an aggregate value type, and an aggregate never passes as `ABI_DIRECT`. |

## `src/antic/sema_index.c`

Q3 of the report of the first batch. The pass that proves an index in
range.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `fits` | A type that is no integer, and the ranges of the integer types below 64 bits. | An index is an `int`, so the pass asks of no other type. |
| `range_of` | An operand without a type, and an unsigned literal below the bound of values. | The checker gives every operand of an index a type, and an index is an `int`, which is signed. |
| `range_of` | A range that leaves the type of its expression. | A range of an `int` stays within the bounds that `fits` checks. |
| `name_range` | A name without a symbol, and an unsigned constant. | The checker binds every name it accepts, and an index is an `int`. |
| `length_of` | A base without a type. | The checker gives every base of an index a type before the pass runs. |
| `whole_expr` | A `try` that marks an expression without a call. | Such a `try` is an error, and an error stops the pass before it walks the body. |
| `sema_check_indices` | A body that is missing, and a body that the checker checks quietly. | The checker calls the pass for a function with a body, and never inside a quiet check. |

## `src/rt/trace.c`

Q4 and Q5 of the report of the first batch. The paths that run on Linux
and Windows alone, as `symbolize_elf` and `symbolize_pe`, are no guards
and stand under "Not in this note".

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `image_slide` | A Mach-O image without a `__TEXT` segment, which gives the slide 0. | Every image that dyld maps has a `__TEXT` segment. |
| `symbolize_macho` | A Mach-O image without `LC_SYMTAB` or `__LINKEDIT`, which gives no function. | Every image that dyld maps has both. |
| `add_line` | A line that `snprintf` failed to write or cut short, which frees the text. | Every line of `anti_rt_trace_text` is at most 60 bytes of formatted numbers, below the room of 128. |
| `add` | A text that `add_line` freed. | `add_line` never frees it. |
| `anti_rt_trace_text` | The three returns of NULL after `add_line` gave 0. | `add_line` never gives 0. |
| `module_number` | The last `return -1`, after the loop over the frames. | The loop returns for every frame with a base, and a frame without one returns before the loop. |

## `src/rt/plugin.c`

Q2 of "Answers of 2026-11-09, third set" and Q4 of the report of the
second batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `stub` | A call of a slot of the sub-object of an older library that only `reflect.call` may reach. | `reflect.call` calls through the table of the object at its start, never through the table of the sub-object. `unit_plugin_stub` calls the slot directly. |
| `fitted` | A buffer of 3 bytes or less, which has no room for `...`. | The callers pass the sizes 512 and 128 alone. |
| `checked` | An entry without a descriptor or without a chain of versions. | `damage_in` refuses both before the load checks an entry. |
| `matches_host` | An entry without a descriptor. | `damage_in` refuses it first. |
| `missing_module` | A module longer than its buffer. | The module is shorter than the symbol, which `missing_symbol` keeps within the same room. |
| `joined` | A path that does not fit, and a directory of no length. | The paths of discovery fit their buffers, and `discover` turns an empty directory into `.`. |
| `index_field` | A key of the index longer than its 64 bytes. | A key is `library.<n>.<field>` with a field of the index, which fits. |
| `index_lists` | A field longer than its 32 bytes, and the end of the count without an empty value. | `interfaces.<k>` fits, and the walk meets an empty value before the count of the document ends. |
| `anti_rt_plugin_provider` | A slot that was closed between the load and `reserve`. | That needs a second thread that unloads a handle more often than it loaded it. |

## `src/rt/object.c`

Q3 of the report of the second batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `anti_rt_array_shape` | An array of length 0, which gives no level. | An array has at least one element, so a descriptor never names the length 0. |
| `put_value` | An array without an element size, without a level or of length 0, which it writes as `null`. | The same: every array a descriptor names has a length and a size. |
| `show_row` | An element of size 0, which ends the row at once. | Every caller passes an element whose size is above 0. |
| `checked_object` | A NULL descriptor, which names `Object` in the trap. | `delete`, `destroy` and `dup` take a class pointer, and the C header passes the descriptor. |

## `src/antic/sema_stmt.c`

Q5 of the report of the second batch, and Q1 of
`docs/reports/2026-11-10-coverage-batch-5.md` for the `EXPR_FREE` arm of
`visit_call`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `try_ends`, `sema_proved_names`, `sema_value_view` and `sema_place_view` | A handler without a body, a condition that is NULL and an expression that is NULL. | Every other kind of handler returns before the test, and the parser gives a handler of a block its body. Every caller passes a condition and an expression. |
| `visit_worker_expr`, `sema_calls_never`, `walked_copy`, `check_assign`, `check_flags_assign`, `mark_let_symbol`, `check_for`, `warn_unused_result` and `sema_refuse_count_write` | A base, a callee, a type, a symbol or a field that is NULL, tested again after a check that succeeded. | The check before gave each one its value, or refused the statement. |
| `sema_check_function`, `check_for`, `check_select`, `check_switch` and `check_variant_arm` | `sema_declare` giving NULL for `self`, a loop variable or a name an arm binds. | Each name is declared in a scope opened for it, so no other name of that scope clashes. |
| `sema_fills` and `sema_filled_somewhere` | A type that is no class or is the abstract class itself, an `impl` field that fills the abstract class, and a class of a library that fills a class of the module being compiled. | The callers pass a concrete class other than the abstract one, and `sema_implemented_in` finds the `impl` field first. A library file cannot name a class of a module that imports it, since two modules that import each other are refused. |
| `sema_check_function` | An anonymous function that reaches its end without `return`. | Its two callers pass the functions of a module and of a class body. `sema_closure.c` checks an anonymous function and gives the same message. |
| `visit_call` | `take` inside an `assert`. | The check of `take` gives the node the kind `EXPR_TAKE` after its last refusal, and it refuses a `take` whose object the expression drops, as a condition does. The walk then meets a call. |
| `sema_refuse_write` | A read-only name as the operand of `take`. | Its one caller is the check of `take`, which refuses every operand but an `own` field first. |
| `visit_call` | `free` inside the condition of an `assert`. | `free` gives no value, and the checker refuses a value of `void` wherever one stands, in a tuple literal too since the fix of T224. |

## `src/antic/driver.c`

Q9 of the report of the second batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `can_link` | A host that is none of the six targets. | antic builds for the six hosts alone. |
| `lower_checked` and `dump_ir` | The IR verifier failing after the lowering and after the optimizer. | The lowering and the optimizer write IR that the verifier takes. The message is an internal error. |
| `whole_checked` | A message of the whole program without its line feed. | Every message of the passes over the whole program ends with one. |
| `build_id` | Spans of the debug marks out of order. | The spans stay sorted and disjoint. |
| `identified_notice` | A notice that does not start with its marker. | antic writes every notice with its marker first. |
| `build_notice` | A package of the module without a name. | The package of the compiled module always has a name. |
| `write_library` | A lowering that fails while a library file is written. | A library file holds one module, and the checker refuses two signatures of one C function in a module first. |
| `write_library` and `compile` | A library file of more than 2^32 entries. | No test input holds that many. |
| `compile` | `--lib shared --bundle-runtime` at the marker of a bundled archive. | R132 refuses the pair before the compile. |
| `library_interface` and `driver_library_header` | A library file that holds no module of its own name. | antic writes every library file with the module it is named after. |
| `driver_library_header` | A header that fails for `anti bind --header`, and a failure of `sema_check` on an empty module. | `unit_library_header` reads the header first, and an empty module passes the check. |
| `report_diagnostics` | The count of a warning that is not promoted. | `anti check` alone asks for the counts, and it passes `--warnings-as-errors`, which promotes every warning. |
| `module_name` | A library file read without a list of diagnostics. | `interface_of`, the one caller without a list, reads a source for a doc page, and a library file takes another path. |

## `src/anti/test.c`

Q1 of the report of the third batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `base_options` and `test_run` | A host that `unit_host` cannot name, after which the run ends with 2. | anti builds for the six hosts alone, and `unit_host` names each of them. |
| `test_options_key` | A version that is NULL, which it writes as an empty word. | A key is written for a run over a project alone, and that run takes the version of the manifest, an empty text where the manifest names none. |
| `compile_imports` | A library file of the graph that is the input, which leaves the list. | Outside a project the run names no library file of the graph, so the list is empty. |

## `src/antic/sema_copies.c`

Q3 of the report of the third batch. Each guard stands behind the checker,
which refuses the form first or never builds it.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `member_of_receiver`, `own_params_copy` and `callee_copy` | A call that names no copy, which `copy_failed` refuses. | The checker gives every call of a generic its copy. |
| `own_params_copy` and `callee_copy` | A class nested in a generic, and a copy of its class that lacks the member. | A library file carries no function of a class with type parameters of its own, so a client gets `` `Stack<int>` has no function `map` `` first. |
| `xe` | A generic named as a value. | The checker refuses `let f = id;` with `` `id` is generic, and names a function where it is called ``. |
| `copy_failed` | A copy of a generic of the module itself that fails, which keeps the position in the body of the generic, and a refusal without a name, which names no generic. | The one form that fails, that of T095, is a copy of a generic of another module, and every clone names the generic it copies. |
| `type_item` and `member_of_copy` | A type item without its generic, a member without a symbol, a type or a copy, and a member that the generic class lacks. | The checker binds each one before the copies are made, and the member is one of the class of the generic. |
| `xf` | A field that no level of the chain of its type declares, which it keeps. | The checker took the field from that chain. |
| `redirect`, `xassign` and `xfor` | An open index, `set_index` and `for` over a slice or an array. | No slice or array meets `index`, so the copy turns every open index into a call. |
| `recheck`, `receiver`, `xparams`, `xi` and `xfor` | A recheck that fails, a receiver without its types, a clone that is not fresh, an item that has its clone already and a walk of a collection that fails. | The checked tree passed the same checks, the argument met the constraint of the walk, and each item is asked for once. |
| `ty`, `map_add`, `xf`, `xsym`, `symbol_open`, `generic_home`, `copy_function`, `direct_callee`, `walked_param`, `xcall`, `xi`, `defaults_of_copies` and `generic_type_item` | An operand that is NULL and a map of the type arguments that is empty. | Every caller passes the operand, and a copy has at least one type argument. |
| `refuse` and `xs` | A value outside the cases of the `switch`. | The `switch` of `refuse` names every refusal, and that of `xs` every kind of statement. |

## `src/antic/driver_link.c`

Q4 of the report of the third batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `driver_archive_tool` | A host that is none of the six targets, which takes no suffix. | antic builds for the six hosts alone. |
| `link_facts` | An object of `--lto none` that is NULL. | A link of a program always names its object. |
| `driver_windows_link_paths` | A call without a `.def` file. | Both callers pass one. |
| `native_level` | A level of another architecture than the target's. | The driver checks the level against the target before the link. |
| `driver_strip_debug` | A strip of Mach-O without the root of the debug map. | `anti build` always gives the root. |

## `src/antic/sema_const.c`

The evaluator of constants, sorted by batch 3a of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `float_as_literal` | Digits of the shortest form that end in 0, which it drops. | The loop stops at the fewest digits that read back as the float, and a last digit 0 reads back with one digit less. |
| `float_as_literal` | A float below 1 in magnitude, which it writes as `0.` and its digits. | A float is spelled only in the message of an `as` to an integer type that it does not fit, and every float below 1 in magnitude fits every integer type. |
| `const_elements` | A type that is NULL. | Every element and field of a checked type has a type. |
| `const_as` and `const_unary` | A conversion and a unary `-` whose value leaves its type. | The check of the expression refuses both through `sema_undefined_on_constants` before the evaluator reads them. |
| `const_integer` | An operator outside its `switch`. | `const_binary` hands it the integer operators alone. A comparison goes to `const_compare`, and `&&`, `||` and `??` return before. |
| `defined_integer` | `mul_high` of `c_long`, `c_ulong` or `c_wchar` in a copy. | `const_binary` refuses it in the generic, so no copy holds the node. |
| `const_repeat` | `[v; n]` whose length comes from `size_of` outside the default of a field. | The checker allows such a length in a field and a local variable alone, and a constant of either form is refused first. |
| `const_field` | A field of a call that the evaluator takes as a constant. | The evaluator refuses every call. |
| `const_field` | A constant of a class that failed. | The check of `Name.NAME` gives the error type for it, so the evaluator never reads it. |
| `const_field` | A field that the value of a struct constant does not hold, and a constant `str` whose value is no text. | The checker finds the field, the value of a struct constant holds every field, and a constant of `str` holds a string or another such constant. |
| `const_index` | An index past the end of a constant array. | `sema_index_outside` refuses it where the checker checks the expression. |
| `sema_eval_const` | A name without a symbol, `free`, a descriptor and `take` in a constant, and a kind outside the `switch`. | The checker binds every name it accepts. `free` gives no value, a descriptor stands among the arguments of a call, and the check of `take` refuses a value that a constant drops. The `switch` names every kind. |
| `const_deps_of` and `sema_refuse_runtime_parts` | An expression that is NULL, and an atomic operation in a default. | The parser gives every constant its value, and every operand the walks follow is set. The walk runs before the check, which makes the atomic node from a call. |
| `sema_bitfield_constant` | A bitfield of no integer type. | The declaration refuses it with `a bitfield has a sized integer type`. |
| `sema_const_symbol` | A `static` item of a class that is not `atomic`. | The parser refuses it with `` `n` is `static` and must be `atomic` ``. |
| `sema_fold_symbolic` | A node that is no operation, and an operation without its operand. | `subst_symbolic` of `sema_generic.c` calls it for an operation alone, and each operation has its operand. |

## `src/antic/sema_expr.c`

The checker of expressions, sorted by batch 3a of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `check_unary` | A unary operator outside `-`, `!`, `~`, `*` and `&`. | The parser makes a unary node of these five alone. |
| `watch_changes` | A read of the fields of `anti.lang.Watch` that the check refuses. | The compiler writes the reads from the fields of `Watch` and `Changes`, which the checker takes. |
| `sema_iterate` | An `iter` that gives the error type or no iterator, and a call of `next` on the cursor that fails. | `sema_hook_well_formed` refuses such a hook first, by T096 and T161, and a well formed `next` takes the cursor, a local of its type. |
| `check_optional` | `sema_declare` giving NULL for the hidden object of `?.`. | The name is declared in a scope opened for it, as the row of `sema_stmt.c` says for `self`. |
| `check_expr_inner` | A node of `to_slice`, of `?.`, of an atomic operation, of a simd operation or of `take`, checked a second time, and a kind outside the `switch`. | The checker gives each such node its type where it makes it from a call or a field, and no later check reaches the node again. The `switch` names every kind. |

## `src/antic/sema_generic.c`

The checker of generics, sorted by batch 3a of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `builtin_meets` | A hook outside the table of hooks. | `hook_names` lists every hook, and each has its arm above. |
| `hook_result` and `walks_as` | A type without `iter`, and an `iter` that gives no type. | `check_meets` and `sema_meets_param` test the hooks of `Iterable` and `Iterator` before the interface, so a type without them is refused first. |
| `check_meets` | A type argument of the error type. | A type argument that does not resolve stops the use before its constraints are read. |
| `has_params` and `names_param` | A type that is NULL. | Every caller passes a type, and every part they walk has one. |
| `subst_case` | A case that the copy of its variant lacks. | The copy of a variant holds every case of its generic, in order. |
| `sema_subst_const` | A constant that is NULL. | Every caller passes the value of a constant that has one. |
| `chan_field_pos` | A channel field of a copy that the declaration of its generic does not list. | Every field of a copy is a field of its generic, which the item lists. |
| `names_param` | A case of a variant as the type of a parameter, and the generic itself, whose own parameters it names. | No type expression names a case. The generic itself stands as a type in its own body alone, where its parameters are bound to themselves. |
| `sema_generic_call` | A parameter of the class left open where the receiver is the generic itself. | `class_args` binds each parameter of the class to itself there, and a constant one to its value. |

## `src/antic/antl_tree.c`

The section of the generics of a library file, sorted by batch 3b of the
third phase. A file that a user hands antic reaches the reader, and every
line of the reader that such a file reaches has a case in `unit_modules`.
The rows below guard the writer and the collecting pass, which see only
the trees the checker builds.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `io_size` | A count or an index above `UINT32_MAX`, which the writer refuses. | No list of a tree holds four billion records, and an index points into one. |
| `io_char` | A character of a format specification outside ASCII, which the writer refuses. | `format_spec` of `parser_expr.c` takes `<`, `>` and `^` as the alignment and one of `xXboef` as the kind, and nothing else. |
| `field_owner` and `io_field` | A field whose `home` is not the type that holds it, which `field_owner` looks for along the chain of the hint, and a field that no type holds, which the writer refuses. | `types_set_fields` sets `home` on every field of a type, and the other arrays of fields are those of the reader and of the IR and a list of `sema_class.c`, which no tree names. |
| `io_ref` and `io_sym` | A record, a local symbol or an extern that the collecting pass did not put in its table, which the writer refuses. | The writer runs the walkers of the collecting pass over the same tree, so every record it meets has its index, and `add_extern` puts every symbol that is not local in the table of externs. |
| `local_symbol` | The symbol of a type parameter, which belongs to the tree. | A name of a type in an expression is refused with "`T` is a type, not a value", so no checked tree names a type parameter by its symbol. |
| `collect` | A kind of table outside the six. | Every caller passes one of `T_SYM` to `T_TYPEX`. |

## `src/antic/sema_escape.c`

The safety check `escaping-local`, sorted by batch 3c of the third phase.
The pass walks a body that the checker has checked without an error, so
it sees only the places and the types the checker accepts.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `place_root` | A unary operator other than `*`, and any other kind of expression, as a place. | The checker refuses `&` of anything but a place with "unary `&` needs a place" and a slice of an array that is no place with "slicing an array needs a place", so the pass meets a name, a field, an index or a `*` alone. |
| `root_of` | The `ptr` of an array. | `check_builtin_field` of `sema_call.c` gives `ptr` to a `str` and a slice alone, and refuses it on an array. |
| `place_name` | A place that is no name, field, index or `*`, and an operand that is NULL. | Its callers pass the target of a store the checker accepted and the receiver of a collection, which are such places, and the parser gives every operand its node. |
| `anchor_of` | A unary operator other than `*` as the target of a store. | The checker refuses an assignment to anything but a place, and the place of an atomic operation and the receiver of a collection are places as well. |
| `sema_check_escapes` | A body that is missing, and a body that the checker checks quietly. | The checker calls the pass for a function with a body, and never inside a quiet check. |

## `src/antic/whole.c`

The pass over the whole program, sorted by batch 3c of the third phase.
It reads the IR that lowering and the reader of a library file give it.
`unit_whole` changes that IR where a library file written by hand could.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `whole_class_of` | A global past the globals of the program. | Every caller passes the descriptor or the base of a class record, or the interface of a subtable, which lowering and the reader set to a global of the program. |
| `whole_free` | No pass to free. | `whole_build` never gives NULL, and its one caller frees what it built. |
| `entry_at` | A table that holds no list of functions, a slot past its end and an entry that is no function. | Lowering writes every table as a list of the functions of its class, a stub in every abstract slot, and a call names a slot of the class it calls through. |
| `same_sym` | A symbolic value past the list, or none, and a kind outside the four. | Lowering names a value of the list, and the reader of the IR refuses a symbolic value of another kind. |
| `same_agg` | An aggregate that is none. | `same_vtype` asks only where the type is an aggregate, and lowering then names one. |
| `names_field` | A symbolic value past the list, a value that is no offset, an operation over offsets and a chain of more than 64 of them. | Lowering adds each offset of an address as a `ptradd` of one `offset_of`, and the offset of an element as a `mul` instruction, so a `ptradd` by a symbolic value names one field of the list. |

## `src/antic/whole_tables.c`

The writers of the runtime tables, sorted by batch 3c of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `whole_write_trampolines` | A count of the items of the function lists that no `size_t` holds. | The lists lie in memory, so their items add up to less than the address space. |
| `reach_calls` | A provider whose walk ran before. | `check_cycles` clears what the walk saw before it walks each provider. |
| `global_value` | A global past the globals of the program. | The reader bounds every address of a library file, and lowering names globals of the program. |

## `src/antic/lower_place.c`

The places and the constants of lowering, sorted by batch 3d of the
third phase. Lowering reads a tree that the checker has checked without
an error.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `bitfield_of` | An expression that is no field, or a field that names a symbol, and a field that its class lacks. | `place_address` asks for an `EXPR_FIELD` alone, after a static field took its own path, and `lower_field_owner` gives the class of the chain that declares the field the checker took. |
| `type_place` | A static field, a field of a union, a field the struct lacks, and a field past the aggregate of the struct, a bitfield or one of another type there. | A static field is `atomic`, and the checker refuses it as a place, so its functions take its address through `lower_address`. `names_union` returns before for a union, and the aggregate of a struct holds each of its fields in order with its type, a bitfield in its unit, which `place_address` sends elsewhere. |
| `place_address` | A static field as a place. | As in `type_place`. |
| `lower_place` | A place that `place_address` cannot give. | `place_address` gives one on every path. |
| `place_address` | A name that is a scalar in its temporary but an aggregate where it is read. | A name is read as an aggregate only where it is declared one, and a `?T` narrowed to a scalar is declared an aggregate. |
| `const_tree` | A field of a `?T` of the length of `has` with another name. | A `?T` holds the fields `value` and `has` alone. |
| `lower_location_value` | A position outside a function, and a field of `SourceLocation` that it does not know. | `here` and `fail` stand in a function, and `anti.lang` gives `SourceLocation` its five fields, each of which it names. |

## `src/antic/coff.c`

The join of COFF objects, sorted by batch 3d of the third phase.
`unit_coff` builds the objects by hand, so every byte a damaged object
can hold is an input.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `put_size` | An offset or a size of the joined object past 4 GiB, the refusal of `put_size` at each of its callers. | The objects lie in memory beside the join, which then needs more than 4 GiB more, and no test holds that. |
| `name_field` | A long name of a section whose `/` and digits do not fit 8 bytes. | The test before it takes offsets up to 9999999 alone, which `/` and 7 digits hold. |
| `write_symbols` | An auxiliary record of a static symbol outside a section, and an associative selection of a section that is no COMDAT. | `read_comdats` refuses an auxiliary record of a static symbol outside a section, and reads a selection only from a COMDAT. |
| `coff_join` | A failure of `read_sections`, and a symbol table whose records do not add up to the count. | `read_sections` returns true on every path, and `write_symbols` writes the records that `number_symbols` counted. |

## `src/antic/llvm_emit_call.c`

The calls, the parameters and the results of the LLVM text, sorted by
batch 3d of the third phase. `unit_llvm_emit` builds the IR by hand where
a library file written by hand could, which the verifier passes.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `param_facts` | A size of a parameter's type that does not fold. | Every size the IR names folds once the back end lays the module out. |
| `store_params` | A failure before the parameters, and a parameter that passes as the hidden pointer of a result. | The parameters are stored first in the entry block, before any slot that can fail, and `abi_classify` gives `ABI_SRET` to a result alone. |
| `reinterpret` | A callee result or an instruction with no LLVM type, and two integer types of one size with two names. | An aggregate result goes to `keep_result` before, an instruction of an aggregate type names its aggregate only where its callee returns one, and an integer type of one size has one LLVM name. |
| `context_floats` | A float argument that does not translate, before the context. | A float that C may read stands in a temporary, and a temporary always translates. |
| `result_range` | A direct result whose word is not the LLVM type of the result. | `abi_classify` names a direct result with the type `llvm_emit_value_type` gives it. |
| `llvm_emit_local_export` | An export fn of another module in the object of one module. | The object of one module defines the functions of that module, and a copy of a generic is no export fn. |

## `src/anti/build.c`

The build command, sorted by batch 3d of the third phase. The fault
cases of `tests/anti-build/messages.cmake` make a call of the system
fail. Each failure of a read or a write that a count can name is then an
input.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `options_key` | Options without the name of a package. | `name_project` names every project, by its manifest or by the path of its root module. |
| `object_compile` | A licence text that it cannot read, and an object compile that links. | The object of a library file takes no options of the header, and it is the object of a module that the program imports, which holds no `main`, so antic stops at the object with status 3. |
| `soname_link` | `--soname` on a library that is not shared, and a Linux library without a version. | antic refuses `--soname` beside `--lib static` and without `--package-version`, so the build that asks fails first. |
| `deliver_notice` | A notice printed by a path dependency. | A path dependency builds with `BUILD_NOTICE_BUILD`. |
| `root_module` | A project without a module. | The build refuses a `src/` that holds no `.anti` file. |
| `build_path_dependency`, `build_targets`, `run_built` | A host that is no target of Anti. | anti runs on the six hosts of its targets alone. |

## `src/antic/sema_lock.c`

The safety check `lock-order` and the warning `blocking-under-lock`,
sorted by batch 3e of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `declaring` | A field that no level of the chain declares, after which it names the type itself. | The checker accepts a field that the type of the place or a base declares, and the first level the walk asks holds it. |
| `mutex_lock` | A static field that is a Mutex. | A static field is atomic, and an atomic field holds no Mutex. The provisional entry on the names of the locks of C4 names one all the same. |
| `function_of` | A function asked for a second time, which gives its index. | The walk asks for each function of the module once, and for each anonymous function once, where it stands. |
| `first_wait` | A walk that reaches no wait, which returns false. | A function waits when one of its sites waits or calls a function that waits, and the walk enters each such callee that it has not entered yet. |
| `warn_waits` | A quiet check, after which it warns of nothing. | The pass runs once per module after the checker, never inside a quiet check. |

## `src/antic/lower_stmt.c`

The statements of lowering, sorted by batch 3f of the third phase.
Lowering reads a tree that the checker has checked without an error.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_forget_received` | A value that another function received. | The list of the received values starts empty for each function, and the body of an anonymous function is lowered as a function of its own. |
| `lower_assign`, `lower_flags_assign` | A place that `lower_place` cannot give. | As in `lower_place` of `src/antic/lower_place.c`. |
| `destructure` | A name of a pattern without a symbol. | The checker declares every name of a pattern, and `_` is a name like any other. |
| `lower_let_unwrap` | A value that ends its path, and a name without a symbol. | The value of `let ... else` is a `?T` of a value, and no such expression ends its path. The checker declares the name. |
| `lower_let_unwrap`, `lower_let_value` | An `else` block that falls through to the rest. | The checker refuses an `else` block whose end can be reached. |
| `in_slot` | A name without a symbol. | Every caller passes the symbol of a name the checker declared. |
| `lower_stmt_kind` | A `yield` outside a handler, and a `yield` with a value in the handler of a call that writes no result. | The checker refuses both, the second with ``the call writes no result, so `yield` takes no value``. |
| `reserve_call_handler` | A statement without an expression. | A `let` always takes a value, and a statement of an expression and an assignment always hold one. |

## `src/antic/lower_expr.c`

The expressions of lowering, sorted by batch 3f of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `flag_op` | The negation of the flags form. | Each caller passes a binary operator, and `lower_flag_operation` takes `IR_NEG_FL` for a negation itself. |
| `lower_cast` | `as` between two `f16` types, and between two floats of one IR type. | `f16`, `f32` and `f64` are one type each, so either pair is the same type, which the test of the same type before took. |
| `lower_sym_of` | A type parameter in a symbolic value. | The comment of the arm gives it: a copy of a generic replaces every parameter before lowering, and the driver refuses a program that needs one. |
| `keep_address_argument` | A value at a pointer parameter that is no `&` of a place. | The checker passes a pointer or the address of a place to every pointer parameter, a receiver among them. |
| `dispatched_params` | A call through a table without the symbol of its function. | A call through a table names the function the checker found in the class, whose symbol has a type. |
| `table_target` | An entry that the table lacks. | The checker names an entry that the table of the class holds, so its index is above 0. |
| `lower_expr_value` | `parallel` and `dispatch` as a scalar, and a bound function there. | Each gives an aggregate, which `lower_address` builds before the switch of the scalar values. |
| `lower_expr_value` | A kind of expression without an arm. | Every kind has its arm, and the literals of aggregates take `lower_address`. |

## `src/antic/lower_desc.c`

The descriptors and the tables of lowering, sorted by batch 3f of the
third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `class_type_args`, `class_ancestors`, `class_functions`, `class_chain`, `class_versions` | A part of a descriptor that the module holds already. | `lower_class_descriptor` builds each part once, after it found no descriptor. A class of another module stops there with a reference. |
| `class_ancestors` | The ancestors of the root. | `lower_class_descriptor` returns the descriptor of the runtime for the root before it asks for any part. |
| `class_functions` | A class whose table holds no entry. | Every table holds the seven functions of the root. |
| `interface_descriptor` | A descriptor of a sub-object that the module holds already. | `lower_interface_table` asks for it once, after it found no table of its own. |
| `lower_interface_table` | An entry of the interface that neither the class nor the interface fills. | The checker refuses a class that leaves an abstract function of its interface without a body. |

## `src/antic/platform.c`

The layer of the host of antic and anti, sorted by batch 3f of the
third phase. The paths of the other hosts are compiled out on the Mac,
and the measurement holds those of macOS.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `self_path` | A path of the executable of length 0, and one that `realpath` cannot resolve. | The system gives the path of every running executable, and the path names that file. |
| `platform_self_directory` | A path of the executable without a directory. | The path that `self_path` resolves is absolute and names a file below a directory. |
| `wait_for` | A wait that a signal interrupts, which it repeats. | No test sends a tool a signal while it waits. |
| `environment_with` | No environment. | A process of macOS starts with `environ` set. |
| `platform_run_on_stack` | Attributes of a thread that the system cannot set up. | The system sets them up, and it takes the size of `PLATFORM_WORK_STACK`. |

## `src/antic/parser_stmt.c`

The statements of the parser, sorted by batch 3g of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `parser_read_handler` | No `catch` where the handler is not required, which reads no handler. | Every caller that passes `false` checks for `catch` first. |
| `parser_read_handler` | A name after `catch` that is no name. | It reads a name only where the next token is one. |
| `arm_body` and `expression_statement` | A `catch` after a call whose handler does not parse, and a `catch` after the call that fills a place. | The expression parser reads the `catch` after a call as part of the call, so the statement sees a call without one. |
| `for_statement` | A name of the loop that is no name. | The lookahead that chose the bound form and the tuple pattern found names at each place. |

## `src/antic/types.c`

The types of a compilation, sorted by batch 3g of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `types_copy_of_parts` | A type that holds no lent part, which it gives back as it is. | The one caller tests `types_holds_lent` first. |
| `types_fn_form`, `types_fn_owned` and `types_fn_never` | An operand that is NULL, no function type or a bound function, which each gives back as it is. | Every caller passes a function type it built or tested, and a bound function takes neither the form of two words nor `own`. |
| `types_member_level` and `types_holds_entry` | A member that no level of the chain declares. | Every caller passes a member that a lookup on the same chain found. |
| `types_make_declared` | A name that no type of `anti.lang` that the compiler declares has. | The reader of a library file looked the name up before, and a damaged file stops there. |
| `types_signature` | An operand that is no function type. | Every caller passes the type of a function. |
| `types_item_spelled` | An item that is NULL. | Every caller and the walk over the members pass an item. |
| `holds_walk` | A type that is NULL. | Every type that the walk reaches is the type of a value or of a field. |
| `types_hash_call_type` | A hash call whose callee is no name of a function with a parameter. | `sema_hash.c` builds each call as `x.hash()` and the checker binds its callee to the function. |
| `operator_spelling` | An operator outside its table, which gives `?`. | A symbolic value holds the operators of an integer alone, and the table names each. |

## `src/rt/patterns.c`

The searches of a pattern, sorted by batch 3g of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `literal` | A pattern literal that does not compile at start. | antic compiled every literal with the same PCRE2 when it checked it. |
| `anti_rt_regex_message` | An error number outside the table of PCRE2, which gives an empty text. | `anti.regex` passes the number that a compile of PCRE2 gave. |
| `failure` | A failure of a search that no limit explains, which stops the program. | The text of a search is a `str`, which holds UTF-8 alone. A slice of a `str` is a `[]byte`, which a pattern of bytes searches. |
| `slice` | An end before the start. | A match ends at or after its start, since `\K` in a lookaround is refused. |
| `search_again` | A search that no longer finds the match it found. | The search runs again on the same text with the same options. |

## `src/anti/repo.c`

The repositories of `anti build`, sorted by batch 3g of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `index_dir` | A name that is no package name. | The manifest reader refuses the name of a dependency by R164, and `deps.c` the name of an index and of a lock file, as `tests/diagnostics-open.txt` says. |
| `repo_module` | A name, a version, a module path or a digest that the rules refuse. | `deps.c` checks each of an index and of a lock file before it calls `repo_module`. |

## `src/antic/sema_call.c`

The calls and the members of the checker, sorted by batch 3h of the third
phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `reached_member` | A body table outside its three kinds. | `types_body_table` gives one of the three. |
| `refuse_abstract_call` | An abstract member that no level of the chain declares. | The member was found on the same chain. |
| `refuse_own_member` | A member without a symbol or a type. | Both callers pass a function that the declaration pass gave a symbol and a type. |
| `from_module_operator` | A copy of a generic `operator fn` of a library, which keeps the module of the generic. | The checker checks the levels in the tree of the generic, where the library compiles it. A client that reads a private field of a copy through `==` and `<` of a library reaches no check of a level. |
| `sema_resolve_origin` | No class `anti.lang.Error`, which it leaves. | A `may fail` function of a module that does not import `anti.lang` is refused at its declaration, and its body is not checked. |
| `check_handled` | A handler of the enclosing `try` outside a `try` block. | The handler takes that kind inside a `try` block alone, and the arm before the `switch` returns there. |
| `refuse_field` | A missing field of a copy that a refused chain of copies left unfilled. | The copy past the limit of 256 levels has no name that a program can write. |
| `unfailing_result` | The interface that `lib.instance` provides on a call that cannot fail. | `instance_at` of `anti.plugin` may fail, so the call goes through `check_handled`. |
| `check_type_member` | A member without a symbol, and a static field that is not atomic. | The declaration pass gives every member a symbol, and the parser refuses `static` without `atomic`. |
| `type_namespace` | `Object` that no lookup finds. | `Object.deserialize` is checked as its own call before the namespace of a type. |

## `src/antic/sema_value.c`

The values and the moves of the checker, sorted by batch 3h of the third
phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `owning_field` and `owns_reason` | A class that owns memory without an `own` field, a `destruct` or a field whose value owns. | `sema_needs_teardown` holds the same rule, and only a class it answers reaches the phrase. |
| `concurrent_in` and `declared_align` | A type that is NULL. | Every caller passes the type of a value or of a field. |
| `field_up` | A name that no level of the chain declares. | The caller found the field on the same chain. |

## `src/antic/sema_uninit.c`

The check `uninitialized-read`, sorted by batch 3h of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `read_element` and `write_element` | A buffer that the walk does not follow, or a path that has ended. | `element_of` found the buffer of the place, and the walk of a block stops at the statement that ends its path. |
| `place_element` | A place that is NULL. | Every caller passes the target of an assignment or an operand. |
| `walk_closure` | An anonymous function without a body. | The parser gives every anonymous function a block. |
| `walk_expr` | `x in lo..hi` without its test. | The checker writes the test of every `in` as two comparisons. |
| `walk_assign` | An element whose buffer and index cannot be read. | `place_element` gives an element of a buffer alone, which `element_of` takes apart. |
| `sema_check_fills` | A function without a body, and a quiet check. | The checker calls the pass for a function with a body, outside a quiet check. |

## `src/rt/registry.c`

The reader of `Object.deserialize`, sorted by batch 3h of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lend` | A size above `INT64_MAX`. | Every size comes from a type or from a count of elements that a text in memory holds. |
| `read_elements` | A count of elements times their size that does not fit. | A text with that many elements does not fit in memory. |
| `read_owned` | An `own` pointer to a class without a descriptor. | The compiler writes the descriptor of every class a field names. |
| `read_array` | An array without an element type or a length. | The compiler writes both for every array field. |
| `read_value` | A number that the scanner of JSON takes and the reader of a float refuses. | `anti_rt_json_valid_number` refuses every text without digits first. |
| `read_value` | A tuple or a `?T` without its descriptor. | The compiler writes the descriptor of every tuple and `?T` a field holds. |
| `anti_lang_Object_deserialize` | No allocator. | `Object.deserialize` takes a `*Allocator`, which is never `none`. |

## `src/antic/header.c`

The C header, sorted by batch 3h of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `scalar_name` | A type with no scalar name of C. | Every caller passes a scalar. |
| `type_names_error` and `emit_tuples` | A type that is NULL. | Every caller passes the type of a field, a parameter or a result. |
| `element_c_name` | A slice in the name of a tuple or a `?T`. | An exported signature that holds one is refused, since C cannot represent it. |
| `vector_typedef` | A `char` lane. | A `char` lane of a simd struct is refused. |
| `class_fields` | The table pointer among the own fields of a class. | The root holds it, and the header writes the root as `anti_Object`. |
| `member_signature` and `class_wrapper` | A parameter without a name. | The declaration and the library file both carry the name of every parameter. |

## `src/antic/parser_decl.c`

The declarations of the parser, sorted by batch 3h of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `parser_member` | `internal` before a member. | The readers of a class body and of an enum body refuse `internal` before they call it. |
| `parser_member` | No `fn` after the marks. | The callers call it where `fn`, `const` or `static` follows the marks. |
| `parser_compatible_line` | A version that is no number. | `tests/diagnostics-open.txt` names the reason. |
| `parser_members_of` | `inherits` and a nested type in the body. | Its one caller reads an enum body and passes no list of nested types. |

## `src/antic/lexer.c`

The lexer, sorted by batch 3h of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lexer_token_kind_name` | The kinds of a literal, a comment and the end. | The parser expects a symbol, a keyword or an identifier alone, and the operators of the checker are symbols. |
| `lexer_token_kind_name` | A spelling longer than its cache entry. | No spelling is that long, by the `[provisional]` entry in `docs/decisions.md`. |
| `lexer_token_category` | The end and an invalid token. | `--dump tokens` stops before the end, and a source with an invalid token prints no tokens. |

## `src/antic/antl_read_ir.c`

The checks of a later library file, which the fixes of T242 to T251
added after batch 3b.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `check_table_read`, `check_table_write` and `check_generic_copy` | A record of the module of the file itself. | A file records what it takes from other modules alone. |
| `copy_of_missing_generic`, `check_table_read`, `check_table_write` and `check_generic_copy` | A module that no file of the program holds. | Every import of a file is read before it. |
| `check_table_write` and `check_generic_copy` | A class or a generic that the later file lacks. | The read of the declarations names it first, as `needs struct` and `needs generic`. |

## `src/antic/sema_free.c`

The check `use-after-free`, sorted by batch 3i of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `value_of` | An expression that is NULL. | Every caller passes an operand, a target or a part of a literal that the parser wrote. |
| `walk_closure` | An anonymous function without a body. | The parser gives every anonymous function a block. |
| `walk_expr` | `x in lo..hi` without its test. | The checker writes the test of every `in` as two comparisons. |
| `sema_check_frees` | A function without a body, and a quiet check. | The checker calls the pass for a function with a body, outside a quiet check. |

## `src/antic/sema_hash.c`

The default hash and the default `==`, sorted by batch 3i of the third
phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `nested_call` | A call of `hash` on a part that the checker refuses. | `add_call` asks for it on a part whose hook has the one form, `operator fn hash(x: T) -> u64`, or on a part with the default. |
| `sema_hash_without_effects` | A receiver without a type. | The checker gave the receiver of the call its type before the statement asks. |
| `sema_default_eq` | A type that the same question answered without `==`. | Each walk returns at the first part without `==`, so no question asks a type again after it answered no. |

## `src/antic/pattern.c`

The reader of a pattern literal, sorted by batch 3i of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `pattern_group_count` and `pattern_group_number` | A pattern that PCRE2 refuses. | The checker asks for the groups of a literal that compiled. |
| `class_char` and `parse_escape` | A `\` at the end of a class or of the pattern. | PCRE2 refuses both before the reader runs, as `values_pattern_that_ends_with_a_backslash` holds. |
| `parse_class` | A range that ends with an escape of a set. | PCRE2 refuses it before the reader runs, as `values_pattern_range_that_ends_with_a_class` holds. |

## `src/antic/optimize.c`

The optimizer of the IR, sorted by batch 3i of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `fold_float` and `fold_conversion` | An operation that the switch has no arm for. | Their callers pass the operations of the arms alone. |
| `mark_reachable` | A function without blocks. | Lowering gives every function with a body its entry block. |
| `mark_reachable`, `merge_blocks` and `drop_overflow_checks` | A block without instructions. | Lowering ends every block with a jump, a branch, a `ret` or `unreachable`. |
| `same_operand` | Two globals, two functions or two blocks. | The callers compare the offsets of slots, which are numbers and temporaries. |
| `split_slot` | A load or a store at an offset that no field of the slot has. | The fields come from the offsets of the same loads and stores. |

## `src/anti/bindwrite.c`

The writer of a binding, sorted by batch 3i of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `declare` | A parameter of an array type in the shim. | clang gives every array parameter as the pointer C passes, and a description in the format of rlparser has no shim. |
| `enum_named` | An enum type that the module does not hold. | The reader binds every enum that a type names. |
| `settable_record` | A field without a name. | The reader names every anonymous member, as `anon0`. |

## `src/antic/sema_safety.c`

The checks of thread-safe classes, sorted by batch 3i of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sema_note_guarded_pointer` | A local without a symbol. | Both callers pass the symbol of a local or a parameter that the checker declared. |
| `sema_check_reach` | A field without a home, and the hidden lock. | Every field of a type has the type as its home, and no program names the lock of a synchronized class. |
| `written_field` | A base without a type, and a field that a base class declares and `types_find_field` does not find. | The checker typed the base and found the field of every place it typed, so the first look finds it. |

## `src/antic/layout.c`

The layouts of the back end, sorted by batch 3j of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `compute` | A struct whose layout depends on itself. | The checker refuses a struct that holds itself, and the reader of a library file refuses an aggregate that names itself, in `map_agg_at`. `holds_itself` of `tests/unit/test_layout.c` builds one by hand, and the layout ends with the message of the guard since T267. |

## `src/antic/lower_simd.c`

The operations of simd structs, sorted by batch 3j of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lane_at` | A lane after the first in a loop over the lanes. | A loop takes one lane per turn, so every caller asks for lane 0 in it. |

## `src/antic/lower_hash.c`

The default hash, sorted by batch 3j of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `hash_at` | A part that is a lock. | `hash_fields` and `hash_member` pass over a lock field, and no other part holds a lock. |
| `hash_member` | A member that holds no data or a bitfield, after the `switch`. | Both return before the `switch`. |

## `src/anti/check.c`

The command `anti check`, sorted by batch 3j of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `base_options` | A host that is no target. | The six hosts that run `anti` are the six targets. |
| `front_end_class` | A file that fails the front end of another target alone. | The checker holds no size, and it refuses a constant that does not fit a type on every target, so the front end gives one answer on all six. |
| `unit_blocks` | A source whose outline fails. | The front-end class read the same source first, and the run ends at its failure. |
| `format_class` | A file that does not lex. | The same: the front-end class reports it, and the formatting class does not run. |

## `src/anti/cache.c`

The keys of `anti build` and `anti test`, sorted by batch 3j of the third
phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `archive_digest` | An archive whose identity cannot be read. | `target_archives` listed the archive on the line before, and the faults of the seam are held while the key is made. |

## `src/antic/sema_thread.c`

The checks of atomics and threads, sorted by batch 3j of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `atomic_place` | A name of a static field that is not marked atomic itself. | The symbol of a `static atomic` item is atomic, so the first test answers. |
| `definite_joins` | A call through `?.`. | The call stands inside the node that `?.` writes, which the walk does not enter. |

## `src/antic/lower_owning.c`

The teardown, the copy and the clear of owning values, sorted by batch 3k
of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_destroy_owned`, `lower_clear_owned` | A kind of type the switch does not name. | Every caller asks `sema_needs_teardown` first, which answers `true` for the kinds the switch names alone. |
| `lower_clear_owned` | A function value without `own` in a part that a clear reaches. | The clear of a struct, a tuple, an array or a variant enters a part that needs a teardown alone, and a function needs one when it is an `own fn`. |
| `lower_take` | A `take` whose field is no place. | The checker refuses `take` of anything but a field of a place, so `lower_place` takes every field it gives. |
| `lower_unbuild` | The end of a block before a field or the base is torn down. | Each teardown of a field continues the block it was given, and no field of a class ends a path. |

## `src/antic/sema.c`

The entry points of the checker, sorted by batch 3k of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `builtin_of_token` | A token that names no built-in type. | The parser hands a token here only after it read one of the built-in types. |
| `resolve_type_inner` | A constant where a type stands, `TYPEX_CONST`. | The parser and the check of a generic argument refuse a constant where a type stands, so no such node reaches the resolution of a type. |
| `function_type_of` | A function with `self` whose owner has no type yet. | The functions of a class body get their types after the class itself is declared. |

## `src/rt/toml.c`

The reader of the TOML subset, sorted by batch 3k of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `next_pair`, `grow_names` | A room whose double does not fit a `size_t`. | A document of that many pairs does not fit in memory before. |
| `read_header` | A name of a `[[table]]` and its number that do not fit the buffer of the table. | `read_header` refuses a name within 24 bytes of the size of the buffer, and the number takes 21 at most. |
| `anti_rt_toml_shape` | A document that is `NULL`. | `anti` calls it with a document it read, and `anti.toml` does not call it. |

## `src/antic/ast_dump.c`

The dumps of the trees, sorted by batch 3k of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `dump_module` | `abstract`, `concrete` and `self` on a function at module level. | The parser refuses each of them outside a class body, and the dump of a class body writes its members through `dump_members`. |

## `src/anti/main.c`

The commands of `anti`, sorted by batch 3k of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `license_project`, `build_for_tests` | A host that is no target of Anti. | Every host that runs `anti` is one of the six targets. |
| `test_command` | A project whose `src/` holds no module, met by `anti test` without a file. | The read of the manifest refuses such a project before, with `holds no .anti file, so this project has no module`. |

## `src/antic/driver_search.c`

The search of the library files, sorted by batch 3k of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `link_names`, `driver_load_libraries` | A library file whose header does not read. | The driver reads the header of every library file of the command line before, and refuses such a file there. |

## `src/antic/lower_eq.c`

The default `==` and the parts it compares, sorted by batch 3l of the
third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_part_of` | A kind of type the switch does not name. | The checker gives the default `==` and the default hash to the kinds the switch names alone, and a lock leaves before. |
| `lower_root_fn` | A root class without the function of the name. | The callers ask for `equals` and `hash`, which `anti.lang.Object` declares with a function of the runtime. |
| `compare_at` | A lock among the parts it compares. | The fields, the elements and the cases of a value pass over a lock before, by `lower_field_skipped`, and the checker refuses `==` on a Mutex and an array of them. |

## `src/antic/lower_class.c`

The singletons, the thunks and `T(args)`, sorted by batch 3l of the
third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `singleton_instance` | A global of the instance that the module holds already. | `lower_singleton_get` runs once for each singleton, for the one `get` the compiler writes. |
| `lower_singleton_get` | A `get` whose body a library file brought. | It runs over the items of the module that is lowered, whose functions follow those of the library files. |
| `lower_find_member_fn` | No public function of the name in the chain. | `lower_interface_table` asks for an entry the class does not fill, which the interface fills, since a concrete class has a body for every abstract function and an abstract class gets no table. `lower_error.c` asks for `fatal`, which `anti.lang.Error` declares. |
| `lower_interface_thunk` | A thunk the module holds already. | `lower_class_global` keeps the table of each sub-object, which is built once, and one function fills one entry. |
| `lower_construct` | `T(args)` of a class without a `construct` of its own. | `check_construct` of `sema_call.c` refuses it with `` has no `construct` with arguments, so a literal builds it ``. |

## `src/antic/antl_write.c`

The writer of the library file, sorted by batch 3l of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `antl_visit_type` | A type parameter that the visit of its hook owner put in the table. | A parameter that a hook gives, `walked` or `indexed`, is named by its owner alone, which puts itself in the table before it visits that parameter. |
| `antl_put_type_ref` | A type the table does not hold. | The writer visits every type the file names before it writes a reference. |
| `put_ir` | A global with relocations. | Layout adds the relocations after the writer ran, and the reader adds them to the program it reads into, which no writer writes. |

## `src/anti/fmt.c`

The formatter, sorted by batch 3l of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `angle_list` | The first `>` of a `>>` taken at the start of a turn. | `angle_close` takes it and returns, and the list starts another turn after a `,` alone, which it reads with that mark clear. |
| `space_before` | A comment. | `emit_comment` places every comment, and every other caller passes a token. |
| `flush_doc_line`, `emit_comment` | An empty line below the first of a comment while a blank line waits. | The first line flushes before, and `flush` writes the blank line and clears the mark. |

## `src/antic/antl_verify.c`

The verifier of library files, sorted by batch 3l of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `mark_in` | A record that is none of the records of its table. | The reader builds every reference of a tree from an index into the table of its kind. |
| `inits_found` | A struct literal without a type. | `stands_as` refuses one at every place but the callee, `callee_read` refuses one there, and the walk stops at the first refusal. |

## `src/rt/symbols.c`

The readers of the symbol tables and the line tables, sorted by batch 3l
of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `dwarf_line` | No line table. | Both callers find the line section before and refuse a file without one. |
| `elf_find` | A caller that asks for the index of the section. | Every caller passes `NULL` for it. |

## `src/antic/docpage.c`

The pages of `anti doc`, sorted by batch 3l of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `const_value` | A constant without a value. | The check gives every constant a value or refuses the module, and the reader reads one for every constant the file carries. |
| `variant_cases` | A variant without the enum of its tags. | The checker gives every variant that enum as its base, and the reader reads it with the variant. |
| `tree_item` | An item without a symbol or a type, or one of a `tests` or `fixtures` block. | `anti doc` builds without `--tests`, so the parser drops both blocks, and the check gives every item it keeps a symbol and a type or refuses the module. |

## `src/antic/ir.c`

The module of the IR, sorted by batch 3l of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `ir_index`, `ir_temp`, `ir_const_agg` | A count above what an index of 32 bits or a `size_t` holds. | A module of that many records does not fit in memory before. |
| `ir_table_write_add` | A write of a table that the module noted already. | `lower_class_global` keeps each table, which notes its write as it is built, and a class implements an interface once. |
| `ir_same_scalar` | An operand of another kind than none, a temporary, an integer and a float. | Both callers answer a global, a function, a block and a symbol before, the four other kinds of operand. |

## `src/rt/conf.c`

The runtime configuration, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `format_text` | A message that `vsnprintf` cannot write, or writes at another length the second time. | Every format is a literal of the file, and its values are texts the runtime holds and numbers, which the C library writes the same way twice. |
| `set_key` | A value of more than `INT_MAX` bytes. | A value comes from an option or from a file the runtime read whole, and a test cannot give a process an argument or a file of 2 GiB. |
| `read_key` | A key of more than `INT_MAX` bytes. | The same bound on a file the runtime read whole. |
| `help` | An option whose form is 34 bytes or longer, which takes no padding. | The table of the options is fixed, and its longest form stands below 34 bytes. |
| `join_elements` | A key after an array that is `runtime` itself or `runtime.` alone. | A key after the array stands under `[runtime]` with a name of its own, or under another table, which the first test turns away. |

## `src/antic/text.c`

The text buffer of antic, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `text_format_name` | A generated name that does not fit its buffer, which aborts. | Each buffer holds the longest name it takes, which `test_name_size` of `tests/unit/test_text.c` checks with the largest count. |

## `src/antic/sema_declare.c`

The declarations of the checker, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `check_owned` | An `own` parameter whose type is the error type, and a function type of fewer parameters than the item. | `function_type_of` of `sema.c` gives the whole function the error type when one parameter has it, and `check_owned` returns before for that type. |

`holds_short_class` of an aggregate whose item is a short class value
waits for T265, whose program the checker refuses before it.

## `src/rt/json.c`

The JSON scanner of the runtime, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `anti_rt_json_member` | An object whose `{`, key, `:` or value fails on the second walk. | The first walk reads the whole input as one JSON object and refuses any other, so the second walk meets what the first read. |

## `src/antic/sema_class.c`

The rules of classes, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `check_replacement` | A qualified `concrete fn` whose qualifier names no table. | The caller skips such a function, whose message `check_qualifiers` gives, by T071. |

## `src/antic/sema_export.c`

The C side of the checker, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `has_plain_pointer`, `no_c_type` | No type. | The checker gives every pointer its element and every function type its result, `void` at the least. |
| `sema_check_extern_fn` | A parameter of the error type. | The function then has the error type, and the check returns before its parameters. |

## `src/antic/lower_error.c`

The error paths of lowering, sorted by batch 3m of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_handle_error` | A call without a handler, and one that the enclosing `try` block handles outside such a block. | The checker refuses a call that may fail without a handler, and `check_handled` of `sema_call.c` marks a call as handled by the block inside a `try` block alone, where lowering holds the scope of the block. |
| `lower_pointer_guard` | A guard without a symbol, or one after the end of a path. | The checker gives every `let` its symbol, and lowering writes no statement after the end of a path. |
| `lower_guard_missing` | A handler of a guard that ends without leaving the block. | `check_pointer_guard` of `sema_stmt.c` holds the handler to leave the block or end with `yield`, as every handler does. |

## `src/antic/sema_operator.c`

The operators and the hooks of the checker, sorted by batch 3n of the
third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sema_hook_well_formed` | A hook without a function type, and a hook other than `next`, `value` and `iter`. | `check_hook` of `sema_class.c` returns before for a hook without a function type, and a loop asks once `sema_declare_all_members` gave every member its type. Every caller names one of the three hooks. |

## `src/antic/lower.c`

The shared parts of lowering, sorted by batch 3n of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `name_of_type` | The name of a type without its module. | Its one caller, `lower_agg_of`, asks for the qualified name. |
| `lower_field_owner` | A field that no class of the chain declares, for which it gives the class itself. | The checker resolved every field that lowering names to a class of the chain. |
| `lower_struct_of_expr` | An expression without a type. | The checker gives every expression it passes a type. |

## `src/antic/lower_hook.c`

The hooks of tracing, sorted by batch 3n of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_hook_call`, `lower_hook_failed` | A function that is not traced, a call after the end of a path, and a `failed` hook without an error. | `lower_function.c` calls the `enter` hook of a traced function alone, once it took the name, and the action of `leave` runs on an exit of such a function, inside its block. A failing exit carries its error. |

## `src/antic/driver_library.c`

The libraries for C, sorted by batch 3n of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `bundle` | `llvm-ar` that `bin/` of the archive lacks. | Its one caller, `build_static`, looks the tool up first and refuses the library without it. |

## `src/anti/units.c`, `src/anti/bind.c` and `src/anti/doc.c`

The units of `anti` and two commands that take the host, sorted by batch
3n of the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `unit_host`, `bind_header`, `doc_run` | A host that is none of the six targets. | `anti` builds for the six hosts alone, and `target_host` knows each. |
| `unit_report_cycle` | A unit of the cycle that imports no unit left. | Every unit left imports one that is left, or it would have been placed, so the walk meets a unit it met before. |
| `write_formatted` | A binding that does not lex. | The writer of `anti bind` writes names, numbers and texts it quoted, which the lexer takes. |

## `src/anti/doc.c`, `src/anti/zip.c`, `src/anti/jsontree.c` and `src/anti/bindexpr.c`

The pages, the archives and the readers of `anti`, sorted by batch 3n of
the third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `doc_markdown` | An empty doc text. | Its one caller, `doc_body`, returns before for one. |
| `zip_write` | Entries of more than 4 GiB, which a zip without its 64-bit records cannot hold. | Every archive `anti` writes holds the map and the debug files of one program, far below the bound. |
| `read_string` | A string that the second walk of the scanner refuses. | The first walk read the same bytes as a string. |
| `list_add` | A list whose room doubled no longer fits a `size_t`. | It needs more memory than a machine holds. |
| `apply` | An operator of integers that the switch does not name. | `binary` passes the ten operators of its levels alone, and the switch names each. |
| `bind_eval_const` | A value of a kind the switch does not name. | `enum bind_eval_kind` has the three kinds the switch names. |

## `src/rt/lock.c` and `src/rt/signal.c`

The locks and the signals of the runtime, sorted by batch 3n of the
third phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `order_take` | A lock that the thread holds already. | A dev build stops at a Mutex its thread holds in `held_already` before the order is taken, and the hidden lock of an object that its thread holds is entered again without one. |
| `order_take` | Both orders of a pair recorded, the first not reported. | The second order of a pair is recorded where the first is found, and that marks the pair reported. |
| `order_give` | A release without a take. | Every `sync` releases what it took. |
| `deliver` | A signal outside the table, and one without a function. | The runtime catches a signal that `on_signal` registered alone, inside the table and with the function it takes, which is no `?fn`. |

## `src/antic/antl_io.c`, `src/antic/linker.c`, `src/antic/llvm_run.c`, `src/antic/userdirs.c`, `src/rt/regex.c` and `src/rt/cpu.c`

The bounds and the memory of six files, sorted by batch 3n of the third
phase.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `antl_put_count` | A count above `UINT32_MAX`. | A module that held one would not fit the memory of antic. |
| `antl_get_name` | A name longer than `INT_MAX`. | `antl_get_count` refuses a count above the bytes left, so the name needs a library file of more than 2 GiB. |
| `next` of `linker.c`, `push` of `llvm_run.c` | A command of more strings than the array holds. | Each bound covers the longest command, as the comment at each says, and the call aborts. |
| `unix_dir`, `windows_dir` | A kind of directory that the switch does not name. | `enum user_dir` has the four kinds the switch names. |
| `parent_of` | A path without a separator, or one at the root. | Its one caller passes the directory of antic itself, which stands below the root. |
| `compile_failed`, `put` of `regex.c` | PCRE2 without memory, and a text whose map would not fit an `int64_t`. | Both need more memory than a machine holds. |
| `anti_rt_cpu_level` | A failure of `anti_rt_getenv`. | It fails for memory alone. |

## `src/anti/bindclang.c`, `src/anti/bindexpr.c`, `src/rt/regex.c` and `src/anti/doc.c`

Batch 3o of the third phase sorted the reader of a header, the evaluator
of macros, the rewriter of byte patterns and the pages of `anti doc`. It
is the first batch chosen by the sides of the conditions
that the suite misses.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `find_clang`, `where_to_get`, `bind_read_clang` | A host that is none of the six targets, and a directory of `anti` that the system does not give or that holds no separator. | `anti` builds for the six hosts alone, and the directory of a running program is a path below the root. |
| `leading_int` | A text that `strtol` reads no digit of. | It reads on from a first byte that is a digit. |
| `say_no_runtime` | No directory above the one of `anti`. | The directory of `anti` stands below the root, so `parent_of` gives one. |
| `type_name_taken` | An enum named as the name it is asked for, under a tag other than that name. | A typedef names an enum without a tag alone, so an enum with a tag has the tag as its name. |
| `typedef_named`, `enum_named` | An entry of an enum without its enum. | `declare_enum` gives each entry its enum. |
| `named_anonymous` | A spelling without `(unnamed` or `(anonymous`. | Its one caller passes a spelling that holds one. |
| `read_fields` | A record inside a record whose C name is not set yet. | `declare_record` sets the C name before it reads the fields, and clears that of an anonymous member after. |
| `declare_enum` | A fixed base that is no scalar where the base is chosen. | The check before it leaves such an enum out. |
| `after_group` | A group that no `)` closes. | The type of the function parsed before, and a spelling that parses closes every group. |
| `read_macros` | A value that `bind_eval_const` does not write. | `bind_eval` gives one of the three kinds the writer names. |
| `take` of `bindexpr.c` | An operator of one byte, `<` or `>`. | The levels of `binary` name `<<` and `>>`, and no operator of one byte of them. |
| `member_end` | A POSIX class whose scan stops at its closing pair, but not at the end of the class. | The scan goes on past every pair that is not the closing one. |
| `anti_rt_regex_group_number` | A name that PCRE2 names more than once and the name table lacks. | PCRE2 gives `PCRE2_ERROR_NOUNIQUESUBSTRING` for a name its table holds. |
| `is_item`, `resolve_name`, `copy_link` | No page. | Every page and the index pass the page they write. |
| `resolve_name` | A name that ends in `(` and a byte other than `)`. | `is_name` refuses a `(` that is not the `()` at its end. |
| `doc_html` | A list without an item. | A list starts with its first item. |

## `src/antic/sema_expr.c`, `src/antic/sema_call.c`, `src/antic/sema_generic.c` and `src/anti/doc.c`

Batch 3p of the third phase sorted the sides of the conditions of three
files of the checker and the sides of `doc.c` that batch 3o left. The
sides of `watch_changes`, `sema_iterate`, `check_optional`,
`check_expr_inner`, `hook_result`, `walks_as`, `builtin_meets`,
`chan_field_pos`, `check_handled`, `refuse_own_member` and
`check_type_member` that the suite misses are the guards of their rows
above.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sema_is_place`, `sema_mark_address_taken`, `refuse_lent`, `refuse_ending`, `sema_whole_optional` | A name without a symbol. | The checker gives every name it resolves its symbol, and a name it does not resolve had its message, after which the caller returns. |
| `sema_bitfield_of`, `through_lent`, `destroy_of_local`, `refuse_ending` | A base or an operand without a type. | Each runs after the check of its operand, which gives every node a type. |
| `is_fn_dup`, `owned_fn` | An object node of another operation, and a value of no type. | The callers pass a value of a function type, which `delete` and `destroy` never give. |
| `require_owned` | A name of an `own fn` that is no parameter. | A local never holds an `own fn`. A `let` of one and a tuple taken apart give a function that is not kept, as `expression_conversions` shows. |
| `sema_null_pointer_maker`, `sema_error_maker`, `sema_error_text_maker`, `trace_capture`, `sema_resolve_origin` | A class of `anti.lang` that lacks the function the compiler calls, or a field of `Error` that is not there. | The library file of `anti.lang` of the runtime archive has each, and a module without the import gets the message of the import first. |
| `write_interfaces` | A source that does not parse, in the loop that compiles the interface files. | The first loop returns for one, by T123. |

## `src/antic/sema_class.c`, `src/anti/fmt.c` and `src/antic/sema_safety.c`

Batch 3q of the third phase sorted the sides of the conditions of the
rules of classes, the formatter and the checks of thread-safe classes,
which no batch had chosen by their branches. The sides of
`check_replacement`, `angle_list`, `space_before`, `flush_doc_line`,
`emit_comment`, `sema_note_guarded_pointer`, `sema_check_reach` and
`written_field` that the suite misses are the guards of their rows above.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `same_signature`, `check_hook`, `eq_signature_allowed`, `check_free_operators`, `member_params`, `sema_entry_signature` | A function without a symbol, or one whose type is no function type. | The declaration gives every function of a body and of the module its symbol and a function type before the checks of the classes and of the operators run. |
| `sets_stmt` | A `let` without a value. | The parser gives every `let` its value, which "Core language" in `docs/decisions.md` makes mandatory. |
| `sema_points_into_fields` | An array field that leaves a public function as a slice, and the `ptr` of an array field. | An array converts to a slice nowhere a `return` stands, and an array has no field `ptr`, so the checker refuses both before the check runs. |

## `src/antic/sema_call.c`, `src/antic/sema_generic.c`, `src/antic/sema_const.c`, `src/antic/sema_copies.c`, `src/antic/sema_expr.c` and `src/antic/lower_expr.c`

Batch 3r of the third phase sorted the sides of an error type in three
files of the checker. Batches 3p and 3q had named them. It sorted sides
of files that no batch had chosen by their branches as well. The rows
hold the probes that reached no side.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `check_handled` | A call that may fail whose result has no type, beside `catch none` and in an expression statement. | A result type that had its message stops the check before the call: the type of a signature ends the check of the bodies, and a local of that type gives no call. |
| `unfailing_result` | The type an `instance` of a plugin gives, on a call that cannot fail. | `lib.instance(I)` may fail, by R134 of "Answers of 2026-10-18", so its call takes `check_handled`. |
| `names_param` | A parameter whose type is a case of a variant. | A case of a generic variant has no type syntax, `Maybe<T>.Some` does not parse, and a case of a variant that is not generic names no parameter. |
| `float_as_literal` | A float below 1 in magnitude, and a last digit 0. | A float that `as` converts to an integer type has a magnitude of 1 or more, since `trunc` gives 0 for a smaller one, which fits every type. The fewest digits that read back as the float end in no 0, or fewer would. |
| `defined_integer` | `mul_high` of a type whose width the target decides. | The checker refuses it where the generic declares it, as `types_generic_length_of_mul_high_of_a_c_long` shows. |
| `xfor` | A slice or an array in the place of a parameter that a loop walks. | A slice and an array meet no `iter`, so no copy holds one there. |
| `refuse_tried` | A name whose type is not nullable and whose narrowed type is. | Narrowing takes `none` away and never adds it. |
| `own_marks`, `lower_expr_value`, `build_value_into` | A callee of a bound function type that names a function, a field of that type read as a value, and one whose symbol is no function. | The checker takes `(s.area)()` for a call of the method, and gives a field of a bound function type the symbol of its function alone. A `let` and a literal build a bound function into their slot, and a parameter of a function type refuses one. The probes found no other place that reads one. |

## The externs, the declarations and the trees of `src/antic/antl_tree.c`

Batch 3s of the third phase sorted the sides of the conditions of the
file, which no batch had chosen by its branches. Batch 3b sorted its
lines in the section of `antl_tree.c` above. The sides of `io_size`,
`io_char`, `field_owner`, `io_field`, `io_ref`, `io_sym`, `local_symbol`
and `collect` that the suite misses are the guards of the rows of that
section.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `io_ctype` | A type that is none, in the collecting pass. | Every record that the pass walks carries the type that the checker gave it, and the writer of the same tree takes none either. |
| `add_extern`, `put_extern` | A symbol of another item without a type, and an owner of a function without a symbol or a type. | A tree names functions, constants and types outside it, and the checker gives each its type. The declaration gives every struct and class its symbol and its type. |
| `generic_named` | A library that is none. | `library_symbol` passes the library that `read_extern` found, and the read stops before it for a module that it does not find. |
| `class_has` | A struct without a type, and a function of a class without a symbol. | The reader gives every item its type from the table, and every function of a type its symbol. |
| `same_function` | A function that the caller did not find, the record itself in place of the function of the library, and a function without a type. | Each caller passes a symbol it found in the library or in its type, which is no record of the file, and the reader gives both their types from the table. |
| `library_symbol` | A function of another library that the record finds as a member of its owner, with another signature, and a member of the module itself that its owner lacks. | `antic -c` reads every other module from its library file, whose functions name no owner, so the record of a function of another module is written without the lookup of a member. A function of the root has an owner and no module, so its record names the module of the generic. The type table carries every function of a class of the module, the private ones as well. |
| `antl_visit_generics` | A generic function without a body, and a function of a generic type without a symbol. | The parser gives every function at module level its body, and an `extern fn` takes no type parameters. The declaration gives every function of a body its symbol. |
| `digest_type_entry`, `antl_read_generics` | A generic class without a base, a function of a generic type without a symbol or a type, and a declaration without a symbol or a type. | Every class has a base, `Object` at the least. The reader gives every function of a type its symbol and its type, and `read_declaration` gives a declaration both or returns none. |

## The sides of `src/antic/pattern.c`

Batch 3t of the third phase sorted the sides of the conditions of the
reader of a pattern literal. No batch had chosen the file by its
branches. The rows of the section of `pattern.c` above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `pattern_compiles` | A room of no bytes for the message. | Each caller passes a buffer of its own. |
| `set_range` | A byte past 255. | Every caller passes a range of bytes. |
| `take_char` | The first byte of a character of UTF-8 at the end of the pattern. | The lexer takes a source of valid UTF-8 alone, so a literal holds every byte of each character. |
| `read_number` | Ten digits or more. | PCRE2 refuses a count of a repeat above 65535 and a reference to a group that the pattern lacks. |
| `class_char` | `\p` and `\c` at the end of the pattern. | PCRE2 refuses both before the reader runs. |
| `posix_class` | An empty name, and a name of four or six letters other than `word` and `xdigit`. | PCRE2 refuses a POSIX name it does not know. |
| `posix_end` and `parse_class` | A class that the end of the pattern stops, inside a POSIX name or `\Q` or after a `-`. | PCRE2 refuses a class without its `]`. |
| `read_flags`, `group_body`, `verb_group`, `flag_group` and `parse_group` | A group, a verb, flags, a condition or a class of set operations that the end of the pattern stops, and `(?` or `(?-` at the end. | PCRE2 refuses a group without its `)`. |
| `first_of`, `narrow_after` and `least` | A group without a child and an alternative without a branch. | `group_body` gives every group the one child that `parse_alt` returns, and `parse_alt` makes an alternative of two branches at the least. |
| `pattern_least_bytes` | A group that the pattern lacks. | `patch_group` of `sema_pattern.c` refuses it before the count. |

## The sides of `src/antic/antl_verify.c`

Batch 3u of the third phase sorted the sides of the conditions of the
verifier of library files. No batch had chosen the file by its branches.
`unit_modules` damages the trees, the type table and the IR of a library
file. The rows of the section of `antl_verify.c` above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `enter` | A record that is none of the records of its table. | `mark_in` gives the mark of every record the reader built, as its row above says. |
| `case_of` | A variant without its union, and a case past the cases the union gives. | The reader builds the cases of every variant from the fields of its union with `types_cases_from_fields`. |
| `field_found`, `walks_param` | A pointer without the type it points at. | `read_pointer` makes a pointer of an element it found alone. |
| `bindings_set`, `check_for` | A name that the tree binds to a local without a type. | `antl_verify_tree` refuses a local or a parameter without a type before the walk starts. |
| `check_expr` | The value of an enum without a type. | `stands_as` refuses it where it stands as a value, and `callee_read` where it is the callee of a call, before the walk reaches it. |
| `check_for` | A tuple of fewer fields than elements. | `read_tuple` gives a tuple one field per element. |
| `walks_param` | A `for` over a `?*T` of a type parameter, and over a value without a type. | The checker refuses a `for` over a pointer that may be `none`, and `check_for` asks for the type of the value before it. |
| `walk_case` | The name of a case at the deepest place of a tree. | The value of the `switch` stands one level below the `switch` as the case does, so `enter` refuses it first. |
| `antl_verify_extern` | The record of a function without a type. | `read_extern` gives every record an entry of the type table, and every entry holds a type. |
| `simd_shape` | No lanes. | Both callers refuse a struct of no fields first. |
| `antl_verify_simd_agg` | A field of a struct of the IR without a name. | `antl_get_cstr` gives every field the reader reads a name, the empty one where the file writes none. |

## The sides of batch 3v

Batch 3v of the third phase sorted the sides of the conditions of
`src/antic/parser_item.c`, `src/anti/syms.c` and `src/rt/trace.c`, which
no batch had chosen by their branches, and some of those of
`src/antic/lower_stmt.c`, and those of the code that the fix of T293 added
to `src/anti/fmt.c`. The rows of the sections of `trace.c` and
`lower_stmt.c` above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `item_level` of `parser_item.c` | A message inside the signature of a function after which the signature reads on. | Every reader of the type parameters, the parameters and the result that gives a message returns false, so the line returns before the test. |
| `load_deployment` of `syms.c` | A key of `module.` without a dot. | The key starts with `module.`, so `strrchr` finds its dot. |
| `syms_map_lookup` of `syms.c` | A name of no byte that is no blank. | The name starts past the blanks, at a byte before the end of the line. |
| `text_of` of `trace.c` | A text that is not empty. | Every caller passes NULL, for the empty text. |
| `keep_in` of `trace.c` | A name or a path of more than `SIZE_MAX` bytes less the head of its entry. | No symbol, path or build id of an image comes near that length. |
| `function_name` of `trace.c` | A function without the bytes of its name, and a name that `stable` keeps where it lies. | The readers of the symbol tables give the name of every function they find, and every caller passes `false` for `stable`. |
| `notice_id` of `trace.c` | A notice that is none, one shorter than its head, and one whose `build` line the room cuts. | On macOS the platform layer reads no notice, and `macho_id` passes the notice of `anti_licenses` with `SIZE_MAX`, which every binary of Anti ends with a line. |
| `load` of `trace.c` | The file of an ELF module. | Only the ELF paths pass `false` for `object`, and they run on Linux alone. |
| `macho_id` of `trace.c` | An image outside the shared cache without a symbol table or without `anti_licenses`. | Every image outside the cache that a trace of the suite meets is a binary of Anti, which has both. |
| `symbolize_macho` of `trace.c` | An image without a symbol table, an address outside every function and a line without its file. | dyld maps every image with its table, a return address lies inside the function that called, and `anti_rt_macho_object_line` gives a file with every line it finds. |
| `module_id` and `anti_rt_trace_symbolize` of `trace.c` | A notice the platform layer read, and the arms of ELF and PE. | Those are the paths of Linux and Windows. |
| `add` of `trace.c` | A size past `INT64_MAX`. | A trace holds a few hundred bytes per frame. |
| `anti_rt_trace_text` of `trace.c` | A trace whose every frame lies outside every module, so that no line of a module stands first. | A capture starts inside the program, which is a module. |
| `range_start` of `lower_stmt.c` | A range of no integer type, and a constant low bound of a type whose width follows the target. | The checker types every range with an integer type, and lowering writes `0 as c_long` as a conversion rather than as a constant. |
| `lower_assign` of `lower_stmt.c` | A compound assignment to an aggregate that is no simd struct. | The checker refuses it with `` `+=` does not apply to `Point` ``. |
| `token_ahead` and `piece_is_word` of `src/anti/fmt.c` | No token after a line break in the body of a class or an enum, a dropped piece, and a piece that is none. | The body closes with `}` after the line, the dropped pieces are the parentheses of a condition, which no line of a body opens with, and each caller passes a piece it found. |
| `opens_member` of `src/anti/fmt.c` | `pub` or `export` after a marker, and `final`, `operator` or `trace` before a token that is no `fn`. | It reads the tokens as `parser_starts_member` does, and antic refuses each after a field without its comma, with `` expected `fn` `` or `` expected `,` ``. |

## The sides of batch 3w

Batch 3w of the third phase sorted the sides of the conditions of the
files of the parser besides `src/antic/parser_item.c`. No batch had chosen
them by their branches. The rows of the sections of `parser_stmt.c`
and `parser_decl.c` above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `primary` of `parser_expr.c` | No `(` after `alloc` in the raw form. | The form is read only where the test before it found the `(`. |
| `element_name` of `parser_expr.c` | A byte below `0` in the number of a tuple element. | The lexer builds the token of the number from digits, letters and `_` alone. |
| `ends_at_line` of `parser_expr.c` | A condition at the first token of the source. | The condition of `do ... while` follows its `while`. |
| `for_statement` of `parser_stmt.c` | A walked value whose text ends before it starts, and a loop without a value and without a range that gave no message. | A value takes one token at the least. A reader of the range or of the value that returns none has given its message. |
| `statement_level` of `parser_stmt.c` | An assertion at the first token, and a condition whose text ends before it starts. | `assert` and its `(` stand before the condition, which takes one token at the least. |
| `scan_list` of `parser_type.c` | A `>>` that closed the list before the next argument. | The loop goes on only after a `,`, and it reads the `,` only where no `>>` set the half. |
| `type_level` of `parser_type.c` | A `?` before a token that is no `fn` in the test of a function type. | The first test of the function returns the optional type for such a `?`. |
| `import_names` of `parser.c` | A `.` after the path of an import that no `{` follows. | `module_path` reads every `.` that no `{` follows as part of the path. |

## The sides of batch 3x

Batch 3x of the third phase sorted the sides of the conditions of
`src/antic/lower_stmt.c` that batches 3f and 3v left, and some of those
of `src/antic/antl_read_ir.c` and `src/antic/antl_read.c`. The rows of
the sections of `lower_stmt.c` and `antl_read_ir.c` above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `drop_result`, `lower_move_argument`, `lower_clear_moved`, `lower_let`, `lower_keep_temp`, `lower_forget_received`, `lower_switch` and `lower_stmt` of `lower_stmt.c` | A block that a value, the test of an arm or the call of a base ended before the code after it. | No expression ends its path. A call of a function that returns `never` gives no value, and antic refuses it as an operand with `` `die` returns no value ``. A `defer` or an `undo` whose call never returns ends the path at an exit of its block alone, which the programs of batch 3x take at a `return`, a `break`, a `continue`, a `yield` and a `fail`. |
| `lower_convert_in_place` of `lower_error.c` | A block that the value of a `let` ended before the move of its slot to an interface. | No expression ends its path, as the row above says. The move left `lower_stmt.c` with D of "Answers of 2026-11-19", and its guard is the guard of `convert_in_place` there. |
| `lower_stmt_kind` of `lower_stmt.c` | A block that the value of a `yield` or a call with a handler ended, and a `may fail` function with a result that returns no value. | The value runs before any exit, and the call that did not fail reaches the join after its handler. A `return` without a value in a function with a result is refused. |
| `lower_let_value` and `lower_assign` of `lower_stmt.c` | A call with a handler that writes no result, in a `let` or an `=`, and `alloc` of a call that builds no class. | antic refuses the first with `` `quiet` returns no value `` and the second with `` `alloc` of one object takes a literal or a class with arguments ``. |
| `drop_result` of `lower_stmt.c` | A call of `hash` whose result the statement drops. | antic refuses it as `unused-result`. |
| `read_classes` of `antl_read_ir.c` | A file that ends inside the sub-object or the `mutable` field of a class record. | The count of each list stands before it, and `antl_get_count` refuses a count whose records the rest of the file cannot hold. Each record is of a fixed size. |

## The sides of batch 3y

Batch 3y of the third phase sorted the sides of the conditions of
`src/antic/ir_verify.c`, which no batch had chosen by their branches, and
some of those of `src/antic/header.c`, `src/anti/licensing.c` and
`src/rt/toml.c`. The rows of the sections of `header.c` and `toml.c`
above keep theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `same_type` and `check_inst` of `ir_verify.c` | An operand of an instruction, the callee of a call or the table of a call through a table that names nothing of the function or the module. | `check_inst` runs only for an instruction whose result and operands `indices_ok` found. |
| `use` and `entry_set` of `ir_verify.c` | A temporary past the count of the function, and a block without an instruction. | `check_definitions` runs only for a function that the checks before it passed, and such a block fails with `the block does not end with a terminator`. |
| `arith_facts_ok` of `ir_verify.c` | A lifetime of a temporary past the count of the function. | `indices_ok` refuses the operand first. |
| `stdint_macro` of `src/antic/header.c` | A byte below `0` in a name that is no capital. | A name holds letters, digits and `_` alone, and `_` and the small letters stand after `9`. |
| `nested_views` of `src/antic/header.c` | A union nested in a class. | antic refuses one with `` a class body declares a struct, an enum or a class, and a union stands at module level ``. |
| `ar_number` of `licensing.c` | A number past `SIZE_MAX`. | The fields of a size and of an offset hold 10 and 15 digits at most, which stay below it. |

## The sides of batch 3z

Batch 3z of the third phase sorted some of the sides of the conditions of
`src/anti/deps.c` and `src/anti/main.c`, which batch 3y named and no
batch had probed. The rows of the section of `src/anti/main.c` above keep
theirs.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `symbols_command`, `build_options`, `license_command`, `check_command`, `doc_command`, `test_command`, `bind_command` and `sdk_command` of `src/anti/main.c` | An option that takes a value and stands last, at the test `i + 1 < argc` of each parser. | `lacks_value` reads the command line first, by R155, and ends the run with `` anti: --lto needs a value `` and 2 before a parser runs. |
| `lock_string` of `src/anti/deps.c` | A control byte or a DEL in a value of the lock file. | A path holds none, since the manifest reader refuses a path with a control byte by R166, and a chained project reads its manifest the same way. A name, a version, a module path, a digest and a repository URL pass the rules of their kinds before the graph takes them. |
| `lock_write` of `src/anti/deps.c` | A name, a version or a digest of the graph that the lock file cannot hold. | The readers of the manifest, the index and the library files check each against the rules of its kind, and none of them holds a double quote or a control byte. |

## The sides of batch 3aa

Batch 3aa of the third phase read the readers of `src/anti/` for an entry
that a missing key or a damaged element ends or drops, and fixed T301 to
T303 in `src/anti/syms.c`, `src/anti/manifest.c` and `src/anti/cache.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `load_deployment` of `src/anti/syms.c` | A key of the index without a dot, at the test `field == NULL`. | The test before it holds a key that starts with `module.`, so the key holds a dot and `strrchr` finds one. |

The sides left unsorted: the key of `load_deployment` whose table has a
name of another length than the table of the last `module` line, which an
index of `[module.a]` and `[module.bb]` reaches. Two sides of
`src/anti/manifest.c`: a byte below 0x20 in the path of a dependency,
which a tab in the string reaches, since the reader of TOML takes every
byte but the quote and the line break, and a key of the manifest no
longer than `inject.` at the test of `interface_of`.

## The sides of batch 3ab

Batch 3ab of the third phase read the readers of `src/rt/`, `src/anti/`
and `src/antic/` for an entry that a missing key or an empty element ends
or drops, and fixed T304 to T308 in `src/rt/plugin.c`, `src/rt/conf.c`,
`src/anti/syms.c`, `src/anti/manifest.c` and `src/rt/toml.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `index_lists` of `src/rt/plugin.c` | A list of interfaces as long as the document, at the end of its loop, and a field name past 32 bytes. | `discover_in` reads the `path` of the entry first and passes over an entry without one, so the pairs of the document outnumber the interfaces. `interfaces.` and 19 digits fit 32 bytes. |
| `index_at` of `src/rt/plugin.c` | A key of the index past 64 bytes. | `library.`, 19 digits, a dot and the longest field, `interfaces.` with 19 digits, take 58 bytes. |
| `read_elements` of `src/rt/registry.c` | An element of a slice without the comma before it. | The scan before it counted the elements with their commas, and it fails the text without one. |
| `syms_map_lookup` of `src/anti/syms.c` | A name of a line of the map that is blank to its start. | `cursor_blank` passed the blanks before the name, so its first byte is no blank. |

The sides left unsorted: the test `key.len <= 8` of `join_elements` of
`src/rt/conf.c`, which an empty quoted key after an array of `[runtime]`
reaches and refuses as a key outside the tables. The test of `inject_of`
of `src/anti/manifest.c` for a key no longer than `inject.`, which a
short key at the top of the manifest reaches. In `src/rt/toml.c`, a
comment that runs to the end of the text without its line break and a
header cut short after its name.

## The sides of batch 3ac

Batch 3ac of the third phase read the readers of `src/anti/`, `src/rt/`
and `src/antic/` that no batch had read, and fixed T309 to T315 in
`src/anti/main.c`, `src/anti/syms.c`, `src/anti/bindclang.c`,
`src/anti/bindexpr.c` and `src/antic/sema_simd.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `license_command` of `src/anti/main.c` | An option of `anti license` that takes a value and stands last, at the test `i + 1 < argc` of the first pass and the tests `i + 1 < count` of `--from`, `--from-archive` and `--runtime`. | `lacks_value` refuses such an option before the command runs, and the first pass copies each option with its value. |
| `command_takes_value` of `src/anti/main.c` | A command without a list of options that take a value. | `license_command` calls it for `license` alone, which has its list. |
| `names_record` of `src/anti/bindclang.c` | A typedef without the type it names, a type without its kind and a record without its id. | clang writes the type of every typedef, the kind of every type and the id of every record. |
| The loop of the records of `bind_read_clang` in `src/anti/bindclang.c` | An anonymous record without `tagUsed`. | clang writes `tagUsed` for every record. |
| `sema_check_simd_binary` of `src/antic/sema_simd.c` | A lane of `char` at `==`, `!=` and the four orders. | The declaration refuses a lane of `char`, so a `simd struct` holds none. |

The sides left unsorted: in `pragma_pack` of `src/anti/bindclang.c`, a
`#pragma pack` whose arguments run to the end of the line without their
`)`, and an empty argument between two commas. clang ignores both with a
warning of `-Wignored-pragmas`, and the reader takes the arguments it
finds.

## The sides of batch 3ad

Batch 3ad of the third phase read the readers of text of the standard
library, the runtime, `anti fmt`, `anti doc`, `anti check` and the lexer,
and fixed T317 to T332 in `src/anti/bindclang.c`, `src/antic/pattern.c`,
`src/anti/fmt.c`, `src/anti/doc.c`, `src/antic/sema_doc.c`,
`src/antic/sema_expr.c`, `src/rt/text.c`, `src/rt/patterns.c` and
`src/rt/regex.c`. It sorted the sides of `src/anti/bindclang.c` that no
test reaches.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `find_clang` of `src/anti/bindclang.c` | A host that `target_host` does not know, a directory of anti that the platform layer cannot give, and one without a separator. | The suite runs anti of a tree on a known host, from a path that holds a separator. |
| `leading_int` of `src/anti/bindclang.c` | A version that starts with no digit, and one past the range of an `int`. | `major_version` hands it the text after `clang version `, which clang starts with its major, and the value of `#pragma pack` starts with a digit. |
| `where_to_get` and `say_no_runtime` of `src/anti/bindclang.c` | A host that `target_host` does not know, and a system that names neither place of the runtime of the user. | The suite runs on a known host whose home and data directory are set. |
| `names_record` of `src/anti/bindclang.c` | A typedef whose type has no list of inner nodes, a type without its kind and a record or a declaration without its id. | clang writes the inner node of every type of a typedef, the kind of every node and the id of every declaration. |
| The search of the enums in `type_name_taken`, `typedef_named` and `enum_named` of `src/anti/bindclang.c` | An entry of an enum without its declaration, and one without its tag where a name matches. | Every entry the reader adds holds its declaration, and an entry that names a typedef holds the tag of the enum. |
| `named_anonymous` of `src/anti/bindclang.c` | The spelling `(anonymous` of clang releases before 13. | The pinned clang writes `(unnamed`. |
| `declare_record` of `src/anti/bindclang.c` | A record without `tagUsed`, an anonymous member of a record without its C name. | clang writes `tagUsed` for every record, and a record that holds a member has its C name already. |
| The enum base of `declare_enum` in `src/anti/bindclang.c` | A fixed base that parses to a type that is no scalar. | C takes an integer type as the base of an enum alone. |
| `after_group` of `src/anti/bindclang.c` | A spelling of clang that ends inside a parenthesis. | clang writes every parenthesis of a type with its pair. |

The sides left unsorted, all in code of the fixes of this batch:
`flags_end` and `flags_extended` of `src/rt/regex.c` for a NUL byte among
the flags, a `-` and a `^`, `widen` there for a group `(?x:`, groups
nested past `WIDEN_GROUPS` and a comment that runs to the end of the
pattern, `skip_callout` of `src/antic/pattern.c` for a callout of a
number, of `{` and of a doubled delimiter and a string that never
closes, and `first_paragraph` of `src/anti/doc.c` for an empty first
paragraph.

## The sides of batch 3ae

Batch 3ae of the third phase read the writers whose output a reader of
the project reads again, and fixed T333 to T337, T339 and T340 in
`src/anti/fmt.c`, `src/anti/syms.c`, `src/anti/doc.c`,
`src/antic/header.c` and `src/anti/bindexpr.c`. It sorted the sides of
`src/antic/pattern.c` that no test reaches, and tested the callouts of a
number and in braces of `skip_callout` there. The fix of T340 gives
`bind_eval_const` a value it refuses, a string of bytes that are no UTF-8,
so the row of `read_macros` left the section of batch 3ad.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `pattern_compiles`, `pattern_group_count` and `pattern_group_number` of `src/antic/pattern.c` | A message of PCRE2 with no room, and a compile that gives no code. | Each caller passes a room of its own, and the checker counts the groups of a pattern that compiled once already. |
| `take_char`, `class_char`, `parse_escape`, `posix_end`, `parse_class`, `read_flags`, `verb_group` and `group_body` of `src/antic/pattern.c` | A pattern that ends inside a character, an escape, a class, a POSIX name, a group or a `\Q`, a group without its `)` and a range whose end is a class escape. | antic hands the reader a pattern that PCRE2 compiled, and PCRE2 refuses each of these, so every construct the reader opens closes before the end. |
| `posix_class` of `src/antic/pattern.c` | An empty POSIX name, a name of four bytes other than `word` and one of six other than `xdigit`. | PCRE2 refuses a POSIX name it does not know, and `word` and `xdigit` are its names of those lengths. |
| `verb_group` of `src/antic/pattern.c` | A named group of six bytes other than `atomic`. | PCRE2 names no other group of six bytes. |
| `read_number` of `src/antic/pattern.c` | A count or a reference of more than nine digits. | PCRE2 refuses a repeat past 65535 and a reference past the groups of the pattern. |
| `first_of`, `least`, `narrow_after`, `follow_within` and `parse_cat` of `src/antic/pattern.c` | A group or an alternation without children, a `\Q` sequence of none and a child outside its parent. | `group_body` adds the alternation it reads to every group, an alternation holds one branch at the least, and `add_child` keeps every child in its parent. The `switch` over every kind of node has no `default`, which the measurement counts. |
| `pattern_least_bytes` of `src/antic/pattern.c` | A group that the tree does not hold. | Each caller passes a group of the literal, which the checker counted from the same pattern. |

The sides left unsorted: `set_range` of `src/antic/pattern.c` for a range
past 255 in a text pattern and `skip_extended` there for a control byte
below a tab under `(?x)`, which a literal may hold. In the code of the
fixes of this batch they are `append_lf` of `src/anti/fmt.c` for a CR
without its LF inside a token, `emit_comment` there for an empty line of
a block comment of its own line, and `first_paragraph` of
`src/anti/doc.c` for a line of fewer than three bytes, of fewer than two
and one that opens with `-` before no space. Two sides of `src/anti/fmt.c`
no input reaches: `scan_gap` for a line comment that holds no byte, since
it holds `//`, and `anonymous_fn` for a last token that is a comment,
since it reads the tokens alone. The other sides of `src/anti/fmt.c` stay
as batches 3l and 3q sorted them, beside the code of the fixes of batch
3ad.

## The sides of batch 3af

Batch 3af of the third phase read the reflection runtime of
`src/rt/reflect.c` and `src/rt/call.c` against the records of the
descriptors that `src/antic/lower_desc.c` writes and the trampolines of
`src/antic/whole_tables.c`. It fixed T346 to T348 there and T349 in
`src/antic/lower.c`, and tested `get` and `set` over a field of each
width and an index below zero.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `anti_rt_reflect_class_name`, `anti_rt_reflect_parent`, `anti_rt_reflect_field_count`, `field_at`, `anti_rt_reflect_function_count` and `function_at` of `src/rt/reflect.c` | A descriptor that is `NULL`. | Every function of `anti.reflect` takes the descriptor as `*byte`, which never holds `none`, and C alone could pass one. |
| `field_at` and `function_at` of `src/rt/reflect.c` | A descriptor without its list of fields or of functions. | `--no-reflect` drops the lists, and antic refuses a call of `anti.reflect` in such a build. |
| `anti_rt_reflect_get` and `anti_rt_reflect_set` of `src/rt/reflect.c` | An object that is `NULL`. | `get` and `set` take the object as `*byte`, which never holds `none`. |
| `append_type_name` of `src/antic/lower_desc.c` | An enum without its underlying integer. | The checker gives every enum a base, `int` when it names none. |
| `signature_text` of `src/antic/lower_desc.c` | A function without a type, and a type that is no function type. | `class_functions` passes the symbol of a function of the table, which the checker typed. |

The sides left unsorted were those of a `c_long`, a `c_ulong` or a
`c_wchar` in the structural hash of an interface. By 7 of "Answers of
2026-11-20" the hash names them, and `append_type_name` of
`src/antic/lower_desc.c` writes them. `plugin_versions` loads a
library built against an interface whose entry takes another of them.

## The sides of batch 3ag

Batch 3ag of the third phase read the facts that the LLVM text gives
opt, in `src/antic/llvm_emit_facts.c`, `src/antic/llvm_emit_nan.c` and
`src/antic/llvm_emit_call.c`, against the rules that allow them, and
the readers of the runtime that write an enum, `src/rt/registry.c` and
`src/rt/reflect.c`. It fixed T363 in `src/antic/lower_desc.c` and
`src/rt/registry.c` and T364 in `src/rt/reflect.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `range_index` of `src/antic/llvm_emit_facts.c` | A load of a range that the list of the module does not hold. | `llvm_emit_collect_ranges` adds the range of every load of the module before the text names one. |
| `tbaa_field_of` of `src/antic/llvm_emit_facts.c` | An access that names an aggregate past those of the module, a union, a simd struct, a member past the fields, a bitfield, a field of an aggregate type and a field whose scalar type differs from the access. | `type_place` of `src/antic/lower_place.c` names a field only of a plain struct, a class or a tuple, at a field of its own aggregate whose type is the type of the access, and never a bitfield. |
| `llvm_emit_collect_tbaa` and `llvm_emit_tbaa_fact` of `src/antic/llvm_emit_facts.c` | A typed access whose scalar type has no node of the tree. | `type_place` types an access of `typed_scalar` alone, and each of those types has a node. |
| `callee_of` and `callees_of` of `src/antic/llvm_emit_facts.c` | A call whose callee is neither a function nor a temporary, and an `addr` of a function without a result. | Lowering calls a function or the temporary that holds its address, and an `addr` always gives one. |
| `is_temp` of `src/antic/llvm_emit_nan.c` | A temporary past the temporaries of the function. | Each caller passes an operand of an instruction of the function, which numbers its temporaries below its count. |

The sides left unsorted, all in `src/antic/llvm_emit_nan.c`: `use_keeps`
for a derivation without a result or with two definitions, `find_slots`
for a result past the temporaries and a slot of two definitions,
`follow_addresses` for a symbolic offset that the layout does not fold,
`find_known` for a float load whose address is no temporary and a copy
of a float of unknown bits or of a constant, `llvm_emit_nan_module` for
an exported function, one named by `by_name` and a function past those
of the module, `llvm_emit_nan_value`
and `llvm_emit_nan_store` for a temporary past the count, and
`llvm_emit_nan_context` for a call through a context that names a
callee of its own and one without arguments.
The fixes of T363 and T364 add the sides of `anti_rt_enum_holds` of
`src/rt/object.c`, which the tests of the two reach.

## The sides of batch 3ah

Batch 3ah of the third phase read the checks of the closures in
`src/antic/sema_closure.c`, the lowering of a closure and a snapshot in
`src/antic/lower_function.c` and the dev checks of
`src/antic/lower_check.c`. It fixed T369 in `src/antic/sema_closure.c`,
`src/antic/sema_stmt.c`, `src/antic/sema_expr.c` and `src/antic/types.c`,
and T372 in `src/antic/lower_check.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sema_capture`, `sema_note_write` and `sema_note_call` of `src/antic/sema_closure.c` | A chain of enclosing functions that ends before the frame of the captured variable. | A captured variable is a local or a parameter of a function around the one that names it, so the chain meets its frame first. |
| `sema_note_call` of `src/antic/sema_closure.c` | A captured callee without a type, and one whose type is no function. | The call refuses a callee that is no function before the note, and the checker types every local and parameter it declares. |
| `place_root` and `sema_check_closure_lifetime` of `src/antic/sema_closure.c` | A base of a place without a type, and a local without a type or without a function of the form of two words among its parts that takes a closure. | The assignment types the place first, and `sema_require` refuses a closure at a local that holds no such function before the lifetime is checked. |
| `anonymous_type` of `src/antic/sema_closure.c` | A parameter marked `keep`, `concurrent` or `own` that names no type. | The parser takes each mark before a written type alone. |
| `snapshot_copies` and `check_snapshot` of `src/antic/sema_closure.c` | A kind below `bool`, a capture without a type or of the error type, a steady capture that cannot be `none`, and one whose narrowing proves no type. | A captured variable holds a value of the type the checker gave it. A steady name is the binding of an `if let`, which may be `none`, and a snapshot that names it stands where the narrowing holds. |
| `lower_bitfield_check` and `lower_bitfield_saturate` of `src/antic/lower_check.c` | A field of no bits or as wide as its type, and a constant that does not fit. | Lowering calls them for a bitfield narrower than its type alone, and the checker refuses a constant that does not fit. |
| `lower_narrow_check` of `src/antic/lower_check.c` | A conversion to the type of the value. | `as` to the type of the value does nothing, by R059, so lowering asks for no check. |

The sides left unsorted: `sema_refuse_worker_closure` of
`src/antic/sema_closure.c` for an argument that is no function, which a
worker given a value of another type reaches after its refusal, and
`anonymous_type` there for a target that returns `never` where the
anonymous function writes its result. `types_plain_parts` of
`src/antic/types.c` for an `own fn` part and an array of a symbolic
length, `check_for` of `src/antic/sema_stmt.c` for a `for` of more than
two names over a collection, and `enum_value` of `src/antic/lower_check.c`
for a signed base of no width.

## The sides of batch 3ai

Batch 3ai of the third phase read the classification of the C convention
in `src/antic/abi.c` against clang, the members of a layout in
`src/antic/layout.c` and the lowering of locks, channels and `select` in
`src/antic/lower_sync.c`. It fixed T373 in `src/antic/abi.c` and
`src/antic/layout.c`, and T374 in `src/antic/abi.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sysv_words` of `src/antic/abi.c` | A vector member whose lower eightbyte is neither the one classed nor the one before it. | A vector of 16 bytes aligns to 16, so in an aggregate of at most 16 bytes it starts at offset 0. |
| `load_handle` and `received_element` of `src/antic/lower_sync.c` | A channel that an operation or an arm of `select` reaches through a pointer. | `channel_of` of `src/antic/sema_thread.c` and `check_select` of `src/antic/sema_stmt.c` take a channel alone, and `types_is_chan` refuses a pointer to one. |
| `lower_select` of `src/antic/lower_sync.c` | An arm that the comparisons reach after the block ended. | Each arm before the last branches to the next block, so the block of every arm exists when the loop reaches it. |

The sides left unsorted, which a test can reach: `sysv_words` of
`src/antic/abi.c` for a union of a vector and floats in its upper
eightbyte, which stays two words, and `object_lock_of` of
`src/antic/lower_sync.c` for `sync a, b` of two synchronized objects
that are locals and not pointers.

## The sides of batch 3aj

Batch 3aj of the third phase read the doc warnings of `anti check` in
`src/antic/sema_doc.c` against "Doc markup" in `docs/tooling-addendum.md`
and the section of the lock orders of a library file in
`src/antic/antl_locks.c`. It fixed T378, T379, T380 and T381 in
`src/antic/sema_doc.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `doc_name_known` of `src/antic/sema_doc.c` | A name that the documented item alone answers for. | The pass reads the doc comments of the items of the module and of their members, and the loop over the items of the module reads each of them before. |
| `doc_check_span` of `src/antic/sema_doc.c` | A span whose last two bytes are a `(` and another byte. | `doc_is_name` refuses such a span before, since a `(` is no byte of a name. |
| `antl_read_locks` of `src/antic/antl_locks.c` | A kind of a wait below zero. | `antl_get_int` refuses a number above the largest `int`, so the kind it gives is never below zero. |

The sides left unsorted, which a test can reach. In `doc_is_name` of
`src/antic/sema_doc.c` a span whose last two bytes are a `(` and a
letter. In `doc_emphasis` there a run of the delimiter before a space,
`a * b`. In `doc_check_text` and `doc_word_ends` there a line of `#`
alone, a word of `#` or of digits before a tab, and a second item of a
numbered list in one comment. In `antl_read_locks` of
`src/antic/antl_locks.c` the end of the file inside the first of two
entries of each loop and before the kind of a wait. Each needs names long
enough that the count of the entries passes the test of the bytes left.

## The sides of batch 3ak

Batch 3ak of the third phase read the clauses `allow` and `unchecked` in
`src/antic/warnings.c` against "Errors, warnings and checks" in
`docs/decisions.md`, the lowering of `parallel`, `dispatch` and `join` in
`src/antic/lower_worker.c`, the checks of `--memory-checks` in
`src/antic/memcheck.c`, the text form of the IR in `src/antic/ir_print.c`
and the rows of the runtime in `src/antic/rt_abi.c`. It fixed T383 in
`src/antic/warnings.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `drop_silenced` of `src/antic/warnings.c` | A promoted diagnostic, and an error with a name that is no safety check. | `settle_diagnostics` of `src/antic/driver.c` promotes the warnings after the clauses apply, and `sema_check_at` of `src/antic/sema.c` gives an error the name of a safety check alone. |
| `warnings_apply` of `src/antic/warnings.c` | No module. | Both callers in `src/antic/driver.c` pass the tree the parser wrote. |
| `pack_context` of `src/antic/lower_worker.c` | A moved argument that is no name, and the error a handler binds moved into a worker. | `sema_note_move` and `sema_move_caught` of `src/antic/sema_value.c` mark a name alone, and `worker_type` of `src/antic/sema_thread.c` refuses the `*Error` of a handler, a pointer to an object that is not thread-safe. |
| `check` of `src/antic/memcheck.c` | An access of no bytes, and an address that is no temporary. | An array has a length of 1 or more and a struct has a field, and lowering takes every address into a temporary, a global through `ir_addr`. |
| `rt_without_effects` of `src/antic/rt_abi.c` | A row of the class none or reads without willreturn, one that never returns and one without a result. | Every such row of `RT_FUNCTIONS` in `src/antic/rt_abi.h` takes `IR_GUARANTEES`, returns and gives a value. |
| `ir_sym_print` and `operand` of `src/antic/ir_print.c` | A kind outside `enum ir_sym_kind` or `enum ir_operand_kind`. | Each switch names every kind of its enum, and the IR holds no other. |
| `param_type` of `src/antic/ir_print.c` | A size of `deref` past the table of symbolic values. | Lowering writes a size it made with `ir_sym_size_of`, an index of the table. |
| `signature` of `src/antic/ir_print.c` | A variadic function without a parameter. | The parser asks for a parameter before `...`, and no function of the runtime is variadic. |
| `instruction` of `src/antic/ir_print.c` | A select of no mask, a flag past `negative` and a member past the fields of its aggregate. | `ir_vselect` of `src/antic/ir.c` writes one mask, lowering reads the four flags of `anti.lang.Flags` alone, and a typed load or store names a member of the aggregate it reads. |

The sides left unsorted, which a test can reach or no probe of the batch
reached. In `src/antic/ir_print.c` a symbolic operation of one operand,
which `shift_wrap_sym` of `src/antic/lower_expr.c` writes, an aggregate
constant of no items and an operand of kind `IR_NONE` in a listing. In
`antic_outline` of `src/antic/antic.c` a function after `main` and a
function of a `fixtures` block after `setup` or `teardown`, and a source
that does not parse in `anti check`, whose front end class ends the run
before the class of the doc blocks reads the outline.

## The sides of batch 3al

Batch 3al of the third phase read the LLVM text of a function and its
instructions in `src/antic/llvm_emit_function.c` against the two entries
on frames of `2^31` bytes under "Compiler behaviour" in
`docs/decisions.md` and the DESIGN comments of the file, and the file
helpers of `anti` in `src/anti/files.c` against the DESIGN comments of
the file and the rules of its callers.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `scalar_type`, `llvm_emit_operand` and `llvm_emit_effects` of `src/antic/llvm_emit_function.c` | A value outside `enum ir_type`, `enum ir_operand_kind` or `enum ir_effects`, and an operand of kind `IR_NONE`, `IR_BLOCK` or `IR_SYM` where a value stands. | Each switch names every kind of its enum, the IR holds no other, and lowering writes a block or a symbolic value where an instruction takes one alone. |
| `llvm_emit_frame_add` of `src/antic/llvm_emit_function.c` | An alignment of 0 and one above the limit of the frame. | Layout gives every type an alignment of 1 or more, and `align(N)` takes at most 4096. |
| `conversion` of `src/antic/llvm_emit_function.c` | A type with no LLVM value type, and `as f16` of a constant. | A conversion takes and gives a scalar, and `fold_conversion` of `src/antic/optimize.c` folds every conversion of a constant, in both modes. |
| `load_store`, `ptradd`, `memcopy`, `bits`, `vsplat`, `vbinary`, `flag_operation`, `overflow_operation`, `wide_product`, `saturating`, `vunary`, `vshuffle`, `vselect`, `vreduce` and `instruction` of `src/antic/llvm_emit_function.c` | An operand that failed, and a type, a unit or a lane with no LLVM value type. | `llvm_emit_operand` fails for an operand kind that no instruction holds, and a scalar, a bitfield unit, a lane and every temporary have a value type. |
| `flag_read` and `branch_overflow` of `src/antic/llvm_emit_function.c` | A flag apart from its flag operation or past `negative`, and a `branchov` apart from its overflow operation. | Lowering writes each read of a flag and each `branchov` right after its operation, and reads the four flags of `anti.lang.Flags` alone. |
| `type_suffix` of `src/antic/llvm_emit_function.c` | An intrinsic of a fold of `f64` lanes. | `vreduce` takes the halving fold for float lanes, and only the unit test `vector_rest` hands it an intrinsic of `f32` lanes. |
| `vunary` of `src/antic/llvm_emit_function.c` | A negation of integer lanes through `fneg`. | `lower_simd_unary` of `src/antic/lower_simd.c` writes `IR_FNEG` for float lanes alone and `IR_NEG` for integer lanes. |
| `llvm_emit_definition` of `src/antic/llvm_emit_function.c` | A signature that fails, a frame that the temporaries take past the limit and a temporary with no LLVM value type. | `llvm_emit_signature` of `src/antic/llvm_emit_call.c` fails through the frame alone, and its slots, the first of the frame, hold coerced aggregates of 16 bytes at most. A temporary holds a scalar, and the slots of the locals follow the temporaries. |
| `files_remove_tree` of `src/anti/files.c` | A directory removed after the removal of an entry of it failed. | A directory that still holds the entry cannot be removed. |
| `files_copy_which` of `src/anti/files.c` | A close of the copy that fails after a write that failed. | A run of a test makes one call of the system fail, so the write and the close never both fail. |

The sides left unsorted, which a test can reach or no probe of the batch
reached. In `files_make_dirs` and `files_make_dirs_or_say` of
`src/anti/files.c` a backslash as a separator and a colon before one,
which no path that the suite hands `anti` holds. In `files_array` of
the same file a count of 0, which no caller in the suite passes.

## The sides of batch 3am

Batch 3am of the third phase read the safety check `escaping-local` in
`src/antic/sema_escape.c` against E1 of "Compile-time checks of
2026-10-20" in `docs/decisions.md`, its correction of 2026-10-25 and its
`[provisional]` entries, and the passes of the optimizer in
`src/antic/optimize.c` against the DESIGN comments of the file. It fixed
T384 and T386 in `src/antic/sema_escape.c`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `find`, `outer_param`, `reached_param`, `anchor_of`, `captured_root`, `check_concurrent`, `walk_let`, `walk_arms` and `walk_for` of `src/antic/sema_escape.c` | A name without a symbol or a type, a local the walk did not declare where a closure captures it, and a switch, a loop or a destructuring without arms, a body or names. | The pass runs on a body the checker checked without an error, so every name has its symbol and its type, every capture names a local of the walk, and the parser gives every arm, loop and destructuring its parts. |
| `place_root` and `anchor_of` of `src/antic/sema_escape.c` | A place reached through a unary operator other than `*`, and a place of another kind of expression. | The checker takes `&` of a name, a field, an element or `*p` alone, and the target of a `=` is one of those as well. |
| `root_of` of `src/antic/sema_escape.c` | `.ptr` of an array, and an element of an array whose base has no type. | An array has no field `ptr`, so the checker refuses `a.ptr`, and every base has its type. |
| `place_name` and `refuse` of `src/antic/sema_escape.c` | A place without a name, and a place outside `enum escape_place`. | Every refused store names a local, a field, an element or a global, and each caller passes one of the places of the enum. |
| `refuse_store` and `check_store` of `src/antic/sema_escape.c` | A store whose anchor is no global, field, element, parameter or local, and a name as the target whose anchor is no local of the walk. | The callers refuse a store that outlives the local alone, which names one of those, and a target name is a local or a parameter the walk declared, since a global is anchored before. |
| `check_collection` of `src/antic/sema_escape.c` | A function of a collection called without a receiver, a receiver that is no pointer, and more arguments than the function takes. | Every call of a function with `self` passes the receiver first as a pointer, and the checker matches the arguments with the parameters. |
| `check_dispatch` of `src/antic/sema_escape.c` | A `dispatch` without its call. | The parser gives every `dispatch` the call of its worker. |
| `leave_expr` and `leave_stmt` of `src/antic/sema_escape.c` | A statement after the one that leaves the block, read while the scan has already found the way out. | The walk stops at the first way out, since each visitor returns false once the scan has found one. |
| `walk_function`, `walk_handler`, `walk_yield` and `sema_check_escapes` of `src/antic/sema_escape.c` | A function or a handler without a body, a `yield` outside a handler, and a check in a quiet context. | The callers pass a function with a body, the checker refuses a `yield` outside a handler before the pass runs, and no caller runs the pass while the checker is quiet. |
| `propagate_copies` of `src/antic/optimize.c` | A copy of a temporary into itself, and a copy whose one definition in the block follows it. | Lowering writes no such copy, and by property 1 of the DESIGN of the file a definition stands before every use. |
| `merge_blocks` of `src/antic/optimize.c` | A jump to the entry block. | Lowering starts a loop in a block of its own, so no jump goes back to the entry. |
| `address_of` and `forward_stores` of `src/antic/optimize.c` | A `ptradd` whose base or offset is written again between it and the load or store that reads it, and a load into its own offset. | Lowering writes each address right before the load or the store that reads it. |
| `element_of` of `src/antic/optimize.c` | An offset of a slot that is the symbol `n * size_of T`. | Lowering writes the offset of element `n` of a local as a `mul` instruction, which `simplify` turns into `size_of T` or a shift, never into a symbol. The products of `src/antic/lower_hook.c` and `src/antic/whole_tables.c` reach globals. |

The sides left unsorted, which a test can reach or no probe of the batch
reached. In `check_collection` of `src/antic/sema_escape.c` a receiver
`*p` of a collection, which the pass anchors nowhere. In `leave_stmt` of
the same file a `fallthrough` outside a `switch` the scan entered. It
ends the arm that holds the `dispatch`, so no `join` follows it in that
block, and the job is refused with or without it. In `fold_inst` of
`src/antic/optimize.c` a second operand of type `c_long` or `c_wchar`
beside a first of another type. In `scalar_bytes` of the same file a
field of a type of no fixed width, and in `offsets_apart` the pairs of
offsets that are no two fields of one struct, no start and field of the
struct of the slot and no two elements of one array.

## The sides of batch 3an

Batch 3an of the third phase read the safety check `index-out-of-range`
in `src/antic/sema_index.c` against C2 of "Compile-time checks of
2026-10-20" in `docs/decisions.md` and its `[provisional]` entries, and
the safety check `lock-order` and the warning `blocking-under-lock` in
`src/antic/sema_lock.c` against C4 and C5 there and their `[provisional]`
entries. It fixed T388 and T389 in `src/antic/sema_index.c`. The rows of
the two files under their own headings above stand as they were.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `collect_stmt` and `collect_expr` of `src/antic/sema_index.c` | A name of a `let`, of a `for` or of an anonymous function without its symbol, a `let` of one name without its symbol, a `for` over a range that destructures, an anonymous function without its item, and a `let` of a local that a statement before it changed. | The checker declares every name of a body it checks without an error, the parser gives every anonymous function its item, a `for` over a range names its counter alone, and the walk reads the statements in the order of the text, where no statement names a local before its `let`. |
| `is_length_base`, `length_of_symbol` and `range_of` of `src/antic/sema_index.c` | A symbol without a type, a name without a symbol, a `.len` whose base is missing or has no type, and an expression that is missing. | The checker gives every name its symbol and every expression its type, and the parser gives every part of a slice both of its bounds. |
| `name_range` of `src/antic/sema_index.c` | A constant whose value folds to no integer, and a local that is neither a parameter nor given a value by its `let` and never changes. | Every constant the checker takes in an integer expression folds to an integer, `size_of` among them. A local whose `let` gives it no value takes one by an assignment, which marks it changed. |
| `name_range` and `length_of_symbol` of `src/antic/sema_index.c` | A local whose value is read while its value is being read. | A `let` cannot name the local it declares, and the bounds of a `for` cannot name its counter, so no value reaches itself. |
| `length_of_symbol` of `src/antic/sema_index.c` | A local that is no slice or `str`, a slice or a `str` that is neither a parameter nor given a value by its `let`, and a literal `str` given to a local of another type. | Its callers ask for a name of a slice or a `str`, and a slice that a `let` copies from a name is a slice, since an array gives one through a part alone. A local without a value changes by an assignment, and the checker refuses a literal `str` where another type stands. |
| `whole_expr`, `sure_stmt`, `sure_block`, `check_index` and `visit_index` of `src/antic/sema_index.c` | A call without a callee or whose callee has no type of a function, an `if` without a branch, a block that is missing, an index without its base or a type of it, and an anonymous function without its item. | The checker gives every call a callee of a function type and every base its type, and the parser gives every `if` its first branch, every statement that holds a block that block, and every anonymous function its item. |
| `refuse` of `src/antic/sema_index.c` | An index of a slice that is no name. | `length_of` knows the length of a slice through its name alone, so no other slice is refused. |
| `same_text` of `src/antic/sema_lock.c` | Two names of length 0. | Every lock names its field, its type and its module, each of one byte or more. |
| `declaring` and `field_lock` of `src/antic/sema_lock.c` | A type that is missing or has no name, and a walk of the chain past a level that does not declare the field. | Every caller passes the type of a struct or a class, which has a name, and the first level holds the field, as the row of `declaring` above says. |
| `mutex_lock` of `src/antic/sema_lock.c` | A place of no type or of a type that is no Mutex, and a static field. | The checker refuses a `sync` on anything but a Mutex or a synchronized object, and the second goes to `object_lock`. The static field stands in the row of `mutex_lock` above. |
| `synchronized_class` of `src/antic/sema_lock.c` | A symbol of no function, a function without a type or a first parameter, a `self` that is no pointer and a `self` of no class. | A function with `self` is a function of a class, whose `self` is a pointer to that class. |
| `library_locks`, `library_wait` and `take_call` of `src/antic/sema_lock.c` | A function of no library, a call without a callee and a name without a symbol. | `take_call` asks for a function of a library alone, and the checker gives every call a callee and every name its symbol. |
| `walk_args` and `walk_expr` of `src/antic/sema_lock.c` | A `dispatch` or a `parallel` without its call, and an anonymous function without its item or its body. | The parser gives each of them its parts. |
| `generic_item`, `collect` and `read_function` of `src/antic/sema_lock.c` | A function of a generic class that names no class around it, a function without a symbol or with the type of an error, and a hidden lock that has no index. | A function of a class names that class as the class around it, the pass runs after a checker that found no error, and a synchronized class has a name. |
| `warned_before` of `src/antic/sema_lock.c` | A site at the place of an earlier site that reaches no wait. | Two sites stand at one place in two copies of one generic alone, and each copy calls the same functions by name. A call whose function differs between copies goes through a table or a constraint, which the pass does not follow. |
| `find_path`, `module_spelling` and `keep_facts` of `src/antic/sema_lock.c` | A path from a lock to itself, a lock of a type without a module, and a function without a symbol. | `find_cycles` skips an edge from a lock to itself, every type names its module, and the pass reads functions with a symbol, apart from anonymous ones, which `keep_facts` skips first. |

## The sides of batch 3ao

Batch 3ao of the third phase read the statements and the function bodies
of the checker in `src/antic/sema_stmt.c`. It read the atomics, the joins
and the workers in `src/antic/sema_thread.c` as well, each against the
specifications and the DESIGN comments of its file. It fixed T390 in
`src/antic/sema_thread.c`. The rows of the two files under their own
headings above stand as they were.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `visit_worker_expr`, `sema_calls_never`, `refuse_borrowed_snapshot`, `walked_copy` and `check_assign` of `src/antic/sema_stmt.c` | A field without a base, a call without a callee, a local without a type, a base of an index or a field without a type, and a name of the target without a symbol. | The parser gives every field its base and every call its callee, and the checker gives every expression its type and every name its symbol before these tests, or refuses the statement. |
| `switch_ends` of `src/antic/sema_stmt.c` | A `switch` without `else` on a value of no type. | The checker gives the value of every `switch` a type, the type of an error among them. |
| `stmt_returns` of `src/antic/sema_stmt.c` | A `switch` that is not an `if let` at the test of an `if let`. | The test of a `switch` that is not an `if let` returns first. |
| `check_assign` of `src/antic/sema_stmt.c` | A target that is no place, an index of a `str`, a field whose base is of no struct, and a name that is neither a local nor a parameter. | `check_index_target` and `sema_check_storage` refuse each of the first two with `cannot assign to this expression` first. Every field that `=` takes is one of a struct, a union, a class, a simd struct or a tuple, and the parts of a slice are no places. The one other name is a static atomic field, whose `=` is refused with the message of an atomic. |
| `same_arm_value` of `src/antic/sema_stmt.c` | Two arms of which one alone is a text. | The checker takes the value of every arm as the type of the `switch`, so the arms of a `switch` on a `str` are texts alone. |
| `name_function` of `src/antic/sema_stmt.c` | A function of a module or a class body without a name. | The parser gives each of them its name. |
| `check_flags_assign` and `check_destructuring_let` of `src/antic/sema_stmt.c` | A local or a parameter without a type, and a name of a `let` without its symbol. | The checker gives every local and every parameter its type when it declares it, and declares every name of a `let` it checks. |
| `spell_mutex` of `src/antic/sema_stmt.c` | A unary operator other than `&` and `*` before the Mutex of a `sync`. | The checker refuses a `sync` on anything but a Mutex or a synchronized object, which no other operator gives. |
| `check_switch` of `src/antic/sema_stmt.c` | An arm of a `switch` on a value whose type is an error, at the test of its value. | Such an arm takes the branch before, which checks its body alone, by the DESIGN of T024 there. |
| `mark_let_symbol` and `sema_value_view` of `src/antic/sema_stmt.c` | A value of a `let` or a conversion without a type. | The checker gives every expression it checked its type. |
| `check_let` of `src/antic/sema_stmt.c` | A value of the form `?.` before the check, and a written type of a function in the form of two words. | The parser writes `?.` as a field or a call, and the check of the value writes the node of `?.`, where `sema_expr.c` hands the guard on. A type the program writes is the plain form, and `check_let` gives a local the form of two words itself. |
| `check_for` of `src/antic/sema_stmt.c` | A `for` of three names or more, a `for i, x` that is not of two names, and a loop variable without its symbol. | The parser reads one name or two before `in`, and the checker declares the name of each loop in a scope opened for it. |
| `warn_unused_result` of `src/antic/sema_stmt.c` | A statement of an operator other than `mul_high`, and a function of the runtime without a type of a function. | The parser takes a call, an assignment or `mul_high` alone as a statement, and every `extern fn` has the type of a function. |
| `check_stmt_kind` of `src/antic/sema_stmt.c` | A statement of a kind the switch does not name, and an `if` of one branch whose names a branch before proved to outlive it. | The switch names every kind of statement, and an `if` of one branch has no branch before it. |
| `sema_check_function` of `src/antic/sema_stmt.c` | `sema_declare` giving NULL for `self`, a function without a body, a function of the type of an error, and a function that may fail without a written `may fail`. | `self` is the first name of a scope opened for it, and the parser gives each function of a module and of a class body its body. The checker checks no body of a function whose type is an error. The parser sets `may fail` where it reads the words, and an anonymous function that takes it from its target goes through `sema_closure.c`. |
| `sema_refuse_write` of `src/antic/sema_stmt.c` | A field of the copy of a loop and a field `count` as the operand of `take`. | The check of `take` refuses an operand that is no `own` field of a place first, with the message of the copy, and `count` is no `own` field. |
| `sema_refuse_singleton_write` of `src/antic/sema_stmt.c` | A write of an atomic field of a singleton, and a write with no function around it. | The checker refuses `=` and `&` of an atomic field with the message of an atomic first, and every write stands in a function. |
| `sema_refuse_count_write` of `src/antic/sema_stmt.c` | A promoted field of no struct or of no field, a `count` of no struct, a `count` without its home, and a walk past the class that declares `count`. | The checker promotes a field of a base or a `use` field of a struct alone, gives every field the type that declares it, and the walk from the receiver reaches the home of `count` or the module first. |
| `sema_refuse_atomic_field` of `src/antic/sema_thread.c` | An atomic field of an `own fn` or of a function bound to an object. | `atomic` and `own` do not stand together before a field, and no type a program writes is a bound function. |
| `unchecked_swap` of `src/antic/sema_thread.c` | `compare_swap` on a place that is no field or has no type, a field of a form other than plain, and a field without its home. | `sema_atomic_call` hands on a field alone, checked before, a `use` field and a base are no fields of one word, and every field has its home. |
| `sema_atomic_call`, `channel_of` and `worker_type` of `src/antic/sema_thread.c` | A place of no type, a channel of no type and a parameter of a worker whose type is an error. | `sema_check_storage` gives every place a type, the checker gives an operand of `send` and `recv` a type, the type of an error among them, and it checks no call of a worker whose type is an error. |
| `join_keys_of` of `src/antic/sema_thread.c` | An element of a tuple of indices, of a base that is no name or a name without a symbol, at an index past `INT64_MAX`, a slice without a bound, and bounds below zero or above 64 at the low end alone. | An index of more than one value is a tuple and no integer, the parser gives every slice both bounds, and the checker refuses an index past the range of `int`. A low bound above 64 has a high bound above it, which the test before turns away. |
| `sema_check_sync_op` of `src/antic/sema_thread.c` | An operation other than `chan`, `send` and `recv`. | The parser writes those three alone. The checker writes `close`, `Mutex.new()`, `destroy` and the `delete` of a channel from a call it typed, and checks the node once. |
| `worker_callee` and `worker_args` of `src/antic/sema_thread.c` | A worker without a symbol, and a parameter of a function bound to an object. | The checker gives the name of every worker its symbol, and no parameter type a program writes is a bound function. |

## The sides of batch 3ap

Batch 3ap of the third phase read the types of a compilation in
`src/antic/types.c` and the helpers of the checker in `src/antic/sema.c`,
each against the specifications and the DESIGN comments of its file. It
fixed T391 in `src/antic/sema_stmt.c` and T392 in `src/antic/sema.c`. The
rows of the two files under their own headings above stand as they were.
The fix of T391 adds one side that no input reaches, in the last row.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `depth_above` of `src/antic/types.c` | A part that stands `UINT32_MAX` levels deep. | The checker refuses a type that nests deeper than `TYPES_NEST_MAX` levels, far below it. |
| `lent_form`, `types_holds_lent`, `types_is_lent`, `types_is_nullable`, `types_with_none` and `types_is_maybe_match` of `src/antic/types.c` | A type that is NULL. | Every caller passes the type of a value, a field or a parameter, which the checker gave it. |
| `types_plain_parts` and `print_fn` of `src/antic/types.c` | A function bound to an object. | No type a program writes is a bound function, and a bound function stands as the callee of a call alone. |
| `types_symbolic_print` of `src/antic/types.c` | A unary minus before an integer that is unsigned or below 2 to the 63. | The checker folds a unary minus of an integer, and a symbolic value stays one where `size_of` stands below it. |
| `types_symbolic_print` and `types_primary_member` and `types_interface_member` of `src/antic/types.c`, `resolve_type_inner` of `src/antic/sema.c` | A value of an enum that the switch does not name. | Each switch names every value of its enum. |
| `types_job` of `src/antic/types.c` | A struct of the same result whose name is not that of `Job`. | A struct carries a result when it is a `Job` alone. |
| `same_literal` of `src/antic/types.c` | A match of a literal met before the plain match, when the plain one is asked for. | The checker declares the plain `Match` and `ByteMatch` of `anti.lang` before it checks any literal. |
| `types_is_chan`, `types_is_flags`, `types_is_job`, `types_is_simd` and `types_is_mask` of `src/antic/types.c` | A type of `anti.lang` of that name without its element, with a result, without one, or a simd struct without lanes. | The compiler makes each of those types itself, with its part, and the parser refuses a simd struct without fields. |
| `types_cases_from_fields` and `types_case_index` of `src/antic/types.c` | A variant of a library file with no field or more than two, and a variant without its tag. | The writer of a library file writes every variant as its tag and, when a case holds fields, its union, so only a damaged file holds another. Every variant the checker declares has its tag. |
| `types_signature` of `src/antic/types.c` | A signature with `self` of a function without parameters. | A function with `self` has it as its first parameter. |
| `types_copy_name` of `src/antic/types.c` | A copy nested in a generic whose name holds no dot after the name of the class around it. | The checker names a nested type `Outer.Inner`. |
| `pointer_free` of `src/antic/types.c` | A field without a type. | The checker gives every field its type, the type of an error among them. |
| `sema_is_error`, `sema_has_body`, `sema_within`, `nested_find` and `sema_std_item` of `src/antic/sema.c` | A type that is NULL, a function without a symbol, an item without an owner, a nested type without a symbol, and an item of the standard library without a type. | The checker gives every value a type and every item it declared a symbol before these tests. |
| `package_shared` of `src/antic/sema.c` | A compilation without a package, and a library file without the name of its package. | `sema_check` takes the module name as the package when no `--package` names one, and every library file carries the name of its package. |
| `sema_direct_item` of `src/antic/sema.c` | A module of a direct import that the scope holds with a home. | A module in the scope has no home. The one caller that may meet a module tests for it first. |
| `sema_warn_catch_shadow` of `src/antic/sema.c` | A `catch` without a name. | The checker calls it for a handler that binds a name alone. |
| `sema_enter_scope` of `src/antic/sema.c` | A scope opened with none around it. | The scope of the module stands around every scope a function opens. |
| `resolve_named` of `src/antic/sema.c` | A struct whose type is an error, named with type arguments. | The checker gives the symbol of a type a type of its kind, and names `NULL` alone while the declaration resolves. |
| `sema_param_form` of `src/antic/sema.c` | A function bound to an object. | No parameter type a program writes is a bound function. |
| `sema_std_function`, `sema_error_class`, `sema_location_type`, `sema_lang_error_or_null` and `allocator_pointer` of `src/antic/sema.c` | An item of the standard library of another kind than the one the compiler names. | The standard library is part of the package, and its items have the kinds the compiler names. |
| `check_member_functions` of `src/antic/sema.c` | A function of a class body without a symbol. | The checker declares every function of a body it checks. |
| `named_by_refused_base` and `report_program` of `src/antic/sema.c` | A class without a symbol or without a type, a worker without a body and an abstract class without a type. | The checker declares every class and gives it its type, and the parser gives every worker its body. |
| `check_switch` of `src/antic/sema_stmt.c` | An `if let` on a value of the error type whose arm without a binding is no name. | The parser writes the arm of every `if let` as a name. |

## The sides of batch 3aq

Batch 3aq of the third phase read the owning values of lowering in
`src/antic/lower_owning.c` and the declarations of the checker in
`src/antic/sema_declare.c`, each against the specifications and the
DESIGN comments of its file. It fixed T393 and T395 in
`src/antic/sema_declare.c` and T394 in `src/antic/sema_const.c`. The rows
of the two files under their own headings above stand as they were. The
fixes of T394 and T395 add the sides of the last four rows.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `lower_destroy_owned`, `lower_clear_owned`, `lower_init_owned`, `lower_copy_owned` and `lower_copies_parts` of `src/antic/lower_owning.c` | A field of a struct or a tuple that is a `use` field, a base, a table or an interface. | A struct body holds plain fields alone, by "Structs" in `docs/anti-object-model.md`, and a tuple holds its elements. |
| `lower_init_owned` of `src/antic/lower_owning.c` | A variant without a case. | The checker refuses a variant without a case, with `variant ... has no case`. |
| `lower_copy_owned` of `src/antic/lower_owning.c` | A function value without `own` in a part that a copy reaches. | `lower_copies_parts` enters a function value when it is an `own fn` alone. |
| `lower_own_field` of `src/antic/lower_owning.c` | An `=` to a field that names an item, and a field that the chain of its class does not hold. | A constant takes no `=`, and a static field is atomic and takes no `=` either. The checker resolved every field of a class through its chain. |
| `lower_chain_unbuilds` and `unbuild_base` of `src/antic/lower_owning.c` | A level of the chain that is no class, and a chain without its root. | The base of a class is a class, and every chain ends at `anti.lang.Object`, whose base is none, where both loops stop. |
| `lower_unbuild` of `src/antic/lower_owning.c` | A class without a base. | Every class has a base, `anti.lang.Object` at the least, and the `construct` of `Object` cannot fail. |
| `prepare` of `src/antic/lower_owning.c` | A level of a class without fields after a constant of the class. | Every level of a class holds its base or its table as field 0. |
| `imports_module` of `src/antic/sema_declare.c` | A library file whose import names no library file of the compilation. | The driver refuses such a file when it loads it, with `needs module`. |
| `bitfield_width`, `alignment` and `check_enum_value` of `src/antic/sema_declare.c` | A width, an N of `align(N)` or the value of an enum that the check takes and the evaluation refuses. | The check of an integer expression folds every operator and refuses what does not fold, and the walk refuses a call and, since T394, a function. |
| `declare_enum_values` of `src/antic/sema_declare.c` | A folded value of an enum that is neither an integer nor a value computed from `size_of`. | The check holds the value to the integer base of the enum. |
| `check_defaults` of `src/antic/sema_declare.c` | A function whose type is missing, is the error type or has fewer parameters than the item. | `function_type_of` of `src/antic/sema.c` gives every function a type of its parameters, and the error type when one parameter has it, as the row of `check_owned` says. |
| `sema_check_provides` of `src/antic/sema_declare.c` | A class of the module without a type. | The checker gives every class it declared its type before it checks a `provides` line. |
| `module_operator` of `src/antic/sema_declare.c` | An `operator fn` among the items of a module that has an owner. | An `operator fn` of a class body is a member of the class and no item of the module. |
| `first_type_name` of `src/antic/sema_declare.c` | A parameter without a type. | The parser gives every parameter of an `operator fn` its type. |
| `resolve_base` of `src/antic/sema_declare.c` | A base whose symbol is a struct of the error type. | The checker gives the symbol of a type a type of its kind. |
| `check_own_field`, `check_transient_field` and `resolve_fields` of `src/antic/sema_declare.c` | A field of a function type bound to an object. | No type a program writes is a bound function. |
| `check_field_defaults` and `check_function_defaults` of `src/antic/sema_declare.c` | A declared field past the fields of its type. | `declare_fields` gives the type every declared field, after the base. |
| `declare_members` of `src/antic/sema_declare.c` | A qualifier that names a struct symbol without a type, and a function of the body of a struct. | The checker gives every struct symbol its type, and a struct body holds fields alone. |
| `sema_declare_all_members` of `src/antic/sema_declare.c` | A singleton, a struct, a union or a class without a type, and a constant of a body without a symbol. | The loops above give every item and every member its symbol and its type. |
| `function_in` of `src/antic/sema_const.c` | A part that is `NULL`. | The parser gives every operand, element and field of a literal its expression. |
| `sema_refuse_value_parts` of `src/antic/sema_const.c` | A place read after the functions have their types, and a refusal that `fail_const` takes back. | Each caller reads its place before, and `sema_const_symbol` calls it before alone. `fail_const` gives `false` always. |

## The sides of batch 3ar

Batch 3ar of the third phase read the expressions of lowering in
`src/antic/lower_expr.c` and the constant expressions of the checker in
`src/antic/sema_const.c`. It read each against the specifications and the
DESIGN comments of its file, and it found no defect. The rows of the two files under
their own headings above stand as they were.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `build_slice` of `src/antic/lower_expr.c` | A base of a slice without a whole length. | A slice is taken of an array, a slice or a `str`, and the checker refuses a pointer with `` cannot slice `*int` ``. |
| `lower_build_into` of `src/antic/lower_expr.c` | A value converted to an interface and wrapped into a `?T` in place. | A `?*T` of an interface is a pointer that holds `none`, and no `?T` of a value wraps one. |
| `lower_build_into`, `lower_collect`, `coalesce_value`, `coalesce`, `lower_optional`, `lower_argument`, `lower_drop_arguments`, `lower_expr` and `lower_expr_value` of `src/antic/lower_expr.c` | A block that the value it lowered ended. | No expression that gives a value ends its path. A call of a `-> never` function gives no value, and the checker refuses it as one with `` `stop` returns no value ``. |
| `build_value_into` of `src/antic/lower_expr.c` | An error of a `construct` that runs on the heap without a handler. | A `construct` that may fail takes a handler, `try` among them, and `lower_expr_value` lowers an `alloc` with a handler itself. |
| `build_value_into` of `src/antic/lower_expr.c` | A bound function whose symbol is no function, whose base has no struct, or whose function has no entry in the table. | No type a program writes is a bound function, the base of one is a class, and the checker refuses a bound function of a private function with `a bound function takes a public function`. |
| `lower_address` of `src/antic/lower_expr.c` | A constant that is neither a name nor a field, a cast of an aggregate that is no simd struct, a binary operation on aggregates other than `??`, and a `?.` whose result is no pointer. | A constant is read by a name or by `T.NAME`. `as` between aggregates converts a simd struct alone, and `?.` gives a `?*U`, by "Small things" of the additions. |
| `lower_unary` of `src/antic/lower_expr.c` | Unary `-` of an unsigned integer, and a dereference or an address that `lower_place` cannot resolve. | The checker refuses `-u` with `` unary `-` needs a signed integer or a float ``, and every operand of `*` and `&` is a place. |
| `flag_op` and `lower_flag_operation` of `src/antic/lower_expr.c` | A unary flag operation passed to `flag_op`, and a sum before a carry whose operands have another type than the result. | `lower_flag_operation` writes unary `-` itself, and the operands of `+` and `-` have the type of the result. |
| `may_be_sub`, `lower_binary` and `lower_load_table` of `src/antic/lower_expr.c` | A pointer that is no class pointer at an identity test, a value that is not optional beside one that is, a `?T` that is no aggregate, an operation on `str` that is no comparison, and a table read through no class pointer. | The identity test runs on two class pointers alone, `none` takes the type of the other side, by `binary_operands` of `src/antic/sema_operator.c`, a `?T` of a value is a struct, the checker refuses every other operator on `str`, and a table is read through a pointer to a class. |
| `descriptor_field` and `narrow_from_i64` of `src/antic/lower_expr.c` | Field 0 of a descriptor, and a result of a runtime operation of 64 bits. | Every caller reads a field after the first, and every caller narrows to a type below 64 bits. |
| `viewed`, `cast_writes_tables` and `lower_cast` of `src/antic/lower_expr.c` | A view of a slice, a function without a module, a cast to its own type that tests a class or a case, an f16 converted to f16, a class pointer converted to no pointer, and a float converted to a float of its own width. | The checker refuses `as` between slices, every function has its module, `is` gives a `bool`, a read of an f16 gives an f32, a class pointer converts to another pointer alone, and a cast to the type the value has returns first. |
| `lower_sym_of` of `src/antic/lower_expr.c` | A kind of symbolic value outside the switch, and a parameter of a generic. | The switch takes every kind, and the comment of `SYMBOLIC_PARAM` says why no parameter reaches it. |
| `fresh_argument`, `keep_address_argument`, `dispatched_params`, `lower_callee`, `own_marks`, `table_target` and `lower_call` of `src/antic/lower_expr.c` | A call without its function type, an argument past the parameters, a value at a pointer parameter, a call through the table without the symbol of its function or without `self`, an entry that is not in the table, and a bound function called by its name. | The checker gives every callee its type and every call its arguments. It writes a call of a function of a class as a call of the function's name with the object first, and a bound function is called through the local or the field that holds it. |
| `lower_expr_value` of `src/antic/lower_expr.c` | `parallel` and `dispatch` as a scalar value, a bound function as a scalar, a function of another module as a field, a value of an enum that is no enum, an `alloc` of a call that builds nothing or whose `construct` cannot fail beside a handler, a `delete` of an error that moved without its symbol, and a kind outside the switch. | `parallel` gives a slice and `dispatch` a job, which are aggregates. The checker writes `m.f` as a name, by `check_qualified` of `src/antic/sema_call.c`, and refuses a handler on a call that cannot fail. Only the error a handler binds moves into `delete`. |
| `one_value` and `folded_bits` of `src/antic/sema_const.c` | A constant of a target-sized type that does not fit the narrowest width. | Every one of the six targets is 64 bits wide. |
| `class_value` of `src/antic/sema_const.c` | The base field of a class without a type. | A base that `resolve_base` refused leaves the field with the error type, by the comment of `declare_fields` of `src/antic/sema_declare.c`. |
| `as_symbolic` of `src/antic/sema_const.c` | A `char` beside a value from `size_of`. | A value from `size_of` is an integer, and the operands of an operation have one type. |
| `operand_text` and `reports_undefined` of `src/antic/sema_const.c` | A literal without its spelling, a constant that is no integer converted to `char`, unary `-` of an unsigned constant, and the least value printed without its sign. | The parser keeps the spelling of every literal. Only an integer converts to `char`, the checker refuses unary `-` of an unsigned type, and the least value of a signed type is below zero. |
| `const_as` and `const_unary` of `src/antic/sema_const.c` | A conversion and a negation of a constant without a value in their type. | `sema_undefined_on_constants` of the check gives the message first, and a constant whose check failed is not evaluated. |
| `const_elements`, `const_integer` and `const_index` of `src/antic/sema_const.c` | A type without a value, an operator outside the switch, and an index past the end. | Every constant has its type, the switch takes every operator the check folds, and the comment of `const_index` says why no index reaches it. |
| `const_compare` of `src/antic/sema_const.c` | A text beside a value that is no text. | The operands of a comparison have one type. |
| `defined_integer`, `sema_fold_symbolic`, `without_value`, `first_without_value` and `sema_refuse_copy_without_value` of `src/antic/sema_const.c` | A shift by a count below zero, a conversion of a `bool` to no integer, a folded value outside its type, a sum whose operands are no integers, an operation without its second operand, and a `<<%` that is not binary. | The checker refuses a negative count of `<<` and `>>` where it is a constant. A `bool` converts to an integer alone. An operation that has a value is folded before `without_value` reads it, and a binary node has two operands. |
| `const_repeat` of `src/antic/sema_const.c` | `[v; n]` with a length from `size_of` outside the default of a field. | The checker refuses such a length outside a struct field and a local with `` a length computed from `size_of` is allowed only in a struct field or a local variable ``. |
| `const_field` of `src/antic/sema_const.c` | A field of a call that has a value, a constant of a class whose value failed, a field without its place in the struct, and a `str` without its text. | A call is never a constant. A constant whose value failed takes the error type, so the check of its use stops first. The checker resolved every field, and a constant `str` holds its text. |
| `sema_eval_const` of `src/antic/sema_const.c` | A kind outside the switch, a name without a symbol, `free(p)`, a descriptor and `take` as a constant. | The switch takes every kind, a name without a symbol had its message, `free` gives no value, a descriptor stands in `lib.instance` alone, and the checker refuses the `take` first. |
| `const_deps_of`, `function_in` and `sema_refuse_runtime_parts` of `src/antic/sema_const.c` | A part that is `NULL`, and an atomic operation in an early place. | The parser gives every part its expression, and the checker writes `EXPR_ATOMIC` after the walk, where the call stands. |
| `sema_refuse_value_parts` of `src/antic/sema_const.c` | A function in an early place once the functions have their types. | The row of batch 3aq says why. |
| `sema_const_symbol` of `src/antic/sema_const.c` | A static field that is not atomic. | The checker refuses one with `` `n` is `static` and must be `atomic` ``. |

## The sides of batch 3as

Batch 3as of the third phase read the values and the moves of the checker
in `src/antic/sema_value.c` and the shared parts of lowering in
`src/antic/lower.c`. It found T396 to T401 and fixed each. The rows of the
two files under their own headings above stand as they were.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `replaces_destroyed` of `src/antic/sema_value.c` | A function of a class without its type, and a `destroyed` without `self`. | The checker gives every function of a class its type before it asks for a teardown, and it refuses a `destroyed` without `self` with `` `Object` already has `destroyed` ``. |
| `own_field_in_chain`, `destruct_in_chain` and `destroyed_in_chain` of `src/antic/sema_value.c` | A chain that starts at `NULL`, and a `destruct` of the root in the chain. | Every caller passes a class, and no class of a chain holds a function of the root, whose entries `sema_table_of` of `src/antic/sema_class.c` adds apart. |
| `owning_field` and `owns_reason` of `src/antic/sema_value.c` | A walk that starts at `NULL`, and an element of a tuple whose name has no `_` before its number. | The caller passes the type of a value, and the checker names every element of a tuple `_0` upwards. |
| `sema_owns_phrase` and `sema_type_owns` of `src/antic/sema_value.c` | A type that is `NULL`. | Every caller passes the type of a value or of a field. |
| `moves_own_param`, `sema_literal_moves` and `moves_at_call` of `src/antic/sema_value.c` | A value without a type or of the error type, and a type that needs a teardown and owns nothing. | The checker gives every name its type, and a caller binds no value whose check failed. The one type that needs a teardown and owns nothing is an `own fn`, which the test of a function type returns before, and `sema_note_move` returns for a parameter of function type. |
| `sema_refuse_own_field_copy` of `src/antic/sema_value.c` | A store into no field, a binary operation other than `??`, and a field read without a type, of the error type, of a type that owns or on a base without a type. | Every caller passes a field it found. Of the binary operators `??` alone gives a pointer, and `sema_require` took the type of the value before. |
| `sema_check_take` of `src/antic/sema_value.c` | A field that names a symbol. | The checker writes a name of another module, `math.PI`, as a name, so `take` refuses it as no field. |
| `sema_owned_param` and `sema_note_move` of `src/antic/sema_value.c` | A call without its callee, an index past the `own` marks, an argument without a type, and a callee without its function type. | Every caller passes the function it called, an index of its parameters and an argument the checker typed, and the checker gives every function its type. |
| `sema_refuse_caller_value` of `src/antic/sema_value.c` | A parameter that a handler binds, and a value without a type. | A handler binds a local, and the checker gives every name its type. |
| `lower_set_optional` of `src/antic/lower.c` | A type that is `NULL`. | Every caller passes the `?T` it writes. |
| `min_bits` and `max_bits` of `src/antic/lower.c` | A lock word in a conversion. | The checker refuses `as` between a `Mutex` and another type with `` cannot convert `Mutex` to `int` ``, and `as` to the type of the value converts nothing. |
| `lower_add_params` of `src/antic/lower.c` | A function type without its result, a result of another type in the IR, and a function without its symbol. | The checker gives every function type its result, each caller made the function from the same type, and each passes its symbol. |
| `has_context_signature` of `src/antic/lower.c` | A pointer of the context with an aggregate. | A pointer in the IR has no aggregate. |
| `lower_offset_address` and `lower_field_at` of `src/antic/lower.c` | An offset that is an integer other than 0. | Every caller passes 0 or a symbolic offset that the back end folds. |
| `lower_range_of` of `src/antic/lower.c` | An enum whose type in the IR lies below `IR_I8`. | The underlying type of an enum is an integer. |
| `lower_literal_global` of `src/antic/lower.c` | A global of bytes of the module without a NUL at its end. | Each global of bytes that lowering writes under the path of a module ends with a NUL: the literals, the version of the package and the path of the module in `src/antic/lower_desc.c`. |
| `lower_bound_is_direct` of `src/antic/lower.c` | A bound function without its symbol, and no class. | The checker gives every bound function its symbol, and a bound function is taken of a class. |
| `known_copy`, `note_generic_copy` and `lower_module` of `src/antic/lower.c` | An item without its symbol or its type, and a copy of a generic whose generic has no symbol or no module. | The checker gives every item that lowering reads its symbol and its type, and a copy is made of a generic it declared. |

## The sides of batch 3at

Batch 3at of the third phase read the C side of the checker in
`src/antic/sema_export.c` and the check `uninitialized-read` in
`src/antic/sema_uninit.c`. It found T402 to T407 and fixed T402, T403,
T405 and T406. The rows of the two files under their own headings above
stand as they were. The line numbers are those of the coverage run of
`docs/reports/2026-11-22-coverage-phase-3at.md`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `nested_in` of `src/antic/sema_export.c`, line 34 | A type of a class body without its symbol. | The checker declares every type of a class body with its symbol before it checks what crosses to C. |
| `c_representable` of `src/antic/sema_export.c`, line 57 | A field, a parameter or a result of the type of `none`. | `none` names no type, so only the literal has that type, and no declaration writes it. |
| `held_nested` of `src/antic/sema_export.c`, lines 289 and 292 | A field without its type, and an array or a pointer without its element. | The checker gives every field its type, the error type among them, and every array and pointer its element. |
| `check_nested_fields` and `sema_check_export` of `src/antic/sema_export.c`, lines 317 and 563 | The table pointer among the fields of an export class or of a class nested in one. | `anti.lang.Object` alone holds the table pointer, and every other class reaches it through its base. No class of a program is `Object`. |
| `c_name_kind` of `src/antic/sema_export.c`, line 432 | A struct symbol without its type. | The checker declares every struct, class, enum and variant with its type, and a library file carries the type of each. |
| `sema_check_export` of `src/antic/sema_export.c`, line 625 | An export struct or union with `align(N)` and no field. | The parser refuses the empty body with `expected identifier`. |
| `sema_check_export` of `src/antic/sema_export.c`, lines 581 and 584 | A function of an export class without its symbol or its type. | `declare_members` of `src/antic/sema_declare.c` gives every member of a class its symbol and every function its type, the error type among them. |
| `sema_check_extern_fn` and `check_c_nullable` of `src/antic/sema_export.c`, lines 174, 248, 251 and 272 | An `extern fn` without its type, a type with fewer parameters than the declaration, a written result of type `void`, and a part of the error type. | The checker gives every `extern fn` its type, with one parameter per parameter of the declaration, since an `extern fn` takes no `self` and `may fail` is refused there. `void` names no type, so antic says `` unknown type `void` ``. A parameter or a result of the error type makes the whole function the error type, and the check returns at line 248 for it. |
| `before` of `src/antic/sema_export.c`, line 676 | Two items that start on one line, a generic and a kept item. | Every source of a test stands in the canonical form, which `fmt_canonical` checks, and that form starts each item on a line of its own. A source in another form reaches the side. |
| `sema_interface` of `src/antic/sema_export.c`, lines 724, 726, 741, 742, 748, 760, 761 and 768 | An item without its symbol or of the error type, a generic other than a function, a struct, a class and a variant, and a function without its function type or whose type has fewer parameters than its declaration. | The driver writes an interface after a check without an error, where every item has its symbol and its type. The parser takes type parameters on those four kinds of item alone, and the type of a function holds every parameter of its declaration, with the out pointer after them. |
| `state_join` and `read_element` of `src/antic/sema_uninit.c`, lines 271 and 434 | A fill whose read was refused. | No statement sets the flag `reported`. A refused read clears the fill at line 451, and the walk stops following the buffer there. |
| `place_element` of `src/antic/sema_uninit.c`, line 525 | A field with a symbol as the place of `=` or `&`. | A field with a symbol is a static field, a constant, an enum value or a function of a type. The checker refuses `=` and `&` on each, a static field with `` `total` is atomic, so it is read with `load()` and written with `store(v)` ``, and C1 runs on a function without an error. |
| `one_turn` of `src/antic/sema_uninit.c`, line 1009 | A loop that is not a `for` with a statement to bind its names. | `walk_for` alone sets the statement of the loop. |
| `walk_loop` of `src/antic/sema_uninit.c`, lines 1070 and 1075 | An exit of a loop that holds a buffer the end of a turn lacks. | The exits join at the head and at the tail of the turn, and the end of the turn lies between them on the same path. A path adds buffers and never removes one, and an expression binds none. |
| `local_of` and `raw_alloc` of `src/antic/sema_uninit.c`, lines 301 and 334 | No expression, and `alloc` with both a count and a value. | Every caller passes an expression. The parser gives `alloc(T, n)` a count and `alloc T { ... }` a value, and never both. |
| `key_of` of `src/antic/sema_uninit.c`, line 402 | A constant index that is no integer. | The checker refuses an index of another type with `` expected `int` ``, and C1 runs on a function without an error. |
| `walk_address` of `src/antic/sema_uninit.c`, line 549 | An element whose buffer and index cannot be read. | `place_element` gives an element of a buffer alone, which `element_of` takes apart. |
| `walk_handler` and `walk_closure` of `src/antic/sema_uninit.c`, lines 587 and 628 | A handler block without its body, and an anonymous function without its item. | The parser gives every handler block its body and every anonymous function its item. |
| `none_test` of `src/antic/sema_uninit.c`, line 693 | An expression that is not binary. | Its one caller is the arm of `EXPR_BINARY` of `walk_expr`. |
| `walk_expr` and `walk_stmt` of `src/antic/sema_uninit.c`, lines 713 and 1171 | A kind outside the switch. | Each switch names every kind of expression and of statement. |
| `walk_assign` of `src/antic/sema_uninit.c`, line 897 | A value that ends the path. | The walk ends a path at a statement alone, so the path that reached the assignment is live after its value. |
| `walk_stmt` of `src/antic/sema_uninit.c`, line 1168 | No statement. | Every caller passes a statement of a block, an arm or a branch. |

## The sides of batch 3au

Batch 3au of the third phase read the descriptors, the tables and the
chains of lowering in `src/antic/lower_desc.c` and the pages of
`anti doc` in `src/antic/docpage.c`. It found T408 to T412 and fixed
T408 to T411. The rows of the two files under their own headings above
stand as they were. The line numbers are those of the coverage run after
the batch, at `0ff649a3`.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `doc_type_name` of `src/antic/docpage.c`, line 35 | No type. | Every caller passes the type of a field, a parameter, a result, a constant or a base, which the checker gives each. |
| `declared_result` and `declared_params` of `src/antic/docpage.c`, lines 130 and 144 | A function with the out pointer and no parameter. | The out pointer is itself a parameter of the type. |
| `declared_result` of `src/antic/docpage.c`, line 134 | A function type without a result. | Every function type carries one, `void` where the declaration writes none. |
| `declared_params` of `src/antic/docpage.c`, line 147 | A count without `self`. | Every caller passes `false`, and `body_entries` reads `self` from the count the declaration wrote. |
| `type_params` of `src/antic/docpage.c`, line 214 | A type parameter without its record. | The checker gives every type parameter of a struct, a class and a variant its record, and the library file carries it. |
| `fn_signature` of `src/antic/docpage.c`, line 256 | More names than the type has parameters. | The count the declaration wrote never exceeds the parameters of its type, which hold `self` and the out pointer besides. |
| `fn_signature` of `src/antic/docpage.c`, line 261 | A parameter of a bound function. | No declaration writes the type of a bound function. `let f = c.area;` gives it to a local alone. |
| `fn_signature` of `src/antic/docpage.c`, lines 268, 269 and 277 | No list of names, and a parameter without its name. | Every caller passes one name per parameter. The parser gives each parameter its name, and the library file carries them. |
| `fn_signature` of `src/antic/docpage.c`, line 284 | A variadic function without a parameter before `...`. | The parser refuses `extern fn only(...)` with `expected identifier`, and `body_entries`, which alone passes `self`, passes no `...`. |
| `const_value` of `src/antic/docpage.c`, lines 343 and 355 | A constant value without its type. | The checker gives every constant value its type, and the reader of a library file reads it with the value. |
| `type_signature` and `enum_values` of `src/antic/docpage.c`, lines 376 and 497 | An enum without its base. | The checker gives every enum a base, `c_int` where it names none, and the library file carries it. |
| `type_signature` of `src/antic/docpage.c`, line 402 | A class without a base. | `anti.lang.Object` alone has none, which the compiler declares and which stands on no page. |
| `field_signature` of `src/antic/docpage.c`, line 429 | The table pointer among the fields of a class. | `anti.lang.Object` alone holds the table pointer, and every other class reaches it through its base. |
| `variant_cases` of `src/antic/docpage.c`, lines 461 and 470 | A tag with more values than the variant has cases, and a payload without fields. | The checker gives a variant one case per value of its tag enum, and a case without fields no payload. |
| `body_values` and `body_entries` of `src/antic/docpage.c`, lines 528, 529, 594 and 595 | A member without its symbol or its type. | `declare_members` of `src/antic/sema_declare.c` gives every member of a class its symbol and every function its type, the error type among them. |
| `constraint_signature` of `src/antic/docpage.c`, lines 648 and 652 | A constraint without its set, and a set without its declaration. | The checker gives every constraint its set, which names the item that declares it, and the reader of a library file does the same. |
| `interface_item` of `src/antic/docpage.c`, line 684 | An item of the interface without its type. | The interface of a module is written after a check without an error, where every item has its type. |
| `tree_items` of `src/antic/docpage.c`, line 825 | Two items that start on one line. | Every source of a test stands in the canonical form, which `fmt_canonical` checks, and that form starts each item on a line of its own. |
| `build_page` of `src/antic/docpage.c`, line 877 | An interface without its doc text. | `sema_interface` of `src/antic/sema_export.c` writes an empty text for a module without `//!`, and the reader reads the text the file carries. |
| `lower_table_index` of `src/antic/lower_desc.c`, line 42 | A function the table of the class lacks. | Every caller asks for a function the checker found in the chain of the class. |
| `lower_note_table_read` and `foreign_class` of `src/antic/lower_desc.c`, lines 75 and 101 | A class with a base that the compiler declares. | `anti.lang.Object` is the one class the compiler declares, and it has no base. |
| `lower_note_table_read` of `src/antic/lower_desc.c`, line 80 | A slot outside the table. | Every caller passes the slot that `lower_table_index` found. |
| `foreign_class`, `note_table_write`, `lower_class_depth` and `lower_declared_copy` of `src/antic/lower_desc.c`, lines 100, 134, 236 and 1849 | No type, and a type that is no class. | Each walks the chain of a class up to the root, whose base ends it. |
| `note_table_write` of `src/antic/lower_desc.c`, line 147 | A copy whose table holds more entries than the table of its generic. | A copy takes the functions of its generic, and no others. |
| `lower_class_global` of `src/antic/lower_desc.c`, lines 286 and 291 | An export item that is no class. | Every caller passes a class. |
| `type_id_of` and `lower_struct_descriptor` of `src/antic/lower_desc.c`, lines 418, 1682 and 1684 | A field of a `Job` and of the lock of an object. | No declaration names either type. A `Job` stands in a local that `dispatch` gives, and the lock of a synchronized class is a `transient` field, which no field list holds. |
| `type_id_of` of `src/antic/lower_desc.c`, line 449 | A kind past the table of the built-in types. | Every kind that the switch above it leaves to the table is a built-in scalar of it. |
| `variant_descriptor` of `src/antic/lower_desc.c`, line 739 | A field list of a variant without its descriptor. | The module writes the descriptor and its list together, and it finds the descriptor first. |
| `append_type_name` of `src/antic/lower_desc.c`, line 1110 | A parameter of a bound function. | No declaration writes the type of a bound function. |
| `signature_code` and `failing_code` of `src/antic/lower_desc.c`, lines 1139 and 1179 | A parameter of type `void`. | `void` names no type, so antic says `` unknown type `void` ``. |
| `failing_code` and `give_enum_descriptors` of `src/antic/lower_desc.c`, lines 1173 and 1201 | An out pointer that is no pointer. | The type of a function that may fail takes a pointer to its result. |
| `entry_code` of `src/antic/lower_desc.c`, line 1215 | No type, and a type that is no function type. | Both callers pass the type of a function, which the checker gives every function of a table. |
| `class_functions`, `class_chain` and `lower_class_table` of `src/antic/lower_desc.c`, lines 1294, 1311, 1312, 1401, 1887 and 1902 | An entry of a table without its item, its symbol or its function type. | `table_of` gives every entry the item of its function, the root's among them, and the checker gives each its symbol and its type. |
| `lower_class_descriptor` of `src/antic/lower_desc.c`, line 1594 | A `destruct` without a body or a symbol. | A class declares `destruct` with a body, and a descriptor is written by the module that declares the class or the copy. |
| `lower_class_descriptor` of `src/antic/lower_desc.c`, lines 1645 and 1646 | A copy of a generic without its arguments or its parameters. | The checker makes a copy from one argument per parameter of a generic. |
| `lower_struct_descriptor` of `src/antic/lower_desc.c`, line 1717 | A struct without a field. | The parser refuses an empty body with `expected identifier`, and `transient` stands in a class body alone. |
| `lower_interface_table` of `src/antic/lower_desc.c`, lines 1955 and 1960 | A function of the class or of the interface without a symbol or a body. | The checker gives every member its symbol and refuses a class that leaves an abstract function of its interface without a body. |
| `constructs_with_arguments` of `src/antic/lower_desc.c`, line 1985 | A `construct` without its symbol or its function type. | `declare_members` of `src/antic/sema_declare.c` gives every member of a class its symbol and every function its type. |
| `requires_class_field` and `lower_class_record` of `src/antic/lower_desc.c`, lines 1999 and 2047 | A base that is no class. | The base of a class is a class, and the root ends the walk. |
| `lower_class_record` of `src/antic/lower_desc.c`, line 2089 | A `provides` line without its interface. | The checker refuses a `provides` line whose interface it cannot resolve, and lowering runs on a module without an error. |
| `lower_static_descriptor` of `src/antic/lower_desc.c`, line 2111 | A type other than a class and a pointer to one. | The checker refuses `delete`, `destroy` and `dup` of any other type, as `` `delete` needs a class pointer, found `*Point` ``, and the teardown of an owned buffer asks for a class alone. |

## The sides of batch 3av

Batch 3av of the third phase read the check `use-after-free` in
`src/antic/sema_free.c` and the checks of simd structs in
`src/antic/sema_simd.c`. It found T413 to T418 and fixed all six, T417
and T418 in `src/antic/sema_uninit.c`. The rows of `sema_free.c` under its
own heading above stand as they were. The line numbers are those of the
coverage run after the batch, at `a75e9eba`. The fixes of T417 and T418
move the lines of `src/antic/sema_uninit.c`, whose rows under batch 3at
name the lines of its own run.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `sema_check_simd_binary` of `src/antic/sema_simd.c`, lines 126, 127 and 138 | Lanes of `char` under `==`, `!=` or an order, and lanes that are neither numbers nor `bool`. | `sema_simd_lane_refused` refuses `char` lanes before the `switch`, and a simd struct takes lanes of numbers, of `f16` and of `bool` alone. |
| `layout_in` of `src/antic/sema_simd.c`, line 258 | A field of a bound function or of a function of two words. | No declaration writes the type of a bound function, the form of two words stands at a parameter alone, and a field of a struct takes no `own`, which antic refuses with ``expected `:` ``. |
| `layout_in` of `src/antic/sema_simd.c`, line 266 | An array without an element. | The checker refuses a length below 1 with `an array length is at least 1`, and the line before refuses a symbolic length. |
| `sema_simd_module_call` of `src/antic/sema_simd.c`, line 378 | A module without its library. | `sema_declare_import` of `src/antic/sema_declare.c` gives every module it declares its library, and the hidden modules of an interpolation and of a pattern get theirs where they are made. |
| `sema_check_simd_value` of `src/antic/sema_simd.c`, line 592 | A constant index of a shuffle that is no integer. | `sema_require` holds the index to `i64`, and a constant of that type is an integer. |
| `sema_simd_method_declared` of `src/antic/sema_simd.c`, lines 628 and 629 | A function without its type, and a function whose type is no function type. | The checker gives every function its function type where it declares it, before it checks a body, and the reader of a library file gives each the type the file holds. |
| `own_field` of `src/antic/sema_free.c`, lines 217 and 226 | No type, and a field without its type. | Every caller passes the type of a checked expression, and the checker gives every field its type. |
| `refuse`, `walk_expr` and `walk_stmt` of `src/antic/sema_free.c`, lines 322, 688 and 1199 | A kind outside the `switch`. | Each `switch` names every kind of its enum. |
| `bytes_pointer` of `src/antic/sema_free.c`, line 369 | A pointer without its element. | Every pointer type has its element. |
| `kill_moved` of `src/antic/sema_free.c`, lines 464 and 465 | A move without its note. | `sema_move_local` of `src/antic/sema_value.c` writes a note for every move it marks, and the error a handler binds, which `sema_move_caught` moves without one, is no value the walk follows. |
| `walk_list` of `src/antic/sema_free.c`, line 473 | No list of operands. | Its one caller passes the operands of a built-in of a simd struct, which `simd_node` of `src/antic/sema_simd.c` writes for each. |
| `walk_parts` of `src/antic/sema_free.c`, line 519 | A part of a literal without its value. | The parser gives every part and every field of a literal its value. |
| `walk_parts`, `walk_call` and `walk_assign` of `src/antic/sema_free.c`, lines 531, 605 and 895 | A path that ends inside a literal, among the arguments of a call or inside the two sides of `=`. | The walk ends a path at a statement alone, and the handler of a call joins the path that goes on. |
| `walk_handler` of `src/antic/sema_free.c`, line 571 | A handler block without its block. | The parser gives every handler block its block. |
| `walk_call` of `src/antic/sema_free.c`, line 614 | More arguments than parameters for a function with an `own` parameter. | Only a variadic `extern fn` takes more, and antic refuses `own` on one with ``an `extern fn` takes no `own` parameter, since C tears down nothing``. |
| `walk_call` of `src/antic/sema_free.c`, line 618 | An `own` parameter at the object of a call of a function of a class. | The object stands at `self`, which takes no `own`. |
| `walk_assign` of `src/antic/sema_free.c`, line 898 | A target that names a function of a class. | The checker refuses it with `cannot assign to this expression`, and the pass runs on a body without an error. |
| `one_turn` of `src/antic/sema_free.c`, line 1003 | A loop other than `for` with its statement. | `walk_for` alone gives the loop its statement. |
| `walk_stmt` of `src/antic/sema_free.c`, line 1196 | No statement. | Every caller passes a statement of a block, of an arm or of a pending list, and `walk_arms` passes `else` only where it stands. |

## The sides of batch 3aw

Batch 3aw of the third phase read the compiled copies of generics in
`src/antic/sema_copies.c` and the lexer in `src/antic/lexer.c`. It found
T419 and T420 and fixed both in `src/antic/sema_call.c`. The rows of
`sema_copies.c` and of `lexer.c` under their own headings above stand as
they were. The line numbers are those of the coverage run after the
batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `ty` of `src/antic/sema_copies.c`, line 132 | A map without arguments that names the class it copies. | `own_params_copy` alone sets the class of a map, and its map holds the arguments of the class and of the function. |
| `xf` of `src/antic/sema_copies.c`, line 222 | A level of the chain without its copy, and a copy with fewer fields than its generic. | `ty` gives a copy of every level, and a copy has the fields of its generic in their order. |
| `symbol_open` and `xsym` of `src/antic/sema_copies.c`, lines 234, 235, 262 and 269 | A symbol that is no constant, a constant without its value or without its symbolic value, and a substitution that gives none. | `xsym` asks about a constant alone, the checker gives every constant its value before the copies are made, a symbolic value holds its expression, and `sema_subst_symbolic` gives NULL for NULL alone. |
| `xsym` of `src/antic/sema_copies.c`, line 259 | The frame of a local that the clone has not mapped. | The frame is the function or the anonymous function that declares the local, and the clone maps each before it walks its body: `make_body` maps the function, and `xi` maps the anonymous function first. |
| `type_item` of `src/antic/sema_copies.c`, lines 492 and 496 | A constant without its symbol, and a member that is no function and no constant, or one without its symbol. | The members of a type are its functions, its constants and its static fields, each declared with its symbol. A field stands in the type and a nested type among the nested items. |
| `copy_function` of `src/antic/sema_copies.c`, line 541 | Two copies of one generic with counts of arguments that differ. | A generic has one count: the parameters of its class and its own. |
| `member_of_receiver` of `src/antic/sema_copies.c`, lines 639, 643 and 646 | A `construct` whose copy no type gives, a literal of the generic class itself without an argument, a copy that keeps a parameter, and a class that lacks the member. | The checker gives every literal the copy it builds, the call reaches `member_of_receiver` from a copy of the class alone, and the copy has the members of its generic. |
| `callee_copy` of `src/antic/sema_copies.c`, lines 722 and 760 | A call without a symbol, and a class without type parameters after the test that let a function of one through. | `direct_callee` passes a callee with a symbol, and the test at the start returns for a function of a class without type parameters that has none of its own. |
| `direct_callee` of `src/antic/sema_copies.c`, line 789 | A field callee whose symbol is no function. | A field of a value carries no symbol, a static field is atomic and holds no function, and a constant holds no function. |
| `member_of_copy` of `src/antic/sema_copies.c`, lines 823 and 830 | A member that is no constant and no function, a function with a symbol of another kind, and a copy without its item. | The members of a type are its functions and its constants, each function has a function symbol, and `type_item` gives an item to every copy of a generic of the program. |
| `walked_param` and `is_param` of `src/antic/sema_copies.c`, lines 1009 and 1016 | No type, and a walk of a pointer that may be `none`. | Every node of a checked tree has a type, and the checker refuses `for x in p` of a `?*T` with `` `for` walks a range, a slice, an array, a collection or an iterator, found `?*T` ``. |
| `xcall` of `src/antic/sema_copies.c`, line 1086 | The receiver of an interface of the constraints outside a copy. | The checker sets the type of the receiver in a generic body alone, which `walk_plain` never walks. |
| `xe` of `src/antic/sema_copies.c`, line 1301 | `chan T(n)` without its type or of another type. | The checker gives every `chan T(n)` the type of a channel of `T`. |
| `xassign` of `src/antic/sema_copies.c`, line 1353 | A `set_index` that leaves the statement an assignment. | `sema_set_index` writes the statement as the call of `set_index` whenever it takes it. |
| `xfor` of `src/antic/sema_copies.c`, line 1387 | A walk of a type parameter outside a copy. | `walk_plain` walks code that is not generic, where no value has the type of a parameter. |
| `xi` of `src/antic/sema_copies.c`, lines 1595 and 1603 | An anonymous function without its symbol or without the function around it, and one whose enclosing function the clone has not mapped. | The checker gives every anonymous function both, and the clone maps the enclosing function before it walks the body that holds the anonymous one. |
| `defaults_of_copies` of `src/antic/sema_copies.c`, line 1704 | A failure while the defaults of the copies are made. | The loop of `sema_compile_copies` stops before it asks once a copy failed, and no default of a class whose generic passed the checker fails. |
| `digit_run` of `src/antic/lexer.c`, line 706 | No buffer for the digits. | Each of its three callers passes the buffer of the literal. |
| `interpolated` of `src/antic/lexer.c`, line 1413 | A doubled brace whose second brace is the closing quote. | The character after the first brace is a brace, and the closing quote is no brace, so the test before decides. |
| `check_type_member` of `src/antic/sema_call.c`, lines 2758, 2759 and 2762 | A member that is no function after the test of a constant, and a function without its type. | The members a type reaches are its functions and its constants, and the checker gives every function its type where it declares it. |
| `check_member_name` of `src/antic/sema_call.c`, line 2860 | A call that reaches the bound function. | A call of a function on a value takes the path of a method call and never reaches the bound function, so `c->callee` differs from the name there, as at the test after it. |

## The sides of batch 3ax

Batch 3ax of the third phase read the writer of the library file in
`src/antic/antl_write.c` and the checker's part of hashing and of the
default `==` in `src/antic/sema_hash.c`. It found T421 to T427 and fixed
T421, T422, T423, T425 and T426. T424 is no defect, and T427 is open. The
rows of the two files under their own headings above stand as they were.
The line numbers are those of the coverage run after the batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `is_local_struct` of `src/antic/antl_write.c`, line 44 | A type without fields. | `antl_visit_type` and `put_type` ask about a struct, a class or a variant alone, and `put_defaults` asks after `types_has_fields`. |
| `carried_member` of `src/antic/antl_write.c`, line 60 | A function of a body without its symbol. | The checker declares every function of a body with its symbol, and a module is written once its check passed. |
| `default_fn` of `src/antic/antl_write.c`, lines 93 to 96 | A name without its symbol, a function without its item, one of a body named alone, a generic function, a function of another module and one without its type. | The checker gives every name its symbol and every function its item and its type. A function of a body named alone is `` unknown name `get` ``, a generic one is `` `pick` is generic, and names a function where it is called ``, and since T426 a function of another module is `a function is not a constant expression`. |
| `carried_value` of `src/antic/antl_write.c`, lines 115 and 116 | A constant without its symbol or its type, a constant of a copy, and one without its value. | The checker declares each constant and static field of a body with its symbol, its type and its value, and a copy of a generic holds no constant of its own. |
| `antl_put_symbolic` and `antl_put_value` of `src/antic/antl_write.c`, lines 395 and 714 | A kind that no case names. | The cases name every kind of the enum. |
| `put_param_type` of `src/antic/antl_write.c`, lines 458 and 461 | A set of a `constraint` without its item. | `declare_params` of `src/antic/sema_generic.c` gives the type of each `constraint` its item, and the reader of a library file gives each set it reads its item. |
| `put_header` of `src/antic/antl_write.c`, lines 1130, 1131 and 1157 | A package without its name or its version, and a module without its doc text. | `sema_interface` of `src/antic/sema_export.c` gives the package the module path and the default version, and the doc an empty text. |
| `antl_write` of `src/antic/antl_write.c`, line 1218 | A function of the interface without its item. | `sema_interface` takes the functions of the interface from the items of the module, and no library file that a reader read is written again. |
| `add_call` of `src/antic/sema_hash.c`, lines 98, 101 and 110 | No hook, a hook that is no function, and a call that the checker refuses. | `walk` calls it where `sema_hook` found the hook, a hook is a function with its type, and the row of `nested_call` gives why the call is never refused. |
| `sema_is_hash_call` of `src/antic/sema_hash.c`, lines 236, 240 and 241 | A node that is no call, a receiver without a type, and a callee `x.hash` without arguments that neither hashes nor stands on a value of a type parameter. | `open_node` of `src/antic/sema_copies.c` and `callee_read` of `src/antic/antl_verify.c` pass a call. The checker types every receiver, and `method_call` of `src/antic/sema_call.c` writes `x.hash()` of a class or of a type with the hook as a call by name, with the receiver as its first argument. |
| `sema_hash_without_effects` of `src/antic/sema_hash.c`, line 306 | A node that is no call. | `warn_unused_result` of `src/antic/sema_stmt.c` asks about a call alone. |
| `nested_eq` of `src/antic/sema_hash.c`, line 343 | A checked `==` of the stand-ins that stays a binary node. | `walk_eq` asks for it where `sema_operator_symbol` found an `operator fn eq` of the type, and the checker writes `==` as the call of that operator or refuses it. |
| `sema_eq_gap` of `src/antic/sema_hash.c`, line 446 | An object lock in a struct, a tuple or a case. | The hidden lock is a field of a synchronized class alone, whose fields `sema_class_gap` reads. |
| `declares` of `src/antic/sema_hash.c`, line 462 | A chain that ends before the root. | Every chain ends at `Object`, whose base is none, and the test of the base leaves the loop there. |
| `default_eq` of `src/antic/sema_hash.c`, line 604 | A simd struct. | `==` on two simd structs compares lane by lane, and `meets_hook` of `src/antic/sema_generic.c` answers for a simd part with `sema_simd_meets` before it asks for the default. |
| `sema_class_defaults` of `src/antic/sema_hash.c`, lines 688 and 689 | A class without its symbol or its type, and an item of kind class whose type is no class. | The pass runs once the checker passed, over items that each have their symbol and their type, and a class item has a class type. |

## The sides of the answers of 2026-11-24

Item 3 of "Answers of 2026-11-24" in `docs/decisions.md` refuses
`abstract fn construct(self);` and `abstract fn destruct(self);`. The line
number is that of the source at the commit that builds it.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `carried_member` of `src/antic/antl_write.c`, line 67 | A private `construct` or `destruct` without a body. | A function of a body without a body is abstract, and an abstract function is `pub`, so the test of the level decides first. An abstract `construct` or `destruct` is refused as well, `` `construct` cannot be abstract ``, so no library file holds one. |

## The sides of batch 3ay

Batch 3ay of the third phase read the pattern methods, the match as a
condition and `if let` on a value that may be `none` in
`src/antic/sema_pattern.c`, and the driver of antic in
`src/antic/driver.c`. It found T429 and fixed it. The rows of `driver.c`
under its own heading above stand as they were. The line numbers are
those of the coverage run after the batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `is_byte_slice` of `src/antic/sema_pattern.c`, line 62 | A type that is NULL, and a slice without its element. | Every caller passes a checked type, and every slice type has its element. |
| `method_call` of `src/antic/sema_pattern.c`, line 247 | A method with one count of arguments other than 1. | Of `str_methods`, `matches` alone takes one count, and it is 1. |
| `method_call` of `src/antic/sema_pattern.c`, line 303 | A template after an argument that failed its check. | The template is the second argument of `replace`, the first that the loop reads. |
| `sema_match_field` of `src/antic/sema_pattern.c`, line 742 | A group number past `u64`. | The lexer refuses such a number, and a library file holds the name of a field that a checked tree held. |
| `test_of` of `src/antic/sema_pattern.c`, line 786 | A type that cannot be `none`. | `sema_check_test` passes a `?Match`, and `sema_if_let_none` a type that `check_switch` of `src/antic/sema_stmt.c` found may be `none`. |
| `names_it` of `src/antic/sema_pattern.c`, line 840 | An expression that is NULL. | The parser writes the target of every assignment and the operand of every `&`. |
| `visit_write_expr` of `src/antic/sema_pattern.c`, line 866 | An anonymous function without its function. | The parser gives every anonymous function its function. |
| `sema_if_let_none` of `src/antic/sema_pattern.c`, line 926 | A binding that its scope holds already. | The binding is the first name of a scope of its own. |
| `pattern_position` of `src/antic/sema_pattern.c`, lines 977, 983 and 984 | A spelling that ends before its closing quote. | The lexer writes a pattern literal with its quotes, and PCRE2 names an offset inside the pattern. |
| `read_file` of `src/antic/driver.c`, line 119 | A source too large, read without its message. | `read_source` reads every source and names its failure, and the quiet read takes a library file, which has no limit. |
| `back_end` of `src/antic/driver.c`, lines 1204 and 1206 | A library file, and a library of `--lib` in dev mode. | `write_library` writes a library file before the back end, and `--lib static` and `--lib shared` build in release mode alone, so `--dev` beside them is refused. |

## The sides of batch 3az

Batch 3az of the third phase read the places, the addresses and the
constants of lowering in `src/antic/lower_place.c` and the run-time side
of `anti.regex` in `src/rt/patterns.c`. It found T430 to T432 and fixed
T430 and T431. The line numbers are those of the sources after the batch.

| Function | What it guards against | Why no input reaches it |
|---|---|---|
| `bitfield_of` of `src/antic/lower_place.c`, lines 104 and 115 | An expression that is no field, a static field, and a field that no struct or class of the chain declares. | `place_address` asks for a field alone, after it took a static field, and the checker gave every field its declaration. |
| `type_place` of `src/antic/lower_place.c`, line 220 | A static field. | Every static field is `atomic`, and its reads and writes are lowered as atomic operations, never through `lower_place`. |
| `type_place` of `src/antic/lower_place.c`, lines 227, 231, 236 and 237 | A union, a field its type does not declare, a member past the aggregate, a bitfield and a member of another IR type. | `names_union` returned for a path through a union, the checker writes `super` before a field of a base, a bitfield returned at the first guard, and the checker narrows names alone, so a field reads the type it holds. |
| `place_address` of `src/antic/lower_place.c`, lines 266 and 278 | A name whose variable is a scalar while its value is an aggregate, and a static field. | A narrowed name reads the value inside its variable, so a variable of a scalar holds a scalar value, and a static field is lowered as an atomic operation. |
| `lower_place` of `src/antic/lower_place.c`, line 331 | A place that `place_address` does not find. | `place_address` gives every expression an address, a value that is no place a slot of its own. |
| `const_tree` of `src/antic/lower_place.c`, line 382 | A field of a `?T` of three bytes that is no `has`. | A `?T` has the fields `value` and `has` alone. |
| `lower_store_constant` of `src/antic/lower_place.c`, line 549 | A type written part by part that has no fields. | A type that holds a class or a repeated array is a class, a `?T`, an array or a type with fields. |
| `lower_location_value` of `src/antic/lower_place.c`, lines 626, 629, 646 and 648 | No function, a function without its module, and a field of `SourceLocation` that is none of its five. | `here` is no constant expression, so lowering meets it inside a function, every function names its module, and `SourceLocation` of `anti.lang` has the five fields. |
| `literal` of `src/rt/patterns.c`, line 58 | A pattern literal that does not compile at start. | antic compiled every literal already, so the compile fails only where PCRE2 finds no memory, which no seam of `src/rt/fault.h` reaches. |
| `anti_rt_regex_message` of `src/rt/patterns.c`, line 110 | A code outside the error numbers of PCRE2. | `anti.regex` passes the code of a compile that failed. |
| `failure` of `src/rt/patterns.c`, line 175 | A search that finds no memory. | PCRE2 takes the memory of a search through `malloc`, which no seam of `src/rt/fault.h` reaches. |
| `slice` of `src/rt/patterns.c`, line 310 | An end before the start. | A match ends at or after its start, since PCRE2 refuses `\K` in a lookaround. |
| `search_again` of `src/rt/patterns.c`, line 542 | A search that finds its match no longer. | It runs the search that found the match, from the same place, with the same options and over the same text. |
| `expand` of `src/rt/patterns.c`, lines 652 and 662 | A piece of no kind, and a group the pattern lacks. | `anti_rt_regex_piece` gives the three kinds alone, and the checker or `anti_rt_regex_template_fits` refused a template that names a group the pattern lacks before the search. |
| `fits_span` of `src/rt/patterns.c`, line 716 | A length below zero. | Each length is that of a slice. |
| `anti_rt_regex_patch` of `src/rt/patterns.c`, line 769 | A walk that fails before its first search. | `walk_begin` with a limit of 0 and no check counts nothing and gives `DONE`. |

## Not in this note

- The paths that run on another host: `find_crt_dir`, the ELF and PE
  symbolizers of `src/rt/trace.c`, `anti_rt_fault_system_error` and the
  refusal of `anti sdk export` off a Mac.
- `work_not_waited`, which a test reaches, and which ends through `_Exit`
  before it writes its counts, with the message of
  `platform_run_on_stack` before it.
- The child of `platform_run_in` between `fork` and `exec`, which ends
  through `execvp` or `_exit` before it writes its counts.
- The failure of `vsnprintf` in `fitted`, which needs a message of more
  than `INT_MAX` bytes.
- The paths of `sema_stmt.c` that the report of the second batch did not
  sort, and the `SYMBOLIC_INT` arm of `size_on_every_target`.
- The sides of a condition in `sema_const.c`, `sema_expr.c` and
  `sema_generic.c` that the report of batch 3a did not sort.
- The sides of a condition in the three readers of a library file that
  the report of batch 3b did not sort.
- The sides of a condition in `sema_escape.c`, `whole.c` and
  `whole_tables.c` that the report of batch 3c did not sort.
- The sides of a condition in `lower_place.c`, `coff.c`,
  `llvm_emit_call.c` and `build.c` that the report of batch 3d did not
  sort.
- The sides of a condition in `sema_copies.c`, `sema_index.c` and
  `sema_lock.c` that the report of batch 3e did not sort.
- The sides of a condition in `lower_stmt.c`, `lower_expr.c`,
  `lower_desc.c` and `platform.c` that the report of batch 3f did not
  sort.
- The sides of a condition in `parser_stmt.c`, `types.c`, `patterns.c`
  and `repo.c` that the report of batch 3g did not sort.
- The sides of a condition in the eight files of batch 3h that its report
  did not sort. A class of 17 interfaces reaches the cap of 16 that
  `ambiguous_member` names.
- The sides of a condition in the eight files of batch 3i that its report
  did not sort. One of them is a field of a concurrent class of a
  library that another module writes under `unchecked`.
- The lines after the `switch` of each operation of `src/rt/atomic.c`,
  which every arm leaves through `return`. The `return` after the
  `switch` of `first_of` and `least` in `pattern.c`, which covers every
  kind of node.
- The quiet check that `sema_move_local` leaves, which no probe of batch
  3h reached. A damaged file reaches the mark of a generic copy above 1
  that `read_generic_copies` refuses.
- The sides of a condition in the files of batch 3j that its report did
  not sort. Among them are the third operand of an instruction in
  `layout_resolve`, the clear of an `own fn` in `lower_clear_owned` and
  the key of a run without a runtime archive in `cache_runtime`.
- The loop of `ready` in `src/rt/fault.c`, which a second thread enters
  while the first reads the variable. The levels below `armv8.5` of
  `anti_rt_arm64_level`, which a Mac of this generation never gives.
- A `}` after a `return` and the other regions that belong to the
  measurement.
- The sides of a condition in `lower_owning.c`, `sema.c`, `toml.c`,
  `ast_dump.c`, `main.c` of `anti` and `driver_search.c` that the report
  of batch 3k did not sort.
- The sides of a condition in the eight files of batch 3l that its report
  did not sort.
- The sides of a condition in the files of batch 3m that its report
  did not sort. Among them are the name of a group of a match past `u64`
  in a damaged library file and the line of `holds_short_class` that
  waits for T265. Batch 3n tests `rt.configure("")`.
- The sides of a condition in the files of batch 3n that its report did
  not sort.
- The sides of a condition in `sema_expr.c`, `sema_call.c` and
  `sema_generic.c` that batch 3p did not sort: 98, 152 and 148 after its
  tests. Most take a NULL operand or an error type after a message, or
  are a short circuit whose other operand decides. Among them are an
  anonymous function that captures nothing where a function is kept, a
  capture that is both changed and called at a `concurrent` parameter
  and the unification of the parameters of a generic operator of a
  class.
- The sides of a condition in `sema_class.c`, `fmt.c` and
  `sema_safety.c` that batch 3q did not sort: 70, 72 and 84 after its
  tests. Most take a member without a symbol, a piece that is no token or
  a field without a home, or are a short circuit whose other operand
  decides. A source that lexes and does not parse reaches some sides of
  `fmt.c`, such as an `if (` that no `)` closes. The files that batch 3q
  named and did not test keep their sides: `antl_read_ir.c` 96, most of
  them a value of a damaged library file that no case of `unit_modules`
  writes, `lower_expr.c` 84, `sema_const.c` 81, `sema_copies.c` 80 and the
  sides of an error type in `sema_call.c` 152, `sema_generic.c` 148 and
  `sema_expr.c` 98.
- The sides of a condition in the files of batch 3r that its report
  did not sort. Most take a NULL operand, a symbol without a type or an
  error type after a message. Others are a short circuit whose other
  operand decides. Among them is the merge of two declarations of a C
  function in `antl_read_ir.c`, whose result carries no range and whose
  code writes no table. `antl_tree.c`, which batch 3r named and did not
  test, keeps its sides.
- The sides of a condition in `antl_tree.c` that batch 3s did not sort.
  `io_ctype`, `io_symbolic`, `io_value`, `io_ref` and `io_sym` hold a
  `switch` over the three modes of a walk, whose missing `default` the
  measurement counts as a side. The others take a value of a damaged
  library file that no case of `unit_modules` writes: an arm of a
  `switch` whose read stops before the arms after it, the lanes of a
  shuffle without its simd struct and a read that stops inside them, and
  the record of a function of the root that names another module.
- The sides of a condition in `pattern.c` and `sema_operator.c` that
  batch 3t did not sort. Most in `sema_operator.c` take a symbol without
  a type, an error type after a message or an operator function that the
  declaration refused, or are a short circuit whose other operand
  decides. Among them is `none` on the left of `==` beside a `?T`, which
  takes the type of the other operand, so both operands are a `?T`.
- The sides of a condition in `antl_verify.c` and `whole.c` that batch
  3u did not sort. In `antl_verify.c` they are a field that names an
  extern function, a `dispatch` in a generic and the flag of a simd
  struct on a class or on a struct of no fields, which no case of
  `unit_modules` writes, and the `switch` over every kind of statement,
  whose missing `default` the measurement counts. In `whole.c` they are
  the caps on a chain of bases, a module of no name, a descriptor past
  the globals, a copy without blocks, an entry of no function in the
  walk of a worker, an extern worker, an aggregate past the list and a
  plugin with the runtime bundled.
- The sides of a condition in `parser_item.c`, `syms.c`, `trace.c` and
  `lower_stmt.c` that batch 3v did not sort. In `syms.c` they need a
  binary of Anti that no case of a table of messages writes: a program
  named by a suffix alone, a present binary whose path holds a quote or a
  line break, an archive beside a present binary, and a debug link of ELF
  or of Mach-O without a line, without a table or whose function the map
  gives. In `lower_stmt.c` most test the block after a
  path that ended inside a statement, or a call with a handler that writes
  no result, as the rows of its section above do.
- The sides of a condition in the files of the parser that batch 3w
  did not sort. In `parser.c` they are an `import` after an item that
  failed, which is a second mistake with a message of its own, a `}`
  without its `{` and a first token at the end in the recovery of a
  statement, a marker of three bytes and the depth limit in panic. In
  `parser_expr.c` they are a `.` after type arguments before a token that
  is no name, `as ?fn`, a `.` after the type of `is` before no name,
  `try` before the raw `alloc`, the depth of `??` and a NUL byte in a
  format. In `parser_type.c` they are the scans of a type named `lent`, of
  the marks of a parameter, of `??`, of `fn` without `(`, of `may fail`
  and of a `>>` in a list of parameters, and in `parser_stmt.c` a `try`
  whose block fails at the depth limit.
- The sides of a condition in `lower_stmt.c`, `antl_read_ir.c` and
  `antl_read.c` that batch 3x did not sort. In `lower_stmt.c` most take a
  symbol, a callee or a type that the checker always sets, and the call
  of a `never` function as a statement whose block ended reaches one
  side that no probe of batch 3x isolated. In `antl_read_ir.c` an index
  of an aggregate past the table in the sub-object of a class record
  reaches the failure of `read_classes`, which no case of `unit_modules`
  writes, and most of the others are the checks of a later file above.
- The lines of `src/rt/f16.h`. Each file that includes it holds a copy
  of its two functions. The summary counts as missed the lines that some
  copies do not reach, and one copy or more reaches every line.
- The sides of a condition in `header.c`, `licensing.c` and `toml.c`
  that batch 3y did not sort. In `header.c` most take a symbol, a type or
  a name that the checker always sets, a chain of bases that ends in no
  class or a body that names no base, and one is a slot of the length of
  the name of a helper whose bytes differ. In `licensing.c` they stand in
  the readers of a notice, of a record of sources and of the members of
  an archive, and no probe of batch 3y reached them. In `toml.c` most are
  the caps on the room of a key or a path, and the end of the text in the
  blanks before a value.
- The sides of a condition in `src/anti/deps.c` and `src/anti/main.c`
  that the report of batch 3z did not sort.
