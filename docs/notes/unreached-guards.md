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
`whole_tables.c`, `lower_place.c`, `coff.c`, `llvm_emit_call.c` and
`build.c` stay by them. A guard stays in the code as it is. A later
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
source of `docs/reports/2026-11-12-coverage-phase-3d.md`. A guard named "the report
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
| `copy_failed` | A copy of a generic of the module itself that fails, which keeps the position in the body of the generic. | The one form that fails, that of T095, is a copy of a generic of another module. |
| `type_item` and `member_of_copy` | A type item without its generic, and a member without a symbol, a type or a copy. | The checker binds each one before the copies are made. |
| `redirect`, `xassign` and `xfor` | An open index, `set_index` and `for` over a slice or an array. | No slice or array meets `index`, so the copy turns every open index into a call. |
| `recheck`, `receiver`, `xparams` and `xi` | A recheck that fails, a receiver without its types, a clone that is not fresh and an item that has its clone already. | The checked tree passed the same checks, and each item is asked for once. |
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

## Not in this note

- The paths that run on another host: `find_crt_dir`, the ELF and PE
  symbolizers of `src/rt/trace.c` and `anti_rt_fault_system_error`.
- `work_not_waited`, which a test reaches, and which ends through `_Exit`
  before it writes its counts.
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
- A `}` after a `return` and the other regions that belong to the
  measurement.
