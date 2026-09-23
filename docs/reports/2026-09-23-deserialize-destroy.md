# Blocked: `destroy` on an object of `Object.deserialize`

The step had two changes and a list of `[provisional]` tags to remove. None of it is done,
because the first change needs a decision that the documents do not hold. Nothing in the
tree changed apart from this report.

## The question

The step asks that the caller call `destroy(p)` on an object `Object.deserialize` made. That
call runs the destruct chain and releases what the object holds without freeing its memory.
The caller then gives the blocks back through the allocator.

`destroy` as built, and as "Destruction" in `docs/anti-object-model.md` defines it, frees
every owned object and every owned buffer and leaves only the object itself. The teardown the
compiler writes (`teardown_field` in `src/lower.c`) calls `anti_rt_delete` on an `own`
pointer to a class and the C library's `free` on any other `own` pointer or `own` slice.

The settled entry under "Object model" in `docs/decisions.md` says every string and every
owned sub-object that `Object.deserialize` creates comes from `from`. So a deserialised
object with an `own` field, such as `Tree` in `tests/std/deserialize_alloc.anti`, would give
memory of `from` to the C library's `free` under `destroy(p)`. Over an `ArenaAllocator` that
frees the middle of a block. The same block would then go back through `from` a second time.

A `construct` without arguments runs before the fields are filled. When it gives an `own`
field memory of the C library, the reader writes over that pointer with memory of `from`,
and the first memory is lost.

Which of these does Eddie want?

1. `destroy` of a deserialised object runs every `destruct` body, the owned objects'
   included, and frees no owned memory. That is a second teardown, or a flag the teardown
   reads. The flag changes the `destruct` entry of every table and `anti_<Class>_destroy`
   in the C header.
2. `Object.deserialize` takes owned objects and owned buffers from the C library, as
   `delete` and `destroy` expect, and only the object and its strings from `from`. That
   changes the settled entry above.
3. A class with an `own` field is refused by `Object.deserialize`, or a deserialised object
   with one is not given to `destroy`. Some other rule is possible as well.

The second change of the step, dropping `f"..."(from)`, and the removal of the tags do not
depend on the answer. The step was given as one piece, so they wait for it too.

