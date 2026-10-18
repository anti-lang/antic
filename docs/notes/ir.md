# The intermediate representation

Choices made in the intermediate representation. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- The IR is typed three-address code in basic blocks, not SSA. Its scalar types are `i8`, `i16`, `i32`, `i64`, `f32`, `f64` and `ptr`, and `agg` names an aggregate passed by value. `clong`, `cwchar` and `lock` stand for `c_long`, `c_wchar` and the word of a Mutex, whose width the target gives and `layout.c` resolves. `bool` is `i8`, `char` is `i32`, pointers and function pointers are `ptr`. Signedness is in the operations, as `sdiv` and `udiv`.
- Aggregates live in memory and move through a `ptr`. The IR names each aggregate type in a table of fields without sizes or offsets. The back end lays the type out and classifies it for its target. An aggregate parameter is a pointer to the value.
- The IR names a function by module and name and never mangles. The verifier of `ir_verify.c` checks terminators, operand references and operand types. It also checks definite assignment, the facts of the signatures and of the arithmetic, and the class records.
