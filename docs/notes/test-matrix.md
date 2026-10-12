# The test matrix

This note holds the lists the matrix steps of the test run work from. It names
every construct of the language, every place a construct can stand and every
kind of value the compiler treats apart. The matrix then crosses them, area by
area. A cell names a source under `tests/` that holds the construct in that
place, or it is open. The last sections describe the two guards of the
diagnostics and the options.

The lists were read on 2026-10-12 from the code of antic 0.2.0. Decision 28 of
"Decisions of 2026-10-11" in `docs/decisions.md` gives the matrix its
directory, `tests/matrix/`. A matrix step takes one area and writes the tests
of its open cells. It names each test in its cell in the same commit.

## How the lists were read

The constructs come from the sources of the front end:

- `src/antic/lexer.c` holds the table `kinds`, with every keyword and every
  symbol, the list `reserved` and the table `string_prefixes`.
- `src/antic/parser.c`, `parser_item.c`, `parser_decl.c`, `parser_stmt.c`,
  `parser_expr.c`, `parser_type.c` and `parser_clause.c` build the tree.
- `src/antic/ast.h` names the node kinds, and each list below says which
  parser file builds a kind.
- `src/antic/pattern.c` reads a pattern literal for the safety check
  `exponential-pattern`.
- `src/antic/types.h` and the `sema_*.c` files give the kinds of value.

The cells come from the 710 `.anti` files under `tests/`:

- `antic --tests --dump-ast <file>` prints the tree of 694 of them. The lexer
  or the parser refuses the other 16, which are sources of `tests/errors/`.
- `antic --tests --runtime <dir> --dump-types <file>` prints the checked tree
  of 513, with the type of every expression. The kinds are read from these
  alone.
- Neither dump shows the functions of a `tests` or a `fixtures` block. The
  body of each block was therefore dumped as a file of its own.
- A node stands in a context when one of its ancestors in the tree is that
  context. "The contexts" below says which node each context is.
- A few forms leave no node in the dump, `synchronized class` and `own`
  among them. Their rows were read from the source text with a pattern, and
  such a row has one cell.

A held cell names the first source that holds the construct there. The order
is `matrix/`, `programs/`, `std/`, `modules/`, `traps/` and the other
directories, with `errors/` last, and then the name. A name is the path under
`tests/` without `.anti`.

Three limits of this reading matter to a matrix step:

- A held cell says that a source holds the construct in that place. It does
  not say what the test checks. Read the source before counting on it.
- A cell that names a source of `errors/` is held by a program antic
  refuses. The construct may stand beside the refusal and never run.
- A kind is held when the construct or one of its operands has a type of that
  kind in a checked tree. The 181 sources without a checked tree add no kind.

## The constructs

### Keywords and symbols

`src/antic/lexer.c` holds 100 keywords in `kinds`:

`as` `break` `const` `continue` `do` `else` `export` `extern` `fn` `if`
`import` `let` `pub` `return` `struct` `while` `union` `true` `false` `none`
`alloc` `free` `size_of` `bool` `byte` `char` `f16` `f32` `f64` `float` `i8`
`i16` `i32` `i64` `int` `str` `u8` `u16` `u32` `u64` `uint` `c_char`
`c_double` `c_float` `c_int` `c_longlong` `c_short` `c_size_t` `c_uchar`
`c_uint` `c_ulonglong` `c_ushort` `c_long` `c_ulong` `c_wchar` `worker`
`parallel` `self` `abstract` `concrete` `enum` `use` `inherits` `class`
`super` `is` `dup` `delete` `destroy` `static` `assert` `switch` `for` `defer`
`implements` `singleton` `internal` `protected` `catch` `try` `yield` `tests`
`fixtures` `fail` `undo` `here` `fallthrough` `variant` `sync` `chan` `send`
`recv` `select` `provides` `atomic` `dispatch` `join` `join_all` `constraint`
`type`

It reserves one more word, `thread`, which is no keyword and no name.

The 64 symbols of `kinds`:

`(` `)` `{` `}` `[` `]` `,` `;` `:` `::` `.` `..` `...` `->` `=>` `+` `-` `*`
`/` `%` `&` `|` `^` `~` `!` `?` `?*` `??` `?.` `<<` `>>` `&&` `||` `==` `!=`
`<` `<=` `>` `>=` `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` `+%`
`-%` `*%` `<<%` `+|` `-|` `*|` `+%=` `-%=` `*%=` `<<%=` `+|=` `-|=` `*|=`

The literals of the lexer:

- Integers in decimal and with `0x`, floats, and characters in `'...'`.
- Strings by the table `string_prefixes`: `"..."`, `r"..."`, `b"..."`,
  `br"..."`, `f"..."`, `rf"..."`, `x"..."` and `re"..."`. Every form but
  `x"..."` takes `#` delimiters. `fr"` is refused with a message of its own.
- The comments, the doc comment `///`, the module doc comment `//!` and the
  notes `//#` and `//#!`, each in a line form and a block form.

### Contextual words

A contextual word is a name everywhere but in the one place a parser file
asks for it with `parser_is_word`:

| File | Words |
|---|---|
| `parser.c` | `framework`, `link`, `linux` |
| `parser_clause.c` | `allow`, `unchecked` |
| `parser_decl.c` | `allow`, `by`, `concurrent`, `final`, `guarded`, `keep`, `lent`, `may`, `never`, `operator`, `own`, `packed`, `simd`, `trace` |
| `parser_expr.c` | `by`, `in`, `snapshot` |
| `parser_item.c` | `align`, `compatible`, `concurrent`, `final`, `inject`, `may`, `mutable`, `operator`, `own`, `packed`, `simd`, `synchronized`, `trace`, `transient` |
| `parser_stmt.c` | `by`, `fatal`, `in` |
| `parser_type.c` | `lent`, `may`, `own` |

### Items

`parser.c` reads what stands once per file, and `parser_item.c` with
`parser_decl.c` the items. The kinds are `enum item_kind` of `ast.h`.

| Form | Node kind | Built in |
|---|---|---|
| `//!` and `//#!` at the top of the file | the doc of the module | `parser.c` |
| `allow(...)` or `unchecked(...)` at the top of the file | `CLAUSE_FILE` | `parser.c` |
| `import a.b;`, `import a.b as c;`, `import a.b.{x, y};` | an import | `parser.c` |
| `provides Interface as Class;` | a provides line | `parser.c` |
| `link framework "Name";`, `link linux "Name";` | a link line | `parser.c` |
| `tests { }` and `fixtures { }` | `BLOCK_TESTS`, `BLOCK_FIXTURES` | `parser.c` |
| `fn f<T>(a: A) -> R may fail { }` | `ITEM_FN` | `parser_item.c`, `parser_decl.c` |
| `worker fn`, `operator fn`, `trace fn` | `ITEM_FN` with a mark | `parser_item.c`, `parser_decl.c` |
| `extern fn f(a: A, ...) -> R;` | `ITEM_EXTERN_FN` | `parser_item.c` |
| `struct S<T> { }`, `packed struct`, `simd struct`, `align(n)` | `ITEM_STRUCT` | `parser_item.c` |
| `union U { }` | `ITEM_UNION` | `parser_item.c` |
| `enum E: u8 { A, B = 4 }` | `ITEM_ENUM` | `parser_item.c` |
| `variant V<T> { A { x: T }, B }` | `ITEM_VARIANT` | `parser_item.c` |
| `class C<T> inherits B { }` | `ITEM_CLASS` | `parser_item.c` |
| `abstract`, `singleton`, `final`, `synchronized`, `concurrent`, `trace` before `class` | `ITEM_CLASS` with a mark | `parser_item.c` |
| `const N: T = e;` | `ITEM_CONST` | `parser_item.c` |
| `constraint Ordered = eq + lt;` | `ITEM_CONSTRAINT` | `parser_item.c` |
| `type Name<T> = ...;` | `ITEM_TYPE` | `parser_item.c` |
| `pub`, `internal`, `export` before an item | `enum visibility`, the export mark | `parser_item.c` |

The members of a class body, from `parser_item.c` and `parser_decl.c`:

| Form | Node kind |
|---|---|
| field `x: T = e`, with `pub` or `protected` | `FIELD_PLAIN` |
| field with `use` | `FIELD_USE` |
| field with `implements` | `FIELD_IMPL` |
| field marks `inject`, `inject final`, `transient`, `atomic`, `own`, `mutable`, `static atomic` | marks of a field |
| bitfield `x: u8 : 3` | a field with bits |
| `guarded by m`, `allow(...)`, `unchecked(...)` after the type of a field | `CLAUSE_FIELD` |
| `fn f(self, a: A)`, a static `fn f(a: A)` | `ITEM_FN`, `FN_PLAIN` |
| `abstract fn`, `concrete fn`, `concrete fn Base::f` | `FN_ABSTRACT`, `FN_CONCRETE` |
| `final fn`, `static fn`, `operator fn`, `trace fn` | `ITEM_FN` with a mark |
| `const N: T = e;` | `ITEM_CONST` |
| `compatible 1.1;` | the compatible line |
| a nested `struct`, `enum` or `class` | the item, named `Outer.Inner` |

The parts of a signature, from `parser_decl.c`:

- A parameter `name: T`, with `self` first in a function of a class.
- The marks `keep`, `concurrent`, `own` and `lent` on a parameter.
- A default value `name: T = e` and the variadic `...` of an `extern fn`.
- The result `-> T`, `-> lent *T` and `-> never`, and `may fail` after it.
- The type parameters `<T>`, `<N: int>` and `<T: A + B>`.

### Statements

`parser_stmt.c` builds every kind of `enum stmt_kind`:

| Form | Node kind |
|---|---|
| `let x = e;`, `let x: T = e;`, `let x: atomic T = e;` | `STMT_LET` |
| `let (a, b) = e;` and the flags form `let (r, flags) = e;` | `STMT_LET` with names |
| `let x = e else { }` | `STMT_LET` with a block |
| `const N = e;` | `STMT_CONST` |
| an expression and `;` | `STMT_EXPR` |
| `=` and the 17 compound assignments | `STMT_ASSIGN` |
| `if c { } else if d { } else { }` | `STMT_IF` |
| `if let Case c = e { } else { }` | `STMT_SWITCH` marked `if_let` |
| `while c { }` | `STMT_WHILE` |
| `do { } while c` | `STMT_DO_WHILE` |
| `for i in lo..hi { }`, with `by k` | `STMT_FOR` |
| `for x in e { }`, `for x in &e { }`, `for i, x in e { }` | `STMT_FOR` |
| `switch e { A => s, Case c => { }, else => { } }` | `STMT_SWITCH` |
| `fallthrough;` | `STMT_FALLTHROUGH` |
| `sync m { }`, `sync a, b { }` | `STMT_SYNC` |
| `select { c x => { } }` | `STMT_SELECT` |
| `defer s;` | `STMT_DEFER` |
| `undo s;` | `STMT_UNDO` |
| `fail e;`, `fail "text";` | `STMT_FAIL` |
| `assert c;`, `assert c, "text";` | `STMT_ASSERT` |
| `break;`, `continue;` | `STMT_BREAK`, `STMT_CONTINUE` |
| `return;`, `return e;` | `STMT_RETURN` |
| `yield e;` | `STMT_YIELD` |
| `try { } catch e { }` | `STMT_TRY` |
| `{ }` | `STMT_BLOCK` |

A call takes a handler, from `enum handler_kind`:

| Form | Node kind | Built in |
|---|---|---|
| `f() catch e { }`, `f() catch { }` | `HANDLE_BLOCK` | `parser_stmt.c` |
| `f() catch fatal` | `HANDLE_FATAL` | `parser_stmt.c` |
| `f() catch none` | `HANDLE_BLOCK`, with a body the parser writes | `parser_stmt.c` |
| `try f()` | `HANDLE_TRY` | `parser_expr.c` |

### Expressions

`parser_expr.c` builds these kinds of `enum expr_kind`:

| Form | Node kind |
|---|---|
| `42`, `0xFF`, `1.5`, `'a'` | `EXPR_INT`, `EXPR_FLOAT`, `EXPR_CHAR` |
| `"text"`, `r"text"` | `EXPR_STRING` |
| `b"text"`, `br"text"`, `x"00 ff"` | `EXPR_BYTES` |
| `f"{x:>8}"`, `rf"..."` | `EXPR_FORMAT` |
| `re"..."` | `EXPR_PATTERN` |
| `true`, `false`, `none`, `here` | `EXPR_BOOL`, `EXPR_NONE`, `EXPR_HERE` |
| a name, `self`, `self.super` | `EXPR_NAME` |
| a prefix operator | `EXPR_UNARY` |
| a binary operator | `EXPR_BINARY` |
| `e as T` | `EXPR_CAST` |
| `x in lo..hi` | `EXPR_IN` |
| `f(a, b)`, `f<T>(a)` | `EXPR_CALL` |
| `e[i]` | `EXPR_INDEX` |
| `e[lo..hi]` | `EXPR_SLICE` |
| `e.x`, `e?.x`, `t.0` | `EXPR_FIELD` |
| `S { x: 1 }`, `V.Case { x: 1 }`, `S<T> { }` | `EXPR_STRUCT_LIT` |
| `(a, b)` | `EXPR_TUPLE` |
| `[]T { ptr: p, len: n }` | `EXPR_SLICE_LIT` |
| `[a, b]` | `EXPR_ARRAY_LIT` |
| `[v; n]` | `EXPR_ARRAY_REPEAT` |
| `alloc(S { })`, `alloc(T, n)`, `alloc(C(args))` | `EXPR_ALLOC` |
| `free(p)` | `EXPR_FREE` |
| `dup(e)`, `delete(p)`, `destroy(p)`, each with `from` | `EXPR_OBJECT` |
| `size_of(T)` | `EXPR_SIZE_OF` |
| `parallel xs -> f(a)`, `parallel xs by n -> f(a)` | `EXPR_PARALLEL` |
| `dispatch p -> f`, `dispatch p -> f(a)` | `EXPR_DISPATCH` |
| `join(j)`, `join_all(js)` | `EXPR_JOIN` |
| `chan T(n)`, `send(c, v)`, `recv(c)` | `EXPR_SYNC_OP` |
| `fn(x) { }`, `fn(x: T) -> R { }`, `snapshot fn` | `EXPR_FN` |

The checker makes five more kinds of a call or a field, which no parser file
builds, and three operations of `EXPR_SYNC_OP`. `src/antic/ast.h` and `src/antic/ast_dump.c` name them:

| Form | Node kind |
|---|---|
| `a.load()`, `store`, `swap`, `add`, `sub`, `and`, `or`, `compare_swap` of an atomic | `EXPR_ATOMIC` |
| `Mutex.new()`, `m.destroy()`, `close(c)`, `delete` of a channel | `EXPR_SYNC_OP` |
| `splat`, `load`, `store`, `shuffle`, `sum`, `min`, `max`, `dot`, `select`, `any`, `all` of a simd struct | `EXPR_SIMD` |
| a `?T` read as its value | `EXPR_OPTIONAL` |
| `it.to_slice()` of an iterator | `EXPR_COLLECT` |
| the descriptor of a class, as `*byte` | `EXPR_DESCRIPTOR` |

### Operators

`parser_precedence` of `parser_expr.c` gives each binary operator its level,
from 1, the loosest, to 11:

| Level | Operators |
|---|---|
| 1 | `\|\|` |
| 2 | `&&` |
| 3 | `\|` |
| 4 | `^` |
| 5 | `&` |
| 6 | `==`, `!=` |
| 7 | `<`, `<=`, `>`, `>=`, and `x in lo..hi` |
| 8 | `??` |
| 9 | `<<`, `<<%`, `>>` |
| 10 | `+`, `+%`, `+\|`, `-`, `-%`, `-\|` |
| 11 | `*`, `*%`, `*\|`, `/`, `%` |

The other operators of `parser_expr.c`:

- The prefix operators `-`, `!`, `~`, `*` and `&`, and `try`.
- `as` and `is`, which bind tighter than every binary operator.
- The postfix forms: a call, an index, a slice, `.`, `?.` and a handler.
- `mul_high(a, b)`, which the dump prints as an operator.

`parser_is_assign_op` of `parser_stmt.c` takes the 18 assignments: `=`, `+=`,
`-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`, `+%=`, `-%=`, `*%=`,
`<<%=`, `+|=`, `-|=` and `*|=`.

### Patterns

Anti has two things called a pattern. The first is what an arm of a `switch`
or an `if let` matches, from `parser_stmt.c`:

- A constant and a string, one value per arm.
- A case of a variant, `Circle`, and a case with a binding, `Circle c`.
- `else`, which takes every other value.
- The tuple pattern of `let (a, b) = e;` and of `for i, x in e`.

The second is the text of a pattern literal `re"..."`. PCRE2 compiles it, and
`src/antic/pattern.c` reads it again into six node kinds for the safety
check. The forms that file reads:

| Form | Node kind |
|---|---|
| a character, `.`, a class `[a-z]`, `[^a]`, a POSIX class `[[:alpha:]]` | `NODE_SET` |
| the escapes `\d \w \s \h \v`, `\p{..}`, `\x`, `\o`, `\0`, `\n \r \t \f \e \a`, `\c` | `NODE_SET` |
| the anchors `^ $ \b \B \A \Z \z \G \K` | `NODE_EMPTY` |
| a sequence | `NODE_CAT` |
| `a\|b` | `NODE_ALT` |
| `( )`, `(?: )`, `(?<name> )`, a lookaround, the flags `(?i)` | `NODE_GROUP` |
| `*`, `+`, `?`, `{n}`, `{n,m}`, each lazy with `?` or possessive with `+` | `NODE_REPEAT` |
| `\Q...\E`, a back reference `\g`, `\k`, the extended mode with `#` | read past |

### Types

`parser_type.c` builds the kinds of `enum type_expr_kind`:

| Form | Node kind |
|---|---|
| `int`, `u8`, `f32`, `str`, `c_int` and the other built-in names | `TYPEX_BUILTIN` |
| `Name`, `module.Name`, `Name<A, B>` | `TYPEX_NAMED` |
| `*T`, `?*T` | `TYPEX_POINTER` |
| `[N]T` | `TYPEX_ARRAY` |
| `[]T` | `TYPEX_SLICE` |
| `fn(A) -> R`, `fn(A) -> R may fail`, `?fn(A)`, `keep fn`, `concurrent fn` | `TYPEX_FN` |
| `(A, B)` | `TYPEX_TUPLE` |
| `chan T` | `TYPEX_CHAN` |
| a constant as a type argument, `Ring<int, 8>` | `TYPEX_CONST` |
| `?T` | `TYPEX_OPTIONAL` |

The marks `own` and `lent` stand before a type, and `atomic` before the type
of a field or a local.

### Clauses

`parser_clause.c` reads `allow(name, "reason")` and
`unchecked(name, "reason")`. `enum clause_level` of `ast.h` names the four
places: before a statement, last in the header of a declaration, after the
type of a field and at the top of the file.

## The contexts

A context is a place a construct can stand. The matrix uses 19 for a
statement or an expression:

| Context | The node that makes it |
|---|---|
| module level | no function around it: a constant, a default of a field or of a parameter |
| function | a `fn` that stands in no class |
| method | a `fn` in a class body |
| loop body | a `for`, a `while` or a `do` |
| switch arm | an arm of a `switch` |
| `if let` | the statement, its arm and its `else` |
| `defer` | the statement of a `defer` |
| `undo` | the statement of an `undo` |
| catch handler | the block of `catch e { }` or of `try { } catch` |
| closure | an anonymous function, `snapshot fn` included |
| worker | a `worker fn` |
| `dispatch` or `parallel` | the call and the arguments of either |
| `sync` block | the block of a `sync` |
| `select` arm | an arm of a `select` |
| `tests` block | a function of a `tests` block |
| `fixtures` block | `setup`, `teardown` or a function of a `fixtures` block |
| generic body | an item with type parameters |
| nested type | a type declared in a class body |
| library module | a source that another source of `tests/` imports |

A statement has no cell at module level. An item has seven contexts, and each
row holds the ones that apply to it: module level, class body, nested type,
generic, `tests` block, `fixtures` block and library module. A type form
stands in a position instead: a `let`, a parameter, a result, a field, an
`as`, an `alloc`, a `size_of`, a `const`, a slice literal, a call, a `type`
alias, a case of a variant and a library module.

The library module is the nearest the dumps come to "a type in a library
file". A matrix step that takes it compiles the module with `-c` and uses
the `.antl`, as `tests/modules/` does.

## The kinds of value

`enum type_kind` of `src/antic/types.h` names the kinds the checker holds
apart, and marks of `struct type` and of a symbol split them further:

| Kind | Read from |
|---|---|
| `i8`, `i16`, `i32`, `i64` and `int` | `TYPE_I8` to `TYPE_I64`, `types_is_signed` |
| `u8`, `u16`, `u32`, `u64`, `uint` and `byte` | `TYPE_U8` to `TYPE_U64`, `types_is_integer` |
| the C types, `c_int` to `c_wchar` | `TYPE_CLONG`, `TYPE_CULONG`, `TYPE_CWCHAR`, and the width of the others |
| `f32` and `f64` | `TYPE_F32`, `TYPE_F64`, `types_is_float` |
| `f16` | `TYPE_F16`, storage alone |
| `bool` and `char` | `TYPE_BOOL`, `TYPE_CHAR` |
| `str` | `TYPE_STR` |
| slice `[]T` | `TYPE_SLICE` |
| fixed array `[N]T` | `TYPE_ARRAY` |
| pointer `*T` and nullable pointer `?*T` | `TYPE_POINTER` with `nullable` |
| optional value `?T` | `TYPE_OPTIONAL` |
| owning value | `owned` of a field and of a parameter, `sema_value.c` |
| lent value | `lent`, `types_is_lent`, `types_holds_lent` |
| may-fail result | `may_fail` and `has_out` of a function type |
| a function that never returns | `never` of a function type |
| atomic | `atomic` of a field and of a local, `sema_thread.c` |
| simd struct and its mask | `simd`, `types_is_simd`, `types_is_mask`, `sema_simd.c` |
| struct | `TYPE_STRUCT` |
| packed struct | `packed` of a struct |
| union | `is_union`, `types_holds_union` |
| enum | `TYPE_ENUM` |
| variant | `TYPE_VARIANT` |
| tuple | `TYPE_TUPLE` |
| class | `TYPE_CLASS` |
| synchronized and concurrent class | `enum thread_safety` |
| interface | a class with `has_abstract` |
| generic parameter | `TYPE_PARAM` |
| function value | `TYPE_FN` |
| closure | `context` of a function type, with `keep` and `concurrent` |
| channel | `types_is_chan` |
| `Mutex`, `Regex`, `ByteRegex`, `Match`, `Job`, `Flags` | `types_is_mutex` and its neighbours |

The kinds tables of the matrix use 38 of them, the ones a checked
tree names in the type of a node:

- i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte
- f32, f64, f16, bool, char, str
- slice, fixed array, pointer, nullable pointer, optional value, lent value, may-fail function value, function value, tuple
- channel, struct, packed struct, simd struct, union, enum
- variant, class, synchronized class, concurrent class, interface, generic parameter

A checked tree does not tell the others apart, so each is a row of the
matrix instead of a kind:

- An owning value is the row "`own` field or parameter".
- An atomic is the rows "`atomic` field or local" and "atomic `load`" to
  "atomic `compare_swap`".
- A closure is the row "anonymous function" and the context "closure".
- A C type prints as the integer of its width, so `c_int` counts as `i32`.
- `f16` prints as `f32` wherever a read gives its value.

## The matrix

Each area has a table of contexts and, where kinds apply, a table of kinds.
A cell of the second column names a context or a kind and the first source
that holds it. The third column lists the open cells. "none" in the second
column means that no source holds the row at all.

| Area | Rows | Cells held | Cells open | Kinds held | Kinds open |
|---|---|---|---|---|---|
| Flow | 27 | 454 | 1 | 91 | 0 |
| Values | 40 | 297 | 456 | 423 | 617 |
| Types | 32 | 131 | 83 | 0 | 0 |
| Ownership | 18 | 79 | 170 | 45 | 80 |
| Errors | 15 | 49 | 124 | 11 | 30 |
| Threads | 29 | 71 | 335 | 52 | 144 |
| Classes | 26 | 47 | 9 | 0 | 0 |
| Modules | 19 | 29 | 1 | 0 | 0 |
| All | 206 | 1157 | 1179 | 622 | 871 |

### Flow

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `if` | function `programs/alias_access`, method `programs/construct_args`, loop body `programs/args`, switch arm `programs/generic_variants`, `if let` `matrix/flow_if`, `defer` `matrix/flow_if`, `undo` `matrix/flow_if`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/parallel_join`, `dispatch` or `parallel` `matrix/flow_if`, `sync` block `programs/sync_exits`, `select` arm `matrix/flow_if`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/caller_params`, nested type `matrix/flow_if`, library module `modules/chain/com/example/animal` | none |
| `else` of an `if` | function `programs/argument_order`, method `programs/lock_free_queue`, loop body `programs/lock_free_queue`, switch arm `matrix/flow_else`, `if let` `matrix/flow_else`, `defer` `matrix/flow_else`, `undo` `matrix/flow_else`, catch handler `matrix/flow_else`, closure `matrix/flow_else`, worker `std/spsc_ring_threads`, `dispatch` or `parallel` `matrix/flow_else`, `sync` block `matrix/flow_else`, `select` arm `matrix/flow_else`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_else`, nested type `matrix/flow_else`, library module `modules/flow/com/example/flow` | none |
| `while` | function `programs/alloc_owning`, method `programs/concurrent`, loop body `programs/handler_exit_temps`, switch arm `matrix/flow_while`, `if let` `matrix/flow_while`, `defer` `matrix/flow_while`, `undo` `matrix/flow_while`, catch handler `matrix/flow_while`, closure `matrix/flow_while`, worker `programs/channel_workers`, `dispatch` or `parallel` `matrix/flow_while`, `sync` block `programs/concurrent`, `select` arm `matrix/flow_while`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_nested`, nested type `matrix/flow_while`, library module `modules/defaults/com/example/greeter` | none |
| `do ... while` | function `matrix/flow_do_while`, method `matrix/flow_do_while`, loop body `matrix/flow_do_while`, switch arm `matrix/flow_do_while`, `if let` `matrix/flow_do_while`, `defer` `matrix/flow_do_while`, `undo` `matrix/flow_do_while`, catch handler `matrix/flow_do_while`, closure `matrix/flow_do_while`, worker `matrix/flow_do_while`, `dispatch` or `parallel` `matrix/flow_do_while`, `sync` block `matrix/flow_do_while`, `select` arm `matrix/flow_do_while`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_do_while`, nested type `matrix/flow_do_while`, library module `modules/flow/com/example/flow` | none |
| `for i in lo..hi` | function `programs/arith_facts`, method `programs/generic_copies`, loop body `programs/generic_nested`, switch arm `matrix/flow_for_range`, `if let` `matrix/flow_for_range`, `defer` `matrix/flow_for_range`, `undo` `matrix/flow_for_range`, catch handler `matrix/flow_for_range`, closure `matrix/flow_for_range`, worker `programs/channel_workers`, `dispatch` or `parallel` `matrix/flow_for_range`, `sync` block `matrix/flow_for_range`, `select` arm `matrix/flow_for_range`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_constants`, nested type `matrix/flow_for_range`, library module `modules/closures/com/example/walk` | none |
| `for x in e` | function `programs/channels`, method `matrix/flow_for_each`, loop body `programs/iteration`, switch arm `matrix/flow_for_each`, `if let` `matrix/flow_for_each`, `defer` `matrix/flow_for_each`, `undo` `matrix/flow_for_each`, catch handler `matrix/flow_for_each`, closure `std/collection_grid`, worker `programs/channels`, `dispatch` or `parallel` `matrix/flow_for_each`, `sync` block `matrix/flow_for_each`, `select` arm `matrix/flow_for_each`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/for_patterns`, nested type `matrix/flow_for_each`, library module `modules/closures/com/example/walk` | none |
| `for x in &e` | function `programs/for_loop`, method `matrix/flow_for_lent`, loop body `matrix/flow_for_lent`, switch arm `matrix/flow_for_lent`, `if let` `matrix/flow_for_lent`, `defer` `matrix/flow_for_lent`, `undo` `matrix/flow_for_lent`, catch handler `matrix/flow_for_lent`, closure `matrix/flow_for_lent`, worker `std/shared`, `dispatch` or `parallel` `matrix/flow_for_lent`, `sync` block `matrix/flow_for_lent`, `select` arm `matrix/flow_for_lent`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/walk_generic`, nested type `matrix/flow_for_lent`, library module `modules/generics/com/example/gen` | none |
| `for i, x in e` | function `programs/for_patterns`, method `matrix/flow_for_index`, loop body `matrix/flow_for_index`, switch arm `matrix/flow_for_index`, `if let` `matrix/flow_for_index`, `defer` `matrix/flow_for_index`, `undo` `matrix/flow_for_index`, catch handler `matrix/flow_for_index`, closure `matrix/flow_for_index`, worker `matrix/flow_for_index`, `dispatch` or `parallel` `matrix/flow_for_index`, `sync` block `matrix/flow_for_index`, `select` arm `matrix/flow_for_index`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_for_index`, nested type `matrix/flow_for_index`, library module `modules/flow/com/example/flow` | none |
| `break` | function `programs/by_ends`, method `programs/lock_free_queue`, loop body `programs/by_ends`, switch arm `matrix/flow_break`, `if let` `matrix/flow_break`, `defer` `matrix/flow_break`, `undo` `matrix/flow_break`, catch handler `programs/catch_exits`, closure `matrix/flow_break`, worker `std/spsc_ring_threads`, `dispatch` or `parallel` `matrix/flow_break`, `sync` block `programs/sync_exits`, `select` arm `matrix/flow_break`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_break`, nested type `matrix/flow_break`, library module `modules/flow/com/example/flow` | none |
| `continue` | function `programs/arith_facts`, method `matrix/flow_continue`, loop body `programs/arith_facts`, switch arm `matrix/flow_continue`, `if let` `matrix/flow_continue`, `defer` `matrix/flow_continue`, `undo` `matrix/flow_continue`, catch handler `programs/catch_exits`, closure `matrix/flow_continue`, worker `std/concurrent_map_threads`, `dispatch` or `parallel` `matrix/flow_continue`, `sync` block `programs/sync_exits`, `select` arm `matrix/flow_continue`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_continue`, nested type `matrix/flow_continue`, library module `modules/flow/com/example/flow` | none |
| `return` of a value | function `programs/alias_access`, method `programs/atomics`, loop body `programs/args`, switch arm `programs/switch_stmt`, `if let` `programs/generic_variants`, `defer` `matrix/flow_return_value_in_defer` T001, `undo` `matrix/flow_return_value_in_undo` T001, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/closures`, `sync` block `programs/concurrent`, `select` arm `matrix/flow_return_value`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger` | `tests` block: a function of a `tests` block returns nothing, so no `return` of a value stands in one |
| `return` alone | function `programs/catch_none`, method `std/builder_take_in`, loop body `std/builder_take_in`, switch arm `matrix/flow_return_plain_in_arm`, `if let` `matrix/flow_return_plain_in_if_let`, `defer` `matrix/flow_return_plain_in_defer` T001, `undo` `matrix/flow_return_plain_in_undo` T001, catch handler `programs/pattern_walk`, closure `matrix/flow_return_plain_in_closure`, worker `matrix/flow_return_plain_in_worker`, `dispatch` or `parallel` `matrix/flow_return_plain_in_dispatch`, `sync` block `matrix/flow_return_plain_in_sync`, `select` arm `matrix/flow_return_plain_in_select`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `std/collection_parts`, nested type `matrix/flow_return_plain_in_nested`, library module `modules/flow/com/example/flow` | none |
| `switch` | function `programs/alias_unions`, method `programs/nested_types`, loop body `programs/select_two`, switch arm `matrix/flow_switch`, `if let` `matrix/flow_switch`, `defer` `matrix/flow_switch`, `undo` `matrix/flow_switch`, catch handler `matrix/flow_switch`, closure `matrix/flow_switch`, worker `programs/select_two`, `dispatch` or `parallel` `matrix/flow_switch`, `sync` block `matrix/flow_switch`, `select` arm `matrix/flow_switch`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_variants`, nested type `matrix/flow_switch`, library module `modules/variants/com/example/geo` | none |
| `else` of a `switch` | function `programs/nested_locals`, method `programs/switch_fallthrough`, loop body `programs/select_two`, switch arm `matrix/flow_switch_else`, `if let` `matrix/flow_switch_else`, `defer` `matrix/flow_switch_else`, `undo` `matrix/flow_switch_else`, catch handler `matrix/flow_switch_else`, closure `matrix/flow_switch_else`, worker `programs/select_two`, `dispatch` or `parallel` `matrix/flow_switch_else`, `sync` block `matrix/flow_switch_else`, `select` arm `matrix/flow_switch_else`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_switch_else`, nested type `matrix/flow_switch_else`, library module `modules/flow/com/example/flow` | none |
| arm that binds a case, `Circle c =>` | function `programs/alias_unions`, method `matrix/flow_arm_binds`, loop body `matrix/flow_arm_binds`, switch arm `programs/alias_unions`, `if let` `matrix/flow_arm_binds`, `defer` `matrix/flow_arm_binds`, `undo` `matrix/flow_arm_binds`, catch handler `matrix/flow_arm_binds`, closure `matrix/flow_arm_binds`, worker `matrix/flow_arm_binds`, `dispatch` or `parallel` `matrix/flow_arm_binds`, `sync` block `matrix/flow_arm_binds`, `select` arm `matrix/flow_arm_binds`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_variants`, nested type `matrix/flow_arm_binds`, library module `modules/variants/com/example/geo` | none |
| arm that is a call or an assignment | function `programs/alias_unions`, method `programs/nested_types`, loop body `programs/select_two`, switch arm `programs/alias_unions`, `if let` `matrix/flow_arm_call`, `defer` `matrix/flow_arm_call`, `undo` `matrix/flow_arm_call`, catch handler `matrix/flow_arm_call`, closure `matrix/flow_arm_call`, worker `programs/select_two`, `dispatch` or `parallel` `matrix/flow_arm_call`, `sync` block `matrix/flow_arm_call`, `select` arm `matrix/flow_arm_call`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_variants`, nested type `matrix/flow_arm_call`, library module `modules/variants/com/example/geo` | none |
| `fallthrough` | function `programs/switch_fallthrough`, method `programs/switch_fallthrough`, loop body `matrix/flow_fallthrough`, switch arm `programs/switch_fallthrough`, `if let` `matrix/flow_fallthrough`, `defer` `matrix/flow_fallthrough`, `undo` `matrix/flow_fallthrough`, catch handler `matrix/flow_fallthrough`, closure `matrix/flow_fallthrough`, worker `matrix/flow_fallthrough`, `dispatch` or `parallel` `matrix/flow_fallthrough`, `sync` block `matrix/flow_fallthrough`, `select` arm `matrix/flow_fallthrough`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_fallthrough`, nested type `matrix/flow_fallthrough`, library module `modules/flow/com/example/flow` | none |
| `if let` | function `programs/generic_variants`, method `matrix/flow_if_let`, loop body `matrix/flow_if_let`, switch arm `matrix/flow_if_let`, `if let` `programs/variants`, `defer` `matrix/flow_if_let`, `undo` `matrix/flow_if_let`, catch handler `matrix/flow_if_let`, closure `matrix/flow_if_let`, worker `matrix/flow_if_let`, `dispatch` or `parallel` `matrix/flow_if_let`, `sync` block `matrix/flow_if_let`, `select` arm `matrix/flow_if_let`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_variants`, nested type `matrix/flow_if_let`, library module `modules/generics/com/example/gen` | none |
| `else` of an `if let` | function `programs/generic_variants`, method `matrix/flow_if_let_else`, loop body `matrix/flow_if_let_else`, switch arm `matrix/flow_if_let_else`, `if let` `programs/generic_variants`, `defer` `matrix/flow_if_let_else`, `undo` `matrix/flow_if_let_else`, catch handler `matrix/flow_if_let_else`, closure `matrix/flow_if_let_else`, worker `matrix/flow_if_let_else`, `dispatch` or `parallel` `matrix/flow_if_let_else`, `sync` block `matrix/flow_if_let_else`, `select` arm `matrix/flow_if_let_else`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/generic_variants`, nested type `matrix/flow_if_let_else`, library module `modules/generics/com/example/gen` | none |
| `defer` | function `programs/closures`, method `matrix/flow_defer`, loop body `programs/defer_stmt`, switch arm `programs/switch_fallthrough`, `if let` `matrix/flow_defer`, `defer` `matrix/flow_defer`, `undo` `matrix/flow_defer`, catch handler `matrix/flow_defer`, closure `matrix/flow_defer`, worker `matrix/flow_defer`, `dispatch` or `parallel` `matrix/flow_defer`, `sync` block `programs/sync_exits`, `select` arm `matrix/flow_defer`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_defer`, nested type `matrix/flow_defer`, library module `modules/flow/com/example/flow` | none |
| `assert` | function `programs/asserts`, method `matrix/flow_assert`, loop body `matrix/flow_assert`, switch arm `matrix/flow_assert`, `if let` `matrix/flow_assert`, `defer` `matrix/flow_assert`, `undo` `matrix/flow_assert`, catch handler `matrix/flow_assert`, closure `matrix/flow_assert`, worker `matrix/flow_assert`, `dispatch` or `parallel` `matrix/flow_assert`, `sync` block `matrix/flow_assert`, `select` arm `matrix/flow_assert`, `tests` block `programs/test_blocks`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_assert`, nested type `matrix/flow_assert`, library module `modules/flow/com/example/flow` | none |
| block as a statement | function `programs/argument_order`, method `matrix/flow_block`, loop body `std/collection_tree`, switch arm `matrix/flow_block`, `if let` `matrix/flow_block`, `defer` `matrix/flow_block`, `undo` `matrix/flow_block`, catch handler `matrix/flow_block`, closure `matrix/flow_block`, worker `std/spsc_ring_threads`, `dispatch` or `parallel` `matrix/flow_block`, `sync` block `matrix/flow_block`, `select` arm `matrix/flow_block`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_block`, nested type `matrix/flow_block`, library module `modules/flow/com/example/flow` | none |
| `&&` | module level `matrix/flow_and_at_module_level`, function `programs/eq_parts`, method `programs/operators`, loop body `programs/pattern_bytes`, switch arm `matrix/flow_and`, `if let` `matrix/flow_and`, `defer` `matrix/flow_and`, `undo` `matrix/flow_and`, catch handler `matrix/flow_and`, closure `std/concurrent_map_threads`, worker `programs/sync_pairs`, `dispatch` or `parallel` `matrix/flow_and`, `sync` block `matrix/flow_and`, `select` arm `matrix/flow_and`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/eq_parts`, nested type `matrix/flow_and`, library module `modules/access/com/example/ledger` | none |
| `\|\|` | module level `matrix/flow_or_at_module_level`, function `programs/channel_workers`, method `std/sorted_order`, loop body `programs/channel_workers`, switch arm `matrix/flow_or`, `if let` `matrix/flow_or`, `defer` `matrix/flow_or`, `undo` `matrix/flow_or`, catch handler `matrix/flow_or`, closure `bench/ablate/mixed_work`, worker `std/sync_list_threads`, `dispatch` or `parallel` `matrix/flow_or`, `sync` block `matrix/flow_or`, `select` arm `matrix/flow_or`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `programs/hash_generic`, nested type `matrix/flow_or`, library module `modules/sync/com/example/queue` | none |
| `x in lo..hi` | module level `programs/in_range`, function `programs/in_range`, method `matrix/flow_in_range`, loop body `programs/in_range`, switch arm `matrix/flow_in_range`, `if let` `matrix/flow_in_range`, `defer` `matrix/flow_in_range`, `undo` `matrix/flow_in_range`, catch handler `matrix/flow_in_range`, closure `matrix/flow_in_range`, worker `matrix/flow_in_range`, `dispatch` or `parallel` `matrix/flow_in_range`, `sync` block `matrix/flow_in_range`, `select` arm `matrix/flow_in_range`, `tests` block `anti-test/com/example/flow`, `fixtures` block `anti-test/com/example/flow`, generic body `matrix/flow_in_range`, nested type `matrix/flow_in_range`, library module `modules/flow/com/example/flow` | none |
| `for ... by k` | `programs/arith_facts` | none |
| `let x = e else { }` | `programs/alias_integers` | none |

The kinds of flow:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `for i in lo..hi` | i8 `programs/arith_facts`, i16 `programs/by_ends`, int `programs/arith_facts`, u64 `programs/by_ends`, byte `programs/arith_facts`, i32 `matrix/flow_for_range_kinds`, i64 `matrix/flow_for_range_kinds`, u8 `matrix/flow_for_range_kinds`, u16 `matrix/flow_for_range_kinds`, u32 `matrix/flow_for_range_kinds`, uint `matrix/flow_for_range_kinds` | none |
| `for x in e` | slice `programs/pattern_bytes`, class `programs/parallel_classes`, struct `programs/pattern_walk`, str `programs/for_patterns`, generic parameter `programs/for_patterns`, fixed array `matrix/flow_for_each` | none |
| `for x in &e` | class `programs/walk_owned_parts`, slice `matrix/flow_for_lent_kinds`, fixed array `matrix/flow_for_lent_kinds`, struct `matrix/flow_for_lent_kinds` | none |
| `return` of a value | i8 `programs/param_facts`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/eq_compiled`, byte `programs/param_facts`, f32 `programs/classes`, bool `programs/alias_pointers`, char `std/reflect_call`, str `programs/effects_edges`, slice `programs/eq_kinds`, fixed array `programs/generic_constants`, pointer `programs/alias_access`, nullable pointer `programs/catch_exits`, optional value `programs/argument_order`, may-fail function value `programs/fn_may_fail`, function value `programs/closures`, tuple `programs/caller_params`, channel `modules/sync/com/example/queue`, struct `programs/flags`, simd struct `clib/com/example/simdlib`, union `clib/com/example/geo`, enum `dump/arith`, variant `programs/generic_copies`, class `programs/alloc_owning`, generic parameter `programs/caller_params`, i64 `matrix/flow_return_kinds`, u8 `matrix/flow_return_kinds`, uint `matrix/flow_return_kinds`, f64 `matrix/flow_return_kinds`, f16 `matrix/flow_return_f16`, lent value `matrix/flow_return_lent`, packed struct `matrix/flow_return_packed_struct`, synchronized class `matrix/flow_return_synchronized`, concurrent class `matrix/flow_return_concurrent`, interface `matrix/flow_return_interface` | none |
| `switch` | int `programs/select_two`, str `programs/switch_str`, enum `programs/arith_facts`, variant `programs/alias_unions`, i8 `matrix/flow_switch_kinds`, i16 `matrix/flow_switch_kinds`, i32 `matrix/flow_switch_kinds`, i64 `matrix/flow_switch_kinds`, u8 `matrix/flow_switch_kinds`, u16 `matrix/flow_switch_kinds`, u32 `matrix/flow_switch_kinds`, u64 `matrix/flow_switch_kinds`, uint `matrix/flow_switch_kinds`, byte `matrix/flow_switch_kinds`, char `matrix/flow_switch_on_char`, bool `matrix/flow_switch_on_bool`, generic parameter `matrix/flow_switch_on_generic` | none |
| `if let` | variant `programs/generic_variants`, optional value `matrix/flow_if_let`, nullable pointer `matrix/flow_if_let_pointer` | none |
| `x in lo..hi` | int `programs/in_range`, byte `programs/in_range`, char `programs/in_range`, i8 `matrix/flow_in_range_kinds`, i16 `matrix/flow_in_range_kinds`, i32 `matrix/flow_in_range_kinds`, i64 `matrix/flow_in_range_kinds`, u8 `matrix/flow_in_range_kinds`, u16 `matrix/flow_in_range_kinds`, u32 `matrix/flow_in_range_kinds`, u64 `matrix/flow_in_range_kinds`, uint `matrix/flow_in_range_kinds` | none |

The step `flow` of the test run filled both tables on 2026-10-12. How to
read a cell it wrote:

- `matrix/flow_<construct>` is a program of `tests/matrix/` that holds the
  construct in every place its row names it for. Each place is a function
  `in_<context>` of its own, and the program prints one line per place and
  input, so the line that differs names the cell. One program per row
  keeps the suite inside its 9 minutes, by "Repository layout" in
  `docs/decisions.md`. A cell that shows a defect gets a program of its
  own.
- `anti-test/com/example/flow` is the module of the test `anti_test_flow`.
  It holds each construct in a function of a `tests` block and in one of a
  `fixtures` block, since every build but `anti test` drops both blocks.
- `modules/flow/com/example/flow` is the library of the test
  `flow_modules`. It holds each construct in a plain function and in a
  generic one, and `client` calls both across the module boundary.
- A cell that the rules refuse names a refusal, a source with an `.err`
  file. `flow_switch_on_char`, `flow_switch_on_bool`,
  `flow_switch_on_generic` and `flow_return_f16` are refusals.
- An id after a source, as `T001`, names the finding of
  `docs/audit/tests-findings.md` that the test shows. The suite expects
  that test to fail.
- The step replaced a cell that a source of `errors/` or of `fmt/` held
  with a program, since that source never ran the construct.

A `break` and a `continue` in a `defer`, an `undo` or a closure have two
tests each. `in_defer` of `flow_break` holds a loop that stands inside the
`defer` with its `break`. `flow_break_out_of_defer` holds a `break` whose
loop stands around the `defer`, which decision 19 refuses.

The tests of the step beside the cells, where two rules of the checker or
of lowering meet:

| Test | What it holds | Finding |
|---|---|---|
| `flow_break_out_of_defer`, `flow_continue_out_of_defer`, `flow_break_out_of_undo`, `flow_continue_out_of_undo` | decision 19: a `break` or a `continue` that leaves a `defer` or an `undo` | T001 |
| `flow_fail_in_defer`, `flow_fail_in_undo`, `flow_try_in_defer`, `flow_try_in_undo` | decision 19: a `fail` and a `try` inside a `defer` and an `undo` | T001 |
| `flow_yield_in_defer` | a `yield` inside a `defer` of a handler | T002 |
| `flow_defer_failing_call_in_try` | a failing call as the statement of a `defer` in a `try` block | T003 |
| `flow_if_let_binding_in_closure` | the name of an `if let` read by a closure in its block | T004 |
| `flow_closure_reads_caught_error`, `flow_closure_reads_try_error` | the error of a handler read by a closure | T005 |
| `flow_defer_directly_in_defer`, `flow_undo_directly_in_undo` | the refusal of a `defer` in a `defer` and of an `undo` in an `undo` | T006 |
| `flow_break_out_of_closure`, `flow_continue_out_of_closure` | a `break` and a `continue` do not reach the loop around a closure | |
| `flow_loop_exits_in_arm`, `flow_loop_exits_in_if_let`, `flow_loop_exits_in_select` | a `break` and a `continue` in an arm leave the loop around the statement | |
| `flow_defer_closure_two_exits` | a `defer` with a closure, written out at two exits | |
| `flow_defer_loop_at_loop_exits` | a `defer` with a loop of its own at the `continue`, the `break` and the brace of a turn | |
| `flow_return_value_and_defer` | a scalar result is fixed before the deferred statements, and a struct local is read after them | |
| `flow_closure_return_and_defer`, `flow_worker_return_and_defer` | the same order in a closure and in a worker | |
| `flow_handler_leaves_loop_with_defer`, `flow_let_else_leaves_loop_with_defer` | a handler and the `else` of a `let` leave a loop through the `defer` of the turn | |
| `flow_yield_in_loop_in_handler`, `flow_yield_in_nested_handler` | a `yield` in a loop of a handler, and in a handler inside a handler | |
| `flow_fallthrough_runs_defer`, `flow_fallthrough_after_inner_switch` | a `fallthrough` runs the `defer` of its arm, and two nested ones each belong to their `switch` | |
| `flow_continue_in_do_while`, `flow_continue_in_for_by` | a `continue` goes to the test of a `do ... while` and to the step of a `for` | |
| `flow_undo_per_block`, `flow_undo_in_try_block` | an `undo` belongs to its block, in a loop and in a `try` block | |
| `flow_for_iterator_exits` | the exits of a `for` over an iterator of the program, with a `defer` | |
| `flow_switch_value_once`, `flow_in_range_value_once` | the value of a `switch` and of `x in lo..hi` is computed once | |
| `flow_for_range_no_binding` | a range without a binding, an empty one and one that runs backwards | |
| `flow_if_let_chain` | `else if let`, `else if` and `else` after an `if let` | |
| `flow_fail_in_catch`, `flow_try_in_catch`, `flow_fail_in_closure`, `flow_try_in_closure`, `flow_catch_in_worker` | `fail` and `try` in a handler and in a closure, and a handler in a worker | |
| `flow_fail_in_worker_dispatched`, `flow_try_in_worker_parallel` | a worker that may fail is refused at `dispatch` and at `parallel` | |
| `flow_loop_variable_in_closure`, `flow_arm_binding_in_closure`, `flow_bindings_in_defer` | the names a `for`, an arm, an `if let` and a `catch` bind, read by a closure or a `defer` | |
| `flow_switch_arms_all_return`, `flow_if_let_arms_all_return`, `flow_while_true_no_return` | a `switch`, an `if let` and a `while true` do not end the path of a function | |
| `flow_switch_two_else`, `flow_switch_arm_not_statement`, `flow_for_step_not_integer`, `flow_for_range_not_integer` | four refusals of the parser and the checker | |
| `flow_yield_needs_value`, `flow_yield_takes_no_value`, `flow_yield_outside_handler` | three refusals of `yield` | |

The triples among them, each a rule that reads three things at once:

- `return`, `defer` and the kind of the result: `flow_return_value_and_defer`.
- A loop, a handler and a `defer`: `flow_handler_leaves_loop_with_defer`
  and `flow_yield_in_loop_in_handler`.
- A `defer`, a closure and the number of exits:
  `flow_defer_closure_two_exits`.
- A construct, a generic function and a library file: every function
  `gen_<construct>` of `modules/flow/com/example/flow`.
- A `for` with a step, a closure and a `continue`:
  `flow_loop_variable_in_closure`.
- An `undo`, a `try` block and a `defer`: `flow_undo_in_try_block`.

Anti 0.2.0 has no labels and no `unreachable`, so neither has a row.

### Values

An operator stands in the first table once per family, since the place does
not change what it does. The second table holds each operator by itself.

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| integer literal | module level `programs/alias_arrays`, function `programs/alias_access`, method `programs/alloc_owning`, loop body `programs/alloc_owning`, switch arm `programs/alias_unions`, `if let` `programs/variant_owning`, `defer` `programs/closures`, `undo` `programs/nested_locals`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, `tests` block `programs/test_blocks`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, library module `modules/access/com/example/ledger` | nested type |
| float literal | module level `programs/classes`, function `programs/alias_bytes`, method `programs/classes`, loop body `programs/effects_edges`, switch arm `programs/variants`, `if let` `programs/generic_variants`, catch handler `programs/construct_args`, closure `programs/generic_methods`, library module `modules/blocks/com/example/geo` | `defer`, `undo`, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| character literal | module level `std/reflect`, function `programs/alias_integers`, loop body `programs/utf8` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| string literal `"..."` | module level `programs/class_literal_defaults`, function `programs/alias_access`, method `programs/array_local`, loop body `programs/arith_facts`, switch arm `programs/generic_copies`, `if let` `programs/variant_owning`, `defer` `programs/may_fail`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, `sync` block `programs/eq_handles`, `tests` block `inject/standard/src/com/example/standard`, `fixtures` block `programs/test_blocks`, generic body `programs/generic_copies`, library module `modules/chain/com/example/animal` | worker, `dispatch` or `parallel`, `select` arm, nested type |
| raw string `r"..."` | function `programs/pattern_match`, loop body `programs/pattern_walk` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| byte string `b"..."` and `br"..."` | function `programs/args`, loop body `programs/args` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| hex string `x"..."` | function `programs/hex_bytes` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| interpolation `f"..."` and `rf"..."` | module level `errors/format_types`, function `programs/eq_default`, method `programs/format_strings`, loop body `programs/format_strings`, catch handler `std/collection_pool`, generic body `modules/generics/com/example/gen`, library module `modules/generics/com/example/gen` | switch arm, `if let`, `defer`, `undo`, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| pattern literal `re"..."` | module level `errors/pattern_malformed`, function `programs/eq_handles`, loop body `programs/pattern_bytes` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `true` and `false` | module level `std/reflect`, function `programs/args`, method `programs/eq_default`, loop body `programs/args`, `if let` `errors/if_let_moves`, catch handler `programs/closures`, closure `std/hash_map`, worker `programs/channel_workers`, generic body `modules/generic_hooks/com/example/row`, library module `modules/generic_hooks/com/example/row` | switch arm, `defer`, `undo`, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| `here` | function `programs/location`, method `programs/location`, generic body `programs/walk_generic`, library module `modules/defaults/com/example/greeter` | module level, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| array literal `[a, b]` | module level `programs/const_data`, function `programs/alias_arrays`, method `traps/collection_changed`, loop body `bench/ablate/mixed_work`, generic body `modules/generic_hooks/com/example/row`, library module `modules/generic_hooks/com/example/row` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| array repeat `[v; n]` | module level `programs/index_several`, function `programs/generic_constants`, method `programs/generic_constants`, generic body `programs/generic_constants`, library module `modules/iteration/com/example/bag` | loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| slice literal | function `programs/assign_owned`, method `programs/generic_copies`, catch handler `std/fs`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen` | module level, loop body, switch arm, `if let`, `defer`, `undo`, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| struct literal | module level `programs/checked_once`, function `programs/alias_access`, method `programs/catch_exits`, loop body `programs/arith_facts`, switch arm `programs/variants`, `if let` `programs/variants`, catch handler `programs/construct_args`, worker `programs/concurrent`, `tests` block `inject/project/src/com/example/app`, generic body `programs/eq_default`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger` | `defer`, `undo`, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `fixtures` block |
| tuple `(a, b)` | module level `programs/for_patterns`, function `programs/alias_view_local`, method `programs/for_patterns`, loop body `std/concurrent_map_threads`, catch handler `programs/generic_may_fail`, worker `std/concurrent_map_threads`, generic body `programs/caller_params`, library module `modules/generics/com/example/gen` | switch arm, `if let`, `defer`, `undo`, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| tuple element `t.0` | function `programs/alias_view_local`, method `programs/index_several`, loop body `programs/lent_tuples`, closure `std/collection_parts`, worker `std/concurrent_map_threads` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `let` | function `programs/alias_access`, method `programs/caller_params`, loop body `programs/arith_facts`, switch arm `programs/generic_variants`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/catch_moves`, closure `programs/snapshots`, worker `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, `tests` block `programs/test_blocks`, `fixtures` block `anti-test/com/example/stack`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/closures/com/example/walk` | `if let`, `dispatch` or `parallel` |
| `let` with a type | function `programs/alias_bytes`, method `programs/none_operators`, loop body `programs/effects_edges`, generic body `dump/constraints`, library module `modules/variants/com/example/geo` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| `let (a, b) = e` | function `programs/carry_chain`, loop body `programs/carry_chain`, worker `std/concurrent_map_threads` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `const` in a body | none | function, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `=` | function `programs/alias_access`, method `programs/alias_class_pointers`, loop body `programs/alloc_owning`, switch arm `programs/alias_unions`, catch handler `programs/closures`, closure `programs/closures`, worker `programs/generic_fn_args`, `sync` block `programs/concurrent`, generic body `programs/generic_constants`, nested type `programs/generic_nested`, library module `modules/chain/com/example/animal` | `if let`, `defer`, `undo`, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block |
| compound assignment, `+=` to `>>=` | function `programs/alloc_owning`, method `programs/class_literal_defaults`, loop body `programs/alloc_owning`, switch arm `programs/switch_fallthrough`, `defer` `programs/closures`, catch handler `std/concurrent_map_threads`, closure `programs/closures`, worker `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, generic body `programs/for_patterns`, library module `modules/access/com/example/ledger` | `if let`, `undo`, `dispatch` or `parallel`, `tests` block, `fixtures` block, nested type |
| wrapping and saturating assignment, `+%=` to `*\|=` | function `programs/arith_facts`, loop body `programs/arith_facts` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| arithmetic operator, `+ - * / %` | module level `programs/checker_context`, function `programs/alias_c_functions`, method `programs/alias_class_pointers`, loop body `programs/by_ends`, switch arm `programs/alias_unions`, `if let` `programs/generic_variants`, `defer` `programs/defer_stmt`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/closures`, `sync` block `programs/sync_pairs`, generic body `programs/generic_constants`, nested type `programs/generic_nested`, library module `modules/blocks/com/example/geo` | `undo`, `select` arm, `tests` block, `fixtures` block |
| wrapping operator, `+% -% *% <<%` | module level `programs/wrapping`, function `programs/arith_facts`, method `bench/objects`, loop body `programs/arith_facts` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| saturating operator, `+\| -\| *\|` | module level `programs/saturating`, function `programs/saturating` | method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| shift, `<< >>` | module level `programs/mul_high`, function `programs/flags`, loop body `programs/llvm_division`, generic body `dump/generics_syntax` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type, library module |
| bitwise operator, `& \| ^ ~` | function `programs/compare_bits`, method `programs/union_name`, loop body `programs/scalar_ops` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| comparison, `== != < <= > >=` | module level `programs/const_text_compare`, function `programs/alias_access`, method `programs/caller_params`, loop body `programs/alloc_owning`, switch arm `programs/generic_variants`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/lock_free_queue`, `sync` block `programs/concurrent`, `tests` block `programs/test_blocks`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger` | `if let`, `defer`, `undo`, `dispatch` or `parallel`, `select` arm, `fixtures` block |
| unary `-` and `!` | module level `programs/class_bitfields`, function `programs/alias_bytes`, method `programs/eq_default`, loop body `programs/arith_facts`, catch handler `programs/closures`, closure `programs/generic_fn_args`, worker `programs/sync_pairs`, `dispatch` or `parallel` `std/sync_map_threads`, `sync` block `programs/sync_exits`, `tests` block `anti-test/com/example/greeting`, generic body `programs/generic_hooks`, library module `modules/generic_hooks/com/example/row` | switch arm, `if let`, `defer`, `undo`, `select` arm, `fixtures` block, nested type |
| `mul_high` | module level `programs/mul_high`, function `programs/mul_high` | method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `as` | module level `programs/eq_hash_agree`, function `programs/alias_access`, method `programs/array_local`, loop body `programs/arith_facts`, switch arm `programs/generic_copies`, `if let` `programs/generic_variants`, catch handler `programs/catch_exits`, closure `programs/effects_edges`, worker `traps/lock_order`, generic body `programs/generic_copies`, nested type `programs/generic_nested`, library module `modules/chain/com/example/animal` | `defer`, `undo`, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block |
| index `e[i]` | function `programs/alias_arrays`, method `programs/for_patterns`, loop body `programs/alloc_owning`, closure `std/collection_grid`, worker `programs/generic_fn_args`, `dispatch` or `parallel` `errors/generic_copies`, `sync` block `programs/eq_hash_agree`, generic body `programs/generic_constants`, library module `modules/generic_hooks/com/example/row` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, `select` arm, `tests` block, `fixtures` block, nested type |
| slice `e[lo..hi]` | function `programs/alias_arrays`, method `programs/lent_slices`, loop body `std/fs`, closure `programs/closures`, `dispatch` or `parallel` `programs/concurrent` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, worker, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| field `e.x` | module level `programs/alias_access`, function `programs/alias_access`, method `programs/alias_class_pointers`, loop body `programs/args`, switch arm `programs/alias_unions`, `if let` `programs/generic_variants`, `defer` `programs/may_fail`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/sync_workers`, `sync` block `programs/concurrent`, `tests` block `inject/project/src/com/example/app`, `fixtures` block `programs/test_blocks`, generic body `programs/eq_default`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger` | `select` arm |
| call | function `programs/alias_access`, method `programs/array_local`, loop body `programs/args`, switch arm `programs/generic_copies`, `if let` `programs/generic_variants`, `defer` `programs/closures`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/closures`, `dispatch` or `parallel` `programs/channel_workers`, `sync` block `programs/eq_handles`, `tests` block `programs/test_blocks`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, library module `modules/access/com/example/ledger` | module level, `select` arm, nested type |
| `size_of` | module level `programs/in_range`, function `programs/alias_view_passed`, method `programs/generic_copies`, generic body `programs/generic_copies`, nested type `programs/generic_nested`, library module `modules/generics/com/example/gen` | loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block |
| anonymous function `fn(x) { }` | module level `errors/closures`, function `programs/closures`, method `programs/closures`, loop body `programs/closures`, `defer` `programs/closures`, closure `programs/closures`, worker `std/concurrent_map_threads`, `dispatch` or `parallel` `programs/closures`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen` | switch arm, `if let`, `undo`, catch handler, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| simd built-in | function `programs/simd`, loop body `bench/simd_loop` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |

The kinds of values:

| Construct | Kinds held | Kinds open |
|---|---|---|
| integer literal | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_c_functions`, byte `programs/alias_bytes` | i64, u8, uint, f32, f64, simd struct, char |
| float literal | f32 `programs/alias_simd` | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte, f64, simd struct, char |
| character literal | char `programs/alias_integers` | byte, i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| array literal `[a, b]` | i32 `programs/alias_c_functions`, int `programs/alias_arrays`, u64 `programs/carry_chain`, byte `programs/checker_context`, f32 `programs/generic_constants`, f16 `programs/half`, bool `std/collection_ring`, str `programs/eq_arrays`, fixed array `programs/alias_arrays`, pointer `programs/object_model`, optional value `programs/owning_locals`, tuple `programs/for_patterns`, struct `programs/dispatch_job`, enum `std/reflect`, variant `programs/variant_owning`, class `programs/array_local`, generic parameter `modules/generic_hooks/com/example/row` | i8, i16, i64, u8, u16, u32, uint, f64, char, slice, nullable pointer, lent value, may-fail function value, function value, channel, packed struct, simd struct, union, synchronized class, concurrent class, interface |
| array repeat `[v; n]` | i16 `programs/structs`, int `programs/generic_constants`, byte `programs/lent_derived`, bool `std/sorted_order`, fixed array `programs/generic_constants`, class `programs/lent_derived`, generic parameter `programs/generic_constants` | i8, i32, i64, u8, u16, u32, u64, uint, f32, f64, f16, char, str, slice, pointer, nullable pointer, optional value, lent value, may-fail function value, function value, tuple, channel, struct, packed struct, simd struct, union, enum, variant, synchronized class, concurrent class, interface |
| slice literal | slice `programs/assign_owned` | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, str, fixed array, pointer, nullable pointer, optional value, lent value, may-fail function value, function value, tuple, channel, struct, packed struct, simd struct, union, enum, variant, class, synchronized class, concurrent class, interface, generic parameter |
| struct literal | struct `programs/bitfields`, union `clib/com/example/geo`, class `programs/alias_class_pointers`, variant `programs/alias_unions` | packed struct, simd struct, generic parameter |
| tuple element `t.0` | i32 `clib/com/example/names`, int `programs/alloc_owning`, str `programs/lent_tuples`, pointer `programs/alias_view_local`, lent value `programs/lent_tuples`, function value `clib/com/example/names`, tuple `programs/alias_view_local`, struct `clib/com/example/names`, class `programs/alloc_owning` | i8, i16, i64, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, slice, fixed array, nullable pointer, optional value, may-fail function value, channel, packed struct, simd struct, union, enum, variant, synchronized class, concurrent class, interface, generic parameter |
| `let` | i8 `programs/arith_facts`, i16 `programs/atomic_widths`, i32 `programs/alias_same`, int `programs/alias_pointers`, u16 `programs/hash_builtin`, u32 `programs/half`, u64 `programs/carry_chain`, byte `programs/arith_facts`, f32 `programs/effects_edges`, f16 `programs/effects_edges`, bool `programs/atomic_widths`, char `programs/snapshots`, str `programs/alias_pointers`, slice `programs/alias_view_local`, fixed array `programs/alias_pointers`, pointer `programs/alias_access`, nullable pointer `programs/alias_integers`, optional value `programs/argument_order`, lent value `programs/lent_derived`, may-fail function value `programs/catch_none`, function value `programs/alias_pointers`, tuple `programs/alias_view_local`, channel `programs/channel_workers`, struct `programs/bitfields`, enum `programs/snapshots`, variant `programs/eq_parts`, class `programs/assign_owned`, synchronized class `std/sync_equal`, concurrent class `std/collection_equals`, generic parameter `programs/caller_params` | i64, u8, uint, f64, packed struct, simd struct, union, interface |
| `const` in a body | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, str, slice, fixed array, pointer, nullable pointer, optional value, lent value, may-fail function value, function value, tuple, channel, struct, packed struct, simd struct, union, enum, variant, class, synchronized class, concurrent class, interface, generic parameter |
| `=` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `programs/arith_facts`, char `programs/alias_integers`, str `programs/catch_exits`, slice `programs/generic_copies`, fixed array `programs/assign_owned`, pointer `programs/alias_pointers`, nullable pointer `programs/alias_pointers`, optional value `programs/alloc_owning`, function value `programs/alias_pointers`, tuple `programs/alloc_owning`, struct `programs/walk_generic`, enum `programs/alias_integers`, variant `programs/alias_unions`, class `programs/alloc_owning`, generic parameter `programs/generic_constants` | i64, u8, uint, f64, lent value, may-fail function value, channel, packed struct, simd struct, union, synchronized class, concurrent class, interface |
| `+=` | i32 `clib/com/example/failing`, int `programs/alloc_owning`, u32 `programs/bitfields`, u64 `clib/com/example/shapes`, byte `programs/class_bitfields`, f32 `programs/half` | i8, i16, i64, u8, u16, uint, f64, simd struct |
| `-=` | int `programs/member_keywords` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, simd struct |
| `*=` | int `programs/for_patterns` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, simd struct |
| `/=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte, f32, f64, simd struct |
| `%=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `&=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `\|=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `^=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `<<=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `>>=` | none | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte |
| `+%=` | int `programs/arith_facts`, byte `programs/compound_wrapping` | i8, i16, i32, i64, u8, u16, u32, u64, uint |
| `-%=` | int `programs/compound_wrapping`, byte `programs/compound_wrapping` | i8, i16, i32, i64, u8, u16, u32, u64, uint |
| `*%=` | i32 `programs/compound_wrapping`, int `programs/compound_wrapping` | i8, i16, i64, u8, u16, u32, u64, uint, byte |
| `<<%=` | int `programs/compound_wrapping` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte |
| `+\|=` | i32 `programs/compound_wrapping`, int `programs/compound_wrapping`, byte `programs/compound_wrapping` | i8, i16, i64, u8, u16, u32, u64, uint |
| `-\|=` | i8 `programs/compound_wrapping`, byte `programs/compound_wrapping` | i16, i32, i64, int, u8, u16, u32, u64, uint |
| `*\|=` | i8 `programs/compound_wrapping`, byte `programs/compound_wrapping` | i16, i32, i64, int, u8, u16, u32, u64, uint |
| `+` | i8 `programs/flags`, i16 `programs/llvm_flags`, i32 `programs/alias_c_functions`, int `programs/alias_class_pointers`, u16 `programs/flags`, u32 `programs/alias_view_passed`, u64 `programs/carry_chain`, byte `programs/effects_edges`, f32 `programs/classes`, simd struct `clib/com/example/simdlib`, pointer `programs/alias_c_functions` | i64, u8, uint, f64, str |
| `-` | i8 `programs/bitfields`, i16 `programs/flags`, i32 `programs/llvm_division`, int `programs/arith_facts`, u64 `programs/carry_chain`, byte `programs/flags`, f32 `programs/float_math` | i64, u8, u16, u32, uint, f64, simd struct |
| `+%` | i8 `programs/scalar_ops`, i16 `programs/scalar_ops`, int `programs/arith_facts`, u16 `programs/scalar_ops`, byte `programs/wrapping` | i32, i64, u8, u32, u64, uint |
| `-%` | i8 `programs/wrapping`, i32 `programs/scalar_ops`, int `programs/wrapping`, u64 `programs/scalar_ops`, byte `programs/wrapping` | i16, i64, u8, u16, u32, uint |
| `+\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u16 `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating` | i64, u8, uint |
| `-\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating` | i64, u8, u16, uint |
| `*` | i8 `programs/flags`, i16 `programs/flags`, i32 `programs/alias_c_functions`, int `programs/alias_unions`, u16 `programs/llvm_flags`, u32 `programs/llvm_flags`, u64 `programs/flags`, byte `programs/flags`, f32 `programs/classes`, simd struct `clib/com/example/simdlib` | i64, u8, uint, f64 |
| `/` | i8 `programs/scalar_ops`, i16 `programs/narrow_ops`, i32 `programs/llvm_division`, int `programs/closures`, u16 `programs/scalar_ops`, u32 `programs/narrow_ops`, u64 `programs/llvm_division`, byte `programs/llvm_division`, f32 `programs/format_strings` | i64, u8, uint, f64, simd struct |
| `%` | i8 `programs/scalar_ops`, i16 `programs/narrow_ops`, i32 `programs/llvm_division`, int `programs/closures`, u64 `programs/llvm_division`, byte `programs/scalar_ops` | i64, u8, u16, u32, uint |
| `*%` | i8 `programs/scalar_ops`, i32 `programs/wrapping`, int `programs/wrapping`, u16 `programs/wrapping` | i16, i64, u8, u32, u64, uint, byte |
| `*\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u16 `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating` | i64, u8, uint |
| `mul_high` | i8 `programs/mul_high`, i16 `programs/mul_high`, i32 `programs/mul_high`, int `programs/mul_high`, u16 `programs/mul_high`, u32 `programs/mul_high`, u64 `programs/mul_high`, byte `programs/mul_high` | i64, u8, uint |
| `<<` | i8 `programs/flags`, i16 `programs/flags`, i32 `programs/llvm_division`, int `programs/flags`, u16 `programs/llvm_flags`, u32 `programs/flags`, u64 `programs/llvm_flags`, byte `programs/flags` | i64, u8, uint |
| `>>` | i8 `programs/flags`, i16 `programs/llvm_flags`, i32 `programs/flags`, int `programs/flags`, u16 `programs/llvm_flags`, u32 `programs/llvm_flags`, u64 `programs/flags`, byte `programs/flags` | i64, u8, uint |
| `<<%` | i8 `programs/wrapping`, i32 `programs/wrapping`, int `programs/wrapping`, u32 `programs/wrapping`, u64 `bench/scalar_loop`, byte `programs/wrapping` | i16, i64, u8, u16, uint |
| binary `&` | i32 `programs/compare_bits`, int `programs/narrow_ops`, u32 `programs/utf8` | i8, i16, i64, u8, u16, u64, uint, byte, bool, enum, simd struct |
| binary `\|` | i32 `programs/compare_bits`, int `programs/narrow_ops`, u32 `programs/union_name` | i8, i16, i64, u8, u16, u64, uint, byte, bool, enum, simd struct |
| binary `^` | i16 `programs/narrow_ops`, i32 `programs/compare_bits`, int `programs/narrow_ops`, u64 `programs/flags` | i8, i64, u8, u16, u32, uint, byte, bool, enum, simd struct |
| `==` | i32 `programs/twice`, int `programs/arith_facts`, u32 `programs/eq_hash_agree`, u64 `programs/effects_edges`, byte `programs/strings`, f32 `dump/constraints`, bool `programs/alias_pointers`, char `programs/hash_generic`, str `programs/const_text_compare`, pointer `programs/alias_pointers`, nullable pointer `programs/alias_pointers`, optional value `programs/alloc_owning`, tuple `programs/eq_hash_agree`, struct `modules/hashing/com/example/keys`, enum `programs/generic_nested`, variant `programs/eq_parts`, class `programs/eq_compiled`, fixed array `programs/eq_arrays`, function value `programs/callbacks`, generic parameter `programs/eq_arrays` | i8, i16, i64, u8, u16, uint, f64, slice, channel, simd struct |
| `!=` | i8 `programs/llvm_division`, i16 `programs/scalar_ops`, i32 `programs/compare_bits`, int `programs/closures`, u16 `programs/scalar_ops`, u32 `programs/scalar_ops`, u64 `programs/llvm_division`, byte `programs/args`, f32 `programs/construct_args`, bool `programs/alias_access`, char `std/reflect`, str `programs/const_text_compare`, pointer `programs/hooks`, nullable pointer `programs/alias_access`, optional value `programs/argument_order`, tuple `programs/eq_parts`, enum `std/log`, class `std/collection_fields`, fixed array `programs/eq_arrays`, generic parameter `modules/generic_hooks/com/example/row` | i64, u8, uint, f64, struct, variant, slice, function value, channel, simd struct |
| `<` | i16 `programs/narrow_ops`, i32 `programs/alias_c_functions`, int `programs/alloc_owning`, u32 `programs/utf8`, u64 `std/interfaces`, byte `programs/narrow_ops`, f32 `programs/construct_args`, str `programs/const_text_compare`, generic parameter `programs/caller_params` | i8, i64, u8, u16, uint, f64, char, pointer, simd struct, struct, class |
| `<=` | i32 `clib/com/example/canvas`, int `programs/effects_edges`, u64 `programs/scalar_ops`, f32 `programs/construct_args`, str `programs/str_hooks`, generic parameter `dump/constraints` | i8, i16, i64, u8, u16, u32, uint, byte, f64, char, pointer, simd struct, struct, class |
| `>` | i32 `programs/alias_c_functions`, int `programs/arith_facts`, u32 `programs/out_place_forms`, u64 `programs/scalar_ops`, byte `programs/scalar_ops`, f32 `programs/object_model`, str `programs/const_text_compare`, simd struct `clib/com/example/simdlib`, generic parameter `dump/constraints` | i8, i16, i64, u8, u16, uint, f64, char, pointer, struct, class |
| `>=` | i16 `programs/narrow_ops`, i32 `clib/com/example/failing`, int `programs/fn_may_fail`, byte `programs/pattern_bytes`, str `programs/str_hooks`, generic parameter `dump/constraints` | i8, i64, u8, u16, u32, u64, uint, f32, f64, char, pointer, simd struct, struct, class |
| unary `-` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_integers`, f32 `programs/construct_args` | i64, u8, u16, u32, u64, uint, byte, f64, simd struct |
| unary `!` | bool `programs/compare_bits` | none |
| unary `~` | int `programs/compare_bits` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte |
| `as` | i8 `programs/arith_facts`, i16 `programs/alias_integers`, i32 `programs/alias_integers`, int `programs/alloc_owning`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/by_unsigned`, byte `programs/arith_facts`, f32 `programs/alias_simd`, f16 `programs/effects_edges`, bool `programs/alias_pointers`, char `programs/alias_integers`, pointer `programs/alias_access`, nullable pointer `programs/alias_c_functions`, enum `programs/alias_integers` | i64, u8, uint, f64, str, slice |
| index `e[i]` | i16 `programs/structs`, i32 `programs/alias_c_functions`, int `programs/alias_arrays`, u16 `dump/tbaa`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `std/collection_ring`, str `programs/args`, slice `programs/alias_arrays`, fixed array `programs/alias_arrays`, pointer `programs/alias_integers`, nullable pointer `std/builder_take_in`, optional value `programs/alloc_owning`, lent value `programs/lent_derived`, tuple `programs/alloc_owning`, struct `programs/eq_hash_agree`, enum `std/reflect`, variant `programs/variant_owning`, class `programs/array_local`, generic parameter `programs/generic_constants` | i8, i64, u8, u32, uint, f64, char, may-fail function value, function value, channel, packed struct, simd struct, union, synchronized class, concurrent class, interface |
| slice `e[lo..hi]` | slice `programs/alias_arrays` | none |
| field `e.x` | i8 `programs/alias_bytes`, i16 `programs/atomic_widths`, i32 `programs/alias_view_passed`, int `programs/alias_access`, u16 `programs/bitfields`, u32 `programs/bitfields`, u64 `programs/sync_pairs`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `programs/arith_facts`, char `programs/hash_builtin`, str `programs/alias_access`, slice `programs/alias_pointers`, fixed array `programs/alias_arrays`, pointer `programs/alias_access`, nullable pointer `programs/alias_access`, optional value `programs/hash_builtin`, lent value `programs/for_patterns`, may-fail function value `programs/fn_may_fail`, function value `programs/alias_pointers`, tuple `programs/eq_hash_agree`, struct `programs/bitfields`, simd struct `clib/com/example/simdlib`, union `std/reflect`, enum `programs/alias_integers`, variant `programs/generic_copies`, class `programs/alias_class_pointers`, synchronized class `std/sync_equal`, concurrent class `std/collection_dup`, interface `programs/inherits_header`, generic parameter `programs/eq_default` | i64, u8, uint, f64, channel, packed struct |
| call | i8 `programs/by_ends`, i16 `programs/alias_integers`, i32 `programs/alias_access`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_c_functions`, byte `programs/by_ends`, f32 `programs/classes`, bool `programs/alias_pointers`, char `programs/hash_generic`, str `programs/args`, slice `programs/alias_arrays`, fixed array `programs/checker_context`, pointer `programs/alias_access`, nullable pointer `programs/alias_access`, optional value `programs/argument_order`, lent value `programs/lent_derived`, may-fail function value `programs/catch_exits`, function value `programs/alias_access`, tuple `programs/caller_params`, channel `programs/channel_workers`, struct `programs/carry_chain`, enum `programs/arith_facts`, variant `programs/alias_unions`, class `programs/alloc_owning`, synchronized class `std/sync_equal`, concurrent class `std/collection_dup`, generic parameter `programs/caller_params` | i64, u8, uint, f64, f16, packed struct, simd struct, union, interface |
| anonymous function `fn(x) { }` | function value `programs/closures`, may-fail function value `programs/closures` | none |

### Types

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| named or built-in type | `let` `programs/alias_bytes`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alias_access`, `as` `programs/alias_access`, `alloc` `programs/alias_integers`, `size_of` `programs/alias_view_passed`, `const` `programs/alloc_owning`, slice literal `programs/assign_owned`, call `programs/caller_params`, `type` alias `programs/generic_unused`, library module `modules/access/com/example/ledger`, base of an enum `programs/alias_integers`, literal `programs/eq_default`, `inherits` `programs/generic_copies`, `chan` `programs/channel_workers`, constraint `std/collection_deque` | case of a variant |
| `*T` | `let` `programs/class_is`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alias_pointers`, `as` `programs/alias_access`, `alloc` `programs/generic_copies`, slice literal `programs/generic_copies`, call `std/collection`, library module `modules/access/com/example/ledger`, literal `programs/generic_copies` | `size_of`, `const`, `type` alias, case of a variant |
| `?*T` | `let` `programs/catch_none`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alloc_owning`, `as` `programs/alias_c_functions`, `alloc` `std/builder_take_in`, `size_of` `programs/optional_values`, library module `modules/chain/com/example/animal` | `const`, slice literal, call, `type` alias, case of a variant |
| `?T` | `let` `programs/argument_order`, parameter `programs/optional_values`, result `programs/argument_order`, field `programs/optional_values`, `alloc` `programs/alloc_owning`, `size_of` `programs/alloc_owning`, slice literal `programs/owned_buffers`, library module `modules/generics/com/example/gen` | `as`, `const`, call, `type` alias, case of a variant |
| `[]T` | `let` `programs/lent_slices`, parameter `programs/alias_arrays`, result `programs/eq_kinds`, field `programs/alias_pointers`, `alloc` `std/serialize_null`, slice literal `programs/assign_owned`, call `errors/copies_many`, library module `modules/closures/com/example/walk` | `as`, `size_of`, `const`, `type` alias, case of a variant |
| `[N]T` | `let` `programs/alias_pointers`, parameter `programs/alias_arrays`, result `programs/generic_constants`, field `programs/alias_arrays`, `as` `programs/alias_arrays`, `alloc` `programs/concurrent`, `size_of` `programs/half`, `const` `programs/const_data`, library module `modules/generic_hooks/com/example/row` | slice literal, call, `type` alias, case of a variant |
| `fn(A) -> R` | `let` `programs/callbacks`, parameter `programs/alias_c_functions`, result `programs/closures`, field `programs/alias_pointers`, `as` `programs/alias_pointers`, call `programs/generic_fn_args`, library module `modules/closures/com/example/walk`, literal `programs/generic_fn_args` | `alloc`, `size_of`, `const`, slice literal, `type` alias, case of a variant |
| `keep fn` | none | `let`, parameter, result, field, `as`, `alloc`, `size_of`, `const`, slice literal, call, `type` alias, case of a variant, library module |
| `concurrent fn` | none | `let`, parameter, result, field, `as`, `alloc`, `size_of`, `const`, slice literal, call, `type` alias, case of a variant, library module |
| tuple type `(A, B)` | `let` `programs/for_patterns`, parameter `programs/for_patterns`, result `programs/caller_params`, field `programs/for_patterns`, `alloc` `programs/alloc_owning`, `size_of` `programs/alloc_owning`, call `std/collection_parts`, library module `modules/generics/com/example/gen` | `as`, `const`, slice literal, `type` alias, case of a variant |
| `chan T` | parameter `programs/channel_workers`, result `modules/sync/com/example/queue`, field `programs/eq_handles`, library module `modules/sync/com/example/queue` | `let`, `as`, `alloc`, `size_of`, `const`, slice literal, call, `type` alias, case of a variant |
| constant as a type argument | `let` `dump/generics`, parameter `std/spsc_ring_threads`, field `std/collection_dup`, `size_of` `programs/generic_constants`, call `programs/checker_context`, `type` alias `doc/src/com/example/app`, library module `doc/src/com/example/shapes`, literal `programs/generic_constants` | result, `as`, `alloc`, `const`, slice literal, case of a variant |
| type arguments `Name<A>` | `let` `std/shared`, parameter `programs/generic_constants`, result `programs/generic_copies`, field `programs/generic_hooks`, `as` `modules/generics/client`, `size_of` `programs/generic_constants`, call `programs/caller_params`, `type` alias `modules/direct/com/example/shapes`, library module `modules/access/com/example/ledger`, literal `programs/eq_default`, `inherits` `programs/generic_copies`, constraint `std/collection_deque` | `alloc`, `const`, slice literal, case of a variant |
| `struct` | module level `programs/alias_access`, nested type `errors/concurrent_forms`, generic `programs/eq_default`, library module `modules/blocks/com/example/geo` | none |
| `packed struct` | module level `abi/abi_structs`, library module `clib/com/example/shapes` | nested type |
| `simd struct` | module level `programs/alias_simd`, library module `modules/generics/com/example/gen` | nested type |
| `align(n)` on a type | `programs/inherits_header` | none |
| `union` | module level `programs/alias_unions`, library module `clib/com/example/geo` | none |
| `enum` | module level `programs/alias_integers`, nested type `programs/generic_nested`, library module `modules/classlib/com/example/shapes` | none |
| `enum` with a base type | `programs/alias_integers` | none |
| `enum` value with a number | `programs/alias_integers` | none |
| `variant` | module level `programs/alias_unions`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `type` alias | module level `programs/generic_unused`, library module `modules/direct/com/example/shapes` | generic |
| `constraint` | module level `programs/generic_unused`, library module `modules/direct/com/example/shapes` | none |
| type parameter `<T>` | `programs/caller_params` | none |
| constant parameter `<N: int>` | `programs/checker_context` | none |
| constraint on a parameter `<T: C>` | `programs/caller_params` | none |
| bitfield `x: u8 : 3` | `programs/bitfields` | none |
| default of a field | `programs/alias_class_pointers` | none |
| default of a parameter | `programs/class_literal_defaults` | none |
| `e is Type.Case` | `programs/generic_variants` | none |
| `static atomic` field | `programs/alloc_owning` | none |

### Ownership

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `alloc` of a literal | function `programs/alias_access`, method `programs/catch_exits`, loop body `programs/parallel_join`, catch handler `errors/worker_walk`, worker `programs/parallel_join`, `tests` block `inject/project/src/com/example/app`, generic body `programs/generic_nested`, library module `modules/chain/com/example/dog` | module level, switch arm, `if let`, `defer`, `undo`, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `fixtures` block, nested type |
| `alloc(T, n)` | function `programs/alias_integers`, method `programs/generic_copies`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen` | module level, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| `alloc` of a class with arguments | function `programs/concurrent` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `free` | function `programs/alias_integers`, method `programs/concurrent`, loop body `programs/concurrent`, worker `programs/parallel_join`, `tests` block `traps/memory_checks_tests`, generic body `programs/generic_copies`, library module `modules/closures/com/example/walk` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `fixtures` block, nested type |
| `delete` | function `programs/alias_class_pointers`, loop body `std/collection`, `defer` `programs/may_fail_table`, catch handler `programs/catch_moves`, worker `errors/worker_walk`, `tests` block `inject/project/src/com/example/app` | method, switch arm, `if let`, `undo`, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `fixtures` block, generic body, nested type, library module |
| `destroy` | function `programs/alloc_owning`, generic body `programs/owning_values` | method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type, library module |
| `dup` | function `programs/caller_params`, method `programs/generic_constants`, loop body `programs/generic_copies`, switch arm `programs/generic_variants`, `sync` block `programs/generic_sync`, generic body `programs/caller_params`, library module `modules/direct/com/example/frame` | module level, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block, nested type |
| address `&x` | function `programs/alias_access`, method `programs/caller_params`, loop body `programs/arith_facts`, switch arm `programs/nested_locals`, `if let` `programs/variant_owning`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/catch_moves`, closure `std/collection_deque`, worker `std/shared`, `dispatch` or `parallel` `programs/sync_workers`, `sync` block `errors/pointer_leak`, generic body `programs/generic_nested`, library module `modules/defaults/com/example/greeter` | module level, `select` arm, `tests` block, `fixtures` block, nested type |
| read through `*p` | function `programs/alias_access`, method `programs/table_rewrite`, loop body `programs/channel_workers`, closure `std/collection_deque`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/concurrent`, `sync` block `programs/eq_handles`, `select` arm `programs/channel_workers`, generic body `programs/generic_variants`, library module `modules/generics/com/example/gen` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, `tests` block, `fixtures` block, nested type |
| `none` | module level `programs/alloc_owning`, function `programs/alias_access`, method `programs/concurrent`, loop body `programs/concurrent`, switch arm `programs/generic_variants`, catch handler `programs/catch_none`, worker `std/spsc_ring_threads`, `sync` block `programs/concurrent`, generic body `programs/generic_nested`, nested type `programs/generic_nested`, library module `modules/generics/com/example/gen` | `if let`, `defer`, `undo`, closure, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block |
| `??` | function `programs/closures`, loop body `std/collection_queue`, worker `std/concurrent_map_threads` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `p?.x` | function `errors/optional_values` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `lent` parameter | `programs/lent_derived` | none |
| `lent` result | `programs/lent_params` | none |
| `keep` parameter | `programs/closures` | none |
| iterator to slice, `to_slice` | function `programs/effects_edges` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `own` field or parameter | none | open |
| `delete(p, from)` and `destroy(p, from)` | `std/deserialize_alloc` | none |

The kinds of ownership:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `alloc` of a literal | pointer `programs/alias_access` | none |
| `alloc(T, n)` | nullable pointer `programs/alias_integers` | slice, pointer |
| `free` | pointer `programs/alias_integers`, nullable pointer `programs/channels` | slice |
| `delete` | pointer `programs/alias_class_pointers`, channel `programs/channel_workers` | nullable pointer |
| `dup` | pointer `programs/delete_owned`, lent value `programs/lent_params`, function value `programs/snapshots`, tuple `programs/owning_values`, variant `programs/variant_owning`, class `programs/caller_params`, generic parameter `programs/caller_params` | i8, i16, i32, i64, int, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, str, slice, fixed array, nullable pointer, optional value, may-fail function value, channel, struct, packed struct, simd struct, union, enum, synchronized class, concurrent class, interface |
| address `&x` | pointer `programs/alias_access` | none |
| read through `*p` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_view_local`, f16 `programs/half`, char `programs/alias_integers`, str `programs/alias_pointers`, slice `programs/alias_pointers`, fixed array `programs/alias_arrays`, pointer `programs/alias_access`, nullable pointer `programs/alias_pointers`, lent value `programs/for_loop`, enum `programs/alias_integers`, variant `programs/alias_unions`, class `programs/eq_compiled`, synchronized class `programs/sync_operators`, concurrent class `std/concurrent_map`, generic parameter `programs/owning_values` | i64, u8, uint, f64, bool, optional value, may-fail function value, function value, tuple, channel, struct, packed struct, simd struct, union, interface |
| `??` | int `programs/optional_values`, str `programs/optional_values`, pointer `programs/none_operators`, nullable pointer `programs/closures`, optional value `programs/optional_values`, function value `programs/closures`, tuple `std/concurrent_map`, class `std/collection_deque` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, slice, fixed array, lent value, may-fail function value, channel, struct, packed struct, simd struct, union, enum, variant, synchronized class, concurrent class, interface, generic parameter |

### Errors

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `may fail` function | module level `programs/catch_exits`, class body `programs/construct_args`, generic `programs/generic_may_fail`, library module `modules/chain/com/example/animal` | `tests` block |
| `fail` | function `programs/catch_exits`, method `programs/construct_args`, loop body `programs/walk_owned_parts`, catch handler `programs/catch_exits`, closure `programs/closures`, `sync` block `programs/sync_exits`, generic body `programs/generic_may_fail`, library module `modules/chain/com/example/animal` | switch arm, `if let`, `defer`, `undo`, worker, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block, nested type |
| `try` on a call | function `programs/closures`, method `programs/construct_base`, loop body `programs/pattern_walk`, `sync` block `programs/sync_exits`, generic body `programs/generic_may_fail`, library module `modules/failing/com/example/counter` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block, nested type |
| `try { } catch` | function `programs/catch_exits`, loop body `programs/catch_exits` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `catch e { }` on a call | function `programs/catch_exits`, method `errors/allow_levels`, loop body `programs/catch_exits`, worker `std/concurrent_map_threads`, `sync` block `programs/sync_exits`, `tests` block `errors/allow_levels` | switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `select` arm, `fixtures` block, generic body, nested type, library module |
| `catch { }` without a name | function `programs/catch_exits`, loop body `programs/catch_exits` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `catch fatal` | function `programs/construct_args`, method `std/interfaces`, loop body `std/sync_equal`, switch arm `programs/nested_locals`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/nested_locals`, worker `std/sync_equal` | `if let`, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `yield` | function `programs/catch_exits`, catch handler `programs/catch_exits` | method, loop body, switch arm, `if let`, `defer`, `undo`, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `undo` | function `programs/generic_may_fail`, generic body `programs/generic_may_fail` | method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type, library module |
| `-> never` | `programs/never_paths` | none |
| `here` as an argument | function `programs/location`, method `programs/location`, generic body `programs/walk_generic`, library module `modules/defaults/com/example/greeter` | module level, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| `catch none` | `programs/catch_none` | none |
| `allow(name, "reason")` | `programs/simd_big` | none |
| `unchecked(name, "reason")` | `programs/lock_free_queue` | none |
| `fail` with a text | `programs/closures` | none |

The kinds of errors:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `fail` | str `programs/closures`, pointer `programs/catch_exits` | nullable pointer |
| `yield` | int `programs/catch_exits`, str `programs/handled_operand`, slice `std/fs`, pointer `programs/none_operators`, nullable pointer `programs/catch_none`, optional value `programs/optional_values`, tuple `programs/generic_may_fail`, struct `std/reflect_call`, class `programs/construct_args` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, fixed array, lent value, may-fail function value, function value, channel, packed struct, simd struct, union, enum, variant, synchronized class, concurrent class, interface, generic parameter |

### Threads

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `worker fn` | module level `programs/channel_workers`, generic `programs/generic_fn_args`, library module `modules/closures/com/example/walk` | none |
| `parallel` | function `programs/parallel_sum` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `parallel ... by` | function `programs/channels`, loop body `programs/parallel_join`, library module `modules/closures/com/example/walk` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| `dispatch` | function `programs/channel_workers`, loop body `programs/parallel_join`, worker `programs/dispatch_job` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `join` | function `programs/channel_workers`, loop body `programs/parallel_join`, worker `programs/dispatch_job` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `join_all` | function `programs/dispatch_job` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `sync m { }` | function `programs/closures`, method `programs/concurrent`, loop body `programs/concurrent`, closure `programs/closures`, worker `programs/sync_pairs`, `sync` block `programs/concurrent`, generic body `programs/generic_sync`, library module `modules/sync/com/example/queue` | switch arm, `if let`, `defer`, `undo`, catch handler, `dispatch` or `parallel`, `select` arm, `tests` block, `fixtures` block, nested type |
| `Mutex.new()` | function `programs/closures`, library module `modules/sync/com/example/queue` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| `m.destroy()` of a Mutex | function `programs/closures` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `chan T(n)` | function `programs/channel_workers`, library module `modules/sync/com/example/queue` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| `send` | function `programs/channel_workers`, loop body `programs/channel_workers`, worker `programs/channel_workers` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `recv` | function `programs/channel_workers`, loop body `programs/channel_workers`, worker `programs/channel_workers`, library module `modules/sync/com/example/queue` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| `close` | function `programs/channel_workers`, worker `programs/channel_workers` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `select` | function `programs/channel_workers`, loop body `programs/channel_workers`, library module `modules/sync/com/example/queue` | method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type |
| atomic `load` | function `programs/alloc_owning`, method `programs/concurrent`, loop body `programs/parallel_join`, worker `programs/parallel_join`, generic body `programs/generic_sync` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type, library module |
| atomic `store` | function `programs/atomic_widths`, loop body `programs/parallel_join`, worker `programs/parallel_join` | module level, method, switch arm, `if let`, `defer`, `undo`, catch handler, closure, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| atomic `swap` | function `programs/atomic_widths` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| atomic `add` | function `programs/alloc_owning`, method `programs/atomics`, loop body `programs/sync_pairs`, switch arm `programs/variant_owning`, `if let` `programs/variant_owning`, closure `programs/concurrent`, worker `programs/dispatch_job`, generic body `programs/generic_sync`, library module `modules/sync/com/example/queue` | module level, `defer`, `undo`, catch handler, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, nested type |
| atomic `sub` | function `programs/atomic_widths`, method `programs/alloc_owning` | module level, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| atomic `and` | function `programs/atomic_widths` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| atomic `or` | function `programs/atomic_widths` | module level, method, loop body, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| atomic `compare_swap` | function `programs/atomic_widths`, method `programs/lock_free_queue`, loop body `programs/lock_free_queue` | module level, switch arm, `if let`, `defer`, `undo`, catch handler, closure, worker, `dispatch` or `parallel`, `sync` block, `select` arm, `tests` block, `fixtures` block, generic body, nested type, library module |
| `concurrent` parameter | `programs/closures` | none |
| `synchronized class` | `programs/generic_methods` | none |
| `concurrent class` | `programs/concurrent` | none |
| `atomic` field or local | `programs/concurrent` | none |
| `guarded by` on a field | `programs/concurrent` | none |
| `sync a, b { }` | `programs/sync_operators` | none |
| `snapshot fn` | `programs/construct_keep_own` | none |

The kinds of threads:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `chan T(n)` | int `programs/channel_workers`, channel `programs/channel_workers` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, str, slice, fixed array, pointer, nullable pointer, optional value, lent value, may-fail function value, function value, tuple, struct, packed struct, simd struct, union, enum, variant, class, synchronized class, concurrent class, interface, generic parameter |
| `send` | int `programs/channel_workers`, pointer `programs/channels`, channel `programs/channel_workers` | i8, i16, i32, i64, u8, u16, u32, u64, uint, byte, f32, f64, f16, bool, char, str, slice, fixed array, nullable pointer, optional value, lent value, may-fail function value, function value, tuple, struct, packed struct, simd struct, union, enum, variant, class, synchronized class, concurrent class, interface, generic parameter |
| atomic `load` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths`, bool `programs/atomic_widths`, nullable pointer `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, pointer, enum |
| atomic `store` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/atomic_widths`, byte `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, enum |
| atomic `swap` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `dump/atomics`, int `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, byte, enum |
| atomic `add` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, bool, pointer, nullable pointer, enum |
| atomic `sub` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, bool, pointer, nullable pointer, enum |
| atomic `and` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `dump/atomics`, int `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, byte, bool, pointer, nullable pointer, enum |
| atomic `or` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `dump/atomics`, int `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, byte, bool, pointer, nullable pointer, enum |
| atomic `compare_swap` | i8 `dump/atomics`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths` | i64, u8, u16, u32, u64, uint, byte, enum |

### Classes

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `class` | module level `programs/alias_class_pointers`, nested type `programs/generic_nested`, generic `programs/generic_constants`, library module `modules/access/com/example/ledger` | none |
| `abstract class` | module level `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `singleton class` | module level `programs/inherits_header`, library module `modules/com/example/board` | none |
| `final class` | module level `programs/classes`, library module `modules/sealed/com/example/sealed` | none |
| `inherits` | `programs/alias_class_pointers` | none |
| function with `self` | class body `programs/alias_class_pointers`, nested type `programs/generic_nested`, generic `programs/generic_constants`, library module `modules/access/com/example/ledger` | none |
| static function of a class | class body `programs/catch_exits`, generic `programs/generic_static`, library module `modules/access/com/example/ledger` | nested type |
| `abstract fn` | class body `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `concrete fn` | class body `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/chain/com/example/dog` | none |
| `concrete fn Base::f` | class body `errors/concrete_signature` | library module |
| `operator fn` | class body `programs/caller_params`, generic `programs/generic_hooks`, library module `modules/generic_hooks/com/example/row` | module level |
| `construct` | class body `programs/class_bitfields`, generic `programs/generic_constants`, library module `modules/chain/com/example/animal` | nested type |
| `destruct` | class body `programs/alloc_owning`, generic `programs/generic_copies`, library module `modules/chain/com/example/animal` | nested type |
| constant of a class | class body `programs/alloc_owning` | nested type, generic, library module |
| field with `implements` | `programs/alias_class_pointers` | none |
| field with `use` | `programs/checked_once` | none |
| `mutable` field | `programs/inherits_header` | none |
| `pub` member | `programs/alias_class_pointers` | none |
| `protected` member | `programs/protected_calls` | none |
| `self.super.f()` | `programs/construct_base` | none |
| `static` member | none | open |
| `inject` field | `programs/inject_default` | none |
| `inject final` field | `inject/service` | none |
| `transient` field | `programs/transient` | none |
| `trace class` and `trace fn` | `programs/hook_trace` | none |
| `compatible` line | `plugin/versions/floor/net/example/greet` | none |

### Modules

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `import a.b` | `programs/argument_order` | none |
| `import a.b.{x, y}` | `std/collection` | none |
| `fn` at module level | module level `programs/alias_access`, generic `programs/caller_params`, `tests` block `programs/test_blocks`, `fixtures` block `programs/test_blocks`, library module `modules/access/com/example/ledger` | none |
| `const` at module level | module level `programs/checker_context`, library module `modules/com/example/scale` | none |
| `pub` item | module level `programs/inherits_header`, generic `modules/access/com/example/ledger`, library module `modules/access/com/example/ledger` | none |
| `export` item | module level `abi/abi_structs`, class body `clib/com/example/canvas`, library module `clib/com/example/geo`, generic `errors/generics` | none |
| `extern fn` | module level `programs/alias_access`, library module `modules/chain/com/example/animal` | none |
| variadic `extern fn` | `programs/alias_access` | none |
| `provides I as C` | `dump/devirt_plugin` | none |
| module doc comment `//!` | `programs/alias_access` | none |
| doc comment `///` | `programs/alias_access` | none |
| note `//#` | `modules/blocks/com/example/geo` | none |
| `import a.b as c` | `std/sync_equal` | none |
| `internal` item | `modules/internal/com/example/core` | none |
| `link framework` | `anti-build/clock/src/com/example/cf` | none |
| `link linux` | `anti-build/window/src/com/example/xlib` | none |
| `tests` block | `programs/test_blocks` | none |
| `fixtures` block | `programs/test_blocks` | none |
| clause at the top of the file | none | open |

### The rules of decisions 19 to 27

"Language rules" under "Decisions of 2026-10-11" in `docs/decisions.md`
holds nine rules that the test run tests. No expected output under `tests/`
held a refusal of any of them on 2026-10-12, so every cell was open. Where
antic 0.2.0 does otherwise, the test becomes an expected failure by decision
30. A matrix step writes its tests into the last column.

| Decision | Area | The refusal or the check | State |
|---|---|---|---|
| 19 | flow, errors | `return`, `break`, `continue`, `fail` and `try`, each inside a `defer` and inside an `undo` | T001, the twelve refusals `matrix/flow_*_in_defer`, `_in_undo`, `_out_of_defer` and `_out_of_undo` that "Flow" names |
| 20 | errors | a `catch` handler of a value that can reach its end | open |
| 21 | errors | `try` on an optional, with a message that names `let ... else` and `??` | open |
| 22 | ownership | `return` of a field that owns something | open |
| 23 | threads | a second `join` of one job, refused where the checker sees it and trapped in dev mode otherwise | open |
| 23 | threads | the arguments of a `dispatch` in a loop, stored per job | open |
| 24 | values | a constant index past the end of a fixed array | open |
| 24 | values | `xs[lo..hi]` past the length, trapped in dev mode | open |
| 25 | modules | a write of `count` of a collection outside its module | open |
| 26 | modules | a `str` in the signature of an `extern fn` | open |
| 27 | classes | a class with an `abstract fn` that is not `abstract` | open |
| 27 | classes | a direct call of `destruct` | open |
| 27 | classes | `mutable` on a field of a plain class | open |
| 27 | classes | `static atomic` on an `f64` | open |
| 27 | classes | `==` on pointers of two unrelated classes | open |
| 27 | classes | `equals` replaced without `hash` | open |
| 27 | values | `<` on an enum, which stays refused | open |
| 27 | values | `+=` on a type with `operator fn add`, which is not derived | open |

antic has a message for the direct call of `destruct` already.
`tests/diagnostics-open.txt` lists it, since no test expects it.

## The two guards

Two tests hold what the matrix cannot: the messages and the options.

`diagnostics_covered` runs `tests/run_diagnostics_covered.cmake`:

- It reads every message that `src/antic/` gives through `diagnostics_add`,
  `diagnostics_warn`, `diagnostics_check` and `diagnostics_doc`. Seven
  functions hand a text on to those four, and the table `TEXTS_ANTIC` of the
  runner names them.
- It reads every text that `src/anti/` writes to stderr with `fprintf`,
  `fputs` and `vfprintf`, and through the three functions of `TEXTS_ANTI`.
- A message is its format. A conditional gives one message per branch, and
  a macro between the pieces of a literal is replaced by its text.
- The format turns into a pattern, in which a conversion matches any text
  of one line.
- The expected outputs are the `.err`, `.expected`, `.pattern` and
  `.warnings` files under `tests/`, the CMake files and the C files of
  `tests/unit/`.
- A message of antic must stand whole in one: after `error: `, `warning: `
  or a `"`, and before the end of the line, a ` [name]` or a `"`.
- A message of anti that ends its line must end there in the output.
- A format of anti with fewer than two words beside `anti:` hands a text on
  and is no message.

`options_covered` runs `tests/run_options_covered.cmake`:

- It reads every `strcmp(arg, "-...")` of `src/antic/main.c` as an option.
- It reads `src/anti/main.c` function by function. `strcmp(argv[1], ...)`
  names a command or an option of anti itself, `strcmp(argv[2], ...)` a
  sub-command and every other comparison with `-...` an option. The function
  names the command, as `doc_command` and `build_options` do.
- It asks ctest for the command line of every registered test. A unit of
  antic is passed when a line holds the path of antic and the option.
- A unit of anti is passed when a line holds the path of anti, the command
  and the option in that order.
- A runner under `tests/` passes a unit of antic when it holds `${ANTIC}`
  and the option.
- It passes a unit of anti when one of its commands holds the command of
  anti and then the option, and the runner holds `${ANTI}`.
- An option also counts in a runner that gives anti that one command alone.
- It counts in a function of a runner that adds it to its `${ARGN}`, when a
  call of the function holds the command.

Each guard reads a list of what is open, `tests/diagnostics-open.txt` and
`tests/options-open.txt`, and fails in four cases:

- A message or an option is neither covered nor listed.
- A line names one that a test covers, so the line must go.
- A line names nothing of the sources.
- A line stands twice.

The diagnostics guard fails in a fifth case: the text of a call is a variable,
in a function the runner does not know. A new function that hands a format on
therefore cannot hide its callers.

`diagnostics_covered_runner` and `options_covered_runner` build a small tree
for each of these cases and run the runner on it.

What the guards do not see:

- The messages that the driver of antic prints with `fprintf`, such as
  `antic: unknown option`. The four functions do not give them.
- A message that reaches a test in part. A runner that looks for
  `is no archive` does not hold `anti: %s is no archive`.
- A message cut at 159 bytes, the room of `struct diagnostic`.
- An option that only a variable of a runner holds.
- The values of an option, such as `static` and `shared` of `--lib`.
- An option that anti passes to antic by itself, such as `--dependency`.
  A test must pass it to antic to count.

## Counts

On 2026-10-12, at antic 0.2.0:

- 100 keywords, 64 symbols and 27
  contextual words.
- 10 item kinds, 22 statement kinds, 38 expression kinds and 10 type kinds
  in `src/antic/ast.h`.
- 206 rows in the matrix. 1157 cells are held and 1179 are
  open. Of the kinds, 622 are held and 871 are open. The step `flow` filled
  its area after the count of 840 held cells and 578 held kinds.
- 17 open cells for the rules of decisions 19 to 27.
- 748 messages: 494 expected by a test and 254 open. antic gives 598 with
  116 open, and anti 150 with 138 open.
- 134 options and commands: 124 passed by a test and 10 open.
