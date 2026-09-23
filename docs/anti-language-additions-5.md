# Anti language additions, round five: generics

Addendum to `docs/anti-language-additions.md`. It adds generics to the language. Functions, structs, classes, variants and interfaces take type parameters, with constraints checked at both ends. Apply it by editing that document, then delete the addendum. Drafted on 2026-09-23.

Nothing in this addendum is work for a session. It becomes work when Eddie names it in a work order or a session message. When this addendum is applied, recording it in the additions document is the whole task.

## What to change in the additions document

- Timing. Generics follows round four. The libraries built on it, [listed at the end](#libraries-built-on-generics), follow generics.
- Keywords. Add `constraint` and `type`.
- The syntax overview gains a section "Generics", marked "Not built yet".

## Syntax

Type parameters and type arguments stand between `<` and `>`, as in C, C++ and Rust.

```anti
fn max<T: lt>(a: T, b: T) -> T { ... }
struct Pair<A, B> { first: A, second: B, }
class List<T> { ... }

let p: Pair<int, str> = Pair { first: 1, second: "one" };
let list = List<Person>.new();
let m = max(3, 7);
```

- In a type position, `<` after a type name always opens type arguments. Type positions are after `:`, after `->`, after `alloc`, and in a field or parameter declaration.
- In an expression, `<` after a name opens type arguments under two conditions. What follows parses as a list of types closed by `>`, and the token after the `>` is `(`, `.` or `{`. Otherwise `<` is a comparison. This is the rule of C#. `List<int>.new()` and `max<int>(a, b)` are generic, and `a < b > c` stays two comparisons.
- That rule can read a generic call where the name is not generic, as in `f(g < a, b > (c))` with an ordinary `g`. The compiler then refuses it and names the fix: parentheses around the comparison.
- `>>` closes two lists of type arguments: `List<List<int>>`.
- Type arguments are inferred from the arguments of a call where they can be. So `max(3, 7)` is `max<int>`. They are written out where nothing gives them, as in `List<Person>.new()`. Inference reads the arguments, never the body and never later uses of the result.

## Compilation

Every use of a generic with concrete arguments gets its own compiled copy, as in C++ and Rust. `List<int>` holds plain `int` values and calls its functions directly, with no boxing and no indirection. Generic code costs what hand-written code costs.

- Two uses with the same arguments are the same type. `List<int>` in one module and in another are one type, passed freely between them, and compiled once.
- The whole-program pass merges copies whose code is identical. `List<*Person>` and `List<*Order>` compile to the same code, since both hold pointers, and one copy serves both. This removes most of the size that one copy per type would cost.
- In a dev build, where modules compile apart, each copy is made in the module that uses it. It is cached by the generic's identity and its arguments. The link keeps one copy of each.
- `size_of(T)` inside a generic is the size of the argument in that copy.

## Constraints

A generic states what it needs from each type parameter. The compiler checks the body against those needs where the generic is written. It checks every use against them where it is used. An error therefore lands in the code that made it. That is either a body that uses more than it declared, or a call with a type that lacks something. The message names the missing hook or interface.

```anti
fn max<T: lt>(a: T, b: T) -> T { ... }
fn sum<T: add>(items: []T) -> T { ... }
fn count<C: iter>(c: C) -> int { ... }
fn save<T: Serializable>(items: []*T) { ... }
fn find<K: eq + hash, V>(m: Map<K, V>, k: K) -> V { ... }
```

- A hook from the table of language hooks is a constraint: `lt`, `add`, `iter` and the rest. A type meets it when it has that hook. The built-in types have their operators as hooks, so `int` meets `lt`, and a struct or class meets it with `operator fn lt`.
- An interface is a constraint: `T: Serializable` requires a class that declares it implements `Serializable`.
- Constraints combine with `+`.
- `constraint Ordered = eq + lt;` names a set for reuse, used as `T: Ordered`. `anti.lang` ships `constraint Number = add + sub + mul + div + neg + lt;` for numeric code. A user's own number type joins it by having those hooks, so the set is open.
- A type parameter without constraints can be stored, copied, moved and passed on, and measured with `size_of`. That is what a container needs, so containers usually take unconstrained parameters.
- A constraint violation at a call gives: `` `Circle` has no `lt`, which `max` needs for `T` ``.

Considered and left for later: requiring an ordinary method of a type, such as `T: fn area(self) -> f32`. Nothing designed so far needs it. A type that must provide a method can be a class implementing an interface. It can be added when real code needs it, and adding it breaks nothing.

## What can be generic

- Functions, structs, classes, variants and interfaces take type parameters.

```anti
variant Result<T, E> { Ok { value: T }, Err { error: E }, }
abstract class Iterable<T> { ... }
```

- A parameter may be an integer constant instead of a type, written `N: int`. Its argument is a constant the compiler knows. It serves sizes that belong to the type, such as the length of a fixed array: `struct Ring<T, N: int> { items: [N]T, ... }`, used as `Ring<Sample, 1024>`, or `Matrix<f32, 4, 4>`. Only integers are allowed.
- A method may take type parameters of its own: `pub fn map<U>(self, f: fn(T) -> U) -> List<U>`. Such a method is not in the class's table, since every `U` makes a separate copy and no table holds them all. It is called directly, and it cannot be `abstract` or replaced with `concrete fn`. The compiler refuses the combination.
- A nested type of a generic class sees the class's type parameters: `class List<T> { struct Node { value: T, next: ?*Node, } }`.
- A generic class may be `synchronized` or `concurrent`, and its rules apply to every copy.
- A generic `worker fn` is checked against the worker rules in every copy, with the concrete types.

## Other features with generics

- Language hooks work in generic types. `class List<T>` may declare `operator fn iter`, and `for x in list` then works for every `List<T>`.
- A generic function may be `may fail`, with the usual two channels.
- Function types, closures and snapshots may appear as type arguments: `List<fn(int) -> int>`. The rules of `keep` and `concurrent` apply as for any value of a function type.
- Each copy of a generic class has its own descriptor, named with its arguments: `List<Person>`. Reflection and `type_name` give that name.

## Libraries and C

- A library file stores a generic as IR with its parameters open, together with its constraints in the public interface. The program that uses it fills in the arguments and compiles the copy. A library ships generics like any other code, with no source and no templates in headers.
- The C header cannot show an open generic, since C has none. A library offers a copy to C by naming it: `export type PersonList = List<Person>;`. The header then writes it as any exported class, `struct anti_PersonList` with its functions.
- `type Name = Generic<Args>;` without `export` names a copy for use in Anti alone.
- An `export fn` has concrete types throughout. A generic `export fn` is refused, and the message suggests a named copy.
- `anti doc` shows generics with their parameters and constraints, and a named copy links to its generic.

## Libraries built on generics

These follow generics, each designed in its own round:

- `anti.collection`: `List<T>`, `Map<K, V>`, `Set<T>`, and the interfaces `Iterable<T>` and `Iterator<T>`, which every collection implements through its `iter` hook.
- The thread-safe collections, synchronized and concurrent, with operations that take criteria rather than positions, as round four describes.
- `anti.par`: `count`, `filter`, `map`, `any` and the other parallel operations over a collection, built on `parallel` and `concurrent` parameters.

## Messages added

- `` `Circle` has no `lt`, which `max` needs for `T` ``
- `` `max` uses `+` on `T`, which its constraints do not give. Add `add` to them ``
- `` `g` is not generic. Put the comparison in parentheses ``
- `` `map` takes type parameters, so it cannot be `abstract` or replaced ``
- `` `size` must be a constant for `N` ``
- `` an `export fn` cannot be generic. Export a named copy with `export type` ``
