# Snapshots: blocked

The step was "Snapshots" of `docs/anti-language-additions.md`: `snapshot fn`,
the read-only snapshot, captures that copy fully, acceptance at `keep` and
`concurrent` parameters, the frame and the heap as the two places a snapshot
lives, and the refusals that name `snapshot fn`. No code changed. The session
stopped before the first edit, because the part at a `keep` parameter needs a
decision that the documents do not contain.

## The question

Two rules of the same section of the specification meet at a `keep` parameter.

- "Snapshots" says a snapshot at a `keep` parameter "goes on the heap. Whoever
  keeps the closure owns it and frees it."
- "Calling convention" names a value of function type held in a field, a
  global or a `keep` parameter. It "stays one pointer, the C function pointer
  it is today". C callbacks such as raylib's then stay unchanged. The syntax overview
  says the same, and the entry under "Anonymous functions and closures" in
  `docs/decisions.md` builds it: nothing converts to the plain form.

A C function pointer has no room for the address of the heap snapshot. The
code of the snapshot cannot find its values. The keeper cannot tell a snapshot
it must free from a named function it must not free. Every way through
changes the representation of a kept function value, which the gap procedure
does not cover:

1. A kept function value becomes two words, the code and a context, in a
   field, a global, a result and a `keep` parameter. The plain C pointer stays
   for `extern fn` alone. This breaks "stays one pointer" and changes
   the header of every exported signature that keeps a function.
2. The runtime makes an executable stub per kept snapshot on the heap, as
   libffi closures do, so the kept value stays one C pointer. This needs
   writable and executable memory on every target: `MAP_JIT` and
   `pthread_jit_write_protect_np` on macOS ARM64, `VirtualAlloc` on Windows.
   It also needs a registry in the runtime that tells a stub from a named
   function when the keeper frees it.
3. Another representation Eddie names.

A second question follows from the first: who the keeper is. The test the
step asks for frees "a kept snapshot with its owner". That reads as the object
whose field holds the value, freed by its `destruct` chain. The documents do
not say what frees a snapshot held in a global, or in a local of the function
that took the `keep` parameter.

The parts at a `concurrent` parameter and at a parameter that does not keep
its argument need neither decision. They were not built on their own, because
the step is one feature and a part of it would ship `snapshot fn` with its
main use refused.

## State

Nothing was committed apart from this report. No `[provisional]` entry was
added. The suites did not run, since no code changed.
