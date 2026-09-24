# Anti language additions, round five: generics and collections

Addendum to `docs/anti-language-additions.md`. It adds generics to the language, then the collections they make possible: the collections of `anti.collection` with their thread-safe versions, the language features they rest on, `Shared<T>` in `anti.mem`, and the `--memory-checks` option. Apply it by editing that document, then delete the addendum. Drafted on 2026-09-23 and 2026-09-24.

Nothing in this addendum is work for a session. It becomes work when Eddie names it in a work order or a session message. When this addendum is applied, recording it in the additions document is the whole task.

## What to change in the additions document

- Timing. Round five follows round four. Generics come first, since every collection is generic.
- Keywords. Add `constraint` and `type`. Add `lent` as a contextual word before a pointer parameter.
- The table of language hooks gains `hash`.
- `anti.lang` ships the constraints `Number` and `Ordered`.
- The syntax overview gains the sections "Generics", "Optional values", "Direct imports" and "Collections", marked "Not built yet".

# Part one: generics

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

# Part two: collections

## Optional values

`?T` is an optional value of any type: a `T`, or `none`. It follows every rule `?*T` already has.

```anti
fn find_user(id: int) -> ?User { ... }

let age = ages.get("Ann") ?? 0;
if let a = ages.get("Ann") { ... }
let first = queue.first() else { return; };
```

- A `?T` compares with `none`, narrows after a test, and works with `if let`, `let ... else` and `??`. A `?T` is used as a `T` only after a test proves it holds one.
- `?T` of a value type is the value and one flag byte, padded to the type's alignment. `?*T` stays one pointer, with `none` as address zero, as now.
- The match result of round four is a `?Match` under this rule.
- `none` means that nothing is there, and that is normal. A failure is `may fail`. `map.get(key)` returns `?V`. `text.parse_int(s)` stays `may fail`, since bad input is an error and not an absence.
- The C header writes `?T` as a struct of the value and a `bool`.

## Ownership at a call

- `own` before a parameter takes ownership of the argument: `pub fn push(self, own item: T)`. Passing a local moves it, and naming the local again is refused: `` `c` was moved into `shapes` by `push` ``. A literal or a call result passed there needs nothing. This extends the `own` parameter of errors to every type.
- For a type that owns no memory, a move is a copy of its bytes. For one that does, the move is what keeps one owner.

## Lending

A collection lends an element to a function instead of handing out a pointer to it.

```anti
list.read(3, fn(p) { io.println(p.name); });
list.modify(3, fn(p) { p.age += 1; });
```

- `lent` before a pointer parameter says the pointer is valid only during the call: `fn(lent *T)`. The function may read and change through it, and pass it on to another `lent` parameter. It may not store it, return it, capture it in a closure that outlives the call, or pass it to a `keep` or `own` place. The checker enforces this as it enforces `keep`.
- A lending function runs while the collection holds the element in place, so the pointer never outlives the element.

## Walking a collection

```anti
for p in people { }      // p is a Person: a copy of each element, read-only
for p in &people { }     // p is a lent *Person: the element itself, for this turn
```

- `for x in c` gives a copy of each element. The loop variable is read-only, so a change that would only reach the copy is a compile error: `` `p` is a copy of each element of `people`. Walk with `&people` to change the elements ``.
- `for x in &c` gives each element as a `lent` pointer, valid for one turn of the loop. The element may be changed in place through it.
- The same two forms hold for slices, with the same read-only rule for the copy form.
- A collection must not change its size while a loop walks it. Every collection keeps a count of its changes, and its iterator remembers the count. A dev build traps when they differ, naming the collection and both places: `` `people` was changed while `for` walked it ``. A release build carries no check. Removing while walking is `remove_all(test)`, which is built for it.

## Hashing and order

- `operator fn hash(self) -> u64` joins the table of language hooks. The built-in types have it. A struct or class gets a default that hashes its fields in order, and may replace it.
- Two values that are equal by `eq` have the same `hash`. The default keeps that rule, and a replacement must.
- `anti.lang` ships `constraint Ordered = eq + lt;`.
- A hashing collection mixes its hash with a random seed chosen when the program starts. Keys chosen by an attacker then cannot all land in one bucket. The seed never decides the order a program sees.

## Direct imports

`import anti.collection.map.{Map, HashMap};` makes the listed names of a module visible in the importing file without the module's name. Code then writes `Map<str, int>` rather than `map.Map<str, int>`.

- The listed names must be public items of that module. A listed name that clashes with a name already visible in the file is refused, naming both.
- The module itself stays reachable by its name, as with a plain `import`.
- `anti fmt` keeps the list sorted.
- It serves every module: `import anti.regex.{Regex};`, `import anti.mem.{Shared, ArenaAllocator};`.

## Module layout of the collections

The collections are split into modules by family, one file each. Each can then be built and read on its own:

| Module | Items |
|---|---|
| `anti.collection` | `Iterable<T>`, `Iterator<T>`, and the parts every collection shares |
| `anti.collection.list` | `List<T>` |
| `anti.collection.deque` | `Deque<T>` |
| `anti.collection.ring` | `Ring<T, N>` |
| `anti.collection.grid` | `Grid<T>` |
| `anti.collection.map` | `Map<K, V>`, `HashMap<K, V>` |
| `anti.collection.set` | `Set<T>`, `HashSet<T>`, `BitSet` |
| `anti.collection.sorted` | `SortedMap<K, V>`, `SortedSet<T>` |
| `anti.collection.pool` | `Pool<T>`, `Handle<T>` |
| `anti.collection.tree` | `Tree<T>` |
| `anti.collection.queue` | `PriorityQueue<T>` |
| `anti.collection.sync` | `SyncList<T>`, `SyncMap<K, V>`, `SyncSet<T>`, `SyncPool<T>` |
| `anti.collection.concurrent` | `ConcurrentMap<K, V>`, `SpscRing<T, N>` |

## Collections

### Principles

- A collection is a class used as a value. It owns its storage and is freed at the end of its block. It moves on return, is refused by `=` and is copied by `dup`.
- Elements are stored by value, in the collection's own memory. A collection of pointers, such as `List<*Circle>`, owns the pointers and not what they point at.
- No pointer to an element ever leaves a collection, except through a `lent` parameter. Reading gives a copy, as a `?T` where nothing may be there. Changing hands a new value in by value, or runs inside the collection.
- An element is addressed by key in a map, and by position in a list while one thread walks it. It is addressed by criteria in any collection, and by handle in a `Pool` or a `Tree`.
- Criteria come in three explicit forms and never an ambiguous one: `_all` acts on every match and gives the count, `_first` on the first match in the collection's order, and `_one` on the one match. `_one` fails when nothing matches, and fails when more than one does, with the count. It is how criteria stand in for an address.
- Every collection takes an optional allocator when it is made, `List<Person>.new(from: &arena)`, with the C library as the default. Everything it allocates comes from that allocator and goes back to it.
- Every collection works with `for` through the `iter` hook.
- A collection that grows takes an initial capacity when it is made, `List<Person>.new(capacity: 1000)`, and allocates once for it. Without one it starts small and doubles as it grows. `reserve(n)` makes room for `n` more elements in one allocation, and `shrink()` gives unused room back. `List`, `Deque`, the maps and the sets have all three.
- A collection has the `eq` hook when its elements have it, comparing element by element, so `a == b` works for two lists. `to_text` writes it as `[1, 2, 3]`, or `{"Ann": 41}` for a map. `serialize` and `deserialize` write and read it as a JSON array, or as an object for a map with text keys and as an array of pairs for other maps.

### Operations every collection has

| Operation | Meaning |
|---|---|
| `T.new(from: &alloc)` | create, with an optional allocator |
| `c.count`, `c.is_empty()` | size |
| `for x in c` | walk |
| `c.find_all(test)`, `find_first(test)`, `find_one(test)` | copies of matches |
| `c.update_all(test, change)`, `update_first`, `update_one` | change matches inside the collection |
| `c.remove_all(test)`, `remove_first`, `remove_one` | remove matches |
| `c.clear()` | remove everything |

### Sequences

**`List<T>`**, one contiguous buffer.

| Operation | Meaning |
|---|---|
| `push(own x)`, `pop() -> ?T` | at the end |
| `insert(i, own x)`, `remove_at(i) -> T` | at a position |
| `l[i]`, `l[i] = x` | a copy, or a replacement. Out of range traps in a dev build, as an array does |
| `get(i) -> ?T`, `first()`, `last()` | `none` when out of range or empty |
| `read(i, f)`, `modify(i, f)` | lend one element |
| `lend_slice(f)` | lend the whole buffer as one `lent` slice, for C and simd |
| `sort()`, `sort_by(key)` | a stable sort. `sort` needs `T: Ordered` |
| `reverse()`, `map<U>(f) -> List<U>`, `filter(test) -> List<T>` | reshape. `map` and `filter` make a new list |

**`Deque<T>`**, a ring buffer that grows: `push_front`, `push_back`, `pop_front`, `pop_back`, `front` and `back`, the last four giving `?T`.

**`Ring<T, N: int>`**, a ring buffer of fixed capacity `N` that allocates nothing after it is made, for real-time code such as an audio callback: `push(own x) -> bool`, which gives `false` when full, `pop() -> ?T`, `peek() -> ?T`, `is_full()`.

**`Grid<T>`**, a two-dimensional array: `Grid<T>.new(width, height, fill)`, `g[x, y]` with bounds checks in a dev build, `get(x, y) -> ?T`, `width`, `height`, and `lend_slice(f)`. It is stored as one buffer, row by row.

### Maps

| Type | Order when walked | Lookup | Key |
|---|---|---|---|
| `Map<K, V>` | insertion order | constant time on average | `K: eq + hash` |
| `SortedMap<K, V>` | key order | logarithmic | `K: Ordered` |
| `HashMap<K, V>` | none, and different on every walk | constant time on average | `K: eq + hash` |

| Operation | Meaning |
|---|---|
| `get(k) -> ?V`, `contains(k)` | look up |
| `set(k, own v)` | insert or replace |
| `add(k, own v) -> bool` | insert only a new key |
| `remove(k) -> ?V` | remove and return |
| `update(k, change) -> bool` | change one value inside the map |
| `read(k, f)`, `modify(k, f)` | lend a value |
| `for (k, v) in m`, `keys()`, `values()` | walk |

`SortedMap` adds `range(lo, hi)`, from `lo` up to and not including `hi`, `floor(k)`, `ceiling(k)`, `first()`, `last()`, `pop_first()` and `pop_last()`.

- `Map` keeps its entries in one array in insertion order, with a hash table of positions into it. Its walking order is the same on every run and every machine.
- `SortedMap` is a B-tree. A node holds many keys in one block, so a lookup touches few cache lines.
- `HashMap` keeps its entries in the hash table itself, with no order to maintain. It suits large maps with frequent removal. Its walking order is random on purpose, on every walk, so no program comes to depend on it. `serialize` writes a `HashMap` sorted by key where the key type has `lt`, so saved files stay stable.

### Sets

`Set<T>`, `SortedSet<T>` and `HashSet<T>` follow the three maps: the same orders and the same constraints on `T`. Each has `add(own x) -> bool`, `remove(x) -> bool`, `contains(x)`, `union(o)`, `intersect(o)` and `minus(o)`, which make new sets, and `is_subset(o)`. `SortedSet` adds `range`, `floor`, `ceiling`, `first` and `last`.

**`BitSet`**, a set of small integers, one bit per value: `add(n)`, `remove(n)`, `contains(n)`, `count`, the set operations, and `for n in b` in increasing order.

### Stable elements

**`Pool<T>`** keeps its elements at fixed addresses. It grows by adding blocks, each twice the size of the one before. Element `i` is then found with a few bit operations. Removing an element frees its slot for reuse and moves nothing.

| Operation | Meaning |
|---|---|
| `add(own x) -> Handle<T>` | insert |
| `get(h) -> ?T`, `contains(h)` | `none` for a handle whose element is gone |
| `set(h, own x) -> bool`, `remove(h) -> ?T` | `false` or `none` for a stale handle |
| `read(h, f)`, `modify(h, f)` | lend an element |
| `get_versioned(h) -> ?(T, int)`, `set(h, own x, if_version: v) may fail` | compare and set |

- A handle is a slot index and a generation. Removing an element increments its slot's generation, so an old handle stops matching even after the slot is reused. A handle never finds the wrong element: at worst it finds nothing.
- A handle may be copied, stored and passed between threads.
- Versioned `set` is on `Pool` alone among these, since only its elements carry a version already. The thread-safe collections gain it too.

**`Tree<T>`** holds a hierarchy, such as a scene graph, a menu or a document, on handles as a `Pool` does: `add_root(own x)`, `add_child(parent, own x)`, `parent(h) -> ?Handle<T>`, `children(h)`, `depth_first(h)`, `breadth_first(h)`, `remove(h)`, which removes the whole subtree, and the lending and reading forms of `Pool`.

### Ordered retrieval

**`PriorityQueue<T: Ordered>`**, a binary heap in one array: `push(own x)`, `pop() -> ?T` and `peek() -> ?T`, which give the smallest element first. It serves schedulers, event queues, timers and path finding.

### Left out, with the reason

- A stack: `List` is one, with `push` and `pop`.
- A map with more than one value per key: `Map<K, List<V>>` expresses it.
- A map in both directions: two maps.
- A cache that drops the least recently used entry: a policy over `Map` and `Deque`, for a later `anti.cache`.
- Tries, interval trees and graphs: specialised, for libraries of their own.
- A list that keeps its first elements inline: an optimisation, and `Ring` covers the real-time case.
- A second balanced search tree: `SortedMap` is one, and a B-tree outperforms a binary tree on modern processors.

## Thread-safe collections

Collections shared between threads follow round four. Each is a thread-safe type, so a closure at a `concurrent` parameter and a worker may change it.

| Type | Kind | For |
|---|---|---|
| `SyncList<T>` | `synchronized` | a list more than one thread adds to and searches |
| `SyncMap<K, V>` | `synchronized` | a shared lookup table with modest traffic |
| `ConcurrentMap<K, V>` | `concurrent` | a busy shared map, split into parts locked apart, so threads using different keys do not wait for each other |
| `SyncSet<T>` | `synchronized` | a shared membership set |
| `SyncPool<T>` | `synchronized` | objects shared between threads by handle |
| `SpscRing<T, N: int>` | `concurrent`, lock-free | one producing thread and one consuming thread, with no lock |

- They have the operations of the plain collections, less every operation by position: no `l[i]`, no `get(i)`, no `insert(i, x)`. Between threads a position can go stale at any moment, so an element is addressed by criteria, key or handle.
- They have no `lend_slice`, since a slice of the whole buffer would be reached without the lock.
- They all have versioned `set`, so a thread can read, compute, and write back only if nothing changed meanwhile.
- `read`, `modify` and the functions given to `update_*` and `remove_*` run inside the lock. A slow function holds up other threads, and the documentation says so.
- `SpscRing` passes samples or messages between an audio thread and the rest of a program. Neither side ever waits, since it holds only two atomic counters, one per side. It is marked `unchecked` with its reason, because its correctness rests on atomics in a pattern the checker cannot follow. Its tests exercise both sides at full speed on every host.

Left out: a queue between threads, since `chan T` is one. Also left out are synchronized versions of the sorted and hash-ordered maps and sets, and of `PriorityQueue`, `Grid`, `Tree` and `BitSet`. Sharing those is rare, and a `synchronized` class around the plain one covers it.

## Shared ownership

`anti.mem.Shared<T>` gives an object more than one owner, by explicit reference counting.

```anti
let c = Shared<Circle>.new(Circle { r: 2.0 });
let list = List<Shared<Circle>>.new();
list.push(c.share());
```

- `share()` gives another handle and increments the count, visibly. `=` stays refused, so a count never changes silently.
- The object is destroyed, through its `destruct`, when its last handle is freed. The count is atomic, so handles may be shared between threads. `T` must be thread-safe for more than one thread to change the object.
- `read(f)` and `modify(f)` lend the object.
- It costs only where it is used: every other pointer stays a plain address.
- Two shared objects that hold each other are never freed. The documentation says so, and the fix is a plain pointer for one of the links.

## Memory checks

`--memory-checks` makes a build detect memory errors while the program runs: `antic --memory-checks`, and the same option for `anti build`, `anti test` and `anti run`.

- It catches a use after free, a double free, and a read or write outside a heap block. At exit it reports the leaks, with where each block was allocated.
- A pointer into a collection kept across a change of its size is a use after free. This is how such a mistake is found.
- The back end emits the checks of AddressSanitizer around every load and store. The build links the AddressSanitizer runtime of the pinned clang. Its reports carry the names and lines of `-g`.
- Windows on ARM64 has no AddressSanitizer runtime, and the option is refused there with a message that says so.
- The option is off by default in every build, since it makes a program run two to three times slower.

## Messages added

- `` `Circle` has no `lt`, which `max` needs for `T` ``
- `` `max` uses `+` on `T`, which its constraints do not give. Add `add` to them ``
- `` `g` is not generic. Put the comparison in parentheses ``
- `` `map` takes type parameters, so it cannot be `abstract` or replaced ``
- `` `size` must be a constant for `N` ``
- `` an `export fn` cannot be generic. Export a named copy with `export type` ``
- `` `a` may be none. Test it or use `??` ``
- `` `c` was moved into `shapes` by `push` ``
- `` `push` takes a `Circle`, and `c` is a `*Circle` ``
- `` `p` is lent for the call and cannot be stored ``
- `` `Circle` has no `hash`, which `Map` needs for `K` ``
- `` `find_one` found 3 elements where it needs exactly one ``
- `` `p` is a copy of each element of `people`. Walk with `&people` to change the elements ``
- `` `people` was changed while `for` walked it ``
- `` `--memory-checks` is not available for windows-arm64 ``
- `` `Map` is already visible here, from `anti.collection.map` and from `geo` ``

## Open questions

None. The word for a pointer valid only during a call is `lent`, chosen over `borrowed` and `scoped`. `borrowed` would suggest Rust's whole system of references.
