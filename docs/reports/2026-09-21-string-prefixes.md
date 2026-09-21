# The string prefixes rf and x

The task: `rf"..."` and `rf#"..."#`, `x"00 AB CC"`, the prefix set `r b br f
rf x` in one table, and `fr` refused with a message naming `rf`, with tests for
each. Nothing was built, and nothing was committed apart from this report.

## Why the session stopped

"Small items, round three" of `docs/anti-language-additions.md` defines
`rf"..."` through `f"..."`: `{expr}` and the format specifications "work as in
`f"..."`". `f"..."` is not built. The lexer in `src/lexer.c` reads the prefixes
`r`, `b` and `br` alone, `docs/anti-syntax-overview.md` lists `f"..."` under
"Not built yet", `docs/decisions.md` defers it under "Core language", and
`CLAUDE.md` names it as step 15 of "First sessions, in order", which is not
done. `anti.text` holds no conversion of an integer or a float to text.

`rf"..."` without the interpolation of `f"..."` is not the item the
specification defines. Building `f"..."` here takes step 15 into this session,
and it needs choices the documents leave open:

- the names and signatures of the `anti.text` functions that the calls of an
  `f"..."` go to, for integers, floats, width, alignment, zero padding and the
  radixes `x`, `X`, `b` and `o`,
- where the text an `f"..."` builds lives and who frees it,
- the form of a float written with no specification,
- the types `{expr}` takes, and whether a class goes through `to_text` as it
  does for `show`,
- whether a module that writes `f"..."` has to import `anti.text`.

`x"..."`, the one prefix table and the refusal of `fr` do not depend on
`f"..."`. They were not built on their own, because that would report a
subset of the task as the task.

## Questions for Eddie

1. Should this session's task wait for step 15, or should it build `f"..."`
   with it?
2. If it builds `f"..."`, what are the answers to the five choices above, or
   may each be taken by the gap procedure as a `[provisional]` entry?
3. Or should `x"..."`, the prefix table and the `fr` refusal go in now, with
   `rf"..."` built in step 15 with `f"..."`?

## Gates

Not run, since no code changed. The docs-style checker reports nothing on
the report.
