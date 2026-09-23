# Anti language additions, round four

Addendum to `docs/anti-language-additions.md`. It adds eight sections. They cover regular expressions, byte search and patching, language hooks, iteration and anonymous functions. They also cover thread-safe classes, nested types, and errors, warnings and checks. It reverses two settled decisions. Apply it by editing that document, then delete the addendum. Drafted on 2026-09-23.

Nothing in this addendum is work for a session. It becomes work when Eddie names it in a work order or a session message. When this addendum is applied, recording it in the additions document is the whole task. The open questions at the end are settled with Eddie before any of it is built.

## What to change in the additions document

- The settled decision "no regex literals in the language" is reversed. Regular expressions are part of the language: a pattern literal, a `Regex` type and methods of `str`.
- String prefixes. Add `re` to the prefix table: `re"..."` is a pattern literal.
- Contextual words. Add `snapshot` before an anonymous `fn`, `keep` and `concurrent` before a parameter of function type, `synchronized` and `concurrent` before `class`, `guarded by` and `unchecked` after a field's type, `unchecked` in a class header, and `allow` for silencing a warning.
- Nested types. The settled decision that every type is declared at module level is reversed, deliberately, by Eddie on 2026-09-23. A class may declare private types inside it, as [Nested types](#nested-types) describes. Update the entry in `docs/decisions.md` and the object model to say so. Keep the reason for the old rule where it still holds. The classes that `anti format` generates from a wire format stay at module level, since a user works with them directly. A session that finds the old entry must follow this addendum, not stop on the conflict.
- Error handling. Add `catch none` beside `catch fatal`: a failure counts as `none` where the result can be `none`.
- The `anti check` regex rule is replaced. The compiler checks every pattern literal itself.

## Regular expressions

Regular expressions belong to `str`, as they belonged to text in Perl, without Perl's hidden global variables. The engine is PCRE2, built in `src/native/`. A program links it only when it uses a pattern.

### Patterns

- `re"..."` is a pattern literal of type `Regex`. It is raw: a backslash is a backslash. It takes the hash delimiters of every string form, `re#"..."#`.
- The compiler checks every pattern literal. A malformed pattern is a compile error with the position PCRE2 names.
- Flags are PCRE2's inline flags, `i`, `m`, `s` and `x`, over the whole pattern or a group: `re"(?i)abc"`, `re"(?i:abc)D"`. There is no suffix form.
- In a text pattern, `\d`, `\w` and `\s` mean their ASCII sets: `\d` is `0` to `9`, and `\w` is `[A-Za-z0-9_]`. Unicode classes are written out, `\p{L}` for any letter and `\p{Nd}` for any decimal digit. `(*UCP)` at the start of a pattern gives the three their Unicode meaning. The default keeps `\d` from accepting a digit that `text.parse_int` rejects.
- `Regex.compile(text: str) -> Regex may fail` compiles a pattern at run time. It fails with `regex.BadPattern`, which carries PCRE2's message and position. A pattern built from parts uses `rf"..."`, which interpolates without escape processing.

### Failures, by where a pattern comes from

A call fails exactly when a failure is possible. That depends on where its pattern comes from, which the compiler knows.

- A pattern literal cannot make a method fail. A replacement template that names a group the pattern lacks is a compile error. A pattern that can take exponential time, such as `(a+)+`, fails the safety check `exponential-pattern`. The compiler finds nested repeats over text that overlaps. Its message names PCRE2's possessive quantifiers and atomic groups, `a++` and `(?>...)`, which forbid the backtracking. Where the input is known to be short, `unchecked(exponential-pattern, "reason")` overrules it.
- A pattern literal can still reach the match limit on a large input, as `.*a.*a.*a` can on a megabyte. That is a mistake in the program. It stops the program with the pattern and the line, as an index out of bounds does. It cannot be caught.
- A pattern compiled at run time makes every method that takes it `may fail`. The failures are classes of `anti.regex`: `TooExpensive` for the match limit and `MissingGroup` for a template that names a group the pattern lacks. Both inherit from `regex.Error`, as does `BadPattern`.
- One name serves both origins. `line.matches(re"...")` needs no handler, and `line.matches(r)` with a compiled `r` needs one. The compiler enforces which.

`catch none` treats a failure as `none`, where the result can be `none`. It is how a program chooses, at the call, to count a failure as no match:

```anti
let m = line.matches(r) catch none;
let m = line.matches(r) catch e {
	if e is *regex.TooExpensive {
		io.println("that pattern is too slow on this text");
	}
	yield none;
};
```

### Thread safety

Nothing a match reads or writes is global.

- A compiled `Regex` never changes. Any number of threads may use one at once, and it may be passed to a worker as a `str` may.
- Every pattern literal is compiled once, at program start before `main`. No pattern is compiled lazily, so no hidden flag guards a first use.
- All state of one match belongs to that call. The engine's working memory and the capture positions live in the caller's frame or in the returned match.
- A match's texts are slices of the searched text. Nothing is copied, and a match is valid while that text is.
- The match limit that stops a runaway pattern is fixed when the pattern is compiled. No thread can change it for another.

### Methods of `str`

- `s.matches(r) -> ?Match` gives the first match, or `none`.
- `s.find_all(r)` gives every match, left to right, for a `for` loop: `for m in s.find_all(r) { }`.
- `s.replace(r, with, limit: int = 0) -> str` replaces matches. `with` is a template or a function, as described below.
- `s.split(r, limit: int = 0) -> []str` splits at each match.

`limit` of 0 means every match. A positive `limit` takes that many matches from the start, and a negative one that many from the end. The search itself always runs from the start. A negative limit picks the last of the matches found that way, so the result never depends on direction. Text is stored in logical order, the order it is read. So "from the start" is the reading direction of every script, right-to-left ones included. No locale or text direction is consulted, since either would be a global setting.

### The match

A match result behaves as a nullable value, with the rules of `?*T`. It compares with `none`, narrows after a test, and works with `if let` and `let ... else`. Its fields are unreachable until a test proves it matched.

```anti
let m = line.matches(re"(?<year>\d{4})-(?<month>\d\d)");
if m == none {
	fail "no date";
}
io.println(f"{m.year} / {m.month}");
```

A match result also stands on its own as a condition, in `if`, `while`, `&&`, `||` and `!`, meaning "matched": `if line.matches(r) { }`. It converts to `bool` nowhere else. When a result is only tested and never bound, the compiler calls the cheaper engine path that computes no captures.

| Field | Meaning | Type |
|---|---|---|
| `m.all` | the whole matched text | `str` |
| `m.group(n)`, `m.group("name")` | a group by position or name | `str` |
| `m.1`, `m.year` | a group as a field, for a pattern literal only | `str` |
| `m.took_part(n)`, `m.took_part("name")` | whether a group took part | `bool` |
| `m.count` | the number of groups | `int` |
| `m.pre` | the text before the first match | `str` |
| `m.post` | the text after the last match | `str` |

- A group that did not take part gives the empty `str`. `took_part` tells it apart from a group that matched nothing.
- `pre`, the matches and `post` together always cover the whole text. With no matches, `pre` is the whole text and `post` is empty.
- For a pattern literal the compiler knows every group, so `m.1` and `m.year` are checked at compile time. A pattern compiled at run time has `group` and `took_part` alone.

### Replacement

- A template names groups with `$1` and `${name}`. `$` reads every digit that follows it, so `$10` is group 10, and group 1 followed by a `0` is `${1}0`. `$$` writes one `$`. Against a pattern literal, a group the pattern lacks is a compile error. Against a compiled pattern, it is a `regex.MissingGroup` failure.
- A function receives each match and returns its replacement: `with: fn(Match) -> str`. It may be a named function or an anonymous one, described below.

```anti
let d = "2026-09-23".replace(re"(?<y>\d{4})-(?<m>\d\d)-(?<d>\d\d)", "${d}/${m}/${y}");
// "23/09/2026"
```

## Bytes

The pattern methods of `str` work on `[]byte` too, in byte mode, for binary data such as file formats and network streams.

### Byte patterns

- There are two pattern types. `Regex` searches text, and `ByteRegex` searches bytes. In byte mode `.` matches any byte, `\x89` means the byte 0x89, and nothing needs to be valid UTF-8.
- A pattern literal takes its mode from where it is used, as an anonymous function takes its types from its target. `data.find_all(re"...")` is byte mode because `data` is a `[]byte`. A literal stored in a variable is a `Regex` unless the variable says otherwise: `let r: ByteRegex = re"\x89PNG";`.
- `ByteRegex.compile(text: str) -> ByteRegex may fail` compiles a byte pattern at run time.
- A `Regex` passed to a method of `[]byte`, or a `ByteRegex` to a method of `str`, is a compile error.
- In a byte pattern, `\d`, `\w` and `\s` are the ASCII sets.
- A character outside ASCII in a byte pattern stands for its UTF-8 bytes. `re"versión"` matches the bytes of `"versión".to_bytes()`, since the source file is UTF-8 as well.
- A class in a byte pattern lists ASCII bytes and ASCII ranges freely. It may also list characters outside ASCII. Each becomes a choice of its byte sequence: `[óa]` is `(?:\xC3\xB3|a)`. A negated class that holds such a character is a compile error that names text mode. So is a range that reaches beyond ASCII. Byte mode cannot know where a character starts in either.

### Text in bytes

UTF-8 gives every byte of a character beyond ASCII a value of 0x80 or more. So an ASCII byte never stands inside one. A byte pattern built only from ASCII pieces matches only whole ASCII bytes, and never cuts a character. A pattern that matches bytes of 0x80 and up can match part of a character, through `.` or an explicit `\xC3`. A replacement can then leave invalid UTF-8. Data that is text in any script is converted with `to_text()` and searched in text mode. There a match never splits a character.

A letter can be encoded as one code point or as a base letter and a combining mark. The two look alike and compare different in both modes. Matching text from mixed sources normalizes both sides to one form first, which is a standard-library function for later.

### Conversions

- `s.to_bytes() -> []byte` copies a `str` into new bytes. It must copy, since a `[]byte` can be written and a `str` never changes. The name says a new value is made, as `to_text()` does.
- `data.to_text() -> str may fail` makes a `str` from bytes. It checks the bytes are valid UTF-8 and fails when they are not.

### Byte methods

- `matches`, `find_all`, `replace` and `split` work on `[]byte` as they work on `str`, with the same `limit`, the same failures by pattern origin and the same rules for threads.
- A match's fields are `[]byte` slices of the searched data, so they copy nothing and go straight to the readers of `anti.binary`.
- A replacement template is a byte string with the same group references, `b"...$1..."`.
- `replace` returns a new `[]byte`, since a replacement may differ in length from what it replaces. Its memory comes from the C library and is freed with `free(result.ptr)`.

### Patching in place

`patch` writes bytes over part of what it finds, where it stands, and allocates nothing. It finds either an exact byte sequence or a pattern.

```anti
data.patch(x"80 10 20 30", x"81");            // 81 10 20 30
data.patch(x"80 10 20 30", x"FF", at: 2);     // 80 10 FF 30

let v = "version: 1.10.1".to_bytes();
v.patch(re"version: \d+\.\d+\.(\d+)", b"2"); // version: 1.10.2
```

- `data.patch(find, with: []byte, into = 0, at: int = 0, limit: int = 0) -> int` gives the number of places it patched.
- `find` is a `[]byte` or a byte pattern. For a `[]byte`, the span to write into is the whole occurrence. For a pattern with one group it is that group, and without groups it is the whole match. A pattern with more groups names its group with `into`, by number or by name. For a literal pattern, leaving it out is a compile error.
- `with` is written at offset `at` inside that span. It must fit: `at` plus the length of `with` is at most the span's length. The rest of the span keeps its bytes. The data never changes length.
- A `with` that does not fit is a compile error when `find`, `with` and `at` are all literals. When any of them is computed at run time, the call `may fail` with `LengthMismatch`, by the same rule of origin as patterns.
- `limit` counts as for `replace`: 0 for every place, a positive number from the start, a negative one from the end.
- A span written by pattern can be longer than `with`. `"version: 1.10.10"` patched with `b"2"` gives `1.10.20`, not `1.10.2`: the span `10` keeps its second byte. `patch` fits fields of a fixed size. Data whose length may change uses `replace`, which returns new bytes.
- `str` has no `patch`. Text never changes, so a `str` can be shared between threads and sliced safely, and only `replace`, which returns new text, applies to it. Text that must be edited in place goes through a `text.Builder`, or a copy made with `to_bytes()` that is patched and turned back with `to_text()`.

## Language hooks

A type joins a construct of the language through `operator fn`, as it already does for `+`. The name comes from a fixed table, so the compiler checks it: `operator fn nxt` is an error that lists the valid names, and a hook with the wrong signature is an error that states the right one. An `operator fn` is also an ordinary method, so `a.add(b)` is the same call as `a + b`.

| Construct | Operator names |
|---|---|
| `+ - * / %`, unary `-` | `add`, `sub`, `mul`, `div`, `rem`, `neg` |
| `==`, `<`, and the comparisons derived from them | `eq`, `lt` |
| bitwise operators | `and`, `or`, `xor`, `shl`, `shr`, `not` |
| `for x in e` | `iter` on the collection, `next` and `value` on the iterator |
| `e[i]` | `index` to read, `set_index` for `e[i] = v` |

`f"..."` writes a class through `to_text`, which every class has.

The guide gives this table as the one place a programmer looks to make a type work with the language.

## Iteration

`for x in e` walks more than ranges and slices. Two roles make it work, and neither allocates.

- An iterator has `operator fn next(self) -> bool` and `operator fn value(self) -> T`, and holds a position. `for` calls `next`, and while it gives `true`, binds `x` to `value()` and runs the body.
- A collection has `operator fn iter(self)`, which returns a new iterator on every call. `for x in e` on a collection calls `iter()` first, so every loop starts at the beginning and nested loops each keep their own position. The collection itself never changes, so any number of loops and threads can walk it at once.
- Each iterator declares its own `value()` with its concrete type, so iteration needs no generics.
- Ranges and slices keep their built-in forms.

`while` uses the same hooks, called by name, when the iterator itself is needed after the loop or halfway through it:

```anti
let it = people.iter();
while it.next() do {
	let p = it.value();
	if p.age >= 65 {
		break;
	}
}
```

`for x in e` is the one form for walking a collection. There is no binding form of `while`.

`find_all` and `split` return iterators. They are lazy: each step finds the next match or piece, and nothing is allocated. `.to_slice()` collects everything into a new `[]Match` or `[]str`, freed with `free(result.ptr)`. The `to_` says a new value is made. `limit` stops the iterator early. The iterator of `find_all` carries `pre`, the text before the first match, and `post`, the text after the last, which is known once the loop has ended.

```anti
for m in line.find_all(r) { }
let parts = line.split(re",\s*").to_slice();
```

With generics, `anti.collection.Iterable[T]` and `Iterator[T]` follow as interfaces, for a function that takes any collection of a given element type. A class with the `iter` hook implements them without extra code. They belong to the generics design.

## Anonymous functions and closures

### Anonymous functions

`fn(params) -> R { body }` written as an expression is an anonymous function. Passed to a parameter of function type, it may leave out its own types. They come from that parameter: `fn(m) { ... }`. The types come from the target, never from the body. A named function always writes its full signature. The signature is the contract its callers, the library file and the header depend on. An anonymous function that uses nothing from the enclosing function is an ordinary function value. It may be stored, returned and passed anywhere a named function may.

### Closures

An anonymous function that uses a local variable or parameter of the enclosing function is a closure. It captures that variable by reference, and may read and change it.

```anti
fn censor(text: str, words: Regex) -> (str, int)
{
	let hits = 0;
	let clean = text.replace(words, fn(m: Match) -> str {
		hits += 1;
		return "*".repeat(m.all.char_count());
	});
	return (clean, hits);
}
```

- A closure never outlives the variables it captures. It may only be passed as an argument to a parameter that does not keep it. It may also be held in a local of the same function that is used the same way. It is refused in a field, a return value, a global, and any parameter marked `keep`.
- The captured variables stay in the enclosing function's frame, and the closure reaches them through a context pointer. Creating a closure allocates nothing.
- A parameter of function type does not keep its argument unless it is marked `keep`: `fn on_click(keep f: fn(Event))`. A `keep` parameter, a field and a global of function type accept named functions and non-capturing anonymous ones alone.
- The checker enforces `keep`. A function that stores a parameter not marked `keep` is a compile error.
- A closure may change what it captures only when it is called from one thread at a time. Each thread that runs a function makes its own closures over its own frame. Closures made on different threads share nothing.
- A parameter of function type that a function passes on to `parallel` or `dispatch` may be called from more than one thread at once. It is marked `concurrent`, and the checker requires the mark. A closure passed to a `concurrent` parameter may read what it captures. It may change a captured variable only when that variable's type is concurrent, as [Concurrent classes](#concurrent-classes) defines. Otherwise the write is refused with the variable's name. The creating thread waits in `parallel` until every worker finishes, so the reads cannot race a write.
- `concurrent` is a permission the function reserves, not a promise: it may use threads, not that it does. Removing the mark later never breaks a caller. Adding it later does, since a closure that writes a captured variable stops compiling.
- A worker may take a function value as a parameter, and a closure only through a `concurrent` parameter.
- A closure may fail when the parameter's type says so: `fn(Match) -> str may fail`.

### Snapshots

`snapshot fn(...) { }` is a closure that takes the values it uses when it is made. It never depends on the caller's variables again.

```anti
let name = "report";
on_click(b, snapshot fn(e) { save(name); });
name = "draft";                      // the closure still saves "report"
```

- The snapshot is read-only. A snapshot closure that writes a captured value is refused. If it could write, two threads calling the same closure would write the same snapshot at once.
- A snapshot may hold only values that copy fully. Those are numbers, `bool`, `char`, structs of those, and `str`, whose bytes are copied into it. Capturing a pointer or a slice is refused. The copy would hold an address, not what it points at.
- A snapshot closure is accepted at a `keep` parameter and at a `concurrent` parameter, where a closure by reference is refused. Each refusal of a closure by reference names `snapshot fn` as the fix.
- Where the snapshot lives follows from where the closure goes, not from the word. At a `concurrent` parameter it sits in the caller's frame, since the caller waits. Nothing is allocated. At a `keep` parameter it must outlive the frame, so it goes on the heap. Whoever keeps the closure owns it and frees it.
- The word is `snapshot` and not `copy`. The values are taken at that moment and stay as they were. A copy suggests something its holder may change.

### Calling convention

A value of function type held in a field, a global or a `keep` parameter stays one pointer, the C function pointer it is today, so C callbacks such as raylib's are unchanged. A parameter that does not keep its argument is passed as two words, the code and a context pointer, and the context is `none` for a function that captures nothing. The header writes such a parameter the C way, as a callback and a `void *` context. An `extern fn` takes plain C function pointers only, so a closure cannot reach C.

## Concurrent classes

Any value may be read from more than one thread at once. Changing a value from more than one thread at once needs a type built for it, a thread-safe type. The built-in thread-safe types are `Mutex`, `chan T` and the atomics. A class becomes thread-safe in one of two ways, and the compiler checks both.

| Declaration | Who handles concurrency | What the compiler checks |
|---|---|---|
| `synchronized class` | Anti: one lock around every public function | everything |
| `concurrent class` | the programmer: own locks, `guarded by`, atomics | every field is guarded, atomic or fixed |
| `concurrent class ... unchecked("reason")` | the programmer, fully | nothing, by explicit choice |

The rules for closures and workers use this term. A closure passed to a `concurrent` parameter may change a captured variable of a thread-safe type.

### Synchronized classes

`synchronized class` gives each object a hidden lock, which the program never declares. Every public function of the class runs under it. The lock is taken when the function starts, and released on every exit, `return` and `fail` included. A public function that calls another on the same object does not wait for itself. A private function runs inside the lock of the public function that called it.

```anti
pub synchronized class PeopleList
{
	struct Node
	{
		person: Person,
		next: ?*Node,
	}

	head: ?*Node = none,
	count: int = 0,

	pub fn add(self, p: Person)
	{
		self.head = alloc Node { person: p, next: self.head };
		self.count += 1;
	}

	pub fn remove(self, test: fn(Person) -> bool) -> int
	{
		...
	}
}
```

- A field of a synchronized class is reached only by the functions of the class. `construct` and `destruct` are exempt, since no other thread can see the object then.
- Every operation on the object waits for every other. That is always correct and fast enough for most objects. A structure whose threads wait on each other more than a profiler allows becomes a concurrent class.
- `anti doc` and the C header mark every public function of a synchronized class as running under the object's lock. The documentation says it at the function.
- `sync obj { }` takes the hidden lock of a synchronized object for a whole block, for a sequence that no method covers.

### Concurrent classes

`concurrent class` leaves the locking to the programmer, for finer locks that let unrelated work run at once. The compiler checks that every field is one of three kinds. A field of none of them is a compile error that names the three:

- Guarded: `field: T guarded by lock`, reached only inside `sync` on that lock. The lock is a `Mutex` field of the same object, or of an enclosing object named by its class, `guarded by PeopleList.lock`.
- Atomic: reached only through atomic operations.
- Fixed: written only in `construct`, and read-only after that.

```anti
pub concurrent class PeopleList
{
	struct Node
	{
		lock: Mutex,
		person: Person guarded by lock,
		next: ?*Node guarded by PeopleList.lock,
	}

	lock: Mutex,
	head: ?*Node = none guarded by lock,

	pub fn add(self, p: Person)
	{
		let n = alloc Node { person: p };
		sync self.lock {
			n.next = self.head;
			self.head = n;
		}
	}
}
```

`add` holds the list's lock only for the two pointer moves. A change to one person holds only that node's lock, so both run at once. The check proves every field is guarded. It cannot prove the design right. The order in which locks are taken, and whether a sequence of calls is safe, stay the programmer's responsibility.

A field that is none of the three fails the safety check `unguarded-field`. `unchecked` overrules it where the checker cannot follow, with the rules of [Errors, warnings and checks](#errors-warnings-and-checks). After a field's type it covers that field alone, so every other field stays checked. A lock-free structure marks the fields it swaps with `compare_swap`. In the class header it covers the whole class. That suits a class that wraps a C library doing its own locking.

```anti
pub concurrent class Queue
{
	head: ?*Node unchecked(unguarded-field, "swapped with compare_swap, see push and pop"),
	tail: ?*Node unchecked(unguarded-field, "swapped with compare_swap, see push and pop"),
	lock: Mutex,
	count: int guarded by lock,
}
```

### Rules for both

- A public function of a thread-safe class never gives out a pointer or a slice into the object's fields. It does not return one, and it does not pass one to a parameter of function type. It returns and passes copies, since a caller holding such a pointer would reach the data without the lock.
- A closure passed to a function of a synchronized class runs inside the lock. It runs on one thread at a time, so it is a plain parameter.
- `atomic` also marks a local: `let hits: atomic int = 0;`. An atomic local is thread-safe.
- A worker may take a pointer to a thread-safe object. It is the one exception to the rule that a worker's parameters hold no pointer.

### Locks

`Mutex` is one word of the program's own memory, and the operating system keeps nothing for it until a thread has to wait: a `futex` word on Linux, `os_unfair_lock` on macOS, `SRWLOCK` on Windows. A lock in every node of a large structure therefore costs memory alone, four or eight bytes each, and `size_of(Mutex)` is documented per target. Where even that is too much, a fixed set of locks shared by address, lock striping, keeps the count constant.

A `Mutex` cannot be copied or assigned. A class that holds one follows the ownership rules for values that cannot be copied.

### Operations that decide for themselves

Locking every function makes each call safe, never a sequence of calls. Between `list.size()` and `list.remove(0)` another thread can empty the list. A thread-safe class therefore takes criteria, not positions: `list.remove(fn(p) { return p.age >= 65; })` finds and removes under one lock, where an index returned by one call can be stale by the next. The compiler cannot tell a position from a count in an `int`, so this is a design rule for the standard library and for every thread-safe class, and the guide teaches it.

### Deadlock

Two threads that take two locks in opposite orders wait for each other forever. The compiler cannot see that in general. A dev build records the order in which each thread takes locks. It reports two orders that conflict, with both call sites, before they hang in production.

## Nested types

A class may declare a `struct`, `enum` or `class` inside its body. The nested type is private to that class.

- Only the enclosing class can name the type. Code outside can neither create one nor receive one. A public signature of the enclosing class that names it is a compile error.
- Its full name is the enclosing class's name followed by its own, `PeopleList.Node`, which the symbols and the C header use.
- A nested type suits what belongs to one class alone, such as the node of a list. A type that users work with directly stays at module level.
- In a thread-safe class, a nested type can never leave the class, so a node can never leak by construction.

## Errors, warnings and checks

The compiler reports four kinds of problem, and each is handled in its own way. The guide explains them together, with how dev and release builds treat each.

| Kind | Example | Dev build | Release build | Overruled by |
|---|---|---|---|---|
| Error | a type mismatch, an unhandled failing call | stops | stops | nothing |
| Warning | a shadowed name | prints and carries on | stops | `allow(name, "reason")` |
| Safety check | an unguarded field in a concurrent class | stops | stops | `unchecked(name, "reason")` |
| Run-time check | an index out of bounds, an overflow | traps with file and line | not compiled | `--checks` puts them in a release build |

- An error is a program the compiler cannot accept. Nothing silences it.
- A warning is code that looks suspicious and is often fine. A dev build prints it and carries on, so work in progress is not blocked. A release build accepts none, so every warning there is fixed or allowed.
- A safety check guards against a class of bug the compiler can prove absent, such as a data race. It stops every build, dev included, so the bug is found when it is written. Where the programmer knows better than the checker, `unchecked` overrules it, with a reason, at the smallest place it applies.
- A run-time check tests at run time what the compiler cannot know, such as an index. Dev builds carry them, and a failure stops the program with the file, the line and the values.

`allow` and `unchecked` look alike, a word and a reason where they apply. They mean different things, so each has its own word.

### Warnings

- Every warning has a stable name, printed at the end of its message: `` `e` shadows the outer `e` [shadowed-catch] ``. The names are listed in one place in the documentation, each with its meaning and its fix.
- A release build accepts no warning. In a release build every warning is an error, always, with no option to turn that off. A warning is fixed, or silenced with `allow`. A dev build prints warnings and carries on. `antic --warnings-as-errors` gives the release behaviour in a dev build, and `anti check` uses it.
- `allow(name, "reason")` silences one warning, and the reason is required. It applies to what it belongs to, at three levels:
  - a statement, when it stands directly before that statement;
  - a function, class or struct, when it stands in that declaration's header, last, after `-> R` and `may fail` or after `inherits`, before the brace;
  - the whole file, when it stands at the top of the module and ends with `;`.
- Silencing more than one warning takes one `allow` clause for each, with its own reason. A long header wraps onto indented lines before the brace.
- `allow` is not part of a signature. The library file, the C header and `anti doc` leave it out. Two functions that differ only in their `allow` have the same type.
- An `allow` that silences nothing is a warning, `[unused-allow]`, so a stale one does not outlive the code it was for.
- Only warnings can be named. An error is never silenced, and `allow` naming one is refused.

```anti
fn parse_all(lines: []str) may fail
	allow(shadowed-catch, "handlers reuse e on purpose")
{
	...
}
```

### Safety checks

`unchecked` works as `allow` does, for safety checks instead of warnings.

- Every safety check has a stable name, printed at the end of its message: `` `head` is not guarded, atomic or fixed [unguarded-field] ``. The names are listed in the documentation with the warnings, each with its meaning and its fix. The first two are `unguarded-field` and `exponential-pattern`.
- `unchecked(name, "reason")` overrules one check, and both the name and the reason are required. It applies at the levels of `allow`. It stands before a statement, last in a declaration's header, or at the top of the file ending with `;`. It also applies after a field's type, for that field alone.
- An `unchecked` that overrules nothing is a warning, `[unused-unchecked]`.
- Only safety checks can be named. An error is never overruled, and `unchecked` naming one is refused.
- `unchecked` is not part of a signature, as `allow` is not.

## Open questions

None. Every question raised for this round is settled above.
