# Decisions of fix step 20, the back end of antic

A later step folds these into `docs/decisions.md`.

- [provisional] The checked allocator of the back end is `ir_alloc`,
  `ir_resize`, `ir_grow`, `ir_product` and `ir_out_of_memory` in
  `src/antic/ir.c`. Every file of the back end includes `ir.h`, and a new
  source file would change `tools/sources.cmake`, which lies outside the
  step. The front end keeps its own copies until a step that crosses the
  boundary moves them.
- [provisional] `src/antic/attributes.h` is the file of antic that holds a
  compiler extension, the format attribute, as `ATTRIBUTE_PRINTF`. It is a
  header alone, so the source list does not change. `diagnostic.h` and
  `antl.c` still write the attribute out.
- [provisional] A message that a fixed buffer cuts ends in three dots,
  which `ir_vformat` writes.
- [provisional] The verifier refuses a call that passes an aggregate among
  the variadic arguments. The checker never writes one, and the back ends
  look an aggregate up among the parameters.
- [provisional] The COFF join refuses an offset or a length past 4 GiB with
  an error instead of cutting it to 32 bits.
- [provisional] `mach_add` and the `next` string of a link command abort
  with a message past `MACH_MAX_OPERANDS` and `LINK_MAX_STRINGS`. Every
  caller stays below them, so the state cannot be reached, which rule 13
  allows.
- [provisional] `lower_module` keeps its `bool` result and its `diags`
  parameter. It reports nothing and returns true, since the flag that
  decided the result was never set. Removing both changes `driver.c` and
  every unit test that calls it.
