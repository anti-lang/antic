# Constraints of generics

The step built "Constraints" of "Generics" in round five of
`docs/anti-language-additions.md`.

## What was built

- Most of the checks already stood from the syntax of generics. Hooks and
  interfaces were constraints, joined with `+` or named with `constraint`.
  A body was checked where it is written and each use where it stands,
  with both messages of the round. The step added what was missing and the tests of each.
- `Number` of `anti.lang`, `add + sub + mul + div + neg + lt`, declared by
  the compiler. `int`, `float` and a struct of the program with the six
  `operator fn` meet it, and `byte` and `str` do not.
- `for x in c`, `e[i]` and `e[i] = v` on a value of a type parameter.
  `iter`, or `next` and `value`, `index` and `set_index` allow them, and
  each is refused otherwise in the form of the round's message:
  `` `walk` uses `for` on `T`, which its constraints do not give. Add `iter` to them ``.
- A parameter without constraints is stored, copied, passed on and
  measured with `size_of`. Operators, fields, functions, `for`, indexing,
  conversions and conditions on it are refused.
- Tests: `listing_constraints.types` (`tests/dump/constraints.anti`) pins
  what the constraints give a body and which types meet them.
  `listing_error_constraints` (`tests/errors/constraints.anti`) pins every
  refusal of a body and of a use.

## Failures

- The first probe wrote `operator fn` in a struct body, which the parser
  refuses. A struct takes free operator functions, and the tests do so.
- The first error listing reported `Plain` lacking `eq` where the test
  meant the interface, and one copy of a generic struct twice on one line.
  The test was corrected. No compiler change followed from either.

## Provisional decisions

- `Number` is declared by the compiler in `anti.lang`, named without a
  module, and a `Number` of the module wins over it.
- The value a walk of a type parameter gives is `C.value`, and the value
  `e[i]` reads is `C.index`, each a parameter without constraints. The
  index and the value written are checked without a type expected. A form
  on one of them is refused with `which no constraint gives`.

## Not built

`Ordered` of `anti.lang` belongs to "Hashing and order" and waits, as the
syntax overview says. A qualified `lang.Number` is not read.

## Gates

Logs: `build/drive/logs/host-suite.log`, `build/drive/logs/asan-suite.log`,
`build/drive/logs/ubsan-suite.log`. Zero warnings. The docs-style checker
reports nothing on the touched files.

- host: 100% tests passed out of 982
- asan: 100% tests passed out of 981
- ubsan: 100% tests passed out of 981

## Proof of the push

Taken after the push of the report.

```
$ git log --oneline -3
c9b10e9 Report the constraints of generics
ef2cfa4 Build the constraints of generics
bd1833d Add the proof of the push to the report of the syntax of generics
$ git status --short
 M docs/reports/2026-09-24-constraints.md
$ git rev-parse HEAD origin/main
c9b10e9c3a567a76a436249de91281c73850e140
c9b10e9c3a567a76a436249de91281c73850e140
```
