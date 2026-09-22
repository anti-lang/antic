# The answers on qualified bodies and the two open bugs

The four answers to `docs/reports/2026-09-22-qualified-bodies.md` are done. The development
Mac passes 623 of 623 tests, and the ASan and UBSan builds pass 622 each, without
`no_paths`. Each commit that changed code ran the full suite first, and the push ran both
sanitizer suites. The VMs did not run.

## What was done

1. Answer 2, `b08f132`. The nearest body wins. `types_interface_member` walks the chain
   level by level, and a plain body below replaces an inherited qualified one in every
   table of its name. The checker refused such a body as filling no abstract function,
   because it looked only at the interfaces of the class itself. It now looks at those of
   the whole chain. `programs/qualified_chain.anti` takes three levels through every
   interface pointer and sub-object.
2. Answer 1 and the second half of answer 3, `e8bc058`. A single qualified body is not
   ambiguous. A call on the class becomes a call on the sub-object whose table the body
   fills, and so does a bound function. `T.f` of such a body is a function of the module
   that names it, `T.sub.f.reach`, which calls through the table. A plain body beside one
   qualified by a base takes the same path, and the entry in the decisions says so without
   its tag. `programs/qualified_single.anti`, a `Cube` in `qualified_bodies.anti` and
   `modules/shared` each test a class below that replaces the body.
3. The first half of answer 3, `1c49658`. The entry on `T.Q.f` lost its tag.
4. The first bug, `0fdd49d`. `self.super.area()`, a call of `Shape.area` and the value
   `Shape.area` are refused with ``` `area` is abstract in `Shape` and has no body to
   call ```. `errors/abstract_call.anti` holds them.
5. The second bug, `69d7cfe`. The section of `Square` declares `Shape_move` with a
   `Shape *self`, the symbol the library holds. `clib_classes` pins the header and calls
   the inherited move of `Tile` from C, compiled as C11 and as C++17 and linked.

## Found and fixed

- A bound function promoted through a field before it looked at the chain's own
  functions. `s.area` on a class whose base and interface both declare `area` was refused
  as provided by both. A bound function of a `final` class now takes the address of the
  body, as its DESIGN comment says, which changed the assembly of `object_model.anti`.
- `points.Crate.size` from another module went straight to the body. A class read from a
  library file has no item that owns its members, so `types_member_level` finds the level
  on the chain.
- The header declared a prototype for an abstract entry, `Ink_colour`, which has no
  symbol. It is the defect of the second bug in another form, so it is fixed in the same
  commit. That choice is provisional.

## Provisional decisions for review

- The header gives an abstract entry no prototype, and its comment stands above the
  wrapper through the table.

`grep -c '\[provisional\]' docs/decisions.md` gives 3 and not 2, because of that entry.

## Questions

- Every derived section of the header now repeats the declaration of each inherited
  function. C and C++ accept it. Keep the repetition, or leave the prototype to the
  section of the declaring class?
