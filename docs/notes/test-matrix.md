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
| `[N]T` | `TYPEX_ARRAY` |, slice literal `matrix/types_forms`, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `[]T` | `TYPEX_SLICE` |, `as` `matrix/types_as_to_a_slice`, `size_of` `matrix/types_forms`, `const` `matrix/values_const_slice`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `fn(A) -> R`, `fn(A) -> R may fail`, `?fn(A)`, `keep fn`, `concurrent fn` | `TYPEX_FN` |
| `(A, B)` | `TYPEX_TUPLE` |
| `chan T` | `TYPEX_CHAN` |, `let` `matrix/types_forms`, `as` `matrix/types_as_to_a_channel`, `alloc` `matrix/types_forms`, `size_of` `matrix/types_forms`, `const` `matrix/values_const_channel`, slice literal `matrix/types_forms`, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| a constant as a type argument, `Ring<int, 8>` | `TYPEX_CONST` |
| `?T` | `TYPEX_OPTIONAL` |, `as` `matrix/types_as_to_an_optional`, `const` `matrix/values_const_optional` T012, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |

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
| Flow | 27 | 455 | 0 | 91 | 0 |
| Values | 40 | 753 | 0 | 1038 | 1 |
| Types | 32 | 194 | 20 | 0 | 0 |
| Ownership | 18 | 248 | 1 | 125 | 0 |
| Errors | 15 | 173 | 0 | 41 | 0 |
| Threads | 29 | 402 | 4 | 189 | 7 |
| Classes | 26 | 58 | 0 | 0 | 0 |
| Modules | 19 | 30 | 0 | 0 | 0 |
| All | 206 | 2313 | 25 | 1484 | 8 |

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
| `return` of a value | function `programs/alias_access`, method `programs/atomics`, loop body `programs/args`, switch arm `programs/switch_stmt`, `if let` `programs/generic_variants`, `defer` `matrix/flow_return_value_in_defer` T001, `undo` `matrix/flow_return_value_in_undo` T001, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/closures`, `sync` block `programs/concurrent`, `select` arm `matrix/flow_return_value`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger`, `tests` block `anti-test/com/example/flow` | none |
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
| `flow_yield_in_defer`, `flow_yield_in_undo`, `flow_yield_in_handler_in_defer` | R002 of "Rules of 2026-10-13": a `yield` inside a `defer` or an `undo` of a handler is refused, and one in a handler that the statement holds is taken | |
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
| `flow_undo_in_defer`, `flow_undo_in_the_block_of_an_undo`, `flow_undo_in_a_function_that_cannot_fail`, `flow_undo_in_a_closure_that_cannot_fail` | R005 of "Rules of 2026-10-13": an `undo` in the block of a `defer` or of an `undo` is refused, and one in a function that cannot fail is the warning `undo-never-runs` | |
| `flow_for_iterator_exits` | the exits of a `for` over an iterator of the program, with a `defer` | |
| `flow_switch_value_once`, `flow_in_range_value_once` | the value of a `switch` and of `x in lo..hi` is computed once | |
| `flow_for_range_no_binding` | a range without a binding, an empty one and one that runs backwards | |
| `flow_if_let_chain` | `else if let`, `else if` and `else` after an `if let` | |
| `flow_fail_in_catch`, `flow_try_in_catch`, `flow_fail_in_closure`, `flow_try_in_closure`, `flow_catch_in_worker` | `fail` and `try` in a handler and in a closure, and a handler in a worker | |
| `flow_fail_in_worker_dispatched`, `flow_try_in_worker_parallel` | a worker that may fail is refused at `dispatch` and at `parallel` | |
| `flow_loop_variable_in_closure`, `flow_arm_binding_in_closure`, `flow_bindings_in_defer` | the names a `for`, an arm, an `if let` and a `catch` bind, read by a closure or a `defer` | |
| `flow_switch_arms_all_return`, `flow_while_true_no_return` | a `switch` whose arms all return and a `while true` do not end the path of a function | |
| `flow_if_let_case_arms_all_return`, `flow_if_let_arms_all_return` | an `if let` whose arm and `else` return ends the path, on a case of a variant and on a `?T` | T007, the second |
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
| integer literal | module level `programs/alias_arrays`, function `programs/alias_access`, method `programs/alloc_owning`, loop body `programs/alloc_owning`, switch arm `programs/alias_unions`, `if let` `programs/variant_owning`, `defer` `programs/closures`, `undo` `programs/nested_locals`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, `tests` block `programs/test_blocks`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, library module `modules/access/com/example/ledger`, nested type `matrix/values_literals` | none |
| float literal | module level `programs/classes`, function `programs/alias_bytes`, method `programs/classes`, loop body `programs/effects_edges`, switch arm `programs/variants`, `if let` `programs/generic_variants`, catch handler `programs/construct_args`, closure `programs/generic_methods`, library module `modules/blocks/com/example/geo`, `defer` `matrix/values_literals`, `undo` `matrix/values_literals`, worker `matrix/values_literals`, `dispatch` or `parallel` `matrix/values_literals`, `sync` block `matrix/values_literals`, `select` arm `matrix/values_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_literals`, nested type `matrix/values_literals` | none |
| character literal | module level `std/reflect`, function `programs/alias_integers`, loop body `programs/utf8`, method `matrix/values_literals`, switch arm `matrix/values_literals`, `if let` `matrix/values_literals`, `defer` `matrix/values_literals`, `undo` `matrix/values_literals`, catch handler `matrix/values_literals`, closure `matrix/values_literals`, worker `matrix/values_literals`, `dispatch` or `parallel` `matrix/values_literals`, `sync` block `matrix/values_literals`, `select` arm `matrix/values_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_literals`, nested type `matrix/values_literals`, library module `modules/values/com/example/values` | none |
| string literal `"..."` | module level `programs/class_literal_defaults`, function `programs/alias_access`, method `programs/array_local`, loop body `programs/arith_facts`, switch arm `programs/generic_copies`, `if let` `programs/variant_owning`, `defer` `programs/may_fail`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, `sync` block `programs/eq_handles`, `tests` block `inject/standard/src/com/example/standard`, `fixtures` block `programs/test_blocks`, generic body `programs/generic_copies`, library module `modules/chain/com/example/animal`, worker `matrix/values_literals`, `dispatch` or `parallel` `matrix/values_literals`, `select` arm `matrix/values_literals`, nested type `matrix/values_literals` | none |
| raw string `r"..."` | function `programs/pattern_match`, loop body `programs/pattern_walk`, module level `matrix/values_at_module_level`, method `matrix/values_text_literals`, switch arm `matrix/values_text_literals`, `if let` `matrix/values_text_literals`, `defer` `matrix/values_text_literals`, `undo` `matrix/values_text_literals`, catch handler `matrix/values_text_literals`, closure `matrix/values_text_literals`, worker `matrix/values_text_literals`, `dispatch` or `parallel` `matrix/values_text_literals`, `sync` block `matrix/values_text_literals`, `select` arm `matrix/values_text_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_text_literals`, nested type `matrix/values_text_literals`, library module `modules/values/com/example/values` | none |
| byte string `b"..."` and `br"..."` | function `programs/args`, loop body `programs/args`, module level `matrix/values_at_module_level`, method `matrix/values_text_literals`, switch arm `matrix/values_text_literals`, `if let` `matrix/values_text_literals`, `defer` `matrix/values_text_literals`, `undo` `matrix/values_text_literals`, catch handler `matrix/values_text_literals`, closure `matrix/values_text_literals`, worker `matrix/values_text_literals`, `dispatch` or `parallel` `matrix/values_text_literals`, `sync` block `matrix/values_text_literals`, `select` arm `matrix/values_text_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_text_literals`, nested type `matrix/values_text_literals`, library module `modules/values/com/example/values` | none |
| hex string `x"..."` | function `programs/hex_bytes`, module level `matrix/values_at_module_level`, method `matrix/values_text_literals`, loop body `matrix/values_text_literals`, switch arm `matrix/values_text_literals`, `if let` `matrix/values_text_literals`, `defer` `matrix/values_text_literals`, `undo` `matrix/values_text_literals`, catch handler `matrix/values_text_literals`, closure `matrix/values_text_literals`, worker `matrix/values_text_literals`, `dispatch` or `parallel` `matrix/values_text_literals`, `sync` block `matrix/values_text_literals`, `select` arm `matrix/values_text_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_text_literals`, nested type `matrix/values_text_literals`, library module `modules/values/com/example/values` | none |
| interpolation `f"..."` and `rf"..."` | module level `errors/format_types`, function `programs/eq_default`, method `programs/format_strings`, loop body `programs/format_strings`, catch handler `std/collection_pool`, generic body `modules/generics/com/example/gen`, library module `modules/generics/com/example/gen`, switch arm `matrix/values_text_literals`, `if let` `matrix/values_text_literals`, `defer` `matrix/values_text_literals`, `undo` `matrix/values_text_literals`, closure `matrix/values_text_literals`, worker `matrix/values_text_literals`, `dispatch` or `parallel` `matrix/values_text_literals`, `sync` block `matrix/values_text_literals`, `select` arm `matrix/values_text_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_text_literals` | none |
| pattern literal `re"..."` | module level `errors/pattern_malformed`, function `programs/eq_handles`, loop body `programs/pattern_bytes`, method `matrix/values_text_literals`, switch arm `matrix/values_text_literals`, `if let` `matrix/values_text_literals`, `defer` `matrix/values_text_literals`, `undo` `matrix/values_text_literals`, catch handler `matrix/values_text_literals`, closure `matrix/values_text_literals`, worker `matrix/values_text_literals`, `dispatch` or `parallel` `matrix/values_text_literals`, `sync` block `matrix/values_text_literals`, `select` arm `matrix/values_text_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_text_literals`, nested type `matrix/values_text_literals`, library module `modules/values/com/example/values` | none |
| `true` and `false` | module level `std/reflect`, function `programs/args`, method `programs/eq_default`, loop body `programs/args`, `if let` `errors/if_let_moves`, catch handler `programs/closures`, closure `std/hash_map`, worker `programs/channel_workers`, generic body `modules/generic_hooks/com/example/row`, library module `modules/generic_hooks/com/example/row`, switch arm `matrix/values_literals`, `defer` `matrix/values_literals`, `undo` `matrix/values_literals`, `dispatch` or `parallel` `matrix/values_literals`, `sync` block `matrix/values_literals`, `select` arm `matrix/values_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_literals` | none |
| `here` | function `programs/location`, method `programs/location`, generic body `programs/walk_generic`, library module `modules/defaults/com/example/greeter`, module level `matrix/values_at_module_level`, loop body `matrix/values_literals`, switch arm `matrix/values_literals`, `if let` `matrix/values_literals`, `defer` `matrix/values_literals`, `undo` `matrix/values_literals`, catch handler `matrix/values_literals`, closure `matrix/values_literals`, worker `matrix/values_literals`, `dispatch` or `parallel` `matrix/values_literals`, `sync` block `matrix/values_literals`, `select` arm `matrix/values_literals`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_literals` | none |
| array literal `[a, b]` | module level `programs/const_data`, function `programs/alias_arrays`, method `traps/collection_changed`, loop body `bench/ablate/mixed_work`, generic body `modules/generic_hooks/com/example/row`, library module `modules/generic_hooks/com/example/row`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, catch handler `matrix/values_aggregates`, closure `matrix/values_aggregates`, worker `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_aggregates` | none |
| array repeat `[v; n]` | module level `programs/index_several`, function `programs/generic_constants`, method `programs/generic_constants`, generic body `programs/generic_constants`, library module `modules/iteration/com/example/bag`, loop body `matrix/values_aggregates`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, catch handler `matrix/values_aggregates`, closure `matrix/values_aggregates`, worker `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_aggregates` | none |
| slice literal | function `programs/assign_owned`, method `programs/generic_copies`, catch handler `std/fs`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen`, module level `matrix/values_slice_literal_at_module_level`, loop body `matrix/values_aggregates`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, closure `matrix/values_aggregates`, worker `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_aggregates` | none |
| struct literal | module level `programs/checked_once`, function `programs/alias_access`, method `programs/catch_exits`, loop body `programs/arith_facts`, switch arm `programs/variants`, `if let` `programs/variants`, catch handler `programs/construct_args`, worker `programs/concurrent`, `tests` block `inject/project/src/com/example/app`, generic body `programs/eq_default`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, closure `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `fixtures` block `anti-test/com/example/values` | none |
| tuple `(a, b)` | module level `programs/for_patterns`, function `programs/alias_view_local`, method `programs/for_patterns`, loop body `std/concurrent_map_threads`, catch handler `programs/generic_may_fail`, worker `std/concurrent_map_threads`, generic body `programs/caller_params`, library module `modules/generics/com/example/gen`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, closure `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_aggregates` | none |
| tuple element `t.0` | function `programs/alias_view_local`, method `programs/index_several`, loop body `programs/lent_tuples`, closure `std/collection_parts`, worker `std/concurrent_map_threads`, module level `matrix/values_at_module_level`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, catch handler `matrix/values_aggregates`, `dispatch` or `parallel` `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_aggregates`, nested type `matrix/values_aggregates`, library module `modules/values/com/example/values` | none |
| `let` | function `programs/alias_access`, method `programs/caller_params`, loop body `programs/arith_facts`, switch arm `programs/generic_variants`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/catch_moves`, closure `programs/snapshots`, worker `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, `tests` block `programs/test_blocks`, `fixtures` block `anti-test/com/example/stack`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/closures/com/example/walk`, `if let` `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls` | none |
| `let` with a type | function `programs/alias_bytes`, method `programs/none_operators`, loop body `programs/effects_edges`, generic body `dump/constraints`, library module `modules/variants/com/example/geo`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `defer` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, closure `matrix/values_bindings_and_calls`, worker `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_bindings_and_calls` | none |
| `let (a, b) = e` | function `programs/carry_chain`, loop body `programs/carry_chain`, worker `std/concurrent_map_threads`, method `matrix/values_bindings_and_calls`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `defer` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, closure `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_bindings_and_calls`, nested type `matrix/values_bindings_and_calls`, library module `modules/values/com/example/values` | none |
| `const` in a body | function `matrix/values_bindings_and_calls`, method `matrix/values_bindings_and_calls`, loop body `matrix/values_bindings_and_calls`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `defer` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, closure `matrix/values_bindings_and_calls`, worker `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_bindings_and_calls`, nested type `matrix/values_bindings_and_calls`, library module `modules/values/com/example/values` | none |
| `=` | function `programs/alias_access`, method `programs/alias_class_pointers`, loop body `programs/alloc_owning`, switch arm `programs/alias_unions`, catch handler `programs/closures`, closure `programs/closures`, worker `programs/generic_fn_args`, `sync` block `programs/concurrent`, generic body `programs/generic_constants`, nested type `programs/generic_nested`, library module `modules/chain/com/example/animal`, `if let` `matrix/values_bits_and_assignments`, `defer` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `select` arm `matrix/values_bits_and_assignments`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values` | none |
| compound assignment, `+=` to `>>=` | function `programs/alloc_owning`, method `programs/class_literal_defaults`, loop body `programs/alloc_owning`, switch arm `programs/switch_fallthrough`, `defer` `programs/closures`, catch handler `std/concurrent_map_threads`, closure `programs/closures`, worker `programs/channel_workers`, `sync` block `programs/concurrent`, `select` arm `programs/channel_workers`, generic body `programs/for_patterns`, library module `modules/access/com/example/ledger`, `if let` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_bits_and_assignments` | none |
| wrapping and saturating assignment, `+%=` to `*\|=` | function `programs/arith_facts`, loop body `programs/arith_facts`, method `matrix/values_bits_and_assignments`, switch arm `matrix/values_bits_and_assignments`, `if let` `matrix/values_bits_and_assignments`, `defer` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, catch handler `matrix/values_bits_and_assignments`, closure `matrix/values_bits_and_assignments`, worker `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `sync` block `matrix/values_bits_and_assignments`, `select` arm `matrix/values_bits_and_assignments`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_bits_and_assignments`, nested type `matrix/values_bits_and_assignments`, library module `modules/values/com/example/values` | none |
| arithmetic operator, `+ - * / %` | module level `programs/checker_context`, function `programs/alias_c_functions`, method `programs/alias_class_pointers`, loop body `programs/by_ends`, switch arm `programs/alias_unions`, `if let` `programs/generic_variants`, `defer` `programs/defer_stmt`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/closures`, `sync` block `programs/sync_pairs`, generic body `programs/generic_constants`, nested type `programs/generic_nested`, library module `modules/blocks/com/example/geo`, `undo` `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values` | none |
| wrapping operator, `+% -% *% <<%` | module level `programs/wrapping`, function `programs/arith_facts`, method `bench/objects`, loop body `programs/arith_facts`, switch arm `matrix/values_arithmetic`, `if let` `matrix/values_arithmetic`, `defer` `matrix/values_arithmetic`, `undo` `matrix/values_arithmetic`, catch handler `matrix/values_arithmetic`, closure `matrix/values_arithmetic`, worker `matrix/values_arithmetic`, `dispatch` or `parallel` `matrix/values_arithmetic`, `sync` block `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_arithmetic`, nested type `matrix/values_arithmetic`, library module `modules/values/com/example/values` | none |
| saturating operator, `+\| -\| *\|` | module level `programs/saturating`, function `programs/saturating`, method `matrix/values_arithmetic`, loop body `matrix/values_arithmetic`, switch arm `matrix/values_arithmetic`, `if let` `matrix/values_arithmetic`, `defer` `matrix/values_arithmetic`, `undo` `matrix/values_arithmetic`, catch handler `matrix/values_arithmetic`, closure `matrix/values_arithmetic`, worker `matrix/values_arithmetic`, `dispatch` or `parallel` `matrix/values_arithmetic`, `sync` block `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_arithmetic`, nested type `matrix/values_arithmetic`, library module `modules/values/com/example/values` | none |
| shift, `<< >>` | module level `programs/mul_high`, function `programs/flags`, loop body `programs/llvm_division`, generic body `dump/generics_syntax`, method `matrix/values_bits_and_assignments`, switch arm `matrix/values_bits_and_assignments`, `if let` `matrix/values_bits_and_assignments`, `defer` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, catch handler `matrix/values_bits_and_assignments`, closure `matrix/values_bits_and_assignments`, worker `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `sync` block `matrix/values_bits_and_assignments`, `select` arm `matrix/values_bits_and_assignments`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_bits_and_assignments`, library module `modules/values/com/example/values` | none |
| bitwise operator, `& \| ^ ~` | function `programs/compare_bits`, method `programs/union_name`, loop body `programs/scalar_ops`, module level `matrix/values_at_module_level`, switch arm `matrix/values_bits_and_assignments`, `if let` `matrix/values_bits_and_assignments`, `defer` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, catch handler `matrix/values_bits_and_assignments`, closure `matrix/values_bits_and_assignments`, worker `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `sync` block `matrix/values_bits_and_assignments`, `select` arm `matrix/values_bits_and_assignments`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_bits_and_assignments`, nested type `matrix/values_bits_and_assignments`, library module `modules/values/com/example/values` | none |
| comparison, `== != < <= > >=` | module level `programs/const_text_compare`, function `programs/alias_access`, method `programs/caller_params`, loop body `programs/alloc_owning`, switch arm `programs/generic_variants`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/lock_free_queue`, `sync` block `programs/concurrent`, `tests` block `programs/test_blocks`, generic body `programs/caller_params`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger`, `if let` `matrix/values_bits_and_assignments`, `defer` `matrix/values_bits_and_assignments`, `undo` `matrix/values_bits_and_assignments`, `dispatch` or `parallel` `matrix/values_bits_and_assignments`, `select` arm `matrix/values_bits_and_assignments`, `fixtures` block `anti-test/com/example/values` | none |
| unary `-` and `!` | module level `programs/class_bitfields`, function `programs/alias_bytes`, method `programs/eq_default`, loop body `programs/arith_facts`, catch handler `programs/closures`, closure `programs/generic_fn_args`, worker `programs/sync_pairs`, `dispatch` or `parallel` `std/sync_map_threads`, `sync` block `programs/sync_exits`, `tests` block `anti-test/com/example/greeting`, generic body `programs/generic_hooks`, library module `modules/generic_hooks/com/example/row`, switch arm `matrix/values_arithmetic`, `if let` `matrix/values_arithmetic`, `defer` `matrix/values_arithmetic`, `undo` `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_arithmetic` | none |
| `mul_high` | module level `programs/mul_high`, function `programs/mul_high`, method `matrix/values_arithmetic`, loop body `matrix/values_arithmetic`, switch arm `matrix/values_arithmetic`, `if let` `matrix/values_arithmetic`, `defer` `matrix/values_arithmetic`, `undo` `matrix/values_arithmetic`, catch handler `matrix/values_arithmetic`, closure `matrix/values_arithmetic`, worker `matrix/values_arithmetic`, `dispatch` or `parallel` `matrix/values_arithmetic`, `sync` block `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_arithmetic`, nested type `matrix/values_arithmetic`, library module `modules/values/com/example/values` | none |
| `as` | module level `programs/eq_hash_agree`, function `programs/alias_access`, method `programs/array_local`, loop body `programs/arith_facts`, switch arm `programs/generic_copies`, `if let` `programs/generic_variants`, catch handler `programs/catch_exits`, closure `programs/effects_edges`, worker `traps/lock_order`, generic body `programs/generic_copies`, nested type `programs/generic_nested`, library module `modules/chain/com/example/animal`, `defer` `matrix/values_arithmetic`, `undo` `matrix/values_arithmetic`, `dispatch` or `parallel` `matrix/values_arithmetic`, `sync` block `matrix/values_arithmetic`, `select` arm `matrix/values_arithmetic`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values` | none |
| index `e[i]` | function `programs/alias_arrays`, method `programs/for_patterns`, loop body `programs/alloc_owning`, closure `std/collection_grid`, worker `programs/generic_fn_args`, `dispatch` or `parallel` `errors/generic_copies`, `sync` block `programs/eq_hash_agree`, generic body `programs/generic_constants`, library module `modules/generic_hooks/com/example/row`, module level `matrix/values_at_module_level`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, catch handler `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_aggregates` | none |
| slice `e[lo..hi]` | function `programs/alias_arrays`, method `programs/lent_slices`, loop body `std/fs`, closure `programs/closures`, `dispatch` or `parallel` `programs/concurrent`, module level `matrix/values_slice_at_module_level`, switch arm `matrix/values_aggregates`, `if let` `matrix/values_aggregates`, `defer` `matrix/values_aggregates`, `undo` `matrix/values_aggregates`, catch handler `matrix/values_aggregates`, worker `matrix/values_aggregates`, `sync` block `matrix/values_aggregates`, `select` arm `matrix/values_aggregates`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_aggregates`, nested type `matrix/values_aggregates`, library module `modules/values/com/example/values` | none |
| field `e.x` | module level `programs/alias_access`, function `programs/alias_access`, method `programs/alias_class_pointers`, loop body `programs/args`, switch arm `programs/alias_unions`, `if let` `programs/generic_variants`, `defer` `programs/may_fail`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/sync_workers`, `sync` block `programs/concurrent`, `tests` block `inject/project/src/com/example/app`, `fixtures` block `programs/test_blocks`, generic body `programs/eq_default`, nested type `programs/generic_nested`, library module `modules/access/com/example/ledger`, `select` arm `matrix/values_aggregates` | none |
| call | function `programs/alias_access`, method `programs/array_local`, loop body `programs/args`, switch arm `programs/generic_copies`, `if let` `programs/generic_variants`, `defer` `programs/closures`, `undo` `programs/generic_may_fail`, catch handler `programs/catch_exits`, closure `programs/closures`, worker `programs/closures`, `dispatch` or `parallel` `programs/channel_workers`, `sync` block `programs/eq_handles`, `tests` block `programs/test_blocks`, `fixtures` block `programs/test_blocks`, generic body `programs/caller_params`, library module `modules/access/com/example/ledger`, module level `matrix/values_call_at_module_level`, `select` arm `matrix/values_bindings_and_calls`, nested type `matrix/values_bindings_and_calls` | none |
| `size_of` | module level `programs/in_range`, function `programs/alias_view_passed`, method `programs/generic_copies`, generic body `programs/generic_copies`, nested type `programs/generic_nested`, library module `modules/generics/com/example/gen`, loop body `matrix/values_bindings_and_calls`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `defer` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, closure `matrix/values_bindings_and_calls`, worker `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values` | none |
| anonymous function `fn(x) { }` | module level `errors/closures`, function `programs/closures`, method `programs/closures`, loop body `programs/closures`, `defer` `programs/closures`, closure `programs/closures`, worker `std/concurrent_map_threads`, `dispatch` or `parallel` `programs/closures`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, nested type `matrix/values_bindings_and_calls` | none |
| simd built-in | function `programs/simd`, loop body `bench/simd_loop`, module level `matrix/values_simd_at_module_level`, method `matrix/values_bindings_and_calls`, switch arm `matrix/values_bindings_and_calls`, `if let` `matrix/values_bindings_and_calls`, `defer` `matrix/values_bindings_and_calls`, `undo` `matrix/values_bindings_and_calls`, catch handler `matrix/values_bindings_and_calls`, closure `matrix/values_bindings_and_calls`, worker `matrix/values_bindings_and_calls`, `dispatch` or `parallel` `matrix/values_bindings_and_calls`, `sync` block `matrix/values_bindings_and_calls`, `select` arm `matrix/values_bindings_and_calls`, `tests` block `anti-test/com/example/values`, `fixtures` block `anti-test/com/example/values`, generic body `matrix/values_bindings_and_calls`, nested type `matrix/values_bindings_and_calls`, library module `modules/values/com/example/values` | none |

The kinds of values:

| Construct | Kinds held | Kinds open |
|---|---|---|
| integer literal | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_c_functions`, byte `programs/alias_bytes`, i64 `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, f32 `matrix/values_int_literal_as_f32`, f64 `matrix/values_int_literal_as_f64`, simd struct `matrix/values_int_literal_as_simd`, char `matrix/values_int_literal_as_char` | none |
| float literal | f32 `programs/alias_simd`, i8 `matrix/values_float_literal_as_i8`, i16 `matrix/values_float_literal_as_i16`, i32 `matrix/values_float_literal_as_i32`, i64 `matrix/values_float_literal_as_i64`, int `matrix/values_float_literal_as_int`, u8 `matrix/values_float_literal_as_u8`, u16 `matrix/values_float_literal_as_u16`, u32 `matrix/values_float_literal_as_u32`, u64 `matrix/values_float_literal_as_u64`, uint `matrix/values_float_literal_as_uint`, byte `matrix/values_float_literal_as_byte`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_float_literal_as_simd`, char `matrix/values_float_literal_as_char` | none |
| character literal | char `programs/alias_integers`, byte `matrix/values_char_literal_as_byte`, i8 `matrix/values_char_literal_as_i8`, i16 `matrix/values_char_literal_as_i16`, i32 `matrix/values_char_literal_as_i32`, i64 `matrix/values_char_literal_as_i64`, int `matrix/values_char_literal_as_int`, u8 `matrix/values_char_literal_as_u8`, u16 `matrix/values_char_literal_as_u16`, u32 `matrix/values_char_literal_as_u32`, u64 `matrix/values_char_literal_as_u64`, uint `matrix/values_char_literal_as_uint` | none |
| array literal `[a, b]` | i32 `programs/alias_c_functions`, int `programs/alias_arrays`, u64 `programs/carry_chain`, byte `programs/checker_context`, f32 `programs/generic_constants`, f16 `programs/half`, bool `std/collection_ring`, str `programs/eq_arrays`, fixed array `programs/alias_arrays`, pointer `programs/object_model`, optional value `programs/owning_locals`, tuple `programs/for_patterns`, struct `programs/dispatch_job`, enum `std/reflect`, variant `programs/variant_owning`, class `programs/array_local`, generic parameter `modules/generic_hooks/com/example/row`, i8 `matrix/values_kinds_in_literals`, i16 `matrix/values_kinds_in_literals`, i64 `matrix/values_kinds_in_literals`, u8 `matrix/values_kinds_in_literals`, u16 `matrix/values_kinds_in_literals`, u32 `matrix/values_kinds_in_literals`, uint `matrix/values_kinds_in_literals`, f64 `matrix/values_kinds_in_literals`, char `matrix/values_kinds_in_literals`, slice `matrix/values_kinds_in_literals`, nullable pointer `matrix/values_kinds_in_literals`, lent value `matrix/values_kind_pairs`, may-fail function value `matrix/values_kinds_in_literals`, function value `matrix/values_kinds_in_literals`, channel `matrix/values_kinds_in_literals`, packed struct `matrix/values_kinds_in_literals`, simd struct `matrix/values_kinds_in_literals`, union `matrix/values_kinds_in_literals`, synchronized class `matrix/values_kinds_in_literals`, concurrent class `matrix/values_kinds_in_literals`, interface `matrix/values_kinds_in_literals` | none |
| array repeat `[v; n]` | i16 `programs/structs`, int `programs/generic_constants`, byte `programs/lent_derived`, bool `std/sorted_order`, fixed array `programs/generic_constants`, class `programs/lent_derived`, generic parameter `programs/generic_constants`, i8 `matrix/values_kinds_in_literals`, i32 `matrix/values_kinds_in_literals`, i64 `matrix/values_kinds_in_literals`, u8 `matrix/values_kinds_in_literals`, u16 `matrix/values_kinds_in_literals`, u32 `matrix/values_kinds_in_literals`, u64 `matrix/values_kinds_in_literals`, uint `matrix/values_kinds_in_literals`, f32 `matrix/values_kinds_in_literals`, f64 `matrix/values_kinds_in_literals`, f16 `matrix/values_kinds_in_literals`, char `matrix/values_kinds_in_literals`, str `matrix/values_kinds_in_literals`, slice `matrix/values_kinds_in_literals`, pointer `matrix/values_kinds_in_literals`, nullable pointer `matrix/values_kinds_in_literals`, optional value `matrix/values_kinds_in_literals`, lent value `matrix/values_kind_pairs`, may-fail function value `matrix/values_kinds_in_literals`, function value `matrix/values_kinds_in_literals`, tuple `matrix/values_kinds_in_literals`, channel `matrix/values_kinds_in_literals`, struct `matrix/values_kinds_in_literals`, packed struct `matrix/values_kinds_in_literals`, simd struct `matrix/values_kinds_in_literals`, union `matrix/values_kinds_in_literals`, enum `matrix/values_kinds_in_literals`, variant `matrix/values_kinds_in_literals`, synchronized class `matrix/values_kinds_in_literals`, concurrent class `matrix/values_kinds_in_literals`, interface `matrix/values_kinds_in_literals` | none |
| slice literal | slice `programs/assign_owned`, i8 `matrix/values_kinds_in_literals`, i16 `matrix/values_kinds_in_literals`, i32 `matrix/values_kinds_in_literals`, i64 `matrix/values_kinds_in_literals`, int `matrix/values_kinds_in_literals`, u8 `matrix/values_kinds_in_literals`, u16 `matrix/values_kinds_in_literals`, u32 `matrix/values_kinds_in_literals`, u64 `matrix/values_kinds_in_literals`, uint `matrix/values_kinds_in_literals`, byte `matrix/values_kinds_in_literals`, f32 `matrix/values_kinds_in_literals`, f64 `matrix/values_kinds_in_literals`, f16 `matrix/values_kinds_in_literals`, bool `matrix/values_kinds_in_literals`, char `matrix/values_kinds_in_literals`, str `matrix/values_kinds_in_literals`, fixed array `matrix/values_kinds_in_literals`, pointer `matrix/values_kinds_in_literals`, nullable pointer `matrix/values_kinds_in_literals`, optional value `matrix/values_kinds_in_literals`, lent value `matrix/values_lent_in_slice_literal`, may-fail function value `matrix/values_kinds_in_literals`, function value `matrix/values_kinds_in_literals`, tuple `matrix/values_kinds_in_literals`, channel `matrix/values_kinds_in_literals`, struct `matrix/values_kinds_in_literals`, packed struct `matrix/values_kinds_in_literals`, simd struct `matrix/values_kinds_in_literals`, union `matrix/values_kinds_in_literals`, enum `matrix/values_kinds_in_literals`, variant `matrix/values_kinds_in_literals`, class `matrix/values_kinds_in_literals`, synchronized class `matrix/values_kinds_in_literals`, concurrent class `matrix/values_kinds_in_literals`, interface `matrix/values_kinds_in_literals`, generic parameter `matrix/values_kinds_in_literals` | none |
| struct literal | struct `programs/bitfields`, union `clib/com/example/geo`, class `programs/alias_class_pointers`, variant `programs/alias_unions`, packed struct `matrix/values_kinds_in_places`, simd struct `matrix/values_kinds_in_places`, generic parameter `matrix/values_kinds_in_places` | none |
| tuple element `t.0` | i32 `clib/com/example/names`, int `programs/alloc_owning`, str `programs/lent_tuples`, pointer `programs/alias_view_local`, lent value `programs/lent_tuples`, function value `clib/com/example/names`, tuple `programs/alias_view_local`, struct `clib/com/example/names`, class `programs/alloc_owning`, i8 `matrix/values_kinds_in_literals`, i16 `matrix/values_kinds_in_literals`, i64 `matrix/values_kinds_in_literals`, u8 `matrix/values_kinds_in_literals`, u16 `matrix/values_kinds_in_literals`, u32 `matrix/values_kinds_in_literals`, u64 `matrix/values_kinds_in_literals`, uint `matrix/values_kinds_in_literals`, byte `matrix/values_kinds_in_literals`, f32 `matrix/values_kinds_in_literals`, f64 `matrix/values_kinds_in_literals`, f16 `matrix/values_kinds_in_literals`, bool `matrix/values_kinds_in_literals`, char `matrix/values_kinds_in_literals`, slice `matrix/values_kinds_in_literals`, fixed array `matrix/values_kinds_in_literals`, nullable pointer `matrix/values_kinds_in_literals`, optional value `matrix/values_kinds_in_literals`, may-fail function value `matrix/values_kinds_in_literals`, channel `matrix/values_kinds_in_literals`, packed struct `matrix/values_kinds_in_literals`, simd struct `matrix/values_kinds_in_literals`, union `matrix/values_kinds_in_literals`, enum `matrix/values_kinds_in_literals`, variant `matrix/values_kinds_in_literals`, synchronized class `matrix/values_kinds_in_literals`, concurrent class `matrix/values_kinds_in_literals`, interface `matrix/values_kinds_in_literals`, generic parameter `matrix/values_kinds_in_literals` | none |
| `let` | i8 `programs/arith_facts`, i16 `programs/atomic_widths`, i32 `programs/alias_same`, int `programs/alias_pointers`, u16 `programs/hash_builtin`, u32 `programs/half`, u64 `programs/carry_chain`, byte `programs/arith_facts`, f32 `programs/effects_edges`, f16 `programs/effects_edges`, bool `programs/atomic_widths`, char `programs/snapshots`, str `programs/alias_pointers`, slice `programs/alias_view_local`, fixed array `programs/alias_pointers`, pointer `programs/alias_access`, nullable pointer `programs/alias_integers`, optional value `programs/argument_order`, lent value `programs/lent_derived`, may-fail function value `programs/catch_none`, function value `programs/alias_pointers`, tuple `programs/alias_view_local`, channel `programs/channel_workers`, struct `programs/bitfields`, enum `programs/snapshots`, variant `programs/eq_parts`, class `programs/assign_owned`, synchronized class `std/sync_equal`, concurrent class `std/collection_equals`, generic parameter `programs/caller_params`, i64 `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, f64 `matrix/values_kinds_in_places`, packed struct `matrix/values_kinds_in_places`, simd struct `matrix/values_kinds_in_places`, union `matrix/values_kinds_in_places`, interface `matrix/values_kinds_in_places` | none |
| `const` in a body | i8 `matrix/values_kinds_in_places`, i16 `matrix/values_kinds_in_places`, i32 `matrix/values_kinds_in_places`, i64 `matrix/values_kinds_in_places`, int `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, u16 `matrix/values_kinds_in_places`, u32 `matrix/values_kinds_in_places`, u64 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, byte `matrix/values_kinds_in_places`, f32 `matrix/values_kinds_in_places`, f64 `matrix/values_kinds_in_places`, f16 `matrix/values_kinds_in_places`, bool `matrix/values_kinds_in_places`, char `matrix/values_kinds_in_places`, str `matrix/values_kinds_in_places`, slice `matrix/values_const_slice`, fixed array `matrix/values_kinds_in_places`, pointer `matrix/values_const_pointer`, nullable pointer `matrix/values_kinds_in_places`, optional value `matrix/values_const_optional` T012, may-fail function value `matrix/values_const_may_fail_fn` T014, function value `matrix/values_const_fn` T014, tuple `matrix/values_kinds_in_places`, channel `matrix/values_const_channel`, struct `matrix/values_kinds_in_places`, packed struct `matrix/values_kinds_in_places`, simd struct `matrix/values_kinds_in_places`, union `matrix/values_const_union`, enum `matrix/values_kinds_in_places`, variant `matrix/values_const_variant`, class `matrix/values_const_class`, synchronized class `matrix/values_const_synchronized_class`, concurrent class `matrix/values_const_concurrent_class`, interface `matrix/values_const_interface`, generic parameter `matrix/values_const_generic` | lent value: `lent` stands before a parameter and a result alone, so no constant is lent |
| `=` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `programs/arith_facts`, char `programs/alias_integers`, str `programs/catch_exits`, slice `programs/generic_copies`, fixed array `programs/assign_owned`, pointer `programs/alias_pointers`, nullable pointer `programs/alias_pointers`, optional value `programs/alloc_owning`, function value `programs/alias_pointers`, tuple `programs/alloc_owning`, struct `programs/walk_generic`, enum `programs/alias_integers`, variant `programs/alias_unions`, class `programs/alloc_owning`, generic parameter `programs/generic_constants`, i64 `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, f64 `matrix/values_kinds_in_places`, lent value `matrix/values_kind_pairs`, may-fail function value `matrix/values_kinds_in_places`, channel `matrix/values_kinds_in_places`, packed struct `matrix/values_kinds_in_places`, simd struct `matrix/values_kinds_in_places`, union `matrix/values_kinds_in_places`, synchronized class `matrix/values_assign_synchronized_class`, concurrent class `matrix/values_kinds_in_places`, interface `matrix/values_kinds_in_places` | none |
| `+=` | i32 `clib/com/example/failing`, int `programs/alloc_owning`, u32 `programs/bitfields`, u64 `clib/com/example/shapes`, byte `programs/class_bitfields`, f32 `programs/half`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_compound_on_simd` | none |
| `-=` | int `programs/member_keywords`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds`, f32 `matrix/values_float_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_compound_sub_on_simd` | none |
| `*=` | int `programs/for_patterns`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds`, f32 `matrix/values_float_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_compound_mul_on_simd` | none |
| `/=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds`, f32 `matrix/values_float_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_compound_div_on_simd` | none |
| `%=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `&=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `\|=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `^=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `<<=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `>>=` | i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `+%=` | int `programs/arith_facts`, byte `programs/compound_wrapping`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds` | none |
| `-%=` | int `programs/compound_wrapping`, byte `programs/compound_wrapping`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds` | none |
| `*%=` | i32 `programs/compound_wrapping`, int `programs/compound_wrapping`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `<<%=` | int `programs/compound_wrapping`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds`, byte `matrix/values_compound_kinds` | none |
| `+\|=` | i32 `programs/compound_wrapping`, int `programs/compound_wrapping`, byte `programs/compound_wrapping`, i8 `matrix/values_compound_kinds`, i16 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds` | none |
| `-\|=` | i8 `programs/compound_wrapping`, byte `programs/compound_wrapping`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds` | none |
| `*\|=` | i8 `programs/compound_wrapping`, byte `programs/compound_wrapping`, i16 `matrix/values_compound_kinds`, i32 `matrix/values_compound_kinds`, i64 `matrix/values_compound_kinds`, int `matrix/values_compound_kinds`, u8 `matrix/values_compound_kinds`, u16 `matrix/values_compound_kinds`, u32 `matrix/values_compound_kinds`, u64 `matrix/values_compound_kinds`, uint `matrix/values_compound_kinds` | none |
| `+` | i8 `programs/flags`, i16 `programs/llvm_flags`, i32 `programs/alias_c_functions`, int `programs/alias_class_pointers`, u16 `programs/flags`, u32 `programs/alias_view_passed`, u64 `programs/carry_chain`, byte `programs/effects_edges`, f32 `programs/classes`, simd struct `clib/com/example/simdlib`, pointer `programs/alias_c_functions`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds`, f64 `matrix/values_float_kinds`, str `matrix/values_add_on_str` | none |
| `-` | i8 `programs/bitfields`, i16 `programs/flags`, i32 `programs/llvm_division`, int `programs/arith_facts`, u64 `programs/carry_chain`, byte `programs/flags`, f32 `programs/float_math`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u16 `matrix/values_arith_kinds`, u32 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_other_operators` | none |
| `+%` | i8 `programs/scalar_ops`, i16 `programs/scalar_ops`, int `programs/arith_facts`, u16 `programs/scalar_ops`, byte `programs/wrapping`, i32 `matrix/values_arith_kinds`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u32 `matrix/values_arith_kinds`, u64 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `-%` | i8 `programs/wrapping`, i32 `programs/scalar_ops`, int `programs/wrapping`, u64 `programs/scalar_ops`, byte `programs/wrapping`, i16 `matrix/values_arith_kinds`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u16 `matrix/values_arith_kinds`, u32 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `+\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u16 `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `-\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u16 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `*` | i8 `programs/flags`, i16 `programs/flags`, i32 `programs/alias_c_functions`, int `programs/alias_unions`, u16 `programs/llvm_flags`, u32 `programs/llvm_flags`, u64 `programs/flags`, byte `programs/flags`, f32 `programs/classes`, simd struct `clib/com/example/simdlib`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds`, f64 `matrix/values_float_kinds` | none |
| `/` | i8 `programs/scalar_ops`, i16 `programs/narrow_ops`, i32 `programs/llvm_division`, int `programs/closures`, u16 `programs/scalar_ops`, u32 `programs/narrow_ops`, u64 `programs/llvm_division`, byte `programs/llvm_division`, f32 `programs/format_strings`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_other_operators` | none |
| `%` | i8 `programs/scalar_ops`, i16 `programs/narrow_ops`, i32 `programs/llvm_division`, int `programs/closures`, u64 `programs/llvm_division`, byte `programs/scalar_ops`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u16 `matrix/values_arith_kinds`, u32 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `*%` | i8 `programs/scalar_ops`, i32 `programs/wrapping`, int `programs/wrapping`, u16 `programs/wrapping`, i16 `matrix/values_arith_kinds`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, u32 `matrix/values_arith_kinds`, u64 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds`, byte `matrix/values_arith_kinds` | none |
| `*\|` | i8 `programs/saturating`, i16 `programs/saturating`, i32 `programs/saturating`, int `programs/saturating`, u16 `programs/saturating`, u32 `programs/saturating`, u64 `programs/saturating`, byte `programs/saturating`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `mul_high` | i8 `programs/mul_high`, i16 `programs/mul_high`, i32 `programs/mul_high`, int `programs/mul_high`, u16 `programs/mul_high`, u32 `programs/mul_high`, u64 `programs/mul_high`, byte `programs/mul_high`, i64 `matrix/values_arith_kinds`, u8 `matrix/values_arith_kinds`, uint `matrix/values_arith_kinds` | none |
| `<<` | i8 `programs/flags`, i16 `programs/flags`, i32 `programs/llvm_division`, int `programs/flags`, u16 `programs/llvm_flags`, u32 `programs/flags`, u64 `programs/llvm_flags`, byte `programs/flags`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds` | none |
| `>>` | i8 `programs/flags`, i16 `programs/llvm_flags`, i32 `programs/flags`, int `programs/flags`, u16 `programs/llvm_flags`, u32 `programs/llvm_flags`, u64 `programs/flags`, byte `programs/flags`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds` | none |
| `<<%` | i8 `programs/wrapping`, i32 `programs/wrapping`, int `programs/wrapping`, u32 `programs/wrapping`, u64 `bench/scalar_loop`, byte `programs/wrapping`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds` | none |
| binary `&` | i32 `programs/compare_bits`, int `programs/narrow_ops`, u32 `programs/utf8`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u64 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, byte `matrix/values_bits_kinds`, bool `matrix/values_and_on_bool`, enum `matrix/values_and_on_enum`, simd struct `matrix/values_other_operators` | none |
| binary `\|` | i32 `programs/compare_bits`, int `programs/narrow_ops`, u32 `programs/union_name`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u64 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, byte `matrix/values_bits_kinds`, bool `matrix/values_or_on_bool`, enum `matrix/values_or_on_enum`, simd struct `matrix/values_other_operators` | none |
| binary `^` | i16 `programs/narrow_ops`, i32 `programs/compare_bits`, int `programs/narrow_ops`, u64 `programs/flags`, i8 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u32 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, byte `matrix/values_bits_kinds`, bool `matrix/values_xor_on_bool`, enum `matrix/values_xor_on_enum`, simd struct `matrix/values_other_operators` | none |
| `==` | i32 `programs/twice`, int `programs/arith_facts`, u32 `programs/eq_hash_agree`, u64 `programs/effects_edges`, byte `programs/strings`, f32 `dump/constraints`, bool `programs/alias_pointers`, char `programs/hash_generic`, str `programs/const_text_compare`, pointer `programs/alias_pointers`, nullable pointer `programs/alias_pointers`, optional value `programs/alloc_owning`, tuple `programs/eq_hash_agree`, struct `modules/hashing/com/example/keys`, enum `programs/generic_nested`, variant `programs/eq_parts`, class `programs/eq_compiled`, fixed array `programs/eq_arrays`, function value `programs/callbacks`, generic parameter `programs/eq_arrays`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, f64 `matrix/values_float_kinds`, slice `matrix/values_equal_on_slice`, channel `matrix/values_other_operators`, simd struct `matrix/values_other_operators` | none |
| `!=` | i8 `programs/llvm_division`, i16 `programs/scalar_ops`, i32 `programs/compare_bits`, int `programs/closures`, u16 `programs/scalar_ops`, u32 `programs/scalar_ops`, u64 `programs/llvm_division`, byte `programs/args`, f32 `programs/construct_args`, bool `programs/alias_access`, char `std/reflect`, str `programs/const_text_compare`, pointer `programs/hooks`, nullable pointer `programs/alias_access`, optional value `programs/argument_order`, tuple `programs/eq_parts`, enum `std/log`, class `std/collection_fields`, fixed array `programs/eq_arrays`, generic parameter `modules/generic_hooks/com/example/row`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, f64 `matrix/values_float_kinds`, struct `matrix/values_other_operators`, variant `matrix/values_other_operators`, slice `matrix/values_not_equal_on_slice`, function value `matrix/values_other_operators`, channel `matrix/values_other_operators`, simd struct `matrix/values_other_operators` | none |
| `<` | i16 `programs/narrow_ops`, i32 `programs/alias_c_functions`, int `programs/alloc_owning`, u32 `programs/utf8`, u64 `std/interfaces`, byte `programs/narrow_ops`, f32 `programs/construct_args`, str `programs/const_text_compare`, generic parameter `programs/caller_params`, i8 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, f64 `matrix/values_float_kinds`, char `matrix/values_other_operators`, pointer `matrix/values_less_on_pointer`, simd struct `matrix/values_other_operators`, struct `matrix/values_less_on_struct`, class `matrix/values_less_on_class` | none |
| `<=` | i32 `clib/com/example/canvas`, int `programs/effects_edges`, u64 `programs/scalar_ops`, f32 `programs/construct_args`, str `programs/str_hooks`, generic parameter `dump/constraints`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u32 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, byte `matrix/values_bits_kinds`, f64 `matrix/values_float_kinds`, char `matrix/values_other_operators`, pointer `matrix/values_less_equal_on_pointer`, simd struct `matrix/values_other_operators`, struct `matrix/values_less_equal_on_struct`, class `matrix/values_less_equal_on_class` | none |
| `>` | i32 `programs/alias_c_functions`, int `programs/arith_facts`, u32 `programs/out_place_forms`, u64 `programs/scalar_ops`, byte `programs/scalar_ops`, f32 `programs/object_model`, str `programs/const_text_compare`, simd struct `clib/com/example/simdlib`, generic parameter `dump/constraints`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, f64 `matrix/values_float_kinds`, char `matrix/values_other_operators`, pointer `matrix/values_greater_on_pointer`, struct `matrix/values_greater_on_struct`, class `matrix/values_greater_on_class` | none |
| `>=` | i16 `programs/narrow_ops`, i32 `clib/com/example/failing`, int `programs/fn_may_fail`, byte `programs/pattern_bytes`, str `programs/str_hooks`, generic parameter `dump/constraints`, i8 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u32 `matrix/values_bits_kinds`, u64 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, f32 `matrix/values_float_kinds`, f64 `matrix/values_float_kinds`, char `matrix/values_other_operators`, pointer `matrix/values_greater_equal_on_pointer`, simd struct `matrix/values_other_operators`, struct `matrix/values_greater_equal_on_struct`, class `matrix/values_greater_equal_on_class` | none |
| unary `-` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_integers`, f32 `programs/construct_args`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_negate_u8`, u16 `matrix/values_negate_u16`, u32 `matrix/values_negate_u32`, u64 `matrix/values_negate_u64`, uint `matrix/values_negate_uint`, byte `matrix/values_negate_byte`, f64 `matrix/values_float_kinds`, simd struct `matrix/values_other_operators` | none |
| unary `!` | bool `programs/compare_bits` | none |
| unary `~` | int `programs/compare_bits`, i8 `matrix/values_bits_kinds`, i16 `matrix/values_bits_kinds`, i32 `matrix/values_bits_kinds`, i64 `matrix/values_bits_kinds`, u8 `matrix/values_bits_kinds`, u16 `matrix/values_bits_kinds`, u32 `matrix/values_bits_kinds`, u64 `matrix/values_bits_kinds`, uint `matrix/values_bits_kinds`, byte `matrix/values_bits_kinds` | none |
| `as` | i8 `programs/arith_facts`, i16 `programs/alias_integers`, i32 `programs/alias_integers`, int `programs/alloc_owning`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/by_unsigned`, byte `programs/arith_facts`, f32 `programs/alias_simd`, f16 `programs/effects_edges`, bool `programs/alias_pointers`, char `programs/alias_integers`, pointer `programs/alias_access`, nullable pointer `programs/alias_c_functions`, enum `programs/alias_integers`, i64 `matrix/values_conversion_kinds`, u8 `matrix/values_conversion_kinds`, uint `matrix/values_conversion_kinds`, f64 `matrix/values_conversion_kinds`, str `matrix/values_as_from_str`, slice `matrix/values_as_from_slice` | none |
| index `e[i]` | i16 `programs/structs`, i32 `programs/alias_c_functions`, int `programs/alias_arrays`, u16 `dump/tbaa`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `std/collection_ring`, str `programs/args`, slice `programs/alias_arrays`, fixed array `programs/alias_arrays`, pointer `programs/alias_integers`, nullable pointer `std/builder_take_in`, optional value `programs/alloc_owning`, lent value `programs/lent_derived`, tuple `programs/alloc_owning`, struct `programs/eq_hash_agree`, enum `std/reflect`, variant `programs/variant_owning`, class `programs/array_local`, generic parameter `programs/generic_constants`, i8 `matrix/values_kinds_in_literals`, i64 `matrix/values_kinds_in_literals`, u8 `matrix/values_kinds_in_literals`, u32 `matrix/values_kinds_in_literals`, uint `matrix/values_kinds_in_literals`, f64 `matrix/values_kinds_in_literals`, char `matrix/values_kinds_in_literals`, may-fail function value `matrix/values_kinds_in_literals`, function value `matrix/values_kinds_in_literals`, channel `matrix/values_kinds_in_literals`, packed struct `matrix/values_kinds_in_literals`, simd struct `matrix/values_kinds_in_literals`, union `matrix/values_kinds_in_literals`, synchronized class `matrix/values_kinds_in_literals`, concurrent class `matrix/values_kinds_in_literals`, interface `matrix/values_kinds_in_literals` | none |
| slice `e[lo..hi]` | slice `programs/alias_arrays` | none |
| field `e.x` | i8 `programs/alias_bytes`, i16 `programs/atomic_widths`, i32 `programs/alias_view_passed`, int `programs/alias_access`, u16 `programs/bitfields`, u32 `programs/bitfields`, u64 `programs/sync_pairs`, byte `programs/alias_bytes`, f32 `programs/alias_simd`, f16 `programs/half`, bool `programs/arith_facts`, char `programs/hash_builtin`, str `programs/alias_access`, slice `programs/alias_pointers`, fixed array `programs/alias_arrays`, pointer `programs/alias_access`, nullable pointer `programs/alias_access`, optional value `programs/hash_builtin`, lent value `programs/for_patterns`, may-fail function value `programs/fn_may_fail`, function value `programs/alias_pointers`, tuple `programs/eq_hash_agree`, struct `programs/bitfields`, simd struct `clib/com/example/simdlib`, union `std/reflect`, enum `programs/alias_integers`, variant `programs/generic_copies`, class `programs/alias_class_pointers`, synchronized class `std/sync_equal`, concurrent class `std/collection_dup`, interface `programs/inherits_header`, generic parameter `programs/eq_default`, i64 `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, f64 `matrix/values_kinds_in_places`, channel `matrix/values_kinds_in_places`, packed struct `matrix/values_kinds_in_places` | none |
| call | i8 `programs/by_ends`, i16 `programs/alias_integers`, i32 `programs/alias_access`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_c_functions`, byte `programs/by_ends`, f32 `programs/classes`, bool `programs/alias_pointers`, char `programs/hash_generic`, str `programs/args`, slice `programs/alias_arrays`, fixed array `programs/checker_context`, pointer `programs/alias_access`, nullable pointer `programs/alias_access`, optional value `programs/argument_order`, lent value `programs/lent_derived`, may-fail function value `programs/catch_exits`, function value `programs/alias_access`, tuple `programs/caller_params`, channel `programs/channel_workers`, struct `programs/carry_chain`, enum `programs/arith_facts`, variant `programs/alias_unions`, class `programs/alloc_owning`, synchronized class `std/sync_equal`, concurrent class `std/collection_dup`, generic parameter `programs/caller_params`, i64 `matrix/values_kinds_in_places`, u8 `matrix/values_kinds_in_places`, uint `matrix/values_kinds_in_places`, f64 `matrix/values_kinds_in_places`, f16 `matrix/values_call_f16`, packed struct `matrix/values_kinds_in_places`, simd struct `matrix/values_kinds_in_places`, union `matrix/values_kinds_in_places`, interface `matrix/values_kinds_in_places` | none |
| anonymous function `fn(x) { }` | function value `programs/closures`, may-fail function value `programs/closures` | none |

The step `values` of the test run filled both tables on 2026-10-12. How to
read a cell it wrote:

- `matrix/values_literals`, `matrix/values_text_literals`,
  `matrix/values_arithmetic`, `matrix/values_bits_and_assignments`,
  `matrix/values_aggregates` and `matrix/values_bindings_and_calls` hold
  the constructs in every place their rows name them for. A function
  `<place>_<construct>` holds one construct in one place, and a line of
  output starts with the construct and the place. Six programs hold the
  forty rows. Every program is an executable the suite builds three times,
  and macOS checks each new one in turn. "Repository layout" in
  `docs/decisions.md` keeps a suite under 9 minutes by shared builds. One
  program for all the rows would compile for minutes: antic 0.2.0 writes
  the LLVM text of 150 such functions in 21 s and of 72 in 2.4 s.
- `anti-test/com/example/values` is the module of the test
  `anti_test_values`, and `modules/values/com/example/values` the library
  of the test `values_modules`. They hold the `tests` block, the
  `fixtures` block and the library module, as the two tests of the flow
  area do.
- `matrix/values_at_module_level` holds each construct that a constant
  takes. A slice, a slice literal, a call and a simd built-in are no
  constant expressions, and each cell names its refusal.
- `matrix/values_arith_kinds`, `matrix/values_bits_kinds` and
  `matrix/values_compound_kinds` hold the operators on every integer type.
  A line names the operator, the type and the operands. It holds the value
  the checker folds in a constant and the value the program computes from
  operands it cannot know before it runs. `checked` stands where a dev
  build stops the case at run time, and `none` where the checker refuses
  the constant.
- `matrix/values_float_kinds` and `matrix/values_conversion_kinds` hold
  the floats and `as` between the numbers in the same form.
- `matrix/values_kinds_in_literals` and `matrix/values_kinds_in_places`
  hold each kind of value in an array literal, an array repeat, a slice
  literal and a tuple, and in a `let`, a `const`, an assignment, a field
  and a call. A function `<construct>_on_<kind>` holds one pair. The array
  literal holds the index of each kind as well, and the field the struct
  literal.
- `matrix/values_other_operators` holds the comparisons on the kinds that
  are no numbers and the operators of a simd struct.
- `matrix/values_kind_pairs` holds a lent pointer, a value of a type
  parameter, an optional value and a failing call.
- A cell that the rules refuse names a refusal, a source with an `.err`
  file of one message.
- An id after a source, as `T012`, names the finding of
  `docs/audit/tests-findings.md` that the test shows.

The expected output of a generated program comes from a model of the rule
in the generator, never from a run. `i64` and `int` are one type, as are
`u64` and `uint` and `u8` and `byte`, and each spelling has its lines.
The row of the character literal named `byte` twice, so the kinds of the
area count one less than before.

The tests of the step beside the cells, where two rules of the checker or
of lowering meet:

| Test | What it holds | Finding |
|---|---|---|
| `values_float_kinds` | `+ - * /`, unary `-` and the comparisons on `f32` and `f64` with a NaN, the infinities and both zeros, folded and computed, and the rounding of an `f64` to an `f32` | |
| `values_conversion_kinds` | a float to every integer type, an integer to a float with its rounding, and every pair of integer types | |
| `values_fold_least_int_from_float` | the least `int` as a float converts to `int` in a constant | T009 |
| `values_fold_int_to_f32` | an `int` that needs one rounding to an `f32` and gets two in a constant | T008 |
| `values_as_other_kinds` | `as` on a `bool`, a `char` and an enum, the rounding of `as f16` and every place an `f16` is stored | |
| `values_as_negative_enum` | a negative value of an enum converted to a wider integer and to a float | T011 |
| `values_as_enum_in_constant` | a number converted to an enum in a constant | T010 |
| `values_const_optional`, `values_const_optional_in_struct` | a constant of an optional type, alone and as a part | T012 |
| `values_array_repeat_widens` | `[v; n]` where the element is a `?T` or a pointer to an interface | T013 |
| `values_const_fn`, `values_const_may_fail_fn` | the refusal of a function in a constant | T014 |
| `values_is_after_error` | `is` on a value that has an error already | T015 |
| `values_class_constant`, `values_class_constant_as_default` | a constant of a class in a constant expression and as the default of a field | T018, T019 |
| `values_types_and_constants` | tuples as values, constants that name constants and what a narrow type folds to, each place a literal takes its type from, and `size_of` of each form of type | |
| `values_text_kinds` | the literals of text and bytes, and `f"..."` on each kind it writes | |
| `values_other_operators` | the comparisons on the kinds that are no numbers, with `>`, `<=` and `>=` from an `operator fn lt`, and the operators of a simd struct | |
| `values_kind_pairs` | a lent pointer and a value of a type parameter in the constructs that take one, the operators on a proved optional, and two failing calls as the operands of one operator | |
| `values_optional_unchecked`, `values_repeat_owning`, `values_atomic_compound`, `values_assign_synchronized_class` | an operator on an optional no test has proved, an array repeat of a value that owns something, `+=` on an atomic, `=` on a synchronized class | |
| `values_literal_above_<type>`, `values_literal_below_<type>` | an integer literal past a limit of its type | |
| `values_const_divides_by_zero` and the other `values_const_<error>` refusals | the errors of a constant expression, with the operation and the values in the message | |

The dev-mode checks stand in `tests/traps/`, since a program that a dev
build stops has another output than its release build. `check_<name>` runs
each in a build that keeps the check and in one that drops it:
`overflow_add_i8`, `overflow_sub_i16`, `overflow_mul_i8`,
`overflow_compound`, `overflow_generic`, `narrow_unsigned`, `narrow_u64`,
`narrow_to_byte`, `divide_byte`, `remainder_i8`, `divide_compound`,
`shift_wide_byte`, `shift_right_wide_i16`, `shift_compound` and
`slice_range`. The cases with a defined result and no check run in the
matrix: the least value divided by -1, unary `-` of the least value, `<<%`
by the width and an unsigned `+` past its limit.

The triples among them, each a rule that reads three things at once:

- An operator, a width below that of a register and the dev-mode check:
  `check_overflow_add_i8`, `check_overflow_mul_i8` and
  `check_shift_wide_byte`.
- An operator, a copy of a generic function and the dev-mode check:
  `check_overflow_generic`.
- A literal, an optional type and the width of its value: the line
  `optional` of `values_types_and_constants`.
- A compound assignment, a narrowed optional and a narrow type: the part
  `optional` of `values_kind_pairs`.
- A conversion, a constant and the width of the float:
  `values_fold_int_to_f32`.
- An enum, a signed underlying type and a conversion:
  `values_as_negative_enum`.
- An array repeat, the element type of its context and a value that
  widens: `values_array_repeat_widens`.
- A constant, a class and the length of an array: `values_class_constant`.
- A failing call, an operator and the order of its operands: the part
  `may_fail` of `values_kind_pairs`.

An integer literal above the greatest `u64` does not lex. `fmt_canonical`
keeps a source that does not lex out of `tests/matrix/`, so that literal
has no test here.

### Types

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| named or built-in type | `let` `programs/alias_bytes`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alias_access`, `as` `programs/alias_access`, `alloc` `programs/alias_integers`, `size_of` `programs/alias_view_passed`, `const` `programs/alloc_owning`, slice literal `programs/assign_owned`, call `programs/caller_params`, `type` alias `programs/generic_unused`, library module `modules/access/com/example/ledger`, base of an enum `programs/alias_integers`, literal `programs/eq_default`, `inherits` `programs/generic_copies`, `chan` `programs/channel_workers`, constraint `std/collection_deque`, case of a variant `matrix/types_forms` | none |
| `*T` | `let` `programs/class_is`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alias_pointers`, `as` `programs/alias_access`, `alloc` `programs/generic_copies`, slice literal `programs/generic_copies`, call `std/collection`, library module `modules/access/com/example/ledger`, literal `programs/generic_copies`, `size_of` `matrix/types_forms`, `const` `matrix/types_const_of_a_pointer`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `?*T` | `let` `programs/catch_none`, parameter `programs/alias_access`, result `programs/alias_access`, field `programs/alloc_owning`, `as` `programs/alias_c_functions`, `alloc` `std/builder_take_in`, `size_of` `programs/optional_values`, library module `modules/chain/com/example/animal`, `const` `matrix/types_forms`, slice literal `matrix/types_forms`, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `?T` | `let` `programs/argument_order`, parameter `programs/optional_values`, result `programs/argument_order`, field `programs/optional_values`, `alloc` `programs/alloc_owning`, `size_of` `programs/alloc_owning`, slice literal `programs/owned_buffers`, library module `modules/generics/com/example/gen`, `as` `matrix/types_as_to_an_optional`, `const` `matrix/values_const_optional` T012, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `[]T` | `let` `programs/lent_slices`, parameter `programs/alias_arrays`, result `programs/eq_kinds`, field `programs/alias_pointers`, `alloc` `std/serialize_null`, slice literal `programs/assign_owned`, call `errors/copies_many`, library module `modules/closures/com/example/walk`, `as` `matrix/types_as_to_a_slice`, `size_of` `matrix/types_forms`, `const` `matrix/values_const_slice`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `[N]T` | `let` `programs/alias_pointers`, parameter `programs/alias_arrays`, result `programs/generic_constants`, field `programs/alias_arrays`, `as` `programs/alias_arrays`, `alloc` `programs/concurrent`, `size_of` `programs/half`, `const` `programs/const_data`, library module `modules/generic_hooks/com/example/row`, slice literal `matrix/types_forms`, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `fn(A) -> R` | `let` `programs/callbacks`, parameter `programs/alias_c_functions`, result `programs/closures`, field `programs/alias_pointers`, `as` `programs/alias_pointers`, call `programs/generic_fn_args`, library module `modules/closures/com/example/walk`, literal `programs/generic_fn_args`, `alloc` `matrix/types_forms`, `size_of` `matrix/types_forms`, `const` `matrix/values_const_fn` T014, slice literal `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `keep fn` | parameter `matrix/types_forms`, library module `modules/types/com/example/types`, `let` `matrix/types_keep_fn_in_a_let` | `keep` marks a parameter and a parameter of a function type, and the parser reads it in no other place: result, field, `as`, `alloc`, `size_of`, `const`, slice literal, call, `type` alias, case of a variant |
| `concurrent fn` | parameter `matrix/types_forms`, library module `modules/types/com/example/types`, `let` `matrix/types_concurrent_fn_in_a_let` | `concurrent` marks a parameter and a parameter of a function type, and the parser reads it in no other place: result, field, `as`, `alloc`, `size_of`, `const`, slice literal, call, `type` alias, case of a variant |
| tuple type `(A, B)` | `let` `programs/for_patterns`, parameter `programs/for_patterns`, result `programs/caller_params`, field `programs/for_patterns`, `alloc` `programs/alloc_owning`, `size_of` `programs/alloc_owning`, call `std/collection_parts`, library module `modules/generics/com/example/gen`, `as` `matrix/types_as_to_a_tuple`, `const` `matrix/types_forms`, slice literal `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `chan T` | parameter `programs/channel_workers`, result `modules/sync/com/example/queue`, field `programs/eq_handles`, library module `modules/sync/com/example/queue`, `let` `matrix/types_forms`, `as` `matrix/types_as_to_a_channel`, `alloc` `matrix/types_forms`, `size_of` `matrix/types_forms`, `const` `matrix/values_const_channel`, slice literal `matrix/types_forms`, call `matrix/types_forms`, `type` alias `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| constant as a type argument | `let` `dump/generics`, parameter `std/spsc_ring_threads`, field `std/collection_dup`, `size_of` `programs/generic_constants`, call `programs/checker_context`, `type` alias `doc/src/com/example/app`, library module `doc/src/com/example/shapes`, literal `programs/generic_constants`, result `matrix/types_forms`, `as` `matrix/types_as_to_a_constant_argument`, `alloc` `matrix/types_forms`, `const` `matrix/types_forms`, slice literal `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| type arguments `Name<A>` | `let` `std/shared`, parameter `programs/generic_constants`, result `programs/generic_copies`, field `programs/generic_hooks`, `as` `modules/generics/client`, `size_of` `programs/generic_constants`, call `programs/caller_params`, `type` alias `modules/direct/com/example/shapes`, library module `modules/access/com/example/ledger`, literal `programs/eq_default`, `inherits` `programs/generic_copies`, constraint `std/collection_deque`, `alloc` `matrix/types_forms`, `const` `matrix/types_forms`, slice literal `matrix/types_forms`, case of a variant `matrix/types_forms` | none |
| `struct` | module level `programs/alias_access`, nested type `errors/concurrent_forms`, generic `programs/eq_default`, library module `modules/blocks/com/example/geo` | none |
| `packed struct` | module level `abi/abi_structs`, library module `clib/com/example/shapes`, nested type `matrix/types_nested_and_simd` | none |
| `simd struct` | module level `programs/alias_simd`, library module `modules/generics/com/example/gen`, nested type `matrix/types_nested_and_simd` | none |
| `align(n)` on a type | `programs/inherits_header` | none |
| `union` | module level `programs/alias_unions`, library module `clib/com/example/geo` | none |
| `enum` | module level `programs/alias_integers`, nested type `programs/generic_nested`, library module `modules/classlib/com/example/shapes` | none |
| `enum` with a base type | `programs/alias_integers` | none |
| `enum` value with a number | `programs/alias_integers` | none |
| `variant` | module level `programs/alias_unions`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `type` alias | module level `programs/generic_unused`, library module `modules/direct/com/example/shapes`, generic `matrix/types_alias_with_type_parameters` | none |
| `constraint` | module level `programs/generic_unused`, library module `modules/direct/com/example/shapes` | none |
| type parameter `<T>` | `programs/caller_params` | none |
| constant parameter `<N: int>` | `programs/checker_context` | none |
| constraint on a parameter `<T: C>` | `programs/caller_params` | none |
| bitfield `x: u8 : 3` | `programs/bitfields` | none |
| default of a field | `programs/alias_class_pointers` | none |
| default of a parameter | `programs/class_literal_defaults` | none |
| `e is Type.Case` | `programs/generic_variants` | none |
| `static atomic` field | `programs/alloc_owning` | none |

The step `types` filled the table above and wrote the tests of the types
themselves. `tests/matrix/` holds its sources under the names `types_*`.

- `matrix/types_forms` holds each form of type in the places the table
  names. A line of its output names the form and the place.
- `matrix/types_packed_and_aligned` holds `packed` and `align(N)` on a
  struct, a union, a variant, a class and a generic struct. A packed struct
  puts a field of every kind at an odd offset, and each is read, written,
  passed and addressed there. An aligned type is checked by its size and
  by the address of a local, an element, a field and memory of `alloc`.
- `matrix/types_bitfields_and_unions` holds bitfields of each width and
  sign in a struct, a packed struct and a union. It holds a union of every
  kind of field as well, read through another field.
- `matrix/types_enums_and_variants` holds enums over every base, with and
  without values, and every pattern on a variant: an arm that binds its
  case, one that does not, `else`, `if let`, `is` and the `tag`.
- `matrix/types_arrays` holds a fixed array of each kind of element. It is
  read and written with a constant index and with one the program
  computes, as decision 24 names both. The arrays of packed structs, of
  unions, of enums and of variants stand in the programs of those types.
- `matrix/types_nested_and_simd` holds the types a class body declares,
  each with a layout word, and simd structs in every type that holds one.
- `modules/types/com/example/types` is the library of the test
  `types_modules`. `client` reads, writes and measures each type across
  the module boundary, and both modules print the same sizes.
- `abi/probe_types` is the pair of the test `abi_probe_types`. A C source
  and an Anti source print the size, the alignment and the bytes of a
  field for the same types, so the layout of `size_of` is the C layout of
  the host. `abi/probe` held structs, unions and bitfields before.
- 103 sources are refusals of one message each. They hold what `as`,
  `const` and a `type` alias refuse, the words `packed` and `align` where
  they do not stand, the types and widths of a bitfield, the values and
  bases of an enum, and every pattern on a variant that the checker
  refuses.
- An `align(N)` below the alignment of the fields is an error of the back
  end that names the target. So is a bitfield whose unit is larger than
  its struct. `errors/layout` and `errors/layout_unit` hold them for
  linux-x86_64, and a matrix refusal has no option for a target.

Where two rules meet, and where three do:

- A packed struct and a failing call: the call writes its result into a
  field at offset 1, in `types_packed_and_aligned`.
- A packed struct, a lent element and a compound assignment: the line
  `lent elements` of `types_packed_and_aligned`.
- A packed struct and a simd struct: a vector of 16 and one of 32 bytes at
  offsets 1 and 17, in `types_nested_and_simd`.
- A packed class and an atomic field, and a packed class and a Mutex:
  `types_atomic_in_packed_class` and `types_mutex_in_packed_class`, T022.
- A bitfield and a failing call, the flags form, a closure, a lent element,
  `==` and the hash: the part `other_rules` of
  `types_bitfields_and_unions`.
- A bitfield, a value past its width and the wrapping and saturating
  forms: the part `values_past_the_width` of the same program.
- An enum, two names of one number and a `switch`: the line `switch on one
  number twice` of `types_enums_and_variants`, and the refusal
  `types_switch_on_one_number_twice`.
- A variant, an arm that binds its case and an assignment to the variant
  in that arm: the line `variant changed in its arm`.
- A variant without fields and a field of a struct or of a class:
  `types_variant_without_fields_as_a_field`, T023.
- A declaration, a constant expression and a call:
  `types_enum_value_from_a_call` and the three refusals beside it, T020.
- An array of arrays, a row assigned and an index the program computes: the
  part `arrays_of_arrays` of `types_arrays`.
- A nested type, a layout word and an array of the type:
  `types_nested_and_simd`.

Three more refusals show T016, a constant index past the end of an array
in a field, in an inner array and in a write. `fmt/alias_of_function` is
the source of the test `fmt_alias_of_function`, T025.

### Ownership

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `alloc` of a literal | function `programs/alias_access`, method `programs/catch_exits`, loop body `programs/parallel_join`, catch handler `errors/worker_walk`, worker `programs/parallel_join`, `tests` block `inject/project/src/com/example/app`, generic body `programs/generic_nested`, library module `modules/chain/com/example/dog`, module level `matrix/ownership_alloc_of_a_literal_at_module_level`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `defer` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_alloc_in_contexts`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_alloc_in_contexts` | none |
| `alloc(T, n)` | function `programs/alias_integers`, method `programs/generic_copies`, generic body `programs/generic_copies`, library module `modules/generics/com/example/gen`, module level `matrix/ownership_alloc_of_many_at_module_level`, loop body `matrix/ownership_alloc_in_contexts`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `defer` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, catch handler `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, worker `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_alloc_in_contexts`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_alloc_in_contexts` | none |
| `alloc` of a class with arguments | function `programs/concurrent`, module level `matrix/ownership_alloc_with_arguments_at_module_level`, method `matrix/ownership_alloc_in_contexts`, loop body `matrix/ownership_alloc_in_contexts`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `defer` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, catch handler `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, worker `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_alloc_in_contexts`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, generic body `matrix/ownership_alloc_in_contexts`, nested type `matrix/ownership_alloc_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `free` | function `programs/alias_integers`, method `programs/concurrent`, loop body `programs/concurrent`, worker `programs/parallel_join`, `tests` block `traps/memory_checks_tests`, generic body `programs/generic_copies`, library module `modules/closures/com/example/walk`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `defer` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, catch handler `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_alloc_in_contexts`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_alloc_in_contexts` | none |
| `delete` | function `programs/alias_class_pointers`, loop body `std/collection`, `defer` `programs/may_fail_table`, catch handler `programs/catch_moves`, worker `errors/worker_walk`, `tests` block `inject/project/src/com/example/app`, method `matrix/ownership_alloc_in_contexts`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_delete_in_a_dispatch`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `fixtures` block `anti-test/com/example/ownership`, generic body `matrix/ownership_alloc_in_contexts`, nested type `matrix/ownership_alloc_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `destroy` | function `programs/alloc_owning`, generic body `programs/owning_values`, method `matrix/ownership_alloc_in_contexts`, loop body `matrix/ownership_alloc_in_contexts`, switch arm `matrix/ownership_alloc_in_contexts`, `if let` `matrix/ownership_alloc_in_contexts`, `defer` `matrix/ownership_alloc_in_contexts`, `undo` `matrix/ownership_alloc_in_contexts`, catch handler `matrix/ownership_alloc_in_contexts`, closure `matrix/ownership_alloc_in_contexts`, worker `matrix/ownership_alloc_in_contexts`, `dispatch` or `parallel` `matrix/ownership_alloc_in_contexts`, `sync` block `matrix/ownership_alloc_in_contexts`, `select` arm `matrix/ownership_alloc_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_alloc_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `dup` | function `programs/caller_params`, method `programs/generic_constants`, loop body `programs/generic_copies`, switch arm `programs/generic_variants`, `sync` block `programs/generic_sync`, generic body `programs/caller_params`, library module `modules/direct/com/example/frame`, module level `matrix/ownership_dup_at_module_level`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, catch handler `matrix/ownership_pointers_in_contexts`, closure `matrix/ownership_pointers_in_contexts`, worker `matrix/ownership_pointers_in_contexts`, `dispatch` or `parallel` `matrix/ownership_pointers_in_contexts`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_pointers_in_contexts` | none |
| address `&x` | function `programs/alias_access`, method `programs/caller_params`, loop body `programs/arith_facts`, switch arm `programs/nested_locals`, `if let` `programs/variant_owning`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/catch_moves`, closure `std/collection_deque`, worker `std/shared`, `dispatch` or `parallel` `programs/sync_workers`, `sync` block `errors/pointer_leak`, generic body `programs/generic_nested`, library module `modules/defaults/com/example/greeter`, module level `matrix/ownership_address_at_module_level`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_pointers_in_contexts` | none |
| read through `*p` | function `programs/alias_access`, method `programs/table_rewrite`, loop body `programs/channel_workers`, closure `std/collection_deque`, worker `programs/channel_workers`, `dispatch` or `parallel` `programs/concurrent`, `sync` block `programs/eq_handles`, `select` arm `programs/channel_workers`, generic body `programs/generic_variants`, library module `modules/generics/com/example/gen`, switch arm `matrix/ownership_pointers_in_contexts`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, catch handler `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, nested type `matrix/ownership_pointers_in_contexts` | module level: a constant holds no pointer but `none`, since `&` of a constant and `alloc` are no constant expressions, so `*p` has nothing to read there |
| `none` | module level `programs/alloc_owning`, function `programs/alias_access`, method `programs/concurrent`, loop body `programs/concurrent`, switch arm `programs/generic_variants`, catch handler `programs/catch_none`, worker `std/spsc_ring_threads`, `sync` block `programs/concurrent`, generic body `programs/generic_nested`, nested type `programs/generic_nested`, library module `modules/generics/com/example/gen`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, closure `matrix/ownership_pointers_in_contexts`, `dispatch` or `parallel` `matrix/ownership_pointers_in_contexts`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership` | none |
| `??` | function `programs/closures`, loop body `std/collection_queue`, worker `std/concurrent_map_threads`, module level `matrix/ownership_none_or_of_a_value_at_module_level` and `matrix/ownership_none_or_of_none_at_module_level` T028, method `matrix/ownership_pointers_in_contexts`, switch arm `matrix/ownership_pointers_in_contexts`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, catch handler `matrix/ownership_pointers_in_contexts`, closure `matrix/ownership_pointers_in_contexts`, `dispatch` or `parallel` `matrix/ownership_pointers_in_contexts`, `sync` block `matrix/ownership_pointers_in_contexts`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, generic body `matrix/ownership_pointers_in_contexts`, nested type `matrix/ownership_pointers_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `p?.x` | function `errors/optional_values`, module level `matrix/ownership_chain_at_module_level`, method `matrix/ownership_pointers_in_contexts`, loop body `matrix/ownership_pointers_in_contexts`, switch arm `matrix/ownership_pointers_in_contexts`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, catch handler `matrix/ownership_pointers_in_contexts`, closure `matrix/ownership_pointers_in_contexts`, worker `matrix/ownership_pointers_in_contexts`, `dispatch` or `parallel` `matrix/ownership_pointers_in_contexts`, `sync` block `matrix/ownership_pointers_in_contexts`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, generic body `matrix/ownership_pointers_in_contexts`, nested type `matrix/ownership_pointers_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `lent` parameter | `programs/lent_derived` | none |
| `lent` result | `programs/lent_params` | none |
| `keep` parameter | `programs/closures` | none |
| iterator to slice, `to_slice` | function `programs/effects_edges`, module level `matrix/ownership_to_slice_at_module_level`, method `matrix/ownership_pointers_in_contexts`, loop body `matrix/ownership_pointers_in_contexts`, switch arm `matrix/ownership_pointers_in_contexts`, `if let` `matrix/ownership_pointers_in_contexts`, `defer` `matrix/ownership_pointers_in_contexts`, `undo` `matrix/ownership_pointers_in_contexts`, catch handler `matrix/ownership_pointers_in_contexts`, closure `matrix/ownership_pointers_in_contexts`, worker `matrix/ownership_pointers_in_contexts`, `dispatch` or `parallel` `matrix/ownership_pointers_in_contexts`, `sync` block `matrix/ownership_pointers_in_contexts`, `select` arm `matrix/ownership_pointers_in_contexts`, `tests` block `anti-test/com/example/ownership`, `fixtures` block `anti-test/com/example/ownership`, generic body `matrix/ownership_pointers_in_contexts`, nested type `matrix/ownership_pointers_in_contexts`, library module `modules/ownership/com/example/ownership` | none |
| `own` field or parameter | `matrix/ownership_own_fields_and_parameters`, and `matrix/ownership_values_in_an_argument` for an `own` parameter of each kind of owning value | none |
| `delete(p, from)` and `destroy(p, from)` | `std/deserialize_alloc` | none |

The kinds of ownership:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `alloc` of a literal | pointer `programs/alias_access` | none |
| `alloc(T, n)` | nullable pointer `programs/alias_integers`, slice `matrix/ownership_alloc_and_free_kinds`, pointer `matrix/ownership_alloc_and_free_kinds` | none |
| `free` | pointer `programs/alias_integers`, nullable pointer `programs/channels`, slice `matrix/ownership_free_of_a_slice` | none |
| `delete` | pointer `programs/alias_class_pointers`, channel `programs/channel_workers`, nullable pointer `matrix/ownership_alloc_and_free_kinds` | none |
| `dup` | pointer `programs/delete_owned`, lent value `programs/lent_params`, function value `programs/snapshots`, tuple `programs/owning_values`, variant `programs/variant_owning`, class `programs/caller_params`, generic parameter `programs/caller_params`, i8 `matrix/ownership_dup_of_an_i8`, i16 `matrix/ownership_dup_of_an_i16`, i32 `matrix/ownership_dup_of_an_i32`, i64 `matrix/ownership_dup_of_an_i64`, int `matrix/ownership_dup_of_an_int`, u8 `matrix/ownership_dup_of_a_u8`, u16 `matrix/ownership_dup_of_a_u16`, u32 `matrix/ownership_dup_of_a_u32`, u64 `matrix/ownership_dup_of_a_u64`, uint `matrix/ownership_dup_of_a_uint`, byte `matrix/ownership_dup_of_a_byte`, f32 `matrix/ownership_dup_of_an_f32`, f64 `matrix/ownership_dup_of_an_f64`, f16 `matrix/ownership_dup_of_an_f16`, bool `matrix/ownership_dup_of_a_bool`, char `matrix/ownership_dup_of_a_char`, str `matrix/ownership_dup_of_a_str`, slice `matrix/ownership_dup_of_a_slice`, fixed array `matrix/ownership_dup_of_a_plain_array`, nullable pointer `matrix/ownership_dup_of_a_pointer_that_may_be_none`, optional value `matrix/ownership_dup_of_a_value_that_may_be_none` T035, may-fail function value `matrix/ownership_dup_of_a_failing_function`, channel `matrix/ownership_dup_of_a_channel`, struct `matrix/ownership_dup_kinds`, packed struct `matrix/ownership_dup_kinds`, simd struct `matrix/ownership_dup_kinds`, union `matrix/ownership_dup_of_a_union`, enum `matrix/ownership_dup_of_an_enum`, synchronized class `matrix/ownership_dup_kinds`, concurrent class `matrix/ownership_dup_kinds`, interface `matrix/ownership_dup_kinds` | none |
| address `&x` | pointer `programs/alias_access` | none |
| read through `*p` | i8 `programs/alias_bytes`, i16 `programs/alias_integers`, i32 `programs/alias_c_functions`, int `programs/alias_access`, u16 `programs/alias_integers`, u32 `programs/alias_integers`, u64 `programs/alias_integers`, byte `programs/alias_bytes`, f32 `programs/alias_view_local`, f16 `programs/half`, char `programs/alias_integers`, str `programs/alias_pointers`, slice `programs/alias_pointers`, fixed array `programs/alias_arrays`, pointer `programs/alias_access`, nullable pointer `programs/alias_pointers`, lent value `programs/for_loop`, enum `programs/alias_integers`, variant `programs/alias_unions`, class `programs/eq_compiled`, synchronized class `programs/sync_operators`, concurrent class `std/concurrent_map`, generic parameter `programs/owning_values`, i64 `matrix/ownership_read_kinds`, u8 `matrix/ownership_read_kinds`, uint `matrix/ownership_read_kinds`, f64 `matrix/ownership_read_kinds`, bool `matrix/ownership_read_kinds`, optional value `matrix/ownership_read_kinds`, may-fail function value `matrix/ownership_read_kinds`, function value `matrix/ownership_read_kinds`, tuple `matrix/ownership_read_kinds`, channel `matrix/ownership_read_kinds`, struct `matrix/ownership_read_kinds`, packed struct `matrix/ownership_read_kinds`, simd struct `matrix/ownership_read_kinds`, union `matrix/ownership_read_kinds`, interface `matrix/ownership_read_kinds` | none |
| `??` | int `programs/optional_values`, str `programs/optional_values`, pointer `programs/none_operators`, nullable pointer `programs/closures`, optional value `programs/optional_values`, function value `programs/closures`, tuple `std/concurrent_map`, class `std/collection_deque`, i8 `matrix/ownership_none_or_kinds`, i16 `matrix/ownership_none_or_kinds`, i32 `matrix/ownership_none_or_kinds`, i64 `matrix/ownership_none_or_kinds`, u8 `matrix/ownership_none_or_kinds`, u16 `matrix/ownership_none_or_kinds`, u32 `matrix/ownership_none_or_kinds`, u64 `matrix/ownership_none_or_kinds`, uint `matrix/ownership_none_or_kinds`, byte `matrix/ownership_none_or_kinds`, f32 `matrix/ownership_none_or_kinds`, f64 `matrix/ownership_none_or_kinds`, f16 `matrix/ownership_none_or_kinds`, bool `matrix/ownership_none_or_kinds`, char `matrix/ownership_none_or_kinds`, slice `matrix/ownership_none_or_kinds`, fixed array `matrix/ownership_none_or_kinds`, lent value `matrix/ownership_lent_values`, may-fail function value `matrix/ownership_none_or_kinds`, channel `matrix/ownership_none_or_kinds`, struct `matrix/ownership_none_or_kinds`, packed struct `matrix/ownership_none_or_kinds`, simd struct `matrix/ownership_none_or_kinds`, union `matrix/ownership_none_or_kinds`, enum `matrix/ownership_none_or_kinds`, variant `matrix/ownership_none_or_kinds`, synchronized class `matrix/ownership_none_or_kinds`, concurrent class `matrix/ownership_none_or_kinds`, interface `matrix/ownership_none_or_kinds`, generic parameter `matrix/ownership_pointers_in_contexts` | none |

The step `ownership` filled the two tables above and wrote the tests of the
places a value goes. `tests/matrix/` holds its sources under the names
`ownership_*`.

- `matrix/ownership_alloc_in_contexts` holds `alloc` of a literal,
  `alloc(T, n)`, `alloc` of a class with arguments, `free`, `delete` and
  `destroy` in each place, and `matrix/ownership_pointers_in_contexts` holds
  `dup`, `&x`, `*p`, `none`, `??`, `p?.x` and `to_slice` there. A line of
  their output names the construct and the place.
- `anti-test/com/example/ownership` is the module of the test
  `anti_test_ownership`. It holds the `tests` block and the `fixtures`
  block.
- `modules/ownership/com/example/ownership` is the library of the test
  `ownership_modules`. It holds each construct in a plain function and in a
  generic one. `client` makes, copies and deletes an object of a class of
  the library.
- `matrix/ownership_dup_kinds`, `matrix/ownership_read_kinds`,
  `matrix/ownership_none_or_kinds` and `matrix/ownership_alloc_and_free_kinds`
  hold the kinds. 26 refusals `matrix/ownership_dup_of_*` hold the kinds
  that `dup` does not copy.
- Eleven programs `matrix/ownership_values_in_*` hold the places a value
  goes: a `let`, an assignment, a `return`, an argument, a field, an
  element, a capture, a dispatch, a channel, a `yield` and a temporary. Each
  takes seven kinds of owning value through its place: a class with a
  `destruct`, a class with an `own` field, an owning struct, tuple, variant,
  `?T` and array. Each counts the values alive, so a teardown that is
  missed or runs twice shows.
- 98 refusals hold the places the rules refuse, 14 forms for each of the
  seven kinds: `matrix/ownership_let_copies_*`, `_assignment_copies_*`,
  `_read_through_a_pointer_copies_*`, `_return_of_a_field_copies_*`,
  `_return_of_a_parameter_*`, `_let_of_a_parameter_*`,
  `_own_argument_of_a_field_*`, `_named_after_a_move_*`,
  `_field_of_a_literal_copies_*`, `_element_of_a_literal_copies_*`,
  `_element_assignment_copies_*`, `_snapshot_captures_*`, `_send_copies_*`
  and `_yield_copies_*`.
- `matrix/ownership_lent_values` holds a lent pointer and a lent slice in
  the places that take one, and 30 refusals `matrix/ownership_lent_*` hold
  the places that do not.
- `matrix/ownership_own_fields_and_parameters` holds `own` pointers and
  `own` slices with their teardown order and their copy.
- `matrix/ownership_teardown_at_every_exit` holds each way out of a block,
  and eleven refusals hold the four moves the checker refuses and what
  follows a move, a literal and `destroy`.
- `matrix/ownership_slices_and_views` holds slices and views by `as`. Three
  trap tests of `tests/traps/` hold the bounds of decision 24.
- `matrix/ownership_allocators` holds `anti.mem` with `delete(p, from)` and
  `destroy(p, from)`. `matrix/ownership_shared_handles` holds a handle of
  `Shared<T>`, `matrix/ownership_generic_values` a value of a type
  parameter and `matrix/ownership_collection_values` a `List` of owning
  values, each in every place a value goes.

Where two rules meet, and where three do:

- A `may fail` function, a variant with an owning case and a release build:
  `matrix/ownership_failing_function_gives_an_owning_variant`, T042.
- A `dispatch`, an `own` parameter and a local that owns memory:
  `matrix/ownership_dispatch_moves_a_local`, T040.
- A `dispatch`, a parameter that is not `own` and a fresh owning value:
  `matrix/ownership_dispatch_of_a_fresh_owning_value`, T039.
- A `catch` handler, `yield` and an existing owning value: the seven
  refusals `matrix/ownership_yield_copies_*`, T038.
- A temporary, an owning value and a read by `is`, by a test for `none`
  or by an index: `matrix/ownership_temporary_read_by_is`,
  `_tested_for_none` and `_read_by_an_index`, T041.
- A closure, a captured owning local and `=` inside the closure:
  `matrix/ownership_values_in_a_capture`.
- `alloc(T, n)`, a class and `=` into memory that holds no value yet:
  `matrix/ownership_values_in_an_element`.
- A type parameter, an `own` parameter and a `let`:
  `matrix/ownership_generic_values`.
- An `undo`, a `defer` and an owning local on the fail path:
  `matrix/ownership_teardown_at_every_exit`.
- A generic of a library module and a `static atomic` field: the test
  `generic_atomic_modules`, T029.
- `if let`, `let ... else` and the result of `as?`:
  `matrix/ownership_downcast_in_an_if_let` and `_in_a_let_else`, T036.
- `alloc(T, n)`, a count that fails and the optimizer of a release build:
  `matrix/ownership_alloc_of_a_negative_count`, T033 and T034.

### Errors

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `may fail` function | module level `programs/catch_exits`, class body `programs/construct_args`, generic `programs/generic_may_fail`, library module `modules/chain/com/example/animal`, `tests` block the refusal `errors/tests_may_fail` | none |
| `fail` | function `programs/catch_exits`, method `programs/construct_args`, loop body `programs/walk_owned_parts`, catch handler `programs/catch_exits`, closure `programs/closures`, `sync` block `programs/sync_exits`, generic body `programs/generic_may_fail`, library module `modules/chain/com/example/animal`, switch arm `matrix/errors_fail`, `if let` `matrix/errors_fail`, `defer` `matrix/flow_fail_in_defer` T001, `undo` `matrix/flow_fail_in_undo` T001, worker `matrix/errors_fail`, `dispatch` or `parallel` `matrix/errors_fail`, `select` arm `matrix/errors_fail`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, nested type `matrix/errors_fail` | none |
| `try` on a call | function `programs/closures`, method `programs/construct_base`, loop body `programs/pattern_walk`, `sync` block `programs/sync_exits`, generic body `programs/generic_may_fail`, library module `modules/failing/com/example/counter`, switch arm `matrix/errors_try`, `if let` `matrix/errors_try`, `defer` `matrix/flow_try_in_defer` T001, `undo` `matrix/flow_try_in_undo` T001, catch handler `matrix/errors_try`, closure `matrix/errors_try`, worker `matrix/errors_try`, `dispatch` or `parallel` `matrix/errors_try`, `select` arm `matrix/errors_try`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, nested type `matrix/errors_try` | none |
| `try { } catch` | function `programs/catch_exits`, loop body `programs/catch_exits`, method `matrix/errors_try_block`, switch arm `matrix/errors_try_block`, `if let` `matrix/errors_try_block`, `defer` `matrix/errors_try_block`, `undo` `matrix/errors_try_block`, catch handler `matrix/errors_try_block`, closure `matrix/errors_try_block`, worker `matrix/errors_try_block`, `dispatch` or `parallel` `matrix/errors_try_block`, `sync` block `matrix/errors_try_block`, `select` arm `matrix/errors_try_block`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, generic body `matrix/errors_try_block`, nested type `matrix/errors_try_block`, library module `modules/errors/com/example/errors` | none |
| `catch e { }` on a call | function `programs/catch_exits`, method `errors/allow_levels`, loop body `programs/catch_exits`, worker `std/concurrent_map_threads`, `sync` block `programs/sync_exits`, `tests` block `errors/allow_levels`, switch arm `matrix/errors_catch_named`, `if let` `matrix/errors_catch_named`, `defer` `matrix/errors_catch_named`, `undo` `matrix/errors_catch_named`, catch handler `matrix/errors_catch_named`, closure `matrix/errors_catch_named`, `dispatch` or `parallel` `matrix/errors_catch_named`, `select` arm `matrix/errors_catch_named`, `fixtures` block `anti-test/com/example/errors`, generic body `matrix/errors_catch_named`, nested type `matrix/errors_catch_named`, library module `modules/errors/com/example/errors` | none |
| `catch { }` without a name | function `programs/catch_exits`, loop body `programs/catch_exits`, method `matrix/errors_catch_unnamed`, switch arm `matrix/errors_catch_unnamed`, `if let` `matrix/errors_catch_unnamed`, `defer` `matrix/errors_catch_unnamed`, `undo` `matrix/errors_catch_unnamed`, catch handler `matrix/errors_catch_unnamed`, closure `matrix/errors_catch_unnamed`, worker `matrix/errors_catch_unnamed`, `dispatch` or `parallel` `matrix/errors_catch_unnamed`, `sync` block `matrix/errors_catch_unnamed`, `select` arm `matrix/errors_catch_unnamed`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, generic body `matrix/errors_catch_unnamed`, nested type `matrix/errors_catch_unnamed`, library module `modules/errors/com/example/errors` | none |
| `catch fatal` | function `programs/construct_args`, method `std/interfaces`, loop body `std/sync_equal`, switch arm `programs/nested_locals`, `defer` `programs/nested_locals`, `undo` `programs/nested_locals`, catch handler `programs/nested_locals`, worker `std/sync_equal`, `if let` `matrix/errors_catch_fatal`, closure `matrix/errors_catch_fatal`, `dispatch` or `parallel` `matrix/errors_catch_fatal`, `sync` block `matrix/errors_catch_fatal`, `select` arm `matrix/errors_catch_fatal`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, generic body `matrix/errors_catch_fatal`, nested type `matrix/errors_catch_fatal`, library module `modules/errors/com/example/errors` | none |
| `yield` | function `programs/catch_exits`, catch handler `programs/catch_exits`, method `matrix/errors_yield`, loop body `matrix/errors_yield`, switch arm `matrix/errors_yield`, `if let` `matrix/errors_yield`, `defer` `matrix/errors_yield`, `undo` `matrix/errors_yield`, closure `matrix/errors_yield`, worker `matrix/errors_yield`, `dispatch` or `parallel` `matrix/errors_yield`, `sync` block `matrix/errors_yield`, `select` arm `matrix/errors_yield`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, generic body `matrix/errors_yield`, nested type `matrix/errors_yield`, library module `modules/errors/com/example/errors` | none |
| `undo` | function `programs/generic_may_fail`, generic body `programs/generic_may_fail`, method `matrix/errors_undo`, loop body `matrix/errors_undo`, switch arm `matrix/errors_undo`, `if let` `matrix/errors_undo`, `defer` `matrix/flow_undo_in_defer`, `undo` `matrix/flow_undo_in_the_block_of_an_undo`, catch handler `matrix/errors_undo`, closure `matrix/errors_undo`, worker `matrix/errors_undo`, `dispatch` or `parallel` `matrix/errors_undo`, `sync` block `matrix/errors_undo`, `select` arm `matrix/errors_undo`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, nested type `matrix/errors_undo`, library module `modules/errors/com/example/errors` | none |
| `-> never` | `programs/never_paths` | none |
| `here` as an argument | function `programs/location`, method `matrix/errors_here`, generic body `matrix/errors_here`, library module `modules/errors/com/example/errors`, module level the refusal `matrix/errors_here_at_module_level`, loop body `matrix/errors_here`, switch arm `matrix/errors_here`, `if let` `matrix/errors_here`, `defer` `matrix/errors_here`, `undo` `matrix/errors_here`, catch handler `matrix/errors_here`, closure `matrix/errors_here`, worker `matrix/errors_here`, `dispatch` or `parallel` `matrix/errors_here`, `sync` block `matrix/errors_here`, `select` arm `matrix/errors_here`, `tests` block `anti-test/com/example/errors`, `fixtures` block `anti-test/com/example/errors`, nested type `matrix/errors_here` | none |
| `catch none` | `programs/catch_none` | none |
| `allow(name, "reason")` | `programs/simd_big` | none |
| `unchecked(name, "reason")` | `programs/lock_free_queue` | none |
| `fail` with a text | `programs/closures` | none |

The kinds of errors:

| Construct | Kinds held | Kinds open |
|---|---|---|
| `fail` | str `programs/closures`, pointer `programs/catch_exits`, nullable pointer the refusal `matrix/errors_fail_of_a_pointer_that_may_be_none` | none |
| `yield` | int `programs/catch_exits`, str `programs/handled_operand`, slice `std/fs`, pointer `programs/none_operators`, nullable pointer `programs/catch_none`, optional value `programs/optional_values`, tuple `programs/generic_may_fail`, struct `std/reflect_call`, class `programs/construct_args`, i8 `matrix/errors_yield_kinds`, i16 `matrix/errors_yield_kinds`, i32 `matrix/errors_yield_kinds`, i64 `matrix/errors_yield_kinds`, u8 `matrix/errors_yield_kinds`, u16 `matrix/errors_yield_kinds`, u32 `matrix/errors_yield_kinds`, u64 `matrix/errors_yield_kinds`, uint `matrix/errors_yield_kinds`, byte `matrix/errors_yield_kinds`, f32 `matrix/errors_yield_kinds`, f64 `matrix/errors_yield_kinds`, f16 the refusal `matrix/errors_failing_function_gives_an_f16`, bool `matrix/errors_yield_kinds`, char `matrix/errors_yield_kinds`, fixed array `matrix/errors_yield_kinds`, lent value the refusal `matrix/errors_yield_of_a_lent_pointer`, may-fail function value `matrix/errors_yield_kinds`, function value `matrix/errors_yield_kinds`, channel `matrix/errors_yield_kinds`, packed struct `matrix/errors_yield_kinds`, simd struct `matrix/errors_yield_kinds`, union `matrix/errors_yield_kinds`, enum `matrix/errors_yield_kinds`, variant `matrix/errors_yield_kinds`, synchronized class `matrix/errors_yield_kinds`, concurrent class `matrix/errors_yield_kinds`, interface `matrix/errors_yield_kinds`, generic parameter `matrix/errors_generic_numbers` | none |

The step `errors` filled the two tables above on 2026-10-12.
`tests/matrix/` holds its sources under the names `errors_*`.

- `matrix/errors_<construct>` holds the construct in every place of its
  row, one function `in_<context>` and one line of output per place and
  input, as `matrix/flow_<construct>` does. `matrix/errors_undo` and
  `matrix/errors_fail` are written by hand, since each place needs a
  function that may fail.
- `anti-test/com/example/errors` is the module of the test
  `anti_test_errors`. It holds the `tests` block and the `fixtures` block.
  A test takes no `may fail`, so a `fail`, a `try` on a call and an `undo`
  of the `tests` block stand in an anonymous function of a test.
- `modules/errors/com/example/errors` is the library of the test
  `errors_modules`. It holds each construct in a plain function and in a
  generic one. `client` handles the errors of the library and tests the
  class of one with `is`.
- `matrix/errors_yield_kinds` takes each kind through a function that may
  fail and through the `yield` of its handler. `matrix/errors_optional_kinds`
  takes each kind as the `T` of a `?T` that a failing function gives, with
  a handler, `catch none`, `if let` and `let ... else`.
  `matrix/errors_generic_numbers` and `matrix/errors_generic_values` pass
  each kind as the type argument of a generic function that may fail.
- Four cells hold a refusal. A function of a `tests` block takes no
  `may fail`, a constant holds no `here`, a function gives no `f16`, and a
  handler yields no lent pointer.

The constructs of the area that have no row of their own:

- A handler that can reach its end, by decision 20: nine refusals
  `matrix/errors_handler_*`, T044. `matrix/errors_handler_exits` holds the
  ways a handler of a value may end.
- `try` on an optional, by decision 21: five refusals
  `matrix/errors_try_on_an_optional_*`, T045 and T046.
- The error a handler binds: six refusals `matrix/errors_caught_error_*`
  where `=`, `yield` and a literal store it, T047, and two where a `let`
  binds it.
- `-> never`: `matrix/errors_never` holds a call of such a function in
  eleven places, and four refusals hold the function types and the
  anonymous function.
- `catch fatal` that ends the program: five programs
  `matrix/errors_fatal_*`. They hold it on a call, on an error without a
  code, on a pointer and a value that hold `none`, and in a worker.
- The origin of an error: `matrix/errors_origins`, and the test
  `generic_origin_modules` for a generic of a library, T043.
- 21 refusals hold messages of the area that antic gives. Some hold a
  handler and a `try` on a call that cannot fail. Some hold a failing call
  without a handler, in a handler and in an anonymous function. The others
  hold `fail` and `try` where the function cannot fail, and the function
  types that do not convert.

Where two rules meet, and where three do:

- A generic function, `may fail` and a `?T` whose type argument may be
  `none`: `matrix/errors_generic_optional_of_a_nullable_pointer` and
  `_of_an_optional_value`, T049, and
  `matrix/errors_generic_failing_gives_an_optional_of_an_optional`.
- A generic function, an `own` parameter and the `yield` of a handler:
  `matrix/errors_generic_yield_of_an_own_parameter`, T038, and
  `matrix/errors_generic_owning` for the teardown on the fail path.
- A generic function, a library file and the origin of a `fail`:
  `generic_origin_modules`, T043.
- A function that may fail, a `?T` result and each kind of value:
  `matrix/errors_optional_kinds`.
- A `?T`, a `??T` and a conversion: `matrix/errors_optional_of_an_optional`,
  T048.
- `p?.f()`, a function that may fail and a handler or `try`:
  `matrix/errors_optional_results`.
- A handler, a guard and one binding: `handler_then_guard` of
  `matrix/errors_optional_results`.
- A handler, a `try` block inside it and `yield`:
  `matrix/errors_yield_in_a_try_handler_of_a_handler`.
- A `try` block, a handler inside it and a failing call in that handler:
  `matrix/errors_nested_handlers`, which counts the errors alive.
- `fail`, `undo` and `defer` and the order they run in:
  `matrix/errors_values_before_the_defers`.
- A `construct` that may fail, `alloc` and `try`:
  `matrix/errors_try_before_an_alloc`, T050.
- A `construct` that may fail, a base and an `own` field:
  `matrix/errors_derived_construct_fails_after_its_base`, T052.
- A synchronized class, a function that may fail and a worker:
  `matrix/errors_synchronized_method_fails`.
- An atomic, `try` as an operand and `undo`: `matrix/errors_atomics`.
- A function type, `may fail` written twice and a call through a value:
  `matrix/errors_function_types`.

### Threads

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `worker fn` | module level `programs/channel_workers`, generic `programs/generic_fn_args`, library module `modules/closures/com/example/walk` | none |
| `parallel` | function `programs/parallel_sum`, module level the refusal `matrix/threads_parallel_at_module_level`, method `matrix/threads_parallel`, loop body `matrix/threads_parallel`, switch arm `matrix/threads_parallel`, `if let` `matrix/threads_parallel`, `defer` `matrix/threads_parallel`, `undo` `matrix/threads_parallel`, catch handler `matrix/threads_parallel`, closure `matrix/threads_parallel`, worker `matrix/threads_parallel`, `dispatch` or `parallel` `matrix/threads_parallel`, `sync` block `matrix/threads_parallel`, `select` arm `matrix/threads_parallel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_parallel`, nested type `matrix/threads_parallel`, library module `modules/threads/com/example/threads` | none |
| `parallel ... by` | function `programs/channels`, loop body `programs/parallel_join`, library module `modules/closures/com/example/walk`, module level the refusal `matrix/threads_parallel_by_at_module_level`, method `matrix/threads_parallel`, switch arm `matrix/threads_parallel`, `if let` `matrix/threads_parallel`, `defer` `matrix/threads_parallel`, `undo` `matrix/threads_parallel`, catch handler `matrix/threads_parallel`, closure `matrix/threads_parallel`, worker `matrix/threads_parallel`, `dispatch` or `parallel` `matrix/threads_parallel`, `sync` block `matrix/threads_parallel`, `select` arm `matrix/threads_parallel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_parallel`, nested type `matrix/threads_parallel` | none |
| `dispatch` | function `programs/channel_workers`, loop body `programs/parallel_join`, worker `programs/dispatch_job`, module level the refusal `matrix/threads_dispatch_at_module_level`, method `matrix/threads_dispatch`, switch arm `matrix/threads_dispatch`, `if let` `matrix/threads_dispatch`, `defer` `matrix/threads_dispatch`, `undo` `matrix/threads_dispatch`, catch handler `matrix/threads_dispatch`, closure `matrix/threads_dispatch`, `dispatch` or `parallel` `matrix/threads_dispatch`, `sync` block `matrix/threads_dispatch`, `select` arm `matrix/threads_dispatch`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_dispatch`, nested type `matrix/threads_dispatch`, library module `modules/threads/com/example/threads` | none |
| `join` | function `programs/channel_workers`, loop body `programs/parallel_join`, worker `programs/dispatch_job`, module level the refusal `matrix/threads_join_at_module_level`, method `matrix/threads_dispatch`, switch arm `matrix/threads_dispatch`, `if let` `matrix/threads_dispatch`, `defer` `matrix/threads_dispatch`, `undo` `matrix/threads_dispatch`, catch handler `matrix/threads_dispatch`, closure `matrix/threads_dispatch`, `dispatch` or `parallel` `matrix/threads_dispatch`, `sync` block `matrix/threads_dispatch`, `select` arm `matrix/threads_dispatch`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_dispatch`, nested type `matrix/threads_dispatch`, library module `modules/threads/com/example/threads` | none |
| `join_all` | function `programs/dispatch_job`, method `matrix/threads_dispatch`, loop body `matrix/threads_dispatch`, switch arm `matrix/threads_dispatch`, `if let` `matrix/threads_dispatch`, `defer` `matrix/threads_dispatch`, `undo` `matrix/threads_dispatch`, catch handler `matrix/threads_dispatch`, closure `matrix/threads_dispatch`, worker `matrix/threads_dispatch`, `dispatch` or `parallel` `matrix/threads_dispatch`, `sync` block `matrix/threads_dispatch`, `select` arm `matrix/threads_dispatch`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_dispatch`, nested type `matrix/threads_dispatch`, library module `modules/threads/com/example/threads` | module level: the call gives no value, and a constant or a default takes one |
| `sync m { }` | function `programs/closures`, method `programs/concurrent`, loop body `programs/concurrent`, closure `programs/closures`, worker `programs/sync_pairs`, `sync` block `programs/concurrent`, generic body `programs/generic_sync`, library module `modules/sync/com/example/queue`, switch arm `matrix/threads_mutex`, `if let` `matrix/threads_mutex`, `defer` `matrix/threads_mutex`, `undo` `matrix/threads_mutex`, catch handler `matrix/threads_mutex`, `dispatch` or `parallel` `matrix/threads_mutex`, `select` arm `matrix/threads_mutex`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, nested type `matrix/threads_mutex` | none |
| `Mutex.new()` | function `programs/closures`, library module `modules/sync/com/example/queue`, module level the refusal `matrix/threads_mutex_new_at_module_level`, method `matrix/threads_mutex`, loop body `matrix/threads_mutex`, switch arm `matrix/threads_mutex`, `if let` `matrix/threads_mutex`, `defer` `matrix/threads_mutex`, `undo` `matrix/threads_mutex`, catch handler `matrix/threads_mutex`, closure `matrix/threads_mutex`, worker `matrix/threads_mutex`, `dispatch` or `parallel` `matrix/threads_mutex`, `sync` block `matrix/threads_mutex`, `select` arm `matrix/threads_mutex`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_mutex`, nested type `matrix/threads_mutex` | none |
| `m.destroy()` of a Mutex | function `programs/closures`, method `matrix/threads_mutex`, loop body `matrix/threads_mutex`, switch arm `matrix/threads_mutex`, `if let` `matrix/threads_mutex`, `defer` `matrix/threads_mutex`, `undo` `matrix/threads_mutex`, catch handler `matrix/threads_mutex`, closure `matrix/threads_mutex`, worker `matrix/threads_mutex`, `dispatch` or `parallel` `matrix/threads_mutex`, `sync` block `matrix/threads_mutex`, `select` arm `matrix/threads_mutex`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_mutex`, nested type `matrix/threads_mutex`, library module `modules/threads/com/example/threads` | module level: the call gives no value, and a constant or a default takes one |
| `chan T(n)` | function `programs/channel_workers`, library module `modules/sync/com/example/queue`, module level the refusal `matrix/threads_chan_at_module_level`, method `matrix/threads_channel`, loop body `matrix/threads_channel`, switch arm `matrix/threads_channel`, `if let` `matrix/threads_channel`, `defer` `matrix/threads_channel`, `undo` `matrix/threads_channel`, catch handler `matrix/threads_channel`, closure `matrix/threads_channel`, worker `matrix/threads_channel`, `dispatch` or `parallel` `matrix/threads_channel`, `sync` block `matrix/threads_channel`, `select` arm `matrix/threads_channel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_channel`, nested type `matrix/threads_channel` | none |
| `send` | function `programs/channel_workers`, loop body `programs/channel_workers`, worker `programs/channel_workers`, method `matrix/threads_channel`, switch arm `matrix/threads_channel`, `if let` `matrix/threads_channel`, `defer` `matrix/threads_channel`, `undo` `matrix/threads_channel`, catch handler `matrix/threads_channel`, closure `matrix/threads_channel`, `dispatch` or `parallel` `matrix/threads_channel`, `sync` block `matrix/threads_channel`, `select` arm `matrix/threads_channel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_channel`, nested type `matrix/threads_channel`, library module `modules/threads/com/example/threads` | none |
| `recv` | function `programs/channel_workers`, loop body `programs/channel_workers`, worker `programs/channel_workers`, library module `modules/sync/com/example/queue`, module level the refusal `matrix/threads_recv_at_module_level`, method `matrix/threads_channel`, switch arm `matrix/threads_channel`, `if let` `matrix/threads_channel`, `defer` `matrix/threads_channel`, `undo` `matrix/threads_channel`, catch handler `matrix/threads_channel`, closure `matrix/threads_channel`, `dispatch` or `parallel` `matrix/threads_channel`, `sync` block `matrix/threads_channel`, `select` arm `matrix/threads_channel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_channel`, nested type `matrix/threads_channel` | none |
| `close` | function `programs/channel_workers`, worker `programs/channel_workers`, method `matrix/threads_channel`, loop body `matrix/threads_channel`, switch arm `matrix/threads_channel`, `if let` `matrix/threads_channel`, `defer` `matrix/threads_channel`, `undo` `matrix/threads_channel`, catch handler `matrix/threads_channel`, closure `matrix/threads_channel`, `dispatch` or `parallel` `matrix/threads_channel`, `sync` block `matrix/threads_channel`, `select` arm `matrix/threads_channel`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_channel`, nested type `matrix/threads_channel`, library module `modules/threads/com/example/threads` | module level: the call gives no value, and a constant or a default takes one |
| `select` | function `programs/channel_workers`, loop body `programs/channel_workers`, library module `modules/sync/com/example/queue`, method `matrix/threads_select`, switch arm `matrix/threads_select`, `if let` `matrix/threads_select`, `defer` `matrix/threads_select`, `undo` `matrix/threads_select`, catch handler `matrix/threads_select`, closure `matrix/threads_select`, worker `matrix/threads_select`, `dispatch` or `parallel` `matrix/threads_select`, `sync` block `matrix/threads_select`, `select` arm `matrix/threads_select`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_select`, nested type `matrix/threads_select` | none |
| atomic `load` | function `programs/alloc_owning`, method `programs/concurrent`, loop body `programs/parallel_join`, worker `programs/parallel_join`, generic body `programs/generic_sync`, module level the refusal `matrix/threads_atomic_load_at_module_level`, switch arm `matrix/threads_atomic_places`, `if let` `matrix/threads_atomic_places`, `defer` `matrix/threads_atomic_places`, `undo` `matrix/threads_atomic_places`, catch handler `matrix/threads_atomic_places`, closure `matrix/threads_atomic_places`, `dispatch` or `parallel` `matrix/threads_atomic_places`, `sync` block `matrix/threads_atomic_places`, `select` arm `matrix/threads_atomic_places`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, nested type `matrix/threads_atomic_places`, library module `modules/threads/com/example/threads` | none |
| atomic `store` | function `programs/atomic_widths`, loop body `programs/parallel_join`, worker `programs/parallel_join`, method `matrix/threads_atomic_places`, switch arm `matrix/threads_atomic_places`, `if let` `matrix/threads_atomic_places`, `defer` `matrix/threads_atomic_places`, `undo` `matrix/threads_atomic_places`, catch handler `matrix/threads_atomic_places`, closure `matrix/threads_atomic_places`, `dispatch` or `parallel` `matrix/threads_atomic_places`, `sync` block `matrix/threads_atomic_places`, `select` arm `matrix/threads_atomic_places`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_places`, nested type `matrix/threads_atomic_places`, library module `modules/threads/com/example/threads` | module level: the call gives no value, and a constant or a default takes one |
| atomic `swap` | function `programs/atomic_widths`, module level the refusal `matrix/threads_atomic_swap_at_module_level`, method `matrix/threads_atomic_places`, loop body `matrix/threads_atomic_places`, switch arm `matrix/threads_atomic_places`, `if let` `matrix/threads_atomic_places`, `defer` `matrix/threads_atomic_places`, `undo` `matrix/threads_atomic_places`, catch handler `matrix/threads_atomic_places`, closure `matrix/threads_atomic_places`, worker `matrix/threads_atomic_places`, `dispatch` or `parallel` `matrix/threads_atomic_places`, `sync` block `matrix/threads_atomic_places`, `select` arm `matrix/threads_atomic_places`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_places`, nested type `matrix/threads_atomic_places`, library module `modules/threads/com/example/threads` | none |
| atomic `add` | function `programs/alloc_owning`, method `programs/atomics`, loop body `programs/sync_pairs`, switch arm `programs/variant_owning`, `if let` `programs/variant_owning`, closure `programs/concurrent`, worker `programs/dispatch_job`, generic body `programs/generic_sync`, library module `modules/sync/com/example/queue`, module level the refusal `matrix/threads_atomic_add_at_module_level`, `defer` `matrix/threads_atomic_arithmetic`, `undo` `matrix/threads_atomic_arithmetic`, catch handler `matrix/threads_atomic_arithmetic`, `dispatch` or `parallel` `matrix/threads_atomic_arithmetic`, `sync` block `matrix/threads_atomic_arithmetic`, `select` arm `matrix/threads_atomic_arithmetic`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, nested type `matrix/threads_atomic_arithmetic` | none |
| atomic `sub` | function `programs/atomic_widths`, method `programs/alloc_owning`, module level the refusal `matrix/threads_atomic_sub_at_module_level`, loop body `matrix/threads_atomic_arithmetic`, switch arm `matrix/threads_atomic_arithmetic`, `if let` `matrix/threads_atomic_arithmetic`, `defer` `matrix/threads_atomic_arithmetic`, `undo` `matrix/threads_atomic_arithmetic`, catch handler `matrix/threads_atomic_arithmetic`, closure `matrix/threads_atomic_arithmetic`, worker `matrix/threads_atomic_arithmetic`, `dispatch` or `parallel` `matrix/threads_atomic_arithmetic`, `sync` block `matrix/threads_atomic_arithmetic`, `select` arm `matrix/threads_atomic_arithmetic`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_arithmetic`, nested type `matrix/threads_atomic_arithmetic`, library module `modules/threads/com/example/threads` | none |
| atomic `and` | function `programs/atomic_widths`, module level the refusal `matrix/threads_atomic_and_at_module_level`, method `matrix/threads_atomic_arithmetic`, loop body `matrix/threads_atomic_arithmetic`, switch arm `matrix/threads_atomic_arithmetic`, `if let` `matrix/threads_atomic_arithmetic`, `defer` `matrix/threads_atomic_arithmetic`, `undo` `matrix/threads_atomic_arithmetic`, catch handler `matrix/threads_atomic_arithmetic`, closure `matrix/threads_atomic_arithmetic`, worker `matrix/threads_atomic_arithmetic`, `dispatch` or `parallel` `matrix/threads_atomic_arithmetic`, `sync` block `matrix/threads_atomic_arithmetic`, `select` arm `matrix/threads_atomic_arithmetic`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_arithmetic`, nested type `matrix/threads_atomic_arithmetic`, library module `modules/threads/com/example/threads` | none |
| atomic `or` | function `programs/atomic_widths`, module level the refusal `matrix/threads_atomic_or_at_module_level`, method `matrix/threads_atomic_arithmetic`, loop body `matrix/threads_atomic_arithmetic`, switch arm `matrix/threads_atomic_arithmetic`, `if let` `matrix/threads_atomic_arithmetic`, `defer` `matrix/threads_atomic_arithmetic`, `undo` `matrix/threads_atomic_arithmetic`, catch handler `matrix/threads_atomic_arithmetic`, closure `matrix/threads_atomic_arithmetic`, worker `matrix/threads_atomic_arithmetic`, `dispatch` or `parallel` `matrix/threads_atomic_arithmetic`, `sync` block `matrix/threads_atomic_arithmetic`, `select` arm `matrix/threads_atomic_arithmetic`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_arithmetic`, nested type `matrix/threads_atomic_arithmetic`, library module `modules/threads/com/example/threads` | none |
| atomic `compare_swap` | function `programs/atomic_widths`, method `programs/lock_free_queue`, loop body `programs/lock_free_queue`, module level the refusal `matrix/threads_atomic_compare_swap_at_module_level`, switch arm `matrix/threads_atomic_places`, `if let` `matrix/threads_atomic_places`, `defer` `matrix/threads_atomic_places`, `undo` `matrix/threads_atomic_places`, catch handler `matrix/threads_atomic_places`, closure `matrix/threads_atomic_places`, worker `matrix/threads_atomic_places`, `dispatch` or `parallel` `matrix/threads_atomic_places`, `sync` block `matrix/threads_atomic_places`, `select` arm `matrix/threads_atomic_places`, `tests` block `anti-test/com/example/threads`, `fixtures` block `anti-test/com/example/threads`, generic body `matrix/threads_atomic_places`, nested type `matrix/threads_atomic_places`, library module `modules/threads/com/example/threads` | none |
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
| `chan T(n)` | int `programs/channel_workers`, channel `programs/channel_workers`, i8 `matrix/threads_channel_kinds`, i16 `matrix/threads_channel_kinds`, i32 `matrix/threads_channel_kinds`, i64 `matrix/threads_channel_kinds`, u8 `matrix/threads_channel_kinds`, u16 `matrix/threads_channel_kinds`, u32 `matrix/threads_channel_kinds`, u64 `matrix/threads_channel_kinds`, uint `matrix/threads_channel_kinds`, byte `matrix/threads_channel_kinds`, f32 `matrix/threads_channel_kinds`, f64 `matrix/threads_channel_kinds`, f16 `matrix/threads_channel_kinds`, bool `matrix/threads_channel_kinds`, char `matrix/threads_channel_kinds`, str `matrix/threads_channel_kinds`, slice the refusal `matrix/threads_channel_of_slices`, fixed array `matrix/threads_channel_kinds`, pointer the refusal `matrix/threads_channel_of_pointers`, nullable pointer the refusal `matrix/threads_channel_of_nullable_pointers`, optional value `matrix/threads_channel_kinds`, may-fail function value the refusal `matrix/threads_channel_of_failing_functions`, function value the refusal `matrix/threads_channel_of_functions`, tuple `matrix/threads_channel_kinds`, struct `matrix/threads_channel_kinds`, packed struct `matrix/threads_channel_kinds`, simd struct `matrix/threads_channel_kinds`, union `matrix/threads_channel_kinds`, enum `matrix/threads_channel_kinds`, variant `matrix/threads_channel_kinds`, class `matrix/threads_channel_kinds`, synchronized class `matrix/threads_channel_kinds`, concurrent class `matrix/threads_channel_kinds`, interface the refusal `matrix/threads_channel_of_interface_pointers`, generic parameter `matrix/threads_channel_kinds` | lent value: `lent` marks a parameter and the result of `operator fn value`, and the type of a channel is written without it |
| `send` | int `programs/channel_workers`, pointer `programs/channels`, channel `programs/channel_workers`, i8 `matrix/threads_channel_kinds`, i16 `matrix/threads_channel_kinds`, i32 `matrix/threads_channel_kinds`, i64 `matrix/threads_channel_kinds`, u8 `matrix/threads_channel_kinds`, u16 `matrix/threads_channel_kinds`, u32 `matrix/threads_channel_kinds`, u64 `matrix/threads_channel_kinds`, uint `matrix/threads_channel_kinds`, byte `matrix/threads_channel_kinds`, f32 `matrix/threads_channel_kinds`, f64 `matrix/threads_channel_kinds`, f16 `matrix/threads_channel_kinds`, bool `matrix/threads_channel_kinds`, char `matrix/threads_channel_kinds`, str `matrix/threads_channel_kinds`, fixed array `matrix/threads_channel_kinds`, optional value `matrix/threads_channel_kinds`, tuple `matrix/threads_channel_kinds`, struct `matrix/threads_channel_kinds`, packed struct `matrix/threads_channel_kinds`, simd struct `matrix/threads_channel_kinds`, union `matrix/threads_channel_kinds`, enum `matrix/threads_channel_kinds`, variant `matrix/threads_channel_kinds`, class `matrix/threads_channel_kinds`, synchronized class `matrix/threads_channel_kinds`, concurrent class `matrix/threads_channel_kinds`, generic parameter `matrix/threads_channel_kinds` | slice, nullable pointer, lent value, may-fail function value, function value, interface: no channel carries the kind, so no `send` takes it |
| atomic `load` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths`, bool `programs/atomic_widths`, nullable pointer `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, pointer `matrix/threads_atomic_kinds`, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `store` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/atomic_widths`, byte `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `swap` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `dump/atomics`, int `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, byte `matrix/threads_atomic_kinds`, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `add` | i8 `programs/atomic_widths`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, bool the refusal `matrix/threads_atomic_add_on_a_bool` T060, pointer the refusal `matrix/threads_atomic_add_on_a_pointer` T060, nullable pointer the refusal `matrix/threads_atomic_add_on_a_nullable_pointer` T060, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `sub` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `programs/atomic_widths`, int `programs/alloc_owning`, byte `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, bool the refusal `matrix/threads_atomic_sub_on_a_bool` T060, pointer the refusal `matrix/threads_atomic_sub_on_a_pointer` T060, nullable pointer the refusal `matrix/threads_atomic_sub_on_a_nullable_pointer` T060, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `and` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `dump/atomics`, int `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, byte `matrix/threads_atomic_kinds`, bool `matrix/threads_atomic_kinds`, pointer the refusal `matrix/threads_atomic_and_on_a_pointer` T060, nullable pointer the refusal `matrix/threads_atomic_and_on_a_nullable_pointer` T060, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `or` | i8 `programs/atomic_widths`, i16 `dump/atomics`, i32 `dump/atomics`, int `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, byte `matrix/threads_atomic_kinds`, bool `matrix/threads_atomic_kinds`, pointer the refusal `matrix/threads_atomic_or_on_a_pointer` T060, nullable pointer the refusal `matrix/threads_atomic_or_on_a_nullable_pointer` T060, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |
| atomic `compare_swap` | i8 `dump/atomics`, i16 `programs/atomic_widths`, i32 `programs/atomic_widths`, int `programs/atomic_widths`, bool `programs/atomic_widths`, pointer `programs/atomic_widths`, nullable pointer `programs/atomic_widths`, i64 `matrix/threads_atomic_kinds`, u8 `matrix/threads_atomic_kinds`, u16 `matrix/threads_atomic_kinds`, u32 `matrix/threads_atomic_kinds`, u64 `matrix/threads_atomic_kinds`, uint `matrix/threads_atomic_kinds`, byte `matrix/threads_atomic_kinds`, enum the refusal `matrix/threads_atomic_field_of_an_enum` T059 | none |

The step `threads` filled the two tables above on 2026-10-12.
`tests/matrix/` holds its sources under the names `threads_*`.

- `matrix/threads_dispatch`, `threads_parallel`, `threads_mutex`,
  `threads_channel`, `threads_select`, `threads_atomic_places` and
  `threads_atomic_arithmetic` hold their constructs in fifteen places
  each, one function `in_<context>` and one line of output per place and
  input, as `matrix/flow_<construct>` does. The place "`dispatch` or
  `parallel`" is the anonymous function at a `concurrent` parameter of a
  worker that a `dispatch` runs. The place "worker" is a worker that a
  `dispatch` runs.
- `anti-test/com/example/threads` is the module of the test
  `anti_test_threads`. It holds the `tests` block and the `fixtures` block.
- `modules/threads/com/example/threads` is the library of the test
  `threads_modules`. It holds each construct in a plain function and in a
  generic one. `dispatch`, `join` and `join_all` in a generic function of
  a library stand in `generic_dispatch_modules`, T061. The client runs two
  workers of the library. A worker is named alone after the arrow, so a
  direct import brings each name.
- A construct that gives a value is refused in a constant: fourteen
  refusals `matrix/threads_*_at_module_level`, and two for the default of
  a field and of a parameter. `join_all`, `m.destroy()`, `close` and
  `store` give no value, so those four cells cannot exist.
- `matrix/threads_channel_kinds` sends and receives each kind a channel
  carries. `matrix/threads_atomic_kinds` runs the operations on each kind
  an atomic holds, on a field and on a local.
  `matrix/threads_worker_kinds` passes each kind through a `dispatch` and
  a `parallel`, which have no row in the kinds table.
- Six kinds of `chan T(n)` hold a refusal, since a channel carries no
  pointer. The same kinds of `send` cannot exist, and neither can a lent
  value in either row.
- An atomic holds no enum. `matrix/types_atomic_local_of_an_enum` refuses
  the local, and the field compiles, T059.

The constructs of the area that have no row of their own:

- Decision 23: `matrix/threads_join_twice` and
  `matrix/threads_join_after_join_all`, T053, the test `check_join_twice`
  of `traps/join_twice`, T054, and `matrix/threads_dispatch_in_a_loop`
  and `matrix/threads_return_before_the_join`, T055. The two programs run
  with `--anti.threads=1`, so each job starts at its `join`.
- What a job stands in: `matrix/threads_jobs`. It holds `join_all` of no
  job and of a part of an array, a worker without a result and a job in a
  tuple. It holds a `dispatch` of an object in flight as well.
- The split of `parallel`: `matrix/threads_parallel_split`. The order of
  `select`: `matrix/threads_select_order`.
- Guarded fields: `matrix/threads_guarded_fields` holds a reach inside
  its `sync` in ten places, and four refusals
  `matrix/threads_guarded_field_*` a reach outside it.
- The lock orders of a dev build in one thread: the tests `lock_orders`
  and `lock_order_once` of `tests/traps/`, T063.
- The stops of the runtime: `trap_chan_capacity`, `trap_send_closed` and
  `trap_send_closed_waiting`.
- The rules of a worker: nine refusals `matrix/threads_worker_argument_*`
  for an argument that holds a pointer, and one each for a result and a
  chunk.
- Eighteen refusals hold the thirteen messages of the area that no test
  expected. They name what `dispatch` and `join` take, the name after the
  arrow and the atomic operations that do not exist. The others name the
  forms of `static`, of `worker` and of an arm of a `select`.

Where two rules meet, and where three do:

- A `dispatch`, a loop and the arguments of a job:
  `matrix/threads_dispatch_in_a_loop`, T055.
- A `dispatch`, an anonymous function and a `join` after its return:
  `matrix/threads_return_before_the_join`, T055.
- A generic function, a channel and a type argument that is a pointer:
  `matrix/threads_channel_of_a_type_parameter_that_is_a_pointer` and
  `matrix/threads_channel_in_a_generic_struct_of_a_pointer`, T058.
- A generic class, an atomic field and its type argument:
  `matrix/threads_atomic_field_of_a_type_parameter_that_is_f64`, T059.
  `matrix/threads_atomic_local_of_a_type_parameter` holds the local.
- A generic function, a library file and a `dispatch`:
  `generic_dispatch_modules`, T061.
- A generic worker, a `dispatch` and a type argument that is a pointer:
  `matrix/threads_worker_argument_of_a_type_parameter_is_a_pointer`.
- A closure, a `concurrent` parameter and each way to change a captured
  variable: seven refusals `matrix/threads_concurrent_closure_changes_*`.
- A closure, a guarded field and a `sync`: `in_closure` of
  `matrix/threads_guarded_fields` and
  `matrix/threads_guarded_field_in_a_closure_inside_its_sync`.
- A `defer`, a guarded field and a `sync`: `in_defer` of
  `matrix/threads_guarded_fields` and
  `matrix/threads_guarded_field_in_a_defer_before_its_sync`.
- A thread-safe class, a public function and a pointer inside a tuple, a
  struct or the result of a private function: four refusals
  `matrix/threads_pointer_into_fields_*`, T062.
- An atomic local, a `snapshot fn` and a `parallel`:
  `matrix/threads_atomic_captures`.
- An atomic, an arithmetic operation and a `bool` or a pointer: ten
  refusals `matrix/threads_atomic_*_on_a_*`, T060.
- A worker, a default value of a parameter and a `dispatch`:
  `matrix/threads_dispatch_leaves_out_a_default_argument`.
- A constant, a struct and a Mutex:
  `matrix/threads_const_of_a_struct_with_a_mutex`, T056.
- A static field, its default and a call:
  `matrix/threads_static_default_is_a_call`, T057.
- Two locks, their two orders and a second run of each: `lock_order_once`,
  T063.

### Classes

| Construct | Held, with the first source that holds it | Open |
|---|---|---|
| `class` | module level `programs/alias_class_pointers`, nested type `programs/generic_nested`, generic `programs/generic_constants`, library module `modules/access/com/example/ledger` | none |
| `abstract class` | module level `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `singleton class` | module level `programs/inherits_header`, library module `modules/com/example/board` | none |
| `final class` | module level `programs/classes`, library module `modules/sealed/com/example/sealed` | none |
| `inherits` | `programs/alias_class_pointers` | none |
| function with `self` | class body `programs/alias_class_pointers`, nested type `programs/generic_nested`, generic `programs/generic_constants`, library module `modules/access/com/example/ledger` | none |
| static function of a class | class body `programs/catch_exits`, generic `programs/generic_static`, library module `modules/access/com/example/ledger`, nested type `matrix/classes_nested_members` | none |
| `abstract fn` | class body `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/direct/com/example/shapes` | none |
| `concrete fn` | class body `programs/alias_class_pointers`, generic `programs/generic_copies`, library module `modules/chain/com/example/dog` | none |
| `concrete fn Base::f` | class body `matrix/classes_tables`, library module `modules/classes/com/example/classes`, nested type `matrix/classes_qualified_body_in_a_nested_class` T084 | none |
| `operator fn` | class body `programs/caller_params`, generic `programs/generic_hooks`, library module `modules/generic_hooks/com/example/row`, module level `matrix/classes_all_operators`, nested type `matrix/classes_nested_members` | none |
| `construct` | class body `programs/class_bitfields`, generic `programs/generic_constants`, library module `modules/chain/com/example/animal`, nested type `matrix/classes_nested_members` | none |
| `destruct` | class body `programs/alloc_owning`, generic `programs/generic_copies`, library module `modules/chain/com/example/animal`, nested type `matrix/classes_nested_members` | none |
| constant of a class | class body `programs/alloc_owning`, nested type `matrix/classes_nested_members`, generic `matrix/classes_constant_of_a_generic_class` T072, library module `modules/class_members/com/example/counter` T076 | none |
| field with `implements` | `programs/alias_class_pointers` | none |
| field with `use` | `programs/checked_once` | none |
| `mutable` field | `programs/inherits_header` | none |
| `pub` member | `programs/alias_class_pointers` | none |
| `protected` member | `programs/protected_calls` | none |
| `self.super.f()` | `programs/construct_base` | none |
| `static` member | `matrix/classes_statics` | none |
| `inject` field | `programs/inject_default` | none |
| `inject final` field | `inject/service` | none |
| `transient` field | `programs/transient` | none |
| `trace class` and `trace fn` | `programs/hook_trace` | none |
| `compatible` line | `plugin/versions/floor/net/example/greet` | none |

The step `classes` filled the table above on 2026-10-12. `tests/matrix/`
holds its sources under the names `classes_*`. The rows of the table are
items. The constructs of the area that stand in a function have no row.
The programs below hold them in the places of "The contexts".

- `matrix/classes_calls`, `classes_objects`, `classes_conversions`,
  `classes_statics`, `classes_operators`, `classes_generics` and
  `classes_root` hold their constructs in fifteen places each, one
  function `in_<context>` and one line of output per place and input, as
  `matrix/flow_<construct>` does. The first three hold the calls through
  the tables with `self.super.f()` and a bound function, the ways an
  object is made and torn down, and `is`, `as`, `as?` and `==` on pointers
  of a chain. The other four hold the static members and a singleton, the
  operators with the default `==` and the hash, the generics, and the
  functions and hooks of the root.
- A worker deletes no object and reaches no `mutable` field of a
  singleton. The place "worker" of `classes_objects` therefore holds the
  values alone, and that of `classes_statics` the field that is fixed.
- `anti-test/com/example/classes` is the module of the test
  `anti_test_classes`. It holds the `tests` block and the `fixtures`
  block. A block holds functions alone, so no row of the table has a cell
  there.
- `modules/classes/com/example/classes` is the library of the test
  `classes_modules`. It holds each construct in a plain function and in a
  generic one. The client inherits a class of it and implements an
  interface of it. A static field or a constant of a class in a generic
  function of a library stands in `generic_atomic_modules` and
  `generic_class_constant_modules`, T029. A library file carries no type
  nested in a generic class yet, so that cell cannot exist.
- A construct that gives a value is refused in a constant: six refusals
  `matrix/classes_*_in_a_constant` and
  `matrix/classes_call_as_the_default_of_a_field`, T028.
- `matrix/classes_field_kinds` takes a field of each of 29 kinds through
  the default `equals`, the hash and `dup`. `matrix/classes_generic_kinds`
  takes each of 31 kinds through a copy of a generic class, function,
  variant and interface. Neither has a row in a kinds table.
- `matrix_classes_equality_no_reflect`, `_field_kinds_no_reflect`,
  `_conversions_no_reflect` and `_copies_no_reflect` run four programs
  under `--no-reflect`, and `matrix_classes_objects_no_hooks` and
  `_construct_order_no_hooks` two under `--no-hooks`.

The rules of the area, each in a program of its own:

- The tables: `matrix/classes_tables` holds an unqualified body for two
  interfaces, two qualified ones, both, and the nearest body of a chain.
  It holds a body qualified by its own class and by its base as well, and
  an interface that inherits another.
- `construct` and `destruct`: `matrix/classes_construct_order` holds the
  order along a chain, an inline field, an owned object, an array, an
  assignment and a `construct` that fails.
- Decision 27 and the default `equals` and hash: `matrix/classes_equality`
  and `matrix/classes_equality_of_an_owned_pointer`, T081.
- The levels of a member: `matrix/classes_visibility`, twelve refusals
  `matrix/classes_private_*`, `_protected_*` and `_internal_*`, and T075.
- A singleton: `matrix/classes_singletons` and seven refusals
  `matrix/classes_singleton_*`, T079 among them.
- `dup` and `copy`: `matrix/classes_copies` and
  `matrix/classes_copy_replaced`, T082.
- An abstract class where a complete value stands: thirteen refusals
  `matrix/classes_abstract_class_*`, seven of them T078.
- Injection with the default provider: `matrix/classes_injection`.
- Every operator of the table on a struct and on a class:
  `matrix/classes_all_operators`.
- Every member in a nested type: `matrix/classes_nested_members`.
- A generic class with a generic base, a generic interface, a function
  with a parameter of its own, an operator, the default `==`, `construct`
  and `destruct`: `matrix/classes_generic_classes`, and twelve refusals of
  the forms of a generic.

Where two rules meet, and where three do:

- A generic class, a constant or a static field and the type argument:
  `matrix/classes_constant_of_a_generic_class` and
  `matrix/classes_static_of_a_generic_class`, T072.
- A nested class, a qualified `concrete fn` and a base of the same class
  body: `matrix/classes_qualified_body_in_a_nested_class`, T084.
- A pointer to an interface, a pointer to the object or to its base and
  `==`: `matrix/classes_identity_of_an_interface_pointer`, T083.
- A pointer to an interface, the abstract base of the object and `as`:
  `matrix/classes_cast_from_an_interface_to_a_base`, T073.
- An `operator fn`, a second parameter of another type and a literal:
  `matrix/classes_operator_with_a_literal`, T074.
- A generic function, a library file and a constant of a class:
  `generic_class_constant_modules`, T029.
- A class of another module, `pub` and a static member:
  `class_members_modules`, T076.
- A class, `dup` and a replaced `copy`: `matrix/classes_copy_replaced`,
  T082.
- A singleton, a field that is fixed and its address:
  `matrix/classes_singleton_field_written_through_its_address`, T079.
- An `inject` field in a nested class, in a generic class and in a class
  value held inline: `matrix/classes_injection`.
- Two interfaces, a chain and the qualified and unqualified bodies of
  one name: `matrix/classes_tables` and
  `matrix/classes_call_of_two_qualified_bodies`.
- A `construct` that fails, a chain and the `destruct` of each level:
  `matrix/classes_construct_order`.
- A type parameter, a literal and the back end:
  `matrix/classes_literal_of_a_type_parameter`, T080.

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
| clause at the top of the file | `matrix/modules_file_clauses` | none |

The step `modules` filled the table above on 2026-10-12. `tests/matrix/`
holds its sources of one file under the names `modules_*`. What crosses
a library file stands in `tests/modules/`. The refusals at that boundary
stand in `tests/errors/` under the names `modules_*`. C stands in
`tests/abi/` and `tests/clib/`, and the plugins in `tests/plugin/`. The
rows of the table are items and lines of a module, and the programs below
hold what stands in a function.

- `matrix/modules_qualified_names`, `modules_extern_calls` and
  `modules_export_calls` hold their constructs in fifteen places each, one
  function `in_<context>` and one line of output per place and input, as
  `matrix/flow_<construct>` does. The first holds a function by the name of
  its module, by an alias and by a direct import, with a class and a type
  of a module. The second holds the calls of an `extern fn`, a variadic one
  and one held as a function value. The third holds the exported items of
  a module used by the module itself.
- A worker builds no `text.Builder` in these programs. The places "worker"
  and "dispatch" of `modules_qualified_names` therefore hold the three
  functions alone.
- The standard library in dev and release: `matrix/modules_std_plain`,
  `modules_std_aliases` and `modules_std_direct`. They import the 23
  modules under `anti.` and the 12 under `anti.collection.`, by path,
  under an alias and with a direct import. Each reaches an item of eight
  of the modules.
- `matrix/modules_file_clauses` holds the clause of the whole file, an
  `allow` before the imports and an `unchecked` among them, and six
  refusals `matrix/modules_file_*` hold its forms.
- `matrix/modules_count_read` holds `count` of twelve collections, and
  `modules_variadic_arguments` every type a variadic argument takes.
  `modules_doc_comments` holds the four comments of documentation before
  every kind of item.
- A block `tests` and a block `fixtures` hold functions alone. No build of
  the matrix reads a library file either. No row of the table therefore
  has a cell in one of the two blocks.

Across a library file, each as `<name>_modules_release` and `_dev`:

- `items_modules`: every kind of item a module makes public, reached by
  the name of the module, by an alias and by a direct import.
  `items_stripped_modules` runs it with a library file written with
  `--strip-docs`.
- `levels_modules`: every kind of item at the level `internal`, from a
  module of the same package. A class below a class of the library reaches
  its protected members.
- `kinds_modules`: one function per kind of value, which takes the kind
  and gives it back. The scalars are the eleven integers, twelve C types,
  the floats, `f16`, `bool`, `char` and `str`. The parts are a slice, an
  array, a pointer, a nullable pointer, an optional value and a tuple. The
  functions are a function value, a closure, and a result and a function
  value that may fail. The types are a struct, a packed struct, a simd
  struct, a union, an enum, a variant and an interface. The classes are a
  class by pointer and by value, a synchronized and a concurrent one. The
  rest is a channel, a Mutex, an owning value, a lent value, a type
  parameter, an error and `never`.
- `field_defaults_modules`: the default of a field of a class of a
  library, one field per kind of value a default takes.
  `optional_default_modules` holds the optional value, T085.
- `diamond_modules`: two libraries import a third and the program imports
  the two. It holds one descriptor per class, one static field, one
  singleton and one copy of a generic per type argument. Three modules
  declare a struct and a class of one name.
- `generic_private_modules`: generics of a library whose bodies reach
  the private items of their module, in copies the client compiles. The
  items are a function, a constant, a struct, an enum, a class, a generic,
  an `extern fn`, an operator and an internal function.
- `exports_modules`: the items that `export` marks, reached from Anti.
- `constants_modules`: the constants of a library where a constant
  expression stands. The places are the length of an array, a constant,
  the value of an enum, the width of a bitfield and the N of `align(N)`.
  The default of a parameter and of a field and the arm of a `switch` are
  the others. `constant_argument_modules` holds one as the argument of
  `N: int`, T094.
- `generic_variant_modules`: a case of a generic variant named in full
  under the name of a module, T091.

The refusals at the boundary, `error_modules_*`, one message each:

- A private item and an internal item from another package, sixteen kinds
  each. They are a constant, a function, a generic function and an
  `extern fn`. A struct as a literal and as a type, a generic struct, a
  union, an enum and a variant follow. A class as a literal and as a base,
  an interface, an alias, a constraint and a name of a direct import
  complete them. An internal singleton makes the thirty-third. Two of
  them are T087.
- The members of a class of a library: ten refusals
  `error_modules_member_*`. They hold a private and a protected field and
  function, in a literal, from a class below and from another class. A
  protected static function is T086.
- `error_modules_direct_import_clash`, `_module_of_a_dependency`,
  `_constant_assigned`, `_function_as_a_type`, `_type_called`,
  `_export_symbol_twice`, `_missing_dependency` and `_import_cycle`, T088.
- A library file carries no function of a class with a type parameter of
  its own yet, and no type nested in a generic class. Those two cells
  cannot exist. `error_modules_generic_function_of_a_class`,
  `_generic_function_of_a_generic_class` and
  `_type_nested_in_a_generic_class` hold what antic says of each, the last
  as T095.
- `error_modules_library_with_main`, `_library_kind` and
  `_soname_without_a_version` hold what the driver refuses of `--lib`.

In one file, the refusals of `tests/matrix/`:

- Thirty for the forms of an `import`, of a name of a module and of a
  direct import, `matrix/modules_import_*`, `modules_module_*` and
  `modules_direct_import_*`.
- The visibility words: `matrix/modules_pub_*`, `modules_protected_item`,
  `modules_internal_field` and eight `modules_export_*` for what `export`
  does not mark.
- An exported signature per kind of value C cannot hold: seventeen
  `matrix/modules_export_function_takes_*` and two `_gives_*`. Fifteen
  more hold a field of an exported struct, union, variant and class, a
  function of an exported class and an exported constant.
- An `extern fn`: its forms, its pointers and its function pointers in
  eighteen `matrix/modules_extern_function_*`, four of them T089. Six
  `matrix/modules_variadic_argument_*` hold the types of a variadic
  argument.
- The `provides` line and the `link` line: seven
  `matrix/modules_provides_*` and `modules_link_*`.

C in both directions:

- `program_abi_ctypes`, with `_dev` and `_memory_checks`: each of `c_char`
  to `c_wchar`, `c_float` and `c_double` to a function of C and back. A
  type crosses alone, at its limits, among fourteen arguments and in a
  struct by value. It crosses through a pointer, through a callback and as
  a variadic argument as well. C then calls fourteen functions the program
  exports.
- `clib_ctypes`, `clib_ctypes_shared` and `clib_ctypes_bundle`: one C
  program calls a library that takes and gives every C type, built with
  `--lib static`, with `--lib shared` and with `--bundle-runtime`.

Plugins:

- `plugin_kinds`: a function of a library per kind of value, called
  through the table of the interface.
- `plugin_locals`, T092, and `plugin_unused`, T093.

Where two rules meet, and where three do:

- A class of a library, a field of type `?T` and its default:
  `optional_default_modules`, T085.
- A class of a library, `protected` and a function without `self`:
  `error_modules_member_protected_static_function`, T086.
- A generic variant, the name of a module and the type arguments of a
  case: `generic_variant_modules`, T091.
- A type parameter `N: int`, a constant and the name of a module:
  `constant_argument_modules`, T094.
- A generic of a library, a private item of its module and a copy the
  client compiles: `generic_private_modules`.
- A generic class of a library, a type nested in it and a copy the client
  asks for: `error_modules_type_nested_in_a_generic_class`, T095.
- Two libraries, a generic of a third and one type argument:
  `diamond_modules`.
- A plugin, a local object and a class without `destruct`:
  `plugin_locals`, T092.
- A plugin, a module of the standard library and a host that imports and
  does not use it: `plugin_unused`, T093.
- An `extern fn`, a function pointer and a closure:
  `matrix/modules_extern_function_takes_a_closure` and
  `modules_extern_function_takes_a_snapshot`.
- An exported class, a `construct` with arguments and a type C cannot
  hold: `matrix/modules_export_class_construct_takes_a_str`.

### The rules of decisions 19 to 27

"Language rules" under "Decisions of 2026-10-11" in `docs/decisions.md`
holds nine rules that the test run tests. No expected output under `tests/`
held a refusal of any of them on 2026-10-12, so every cell was open. Where
antic 0.2.0 does otherwise, the test becomes an expected failure by decision
30. A matrix step writes its tests into the last column.

| Decision | Area | The refusal or the check | State |
|---|---|---|---|
| 19 | flow, errors | `return`, `break`, `continue`, `fail` and `try`, each inside a `defer` and inside an `undo` | T001, the twelve refusals `matrix/flow_*_in_defer`, `_in_undo`, `_out_of_defer` and `_out_of_undo` that "Flow" names |
| 20 | errors | a `catch` handler of a value that can reach its end | T044, the nine refusals `matrix/errors_handler_reaches_its_end_*`, `matrix/errors_handler_of_*_reaches_its_end` and `matrix/errors_handler_without_a_name_reaches_its_end`, and the program `matrix/errors_handler_exits` for the ways a handler may end |
| 21 | errors | `try` on an optional, with a message that names `let ... else` and `??` | T045 for a call that the checker takes and T046 for the message of the others, the five refusals `matrix/errors_try_on_an_optional_*` |
| 22 | ownership | `return` of a field that owns something | T037, the seven refusals `matrix/ownership_return_of_a_field_copies_*`, one per kind of owning value, and `matrix/ownership_return_through_a_pointer_copies` and `matrix/ownership_return_of_an_element_copies` for `*p` and an element |
| 23 | threads | a second `join` of one job, refused where the checker sees it and trapped in dev mode otherwise | T053, the refusals `matrix/threads_join_twice` and `matrix/threads_join_after_join_all`, and T054, the test `check_join_twice` of `traps/join_twice` |
| 23 | threads | the arguments of a `dispatch` in a loop, stored per job | T055, the programs `matrix/threads_dispatch_in_a_loop` and `matrix/threads_return_before_the_join` |
| 24 | values, types | a constant index past the end of a fixed array | T016, the refusal `matrix/values_index_past_end`, and `matrix/types_array_field_index_past_end`, `matrix/types_array_inner_index_past_end` and `matrix/types_array_write_past_end` for a field, an inner array and a write |
| 24 | values, ownership | `xs[lo..hi]` past the length, trapped in dev mode | T017, the test `check_slice_range` of `traps/slice_range`, and `check_slice_of_slice_range`, `check_slice_range_reversed` and `check_slice_range_negative` for a slice of a slice, a low bound above the high one and one below zero |
| 25 | modules | a write of `count` of a collection outside its module | T090, the ten refusals `matrix/modules_count_of_*`, and the program `matrix/modules_count_read` for a read |
| 26 | modules | a `str` in the signature of an `extern fn` | T089, the five refusals `matrix/modules_extern_function_takes_a_str`, `_gives_a_str`, `_callback_takes_a_str`, `_takes_a_pointer_to_a_str` and `matrix/modules_variadic_extern_function_takes_a_str` |
| 27 | classes | a class with an `abstract fn` that is not `abstract` | T064, the refusal `matrix/classes_open_functions_without_abstract` |
| 27 | classes | a direct call of `destruct` | the refusal `matrix/ownership_destruct_called_directly`, and T068, the refusal `matrix/classes_super_destruct_called_directly` for `self.super.destruct()` |
| 27 | classes | `mutable` on a field of a plain class | T065, the refusal `matrix/classes_mutable_field_of_a_plain_class` |
| 27 | classes | `static atomic` on an `f64` | T059, the refusal `matrix/threads_static_atomic_of_f64`, which the step `threads` wrote with the atomic fields |
| 27 | classes | `==` on pointers of two unrelated classes | T066, the refusals `matrix/classes_equal_pointers_of_unrelated_classes` and `matrix/classes_unequal_pointers_of_unrelated_classes` |
| 27 | classes | `equals` replaced without `hash` | T067, the refusals `matrix/classes_equals_replaced_without_hash` and `matrix/classes_hash_replaced_without_equals` |
| 27 | values | `<` on an enum, which stays refused | the refusal `matrix/values_less_on_enum` |
| 27 | values | `+=` on a type with `operator fn add`, which is not derived | the refusals `matrix/values_compound_on_operator_add` and `matrix/values_compound_on_generic` |

antic has a message for the direct call of `destruct` already, and the
step `ownership` wrote the refusal that expects it.

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
- 206 rows in the matrix. 2313 cells are held and 25 are
  open. Of the kinds, 1484 are held and 8 are open. The review of
  2026-10-13 counted the cells of every table again. It found 58 held cells
  in the area classes, where the table of the areas said 59. It filled the
  one open cell of the area flow: a `return` of a value in a `tests` block
  stands in an anonymous function of a test. The step `flow` filled
  its area after the count of 840 held cells and 578 held kinds. The step
  `values` filled its area after the count of 1157 held cells and 622 held
  kinds. The step `types` filled its area after the count of 1613 held
  cells. Its 20 open cells cannot exist, and each states why. The step
  `ownership` filled its area after the count of 1676 held cells and 1237
  held kinds. Its one open cell cannot exist, and it states why. The step
  `errors` filled its area after the count of 1845 held cells and 1317
  held kinds. The step `threads` filled its area after the count of 1969
  held cells and 1347 held kinds. Its 4 open cells and its 7 open kinds
  cannot exist, and each states why. The step `classes` filled its area
  after the count of 2300 held cells, and no cell of it stays open. The
  step `modules` filled its area after the count of 2312 held cells, and
  no cell of it stays open.
- No open cell for the rules of decisions 19 to 27.
- 748 messages: 581 expected by a test and 167 open. antic gives 598 with
  29 open, and anti 150 with 138 open.
- 134 options and commands: 126 passed by a test and 8 open.

On 2026-10-13, after the last step of the test run:

- 748 messages: 731 expected by a test and 17 open. antic gives 598 with 6
  open, and anti 150 with 11 open. Each open line states why it stays.
- 134 options and commands: 134 passed by a test and none open.
