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
- The tests are `programs/regex_compile.anti`, `programs/pattern_literals.anti` in both
  modes, and the listings `pattern_malformed`, `pattern_exponential`, `pattern_import` and
  `pattern_level` of `tests/errors/`.
