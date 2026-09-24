# Compilation of generics: blocked

The step was to build "Compilation" of "Generics" in round five of
`docs/anti-language-additions.md`. It stopped before any code, on one
question that the documents answer in a way the IR cannot follow as it
stands.

## What was done

- Read "Generics" of `docs/anti-language-additions.md` in full, "Generics
  and collections" of `docs/decisions.md`, `docs/notes/generics.md`, the
  checker's part of generics in `src/antic/sema_generic.c`, the IR in
  `src/antic/ir.h`, the helpers of lowering in `src/antic/lower_lowerer.h`
  and the loading of library files in `src/antic/driver.c`.
- No code was changed and no suite was run.

## Why the step needs a decision

The step asks for two modules that share one copy. That needs a generic
to cross a library file. "Libraries and C" says how: "A library file
stores a generic as IR with its parameters open". The program that uses
it compiles the copy. `docs/decisions.md` keeps generics out of
library files for now. Its reason names that sentence as part of
compiling the copies.

The IR of `src/antic/ir.h` cannot hold a body whose parameter types are
open. Lowering fixes, per checked type, what the IR then records:

- whether a value is a scalar in a temporary or an aggregate reached
  through a pointer. That fixes how a parameter, a result and a copy are
  written. An aggregate parameter's temporary holds a pointer, and a
  scalar one holds the value;
- the operation of an operator, since signedness lives in the operation:
  `IR_SLT`, `IR_ULT` or `IR_FLT`, or a call of an `operator fn` for a
  struct;
- whether leaving a scope runs `destruct`, a hook or a free;
- the offset that turns a pointer to a class into a pointer to one of
  its interfaces.

About 320 places in `src/antic/lower*.c` take such a decision from a type.
A body over `T` would need an open type in the IR and open operations
for each of the decisions above. A pass would then make them for each
copy, and that pass repeats part of lowering over the IR.

The other route keeps the IR as it is. A copy is the generic's tree
checked again with the arguments in place of the parameters, then
lowered as ordinary code. Descriptors, tables, `destruct`, hooks and
`size_of(T)` per copy then need no new machinery. A library file would
carry the generic's checked tree, with the names it reaches resolved,
and not IR. That differs from "stores a generic as IR", although the
library still ships no source and no templates in headers.

Either route decides how copies are made inside one module as well, so
no part of the step was started. Building one route before the choice
would risk throwing it away.

## Question for Eddie

How does a library file store a generic? The first answer is IR with its
parameters open, which adds an open type, open operations and a pass
that makes each copy from them. The second is the generic's checked
tree. Each module that uses it checks the tree with the arguments in
place and lowers it as ordinary code.

## Provisional decisions

None.

## Gates

Nothing was built, so the suites did not run. The docs-style checker
reports nothing on this report.
