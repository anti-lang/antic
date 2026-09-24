# Patterns

Choices made for the pattern literal, `Regex` and `anti.regex` in each pass. They describe
the inside of the compiler. `docs/decisions.md` holds what a reader of the language or a
user of the tools can observe, under "Regular expressions".

- The lexer reads `re` from the table of string prefixes as a raw form and writes
  `TOKEN_PATTERN` with the bytes between the quotes. The token stands after every earlier
  kind. The parser writes `EXPR_PATTERN`, whose `spelling` keeps the source text for the
  positions of the checker.
- `src/rt/regex.c` compiles a pattern. antic compiles the file into its own program and
  calls it from `src/antic/pattern.c`, so the check and the program read a pattern with
  one set of options. `pattern_position` of `src/antic/sema_expr.c` walks the source text
  of the literal to the byte PCRE2 names.
- `pattern_exponential` reads the pattern into a tree of its own: sets, empty matches,
  sequences, alternatives, groups and repeats. A set is 256 bits, one per byte value, and
  a character outside ASCII sets the upper half. Under `(?i)` a letter sets both cases.
  Backreferences, calls of a group and the classes of set operations match every byte.
  The walk asks each varying repeat for the first bytes of its body. It meets them with the
  bytes that can follow the repeat up to each unbounded repeat around it. It stops at an
  atomic group, a lookaround or a possessive repeat.
- The checker takes `Regex` for a type or a literal where no name of the module has it, as
  it takes `Mutex`. A call `Regex.compile(...)` has its callee rewritten to
  `<regex>.compile`, a module name in a scope of its own bound to `anti.regex`, and then
  goes through `sema_check_call` as written.
- Lowering keeps one mutable global of 8 bytes per distinct pattern of the module, named
  `pattern.<n>`, beside the literal global of its bytes. A literal lowers to the address of
  that global. `patterns_start` of `src/antic/lower.c` writes `patterns.start`, which calls
  `anti_rt_regex_literal` with each text and stores the handle.
- No call reaches `patterns.start`. `ir_is_patterns_start` therefore makes it a root of the
  removal of unused code in the optimizer and of the reach of the whole-program pass.
  `emit` writes one constructor entry for each such function it emits.
- The driver sets `regex` in its extras when a function of `anti.regex` stands in the
  program, and `native_inputs` adds `anti_rt_regex` and `pcre2-8` of `lib/<target>/`.
- `anti_rt_regex_message` keeps the message of each error number in a table of 768
  pointers, filled by compare and swap, so a `BadPattern` holds a `str` that outlives it.
- `sema_pattern_call` of `src/antic/sema_pattern.c` takes a call whose callee names
  `matches`, `find_all`, `replace` or `split` on a `str`, or `group` or `took_part` on a
  match. It checks the receiver once and marks the callee `checked`, so the path of an
  ordinary member call reads the type it found. It checks every argument itself, writes
  the callee as a name bound to the function of `anti.regex` and puts the text first. For
  a literal it puts the text of the literal third. `sema_check_call` then appends the
  defaults, `at = here` among them, and the out pointer of a failing call.
- The call keeps its literal in `call.pattern`. `sema_check_call` gives the result the
  match of that literal, and the loop of `for` gives its variable the same when it walks
  such a call.
- `types_match` makes one `Match` and one `?Match` per literal and one pair without. Each
  pair points at its twin, so `types_with_none` and `types_without_none` take the twin,
  and `type_is_nullable` holds for `?Match`. `sema_require` lets a match of a literal
  reach a plain match and a match reach `?Match`.
- `sema_check_test` checks the operand of `if`, `while`, `&&`, `||` and `!`. It marks a
  call of `matches` as `tested` first, and writes a match it finds as `e != none`, which
  `sema_proved_names` reads as it reads a pointer's test. `sema_match_field` writes `m.1`
  and `m.year` as the call `m.group(1)` and `m.group("year")` over the checked base.
- `if let m = e` reaches the `switch` of `if let` without a case. When `e` is a match,
  `sema_if_let_match` writes the statement as a block of a `let` and an `if`, both
  checked, with the symbol of the `let` narrowed in the first block.
- Lowering tests the first word of a match with `lower_none_in_first_word`, as it tests a
  function with its context: the comparison with `none`, `let ... else` and `??`. `none`
  of a match writes zero into every field.
- `lower_call` reads the out place of a failing call before it lowers the arguments. A
  failing call among them sets the out place of its own slot.
- `src/rt/patterns.c` holds the walk of every method, the groups found again, the
  templates and the stops. `anti_rt_regex_piece` of `src/rt/regex.c` reads a template for
  the runtime and for the checker alike.
- `check_pattern` of `src/antic/sema_expr.c` takes the expected type. A literal checked
  against a `ByteRegex` compiles in byte mode and has that type, and every other one is
  a `Regex`. The type of the checked literal carries the mode from then on.
  `types_match` gives a `ByteMatch` for a literal whose type is a `ByteRegex`.
  `lower_pattern` keys its globals by the text and the mode, and calls
  `anti_rt_regex_literal_bytes` for a byte literal.
- `anti_rt_regex_compile_bytes` of `src/rt/regex.c` writes the pattern PCRE2 compiles.
  A map gives for each byte it writes the byte of the written pattern. An error of
  PCRE2 goes back through the map, so its position is that of the source. A class is
  walked three times. The first finds its end, its wide characters and a refusal, the
  second writes its ASCII members and the third the wide ones. A class without its `]` goes to PCRE2 as it
  stands.
- `types_is_regex` holds for both pattern types and `types_is_match` for all four forms
  of a match, so the descriptor, the header, the worker and the narrowing treat them
  alike. `types_is_byte_regex` and `types_is_byte_match` tell the byte forms apart, and
  `sema_require` keeps a `Match` and a `ByteMatch` apart.
- `method_call` of `src/antic/sema_pattern.c` serves `str` and `[]byte`, and puts
  `bytes_` before the name of the function for bytes. `patch_call` writes every argument
  that `patch` leaves out as a literal 0, resolves a literal `into` to a group number and
  measures the fit with `pattern_least_bytes`, which counts on the tree of
  `pattern_exponential` with the number of each capturing group. `convert_call` writes
  `to_bytes` and `to_text` as calls of `anti.text`.
- The runtime searches bytes with the functions of text. The functions whose names end
  in `_bytes` stand under names of their own because `anti.regex` declares each external
  function with one signature. `anti_rt_regex_patch` and `anti_rt_bytes_patch` of
  `src/rt/patterns.c` give the count of places, or -1, -2 and -3 for the match limit, a
  span that does not fit and a missing group.
- The tests are `programs/regex_compile.anti`, `programs/pattern_literals.anti` in both
  modes, `programs/pattern_match.anti`, `programs/pattern_walk.anti`,
  `programs/pattern_replace.anti` in both modes, `programs/failing_nested.anti`,
  `traps/pattern_limit.anti`, `programs/pattern_bytes.anti` and
  `programs/pattern_patch.anti` in both modes, and the listings `pattern_malformed`,
  `pattern_exponential`, `pattern_import`, `pattern_level`, `pattern_methods` and
  `pattern_bytes` of `tests/errors/`.
