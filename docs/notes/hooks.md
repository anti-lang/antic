# Hooks and tracing

The choices of the passes behind the nine hooks of `anti.lang.Object`,
`anti.lang.TraceHandler` and the contextual `trace`. The rules are in
"Hooks and tracing" of `docs/anti-language-additions.md` and in "Hooks
and tracing" of `docs/decisions.md`.

## The entries

`root_names` in `src/lower.c` names the seven functions of the root and
then the nine hooks, in the order `created destroyed copied dispatched
joined enter leave failed changed`. `enum anti_hook` in `rt/object.h`
repeats that order and `ANTI_ENTRY_OF_HOOK` gives the entry of one,
counted from `ANTI_ENTRY_HOOK`. The unit test `records_hook_entries` in
`tests/unit/test_lower.c` reads the table of a class that replaces no
hook and checks that every entry holds the runtime's empty body under the
right name.

`ANTI_ENTRY_OF_HANDLER` gives the entry of the same hook in the table of
an `anti.lang.TraceHandler`, which declares the nine again with the
object after `self`. Those nine take the entries after the root's
sixteen, so every handler shares one place per hook.

A table entry is keyed by its name and its parameter count. `table_add`
and `table_index` of `src/lower.c` compare both, so the handler's
`created(self, o)` takes an entry of its own beside the root's
`created(self)`. Anti has no overloading, so the count changes nothing
for any other name of a chain.

## The sites

`hook_object`, `hook_copied`, `hook_call`, `hook_failed` and
`hook_changed` of `src/lower.c` each write one call of the runtime.
`rt/hooks.c` holds what a site does: it loads the handler from one atomic
word, calls the handler's function when there is one, and dispatches the
object's own hook unless that entry still holds the root's empty body.
`leave` runs the two in the other order, so the handler and the object's
hook nest around the call.

| Hook | Where the call is written |
|---|---|
| `created` | after the `construct` bodies of the chain, in `run_construct` and at the end of `lower_construct` |
| `destroyed` | at the head of the teardown, in `class_teardown` |
| `copied` | after the `dup` operator, in the `EXPR_OBJECT` arm |
| `dispatched` | after `anti_rt_dispatch`, in `lower_dispatch` |
| `joined` | in `anti_rt_join_hooked` of `rt/threads.c` |
| `enter` | before the first statement, in `lower_function` |
| `leave` | an exit action of a scope around the body |
| `failed` | before that `leave`, on an exit that carries an error |
| `changed` | after each store of `lower_assign` |

A `construct` that may fail writes its `created` on the success side of a
branch. A failed one leaves no object, and its memory goes back before
the handler runs.

`leave` is an exit action rather than a call at each `return`, so every
exit runs it: `return`, `fail`, the error of a `try`, and the closing
brace. It sits in a scope that `lower_function` opens around the body, so
it runs after the locals of the body are gone. `l->failing_error` carries
the error of an exit to the action, which calls `failed` first.

## What is instrumented

`traced_class` answers for a class: it was written `trace class` and the
build compiles marked code, or a `--trace` pattern names the class, its
module path or a package above it. `traced_function` answers for a
function: it has `self` and its name is none of the nine. It was then
written `trace fn`, or it is `pub` in an instrumented class.

The marking lives on the type and the library file carries it. A write to
a field of a class of another module therefore answers the same question
where the write stands.

`enter`, `leave` and `failed` stand in the body, so the build of the
module that holds the body decides them. A library file carries the IR of
a body as it was lowered, and a `--trace` on the program that links it
adds nothing there. `hooks_modules_release` and `hooks_modules_dev` pin
both halves with a library built under `--trace` alone and a program
built under `--trace` and `--trace writes`.

## Options

`--no-hooks` drops every site the compiler writes. `join` and `join_all`
then call `anti_rt_join` and `anti_rt_join_all`, which fire nothing, so
the option reaches the one site that lives in the runtime.

`--trace` and `--no-trace` decide for marked code instead of the mode,
which is on in dev and off in release. `--trace <pattern>` adds a class
that did not ask, in any mode. `--trace writes` adds the `changed` hook.
`--trace` takes the word after it as a pattern when that word is no
option and no last argument, since the last argument is the source file.
