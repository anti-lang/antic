# The bind command

The choices inside `anti bind`. The rules are in "Binding generator", "C
types", "Bitfields", "Packed and aligned structs", "Inline function shims"
and "ABI probe" of `docs/tooling-addendum.md`. The settled points are in
`docs/decisions.md` under "The bind command". The code is in `src/anti/`:
`bind.c` holds the command, `bindapi.c` reads a description of rlparser,
`bindclang.c` reads a header through clang, `bindtype.c` parses a C type
spelling, `bindexpr.c` evaluates a constant and `bindwrite.c` writes the
module, the shim and the probes. `bindmodel.c` holds the memory of the
model, the warnings and the libraries each bundled library links. `jsontree.c` builds a tree over the scanner
of `src/rt/json.c`.

## One model, two readers

Both readers fill one model of `bindmodel.h`: records, enums, functions and
constants, with C types and never a size. The writer turns the model into
the module, so a description and a header of one API give one binding.
Both readers name a C type by its spelling, the one clang prints as
`qualType` and the one `raylib_api.json` writes, and one parser reads both.

The parser takes the specifiers, then the declarator of C with its
grouping: `void (*(*)(int))(char)` is a pointer to a function of `int` that
returns a pointer to a function of `char`. A qualifier goes, since Anti has
none. A typedef name stands for the type it names, because Anti has no
alias. The names of the C library stand for their meaning and never for the
type of the machine that ran clang: `int64_t` is `i64` whether it is `long`
or `long long` there.

## The header path

`anti bind --clang` runs clang twice with the same options. `-E -dD` gives
the macros and the pragmas, and `-fsyntax-only -Xclang -ast-dump=json` gives
the declarations. The options aim clang at the target and at the headers of
its sysroot in the runtime archive. The build compiles the runtime of that
target with the same options. A binding then reads the same headers on every
host.

clang writes the file and the line of a location only where they differ
from the location it wrote before. The reader walks every location in the
order of the document and keeps the last of each, which gives the file of
every declaration. A declaration of the header is bound. A typedef of any
file resolves a name.

`#pragma pack` leaves no value in the AST, only an attribute. The line
markers of `-E` count the lines of the header. The reader keeps the value
of the pack at each line and reads it at the line of the record.

A record defined inside another is bound as a record of the module, as C
declares it at file scope. An anonymous one takes the name of the record
and its field, `LayoutHolder_as`. An anonymous member of C11 has no Anti
form, so it becomes a field `anon0` with a warning.

## Constants

A macro is bound when it has no parameters and its text is a constant
expression. That covers the literals of C, the names of macros and
enumerators and the casts to an arithmetic type. It covers the unary
operators and the binary operators of arithmetic, shifts and bits. The type
follows C, a suffix included, and a value wraps at the width of its type. A
float is written with the digits that give it back, 9 for a `c_float` and 17
for a `c_double`. A macro that is one enumerator
becomes a constant of the enum. Every other macro is skipped with a
warning.

The macros `anti bind` skips, each with a warning, by R174 of "Rules of
2026-10-13":

- A macro that takes parameters, and one without a value, as an include
  guard.
- A text with an operator the evaluator does not take: `!`, `==`, `!=`, `<`,
  `<=`, `>`, `>=`, `&&`, `||`, `?:`, `sizeof`, `defined` and the comma.
- A text with a character literal, as `'a'`, or a cast to a type that is not
  arithmetic.
- A text with a name that is no macro bound before and no enumerator, and two
  macros that name each other.
- A value that C does not define or Anti cannot hold. A division by zero, a
  shift by 64 or more and a float that leaves its range are such values. So
  are a string in an operation and an integer literal past 64 bits.
- A string literal with an escape other than the six of Anti, `\n`, `\t`,
  `\r`, `\\`, `\"` and `\'`. That is `\0`, an octal escape, `\x`, `\u`,
  `\U`, `\a`, `\b`, `\f`, `\v` and `\?`.

`tests/bind/values.h` and `tests/bind/forms.h` hold such macros, as
`VALUES_NOT_ZERO`, `(!0)`, and `VALUES_BELL`, `"bell\a"`.

A compound literal of a bound struct, `(Color){ 200, 200, 200, 255 }`,
becomes a constant of the struct when it gives one constant per field in
field order. A leading call of a macro whose body is its one parameter, as
raylib's `CLITERAL(Color)` is, is put in place first.

rlparser gives each define a kind. `INT`, `FLOAT`, `FLOAT_MATH`, `STRING`
and `UNKNOWN` go through the same evaluator, so a define and a macro give
the same constant. `COLOR` becomes a struct literal of the struct it names.

## Functions and the shim

A function of the header reaches a symbol in one of three ways. A `static`
definition has none. The shim renames it with a macro around the include
and defines a wrapper of the name that calls it. A C99 `inline` definition
has an external definition in the one translation unit that declares it
`extern`, so the shim declares it with `extern __typeof__(f) f;`. The
definition of the header then becomes the symbol, and the shim holds no
copy of a body. Every other function is defined by the library.

A variadic inline function cannot be wrapped and is left out. A `static`
function without a body has no symbol anywhere and is left out.

## Left out

A record is bound when every field has an Anti type. A record that names a
record left out is left out too, until nothing changes. A function is bound
when its result and every parameter have one. A `long double` has none.
Neither has an array member of no length, nor a struct passed by value that
the API never defines. Each declaration left out gets a warning that says
why, and the command goes on.

A description leaves out a function without its name or its result and a
struct without its name, with a warning each. A type that does not parse
leaves out the function that names it, through an alias or a callback as
well. Its one warning names the function and the type. A field without
its name or its type is lost, and the struct without it would have another
layout than the one of C. The description is then refused. These are R170,
R171 and R172 of "Rules of 2026-10-13".

An identifier of C is a letter or `_` and then letters, digits and `_`.
Any other name refuses the input as a whole, a header as a description.
clang takes `$` in an identifier, and the binding writes every name into
Anti and C as it stands.

A field or a parameter whose C name is a word of Anti takes a trailing `_`,
which changes neither a layout nor a call. A function with such a name
cannot be renamed, since its name is its symbol, and is left out.

## The probe

`--probe` writes `probe_<library>.c` and `probe_<library>.anti`, the ABI
probe of the addendum for every bound record. Each prints the size and the
alignment of a record, and per field the bytes of an object cleared and
given that one field. The field takes 1, `true` or the first enumerator
that is not 0, reached through arrays and nested records. A pointer takes
no value, since C and Anti write no pointer constant alike. The Anti probe
finds the alignment as the offset of the record after one byte, which
`tests/abi/probe.anti` does too. A record that C names only through its
field takes `__typeof__` of that field, and an anonymous member is not
probed.

## Output

The module passes through the formatter of `anti fmt` before it is
written. A binding committed into a tree that `anti fmt --check` guards
then stands as the generator wrote it.
