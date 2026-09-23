# Back-end decisions of the audit fixes

Decisions made while fixing the audit findings of the back end. A later
step folds them into `docs/decisions.md`.

## Register allocation

- [provisional] An instruction that reads more spilled registers of one
  class than the target has scratch registers borrows a register for each
  read past them. The register is allocatable and caller-saved, and the
  instruction names it nowhere. Its value goes to a slot reserved before
  frame layout and comes back after the instruction and the store of a
  spilled result. An instruction that leaves its block and needs one is
  refused with an error. Reason: the finding S18 offers a third scratch
  register or a refusal to select. A third scratch register takes one
  register from every function and changes the allocation order of
  `docs/notes/regalloc.md`. A refusal rejects a valid program. Borrowing
  costs a store and a load in the rare instruction that needs it and
  changes the output of no other program.
