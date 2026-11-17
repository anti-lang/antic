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
`cache.c`, `sema_thread.c` and the files of batches 3l, 3m, 3n, 3o and 3p stay by them. A guard stays in the code as it is. A later
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
| `same_sym` | A symbolic value past the list, or none, two values of different kinds or types, and a kind outside the four. | The values of two copies whose instructions match come from the same expression of the generic, so they are of one kind and one type, and lowering names a value of the list. |
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
- The lines of `src/rt/f16.h`. Each file that includes it holds a copy
  of its two functions. The summary counts as missed the lines that some
  copies do not reach, and one copy or more reaches every line.
