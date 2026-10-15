# Solid: the findings of the area classes

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_classes_`, in one session on 2026-10-15. Every
one is fixed, and `tests/known-failures.txt` holds no line of the area.
The logs lie in `build/drive/logs/solid-classes/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T072 | severe | `0b3198d3` | A constant and a static field of a generic class were members of the generic alone, so every copy read one value and one global. |
| T081 | severe | `00c9dab3` | The default `equals`, `hash` and `==` compared an `own` pointer field by its address. |
| T082 | severe | `3ffcb6ab` | `dup` called a replaced `copy` with memory that held no table. |
| T083 | severe | `59afaff1` | `==` moved a pointer to the start of its object only when both sides were abstract. |
| T112 | severe | `dfb5c997` | The copy pass named the function of a copy of a generic class at a call alone, so a name as a value kept the generic's symbol. |
| T114 | severe | `9bba57b9` | `--trace writes` took `offset_of` of an index in an array type, which the back end gave as 0. |
| T064 | major | `996ed6bf` | Nothing refused a class with an `abstract fn` and no `abstract`. |
| T065 | major | `d4a8c537` | Nothing refused `mutable` outside a singleton. |
| T066 | major | `151df9ff` | `==` took two pointers of any two classes. |
| T067 | major | `adf0783a` | Nothing refused a class that replaces one of `equals` and `hash` alone. |
| T068 | major | `28202249` | `self.super.destruct()` of a private `destruct` reached the root's, which the check of a direct call passed over. |
| T069 | major | `c7669087` | The parser read every line after the first member of a class body as a member. |
| T073 | major | `90d1d095` | A cast from an abstract pointer to a class of another chain was refused. |
| T074 | major | `1ee59c73` | A literal on the right of an operator took the type of the left operand, not of the parameter of the `operator fn`. |
| T075 | major | `68c38310` | A member reached through its type was never checked for its level of visibility. |
| T078 | major | `d554820e` | Only a plain field, a local, a literal and `alloc` refused an abstract class held in place. |
| T079 | major | `5af82885` | `&` of a field of a singleton that is read-only after creation was taken. |
| T080 | major | `47cb826d` | The checker took a literal of a type parameter. |
| T084 | major | `8eb80089` | A qualifier was compared with the full name of a nested type, `Garage.Vehicle`. |
| T104 | major | `355253b1` | The pass over the whole program decided the default of the backtraces before it wrote the table of the injections. |
| T105 | major | `61f612f3` | The walk for a union passed over a tuple. |
| T126 | major | `1b891e65` | A copy kept the default of a field whose value names no parameter, though its type does. |
| T070 | minor | `c75c8627` | Four messages of the class checker had another text than "Messages" in the object model. |
| T071 | minor | `c75c8627` | Each of six mistakes of a class declaration reached a second check that reported it again. |
| T077 | minor | `80c7a221` | Messages for a static member reached through a value, and `get` of a singleton declared after the constants. |
| T101 | minor | `c7669087` | A class body that failed to parse left the parser inside the body. |
| T102 | minor | `fb0d3093` | The lookahead for a nested type stopped after four tokens. |

- T086 of the area modules shares the cause of T075 and closes with it.
- T070 and T071 share one commit, since the message of a `concrete fn`
  that fills nothing and the check that skips it stand on adjacent lines.
- `4eb30ea3` writes the manifest of `emit_identity` and the excerpts of
  the developer's guide again after T081 and T082. The default `equals`
  and `hash` of `anti.lang.Error` compare its `own` cause and frames by
  what they point at, so the text of every program that links
  `anti.lang` changed. `adf0783a` changes the digests of `hash_default`
  and `hash_generic`, whose classes gain an `equals`. Every
  `link_identity_<target>` stayed as it was.

## Findings left open

- T096. The case `matrix_classes_base_with_an_unknown_type_argument` is
  fixed in `d5aa4832` and its line is gone. Five other cases belong to
  other areas. Two refuse an `operator fn iter` and one an `operator fn
  next`. One refuses the lanes of a simd struct, and one a copy of a
  generic.
- T087. The case `matrix_modules_extern_function_in_a_class` is fixed in
  `c7669087` by the recovery of T101. Its two other cases belong to the
  area modules.

## Tests corrected

- Six tests expected a column the rest of the suite does not use.
  `classes_equal_pointers_of_unrelated_classes` and its `!=` twin
  expected the operator, where every refusal of a binary operator stands
  at its left operand. The three of T075 and
  `classes_super_destruct_called_directly` expected the name of the
  member, where a member stands at its first token. Each now expects
  the column of the convention.
- `classes_nested_class_after_four_words` wrote `pub singleton final
  trace class`, which no grammar of the class words takes, so it held a
  second mistake. It now writes `pub internal final synchronized class`.
- `matrix_modules_internal_field` expected `expected identifier` and now
  expects the message that names the levels of a member.
- The rule of T075 refused 97 programs of the suite, and examples of
  the overview and of five pages of the guide, that read a static of a
  class from outside it. Each such member is `pub`,
  and a constant that a class below reads is `protected`. Four programs
  that replaced one of `equals` and `hash` alone gain the other, which
  compares what their hash reads, and two lose a `mutable` on a field
  of a plain class. Their output is the same.
- `destroy(&Config.get().part)` keeps its address under T079, since R083
  waits for Eddie.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`: `dup` of an
object whose chain replaces `copy` writes the tables of the class into
`to` and then calls the replaced `copy`, which fills the fields.

The object model now refuses a class that replaces one of `equals` and
`hash` alone, as decision 27 states. It names the operators that stand
for either. No feature, option or public item was
added.

## Counts and gates

`tests/known-failures.txt` held 336 lines, 316 of them tests, before the
step. It holds 261 lines, 241 of them tests, after it.

- Host, `ctest -j14` in two parts: 2000 and 2978 tests pass, two
  `mimalloc_environment` tests skip, and the 241 listed tests fail as
  expected. Logs `final-h1.log` and `final-h2.log`.
- ASan: 1150, 1112 and 2724 pass, `final-a1.log` to `final-a3.log`.
- UBSan: 1150, 1112 and 2724 pass, `final-u1.log` to `final-u3.log`.
- The build has no warning. No change touched a platform layer, a link,
  a sysroot or a package, and no finding named a host, so no VM ran.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
bcc222de Record the commits of the findings the step classes fixed
1b891e65 Give a copy the default of a field whose type names a parameter
355253b1 Write the default of the backtraces for a provider that may fail
$ git status --short
?? docs/reports/2026-10-15-solid-classes.md
$ git rev-parse HEAD origin/main
bcc222de311728a503562589e13a161282bad17a
bcc222de311728a503562589e13a161282bad17a
```
