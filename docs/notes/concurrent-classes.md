# Concurrent classes

The choices of the passes for "Concurrent classes" of
`docs/anti-language-additions.md`. The decisions stand under "Concurrent
classes" in `docs/decisions.md`. `docs/notes/locking.md` holds `sync` and the
channels.

## Mutex

- A Mutex is one word of the program's memory. `types_mutex` gives it the one
  field `word`, a `u32` to the checker with `lock_word` set. Lowering gives that
  field the IR type `IR_LOCK`, and the layout of each target makes it `i32` on
  Linux and macOS and `i64` on Windows. The IR holds no size of it.
- Zero is the unlocked state of the futex word, of `os_unfair_lock` and of
  `SRWLOCK`. `Mutex.new()` stores a zero word, and a field without a default
  takes the zero word in a literal and in `construct`, as
  `sema_field_takes_literal` says. `m.destroy()` releases nothing of the system
  and forgets the orders a dev build recorded for the lock.
- `sema_holds_mutex` answers whether a value holds a Mutex by value.
  `sema_refuse_lock_copy` refuses such a value read from a place at a `let`, an
  assignment, an argument, a `return`, a literal and `send`. An assignment to a
  place that holds a Mutex is refused whatever the value. A copy of an object
  writes a free lock in each Mutex field and in the hidden lock.
- `sync` takes the address of its Mutex, so the operand is a place or a pointer.
  `src/rt/lock.c` holds the lock of each system.

## Synchronized classes

- `synchronized` and `concurrent` are contextual words directly before `class`,
  after `abstract`, `final` or `singleton`. The item carries the mark, and the
  type carries it in `safety`, which the library file writes in a byte after the
  flags of the class.
- The hidden lock is the last field of the first synchronized class of a chain,
  named `(lock)`, which no program spells. Its type is the struct `Object lock`
  of `anti.lang`: a Mutex, the thread that holds it and a count. The field is
  `transient`, so the field list, `serialize` and the literals pass over it.
- Lowering takes the lock at the start of every function of the class with
  `self` that is not private, `construct` and `destruct` aside, and records the
  unlock as an exit action of the scope around the body. `sync obj` on a
  synchronized object takes the same lock. A thread that holds the lock takes it
  again and counts, so the calls inside need no analysis of their receiver.
- `sema_check_reach` refuses a field of a synchronized class reached outside a
  function with `self` of the class or of a class that inherits it, and outside
  its `construct` and `destruct`.

## Concurrent classes

- `guarded by lock` and `guarded by Class.lock` follow a field's type or its
  default. The parser keeps the two names, and `sema_check_guards` resolves them
  once every field of the module is declared, since a nested type stands before
  its class. A field of a nested type guarded by the class around it records
  that class in `guard_class`.
- A reach of a guarded field is checked against the `sync` blocks the checker
  holds in `c->held`. A lock of the same object matches a `sync` on the field of
  that name whose base names the same place. A lock of an enclosing class
  matches a `sync` on the field of that name of any object of the class.
  `construct` and `destruct` of the class, or of the enclosing class, reach it
  freely. A closure checks its body with no `sync` held.
- A write goes through `sema_note_write`, which assignments, compound
  assignments and `&` already call. A write outside `construct` and `destruct`
  to a field that is neither guarded nor atomic is kept, and
  `sema_report_unfixed` reports it at the field's declaration at the end of the
  module, so a clause after the field's type or in the class header covers it.
  A field of another module is reported at the write, unless its library says
  `unchecked`.
- A field that code outside the class reaches and that carries `unchecked` is
  reported at its declaration as well, where the clause silences it. The clause
  is therefore used, since another module may write the field.
- `compare_swap` on a plain field is `unchecked_swap` in `sema_call.c`. It takes
  a field of one word of a concurrent class, or of a type nested in one, that
  `unchecked(unguarded-field)` marks after its type or in the class header, and
  builds the node of an atomic field. It calls `sema_note_field_write`, so the
  field is reported at its declaration and the clause is used. Any other plain
  field is refused with both fixes. The parser reads the clauses after the type
  of a struct's field, which the node of a lock-free queue needs.
- The library file writes the name of a field's guard and two marks, `hidden`
  and `unchecked`, in the byte of `inject`. The class that a nested type's guard
  names stays behind, since no other module reaches a nested type. The format is
  version 57.

## Pointers into the fields

- `sema_points_into_fields` holds for `&` of a place reached from `self` through
  fields and array elements, a slice of such a place, the `ptr` of one, a field
  of slice type and a local whose `let` took one of those. A `return` of a
  function with `self` that is not private refuses it, and so does an argument
  of a call through a function value that is a local or a parameter.

## Atomic locals and workers

- `let n: atomic T = e;` sets `atomic` on the symbol and puts the local in
  memory. The atomic operations of fields take it as their place, and a plain
  read or write is refused with the message of a field.
- A worker's argument of type `*T` passes the pointer-free rule when `T` is a
  Mutex, a synchronized class or a concurrent class. `sema_thread_safe` counts
  the two classes and `sema_thread_safe_symbol` an atomic local, so a
  `concurrent` closure may change either.

## The order of the locks

- A dev build calls `anti_rt_mutex_lock_at` and `anti_rt_object_lock_at` with a
  site, the text `file:line` of the `sync` or of the function. A thread keeps
  the locks it holds on a stack of 32, and the runtime records one order per
  pair of locks in a table open addressed by the two addresses. The check runs
  before the thread waits, so a real deadlock is reported before it hangs. Each
  conflict is reported once.
- The records of a lock whose memory is freed without `destroy` stay. A later
  lock at the same address can then meet an order it never took.

## Tests

- `programs/synchronized.anti` and `programs/concurrent.anti` run workers of
  `parallel` against one object in both modes. `traps/lock_order.anti` with
  `run_lock_order.cmake` reads the report of a dev build and the silence of a
  release build. The error tests `synchronized`, `concurrent`,
  `concurrent_forms`, `pointer_leak`, `mutex_copy` and `concurrent_modules`
  hold the refusals. `sync_modules` takes both classes across a library,
  `clib_ledger` calls a synchronized class from two threads of C, and
  `mutex_size` reads the size of a Mutex from the listing of each target.
