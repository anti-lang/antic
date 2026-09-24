# Anonymous functions and closures

The choices of the passes for "Anonymous functions and closures" of
`docs/anti-language-additions.md`. The decisions stand under "Anonymous
functions and closures" in `docs/decisions.md`. The last part holds the
choices of `snapshot fn` and `own fn`.

## Types

- A function type carries two more flags. `context` marks the form of two
  words, the code and a context, which a parameter that does not keep its
  argument takes. `concurrent` marks that form at a `concurrent` parameter.
  `types_fn_form` gives the same signature in another form.
- `type_name` writes `keep` before a plain parameter of a function type and
  `concurrent` before a marked one, which is how a program writes the list.
  The top level writes no mark.

## Parser

- `keep` and `concurrent` are contextual words before a parameter, in a
  parameter list and in the list of a function type. `is_fn_mark` reads one
  before a name and a colon, before `fn` or `?fn`, and before the other mark.
- `fn` in the place of an expression is `EXPR_FN`, which holds an `ITEM_FN`
  of its own. A parameter may leave out its type.

## Checker

- `sema_param_form` gives a parameter its form: two words without `keep`, one
  C function pointer with it and in every `extern fn` signature. `plain_fns`
  of the checker is above zero while an `extern fn` signature resolves.
- `sema_check_anonymous` builds the type from the written types and the
  target. It then checks the body as a function whose scope sits inside the
  scope where the expression stands. The checker's state of the body around
  it waits in a `struct body_state`.
- `sema_declare` records the frame, the function whose body declares a
  local, and the depth of its block. Naming a local of another frame calls
  `sema_capture`. It adds the variable to every anonymous function between
  the one that names it and its frame, and marks it address-taken.
- `sema_note_write` records the first write of a captured variable,
  `sema_note_call` the first call through a captured function of the form
  that is not `concurrent`. The type of a closure is `concurrent` when it has
  neither on a variable whose type is not thread-safe.
- `require_fn_form` in `sema_require` converts between the forms and gives the
  messages of `keep` and `concurrent`. A plain function where the two words
  are expected marks the expression `to_context`.
- `check_closure_lifetime` refuses a closure held in a local of an outer block
  than a variable it captures. A local keeps the anonymous function it holds
  for the message of `concurrent`, and the captured variable declared deepest
  for the lifetime.

## Lowering

- `lower_add_param` writes a parameter of two words as two IR parameters.
  `lower_function` joins them in a slot, so the body reads the pair as any
  aggregate. `lower_push_argument` splits a pair into two arguments at a call,
  and the thunks pass every IR parameter on as it came.
- `lower_closure` writes the record of the context into a slot of the entry
  block and the pair into another. `lower_anonymous_function` declares the IR
  function the first time and queues it. `lower_module` lowers the queue after
  each item, and a closure met in the body of another joins the queue.
- The entry of a closure loads the address of each captured variable from its
  context into the `ir` of the variable's symbol. Every read and write of the
  body then goes through that address as it does in the frame of the
  variable.
- A call through a pair loads the code as the target and passes the context
  last. `lower_context_signature` names the signature with the extra pointer.
- A variable of a `for` that a closure captures has a slot, and each pass
  stores its value there.

## Header and tools

- The header writes a parameter of two words as the callback with a `void *`
  after its parameters and a `void *<name>_context` after it.
- `anti fmt` keeps the brace of an anonymous function on the line of its
  signature. The frame of its body keeps the indent, the continuation and the
  open brackets of the line it interrupts.
- `anti doc` writes the mark of a parameter from its type.

## Snapshots and owned function values

- `own fn` is a third flag of a function type, `owned`, which stands with
  `context` and `concurrent`. `types_fn_owned` gives it. It has the layout
  and the calls of the form of two words, so every pass that handles that
  form handles it. `types_fn_form` drops the flag, so the conversions
  compare the signature alone.
- The parser reads `snapshot fn` as an anonymous function with the flag
  `snapshot`. `fn_param_marks` reads `own` after `keep`, in a parameter
  list and in the list of a function type. `resolve_fields` gives an `own`
  field of function type the owned form.
- `check_snapshot` runs after the body of a snapshot. It refuses a capture
  that does not copy fully and the first write of one. A snapshot that
  captures something is `concurrent`.
- `require_owned` in `sema_require` takes a value into an `own fn` place.
  It marks a snapshot `snapshot_heap`, pairs a plain function with `none`,
  lets `dup` through and moves a `keep own` parameter of the function.
  `=` of any other owned value is refused with `dup` as the fix.
- A `let` of an owned value takes the form of two words without the flag,
  so a local borrows and never owns.
- `lower_snapshot` writes the record `<name>.snapshot`: the size, then one
  field per capture. On the heap the size adds the length of each `str`,
  `anti_rt_snapshot_new` makes the block, and `anti_rt_snapshot_text`
  copies the bytes after the record. `snapshot_entry` points each captured
  name at its field, and rebuilds a `str` of the heap in a slot.
- `lower_free_snapshot` frees the context word and writes `none` there. The
  teardown of a class, `=` into an `own fn` field and the exit action of a
  `keep own` parameter call it. `lower_dup_snapshot` serves the copy of a
  class and `dup(f)`.
- A moved `keep own` parameter hands a copy of its two words to the new
  owner and keeps `none`, so the exit action frees nothing.
- A named function as the default of an `own fn` field has no type when
  the fields are checked, if it is declared later. `lower_store_value`
  pairs it with `none` there.
- The header writes an `own fn` field as `struct { code; snapshot; }` and
  declares `anti_rt_snapshot_free` when an exported class has one.
- `src/rt/snapshot.c` holds the five functions of the runtime and the count
  of the snapshots alive. It is a member of its own, so a program that makes
  no snapshot links none of it.
