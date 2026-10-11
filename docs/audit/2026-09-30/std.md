# The standard library

This report audits the structure of `src/std/`, the Anti code of the
standard library, at `238a4f1`. It reads the code against
`docs/anti-language-additions.md`, `docs/anti-object-model.md` and
`docs/decisions.md`, not against the C guidelines. Every module was read
where the questions of the audit reach it: the purpose of each module,
what the collections of `anti.collection` share through the base and
what they copy, whether the synchronized and concurrent collections wrap
the plain ones, and one style of names and signatures across modules,
`_all`, `_first` and `_one`, `may fail`, `?T` and `own` among them. The
blocks that `docs/audit/data/duplication.txt` lists in `src/std/` were
each read, and so were copies that the tool does not see because their
names differ. Where a copy has drifted, the finding says how, from the
code and, for the hash of a map, from `src/antic/lower_hash.c`, which
writes the default hash. Nothing was built and no test ran.

The first audit, in `docs/audit/2026-09-23/`, audited the C code alone
and has no finding in `src/std/`, so every finding here is new. The tool
pass of this audit, `docs/audit/tool-pass.md`, listed the place
functions of the sequences as a minor finding. That finding is part of
the second major finding below, which grades it by the copies that have
drifted since.

The criteria forms keep one style across every collection. `_all` gives
the count, `_first` a `?T` or a `bool` and `_one` fails with
`collection.NotOne`. Every function of a collection that changes its
size takes `at: lang.SourceLocation = here` last, as the decisions ask.
`SyncList`, `SyncMap`, `SyncSet` and `SyncPool` hold a plain `List`,
`Map`, `Set` or `Pool` and call it, and `ConcurrentMap` holds a
`HashMap` per part. No finding stands against those.

## Counts

| Severity | Rule or question | Findings |
|---|---|---|
| Severe | | 0 |
| Major | One purpose per module | 1 |
| Major | Duplicated code that has drifted | 3 |
| Major | One layout rule | 1 |
| Major | One way of reading a file | 1 |
| Minor | Duplicated code | 5 |
| Minor | One layout rule | 1 |
| Minor | A module doing the work of another | 2 |
| Minor | Placement | 2 |
| Minor | Names and signatures | 5 |

All 21 findings are new.

## Severe

None.

## Major

### `anti.collection` still holds the list and maps over objects

`src/std/anti/collection.anti:708`, one purpose per module, major, new.

The header of the module names two purposes: the base of the generic
collections, and "a list and two maps over objects from before
generics". `List`, `Map` and `IntMap` at lines 708, 803 and 900 take the
names of `anti.collection.list.List<T>` and `anti.collection.map.Map<K,
V>` one level up. The entry of `docs/decisions.md` that kept them, "stay
as they are until the collections of round five are built", no longer
holds, since round five is built. `tests/std/collection.anti` is their
only user. `Map` and `IntMap` repeat `set`, `get` and `grow` line for
line with another key. The copies have drifted from the rest of the
library: `hash_text` at line 997 says it is "what the runtime hashes
with", but `anti_rt_hash_bytes` of `src/rt/hash.c` now mixes eight bytes
at a time through the finalizer of `src/rt/hash.h`. Neither map mixes in
the seed of `lang.hash_seed()`, which every other hashing collection
does. `hash_int` at line 1009 is the SplitMix64 step of
`random.anti:117`. The decision names the step: remove the three
classes and their test once Eddie confirms it.

### The sequences repeat their places, walks, copies and teardowns

`src/std/anti/collection.anti:600`, duplicated code that has drifted,
major, new.

`List`, `Copies`, `Deque`, `Ring`, `Grid` and `PriorityQueue` each write
`first_place` and `next_place` with the same body, at `list.anti:236`,
`collection.anti:584`, `deque.anti:149`, `ring.anti:106`,
`grid.anti:190` and `queue.anti:87`. The walk classes repeat as well:
`ListWalk` at `list.anti:333`, `CopiesWalk` at `collection.anti:681`
and `GridWalk` at `grid.anti:266` are one class under three names, and
`DequeWalk` at `deque.anti:269` and `RingWalk` at `ring.anti:184` are
another. `take_place` of `Deque` and of `Ring` differ in `% N` alone.
`copy` and `destruct` repeat in `List`, `Copies`, `Deque` and
`PriorityQueue`. `Copies<T>` is a `List<T>` less most of its operations,
since `anti.collection` cannot import `anti.collection.list`.

Two of the copies have drifted. `Copies.lend_place` at
`collection.anti:602` returns without calling `f` when the room is
missing, where `List.lend_place` at `list.anti:254` stops the program
with `catch fatal`. `new(capacity)` of `Copies` at
`collection.anti:571` and of `PriorityQueue` at `queue.anti:37` go on
empty when the allocator has run out, where `List.new` and `Deque.new`
stop through `resize`. The decision on `List` says a list stops "as
`Copies<T>` does", which holds for `add` and not for `new`. A base of the
collections indexed from 0 would hold the places, the walk, the copy and
the teardown once, with the index of a slot as its one function.

### The key type of a map is read two ways, one of them a copy of a C struct

`src/std/anti/collection/map.anti:57`, one layout rule, major, new.

`serialize` of a map writes a JSON object when the key is `str`, so each
map reads the type id of its key from the record `anti_rt_type_arg`
gives. `map.anti` declares `struct Record` at line 57 as a copy of
`struct anti_field` of `src/rt/object.h`, and its own `TYPE_STR` at line
50 as a copy of `ANTI_TYPE_STR`. `sorted.anti` reads the same record as
the `FieldDescriptor` that the compiler declares in `anti.lang`, at line
187, and compares it with `reflect.TypeId.Str`. For that one constant
`sorted.anti:15` imports `anti.reflect`, which the DESIGN comment of
`map.anti:44` avoids because it links the descriptors of `anti.reflect`
into every program with a map. A program with a `SortedMap` now links
them, and a program with a `Map` does not. `Record` is the one of the
three that nothing ties to the C struct. A change of `struct anti_field`
reaches `FieldDescriptor` through the compiler and leaves `Record`
behind, and `tests/std/map.anti` then shows it as a map of `str` keys
written as an array of pairs.

The writer of the entries repeats with it. `to_text` of `Map` at
`map.anti:819`, of `HashMap` at `1164` and of `SortedMap` at
`sorted.anti:1063` write `{"Ann": 41}` three times. `serialize` of
`Map` at `map.anti:842` uses `open_entries` and `write_entry`, and
`SortedMap` at `sorted.anti:1087` writes the same bytes through
`bracket`. The reading of the key type and the writer of an entry
belong once in `anti.collection`, over `FieldDescriptor`.

### `ConcurrentMap` hashes its entries by a formula of its own

`src/std/anti/collection/concurrent.anti:630`, duplicated code that has
drifted, major, new.

`hash_entries` sums `(k.hash(), value_hash(&v)).hash()` over the
entries. `Map`, `HashMap` and `SortedMap` hash through
`collection.hash_in_any_order<(K, V)>` at `map.anti:902`, which sums
`(k, v).hash()`, the default hash of the tuple. The two differ:
`hash_fields` of `lower_hash.c` combines the hash of each part, and the
hash of a `u64` part is the finalizer of its value. The first form
therefore mixes the hash of the key once more. A `ConcurrentMap` hashes
unlike a `HashMap` that holds the same entries, while `SyncMap` at
`synchronized.anti:898` hashes like its `Map`. "Thread-safe collections"
in `docs/anti-language-additions.md` gives a thread-safe collection the
hash "as those of the plain collections do". `std/collection_hash.anti`
compares two maps of one kind and so does not see it. The fix is to sum
the tuple's own hash, as `hash_in_any_order` does.

### `json.unquote` reads JSON strings unlike the runtime and unlike its own writer

`src/std/anti/json.anti:146`, duplicated code that has drifted, major,
new.

The runtime reads a JSON string in `anti_rt_json_string` of
`src/rt/json.c:74`, and `json.write_text` at line 31 is a copy of
`put_text` of `src/rt/object.c:360`. `unquote` is a second reader and has
drifted from both. For an escape it does not know, it writes the letter
after the backslash, so `\u0008` becomes `u0008`. `write_text` writes
every control byte other than a newline, a return and a tab as `\u00XX`,
so a `str` that holds such a byte does not come back from
`write_text` then `unquote`. The runtime decodes `\u` into UTF-8 and
refuses an unknown escape and a raw control byte. The doc comment says
"a `\u` escape is left as it stands", which the code does not do either.
`member` at line 100, `skip_space`, `end_of_text` and `end_of_value`
scan JSON a second time beside `src/rt/json.c`. A binding of the
runtime's reader would leave one reader.

### `anti.log` opens, reads and writes files without `anti.fs`

`src/std/anti/log.anti:476`, one way of reading a file, major, new.

`log.anti` declares `fopen`, `fread`, `fwrite`, `fseek`, `ftell`,
`fflush` and `fclose` at lines 29 to 37 and does not import `anti.fs`.
`read_file` at line 476 is a second `fs.read_file`, and `FileSink` at
line 69 a second `fs.open` and `fs.write`. The copies have drifted from
`anti.fs`: `fopen(path.ptr, ...)` at lines 80 and 478 reads the path as
a C string, where `fs.open` passes `path.len` to `anti_rt_fs_open`, so a
path that is a slice of a longer `str` names the wrong file. `read_file`
checks no `ferror` after `fread`, which `fs.read` does, and
`text.from_c(room)` at line 496 cuts the text at the first NUL byte.
`anti.fs` depends on `anti.error` and `anti.lang` alone, so `anti.log`
can import it without a cycle.

## Minor

### The matching loops of the base are private and written again

`src/std/anti/collection.anti:453`, duplicated code, minor, new.

`accepts`, `first_match` and `one_match` of `Collection<T>` are private,
so a collection that replaces a criteria form writes the loop again.
`Grid.holds` at `grid.anti:245` is `accepts`, and `Grid.remove_one` at
`grid.anti:137` counts as `one_match` does. `Slots.update_one` at
`pool.anti:251` counts once more, and `SpscRing` writes `first_match`
and `one_match` at `concurrent.anti:1330` and `1346`. None has drifted
in its result. Protected helpers would hold the count of `NotOne` once.

### `SpscRing` repeats every operation of a collection

`src/std/anti/collection/concurrent.anti:1081`, duplicated code, minor,
new.

The synchronized collections and `ConcurrentMap` wrap a plain collection.
`SpscRing` writes the nine criteria forms and `clear` itself, from line
1081 to 1230, over positions from `taken` to `given`. Its storage has a
reason of its own, two counters that the checker cannot follow, and its
removal moves the elements towards the newest by design, the opposite
of `Ring`. The loops that test the elements are copies of those of the
base. Its `to_text` and `serialize` at lines 1246 and 1255 collect with
`find_all` and match `SyncList` at `synchronized.anti:327`, the block
`duplication.txt` lists.

### The hash of `SyncList` is `hash_in_order` written again

`src/std/anti/collection/synchronized.anti:413`, duplicated code,
minor, new.

`hash_items` repeats `collection.hash_in_order` over the first part of
each pair, since the list holds `(T, int)`. The decision on the
synchronized collections requires the hash to equal that of a `List`
with the same elements, and `std_sync_equal` checks it, so the copy is
pinned. A change to `hash_in_order` has to reach it by hand.

### The base writes text by comparing each place with `first_place()`

`src/std/anti/collection.anti:299`, duplicated code, minor, new.

`to_text` and `serialize` of `Collection<T>` decide the separator with
`place != self.first_place()`, a new call per element, at lines 299 and
318. `first_place` of `SlotTable` draws a new random start on each call,
so `HashSet` writes its own `to_text` at `set.anti:350`, as its comment
says, and `HashMap` at `map.anti:1164`. `first_place` of a B-tree walks
down from the root on each call. A flag for the first element would let
both keep the base's text.

### `toml.Document` and `config.Config` read integers and booleans twice

`src/std/anti/config.anti:32`, duplicated code, minor, new.

`Config.int_of` and `bool_of` at lines 32 and 44 repeat
`Document.int_of` and `bool_of` of `toml.anti:73` and `85`: the same
`text.parse_int`, the same `"true"` and `"false"`. `FileConfig` holds a
`toml.Document`. The two agree today.

### The alignment of a room is written five times

`src/std/anti/collection/map.anti:1595`, one layout rule, minor, new.

The largest power of two that divides the size of an element is
`element_align` at `collection.anti:546` and at `sorted.anti:174`,
`room_align` at `concurrent.anti:1415`, `block_align` at `pool.anti:638`
and `block_align` at `map.anti:1595`. The last floors it at a literal 8
where `pool.anti` takes `size_of(int)`, which agree on every target.
`Snapshot.grow` and `ring_room` of `concurrent.anti` repeat `room` of
the base for classes outside it.

### `anti.json`, `anti.os` and `anti.regex` do what `anti.text` exists to do

`src/std/anti/json.anti:59`, a module doing the work of another, minor,
new.

`json.write_int` writes decimal digits by hand beside
`text.Builder.append_int`, and `anti.log` calls it at `log.anti:452` and
`461` to build the keys of a TOML document, which is no JSON.
`os.owned` at `os.anti:64` copies the bytes of a builder into new
memory, which `Builder.take` does. `anti.regex` keeps a builder of its
own, `Growing` at `regex.anti:278`, with `anti_rt_regex_append` and
`anti_rt_regex_take` in the runtime. `lang.put_int` and
`trace.eprint_int` have reasons of their own: `anti.lang` imports
nothing, and a hook of `anti.trace` makes no object.

### `anti.log` and `anti.trace` read the runtime configuration past `anti.runtime`

`src/std/anti/log.anti:28`, a module doing the work of another, minor,
new.

`anti.runtime` is the module of "the runtime of the program, as far as a
program reaches it", and it offers `configure` alone. `log.anti:28` and
`trace.anti:39` each declare `anti_rt_conf_get` and read the keys
`logger` and `trace` through it. A `get` of `anti.runtime` would keep
the binding in one place.

### Shared parts of the collections stand in sibling modules

`src/std/anti/collection/synchronized.anti:29`, placement, minor, new.

`Changed`, the failure of every versioned set, stands in
`anti.collection.pool`, and `concurrent.anti:17` imports the pool for it
alone. `Snapshot<T>`, the walk of both thread-safe modules, stands in
`anti.collection.concurrent`, and `synchronized.anti:29` imports it
from there. `struct Stamp` is declared at `synchronized.anti:45` and at
`concurrent.anti:57`, each with the same comment on the fault of antic
it works around. `Snapshot` grows its room as `Copies` does. The entry of
`docs/decisions.md` on `anti.collection` puts "the parts every
collection shares" there. The decisions on `Changed` and `Snapshot` are
provisional and name their modules.

### `anti.collection.map` exports the helpers of `anti.collection.set`

`src/std/anti/collection/map.anti:1398`, placement, minor, new.

`Stride`, `stride_of`, `stride_at` and `advance`, the walk of a table of
slots, and `kind_of`, `numbers`, `text_at`, `sort_by_text`,
`sort_by_key` and `TYPE_DEPTH` are `pub` so that `set.anti` can reach
them. They are part of the public interface of the module a program
imports for `Map`. `sort_by_key` at line 1510 is a heap sort whose
`sift` repeats `sift_down` of `queue.anti:205`, beside the stable sort of
`list.anti:415`. `internal` would keep them inside the package.

### Size and presence each have three names

`src/std/anti/text.anti:141`, names and signatures, minor, new.

The collections give their size as the field `count`, and the
thread-safe ones as `count()`, which a decision explains.
`text.Builder.len()`, `toml.Document.len()` at `toml.anti:32` and
`args.Parser.len()` at `args.anti:123` call it `len`. A collection asks
for a key with `contains`, `Config` and `Document` with `has` at
`config.anti:25` and `toml.anti:56`, and `args.Parser` with `given` at
`args.anti:97`.

### "Not present" is answered four ways

`src/std/anti/args.anti:104`, names and signatures, minor, new.

`anti.lang` says a function whose only failure is "not present" returns
`?*T` or `bool`, and the collections give `?T`. `args.text_of` and
`args.at` at lines 104 and 129 give `""`. `json.member` at `json.anti:100`
gives a `bool` and writes the text through an `out: *str`. `reflect.get`
at `reflect.anti:137` gives a `Value` of kind `None`. Each predates `?T`
of any type.

### `new` gives a value in some modules and a pointer in others

`src/std/anti/log.anti:57`, names and signatures, minor, new.

`StderrSink.new`, `FileSink.new`, `CallbackSink.new` and
`SinkLogger.new` of `anti.log` give a pointer. `text.Builder.new`,
`mem.ArenaAllocator.new`, `random.Random.new`, `args.Parser.new` and
every collection give a value. `toml.Document` has both, `read` for a
value at `toml.anti:110` and `open` for a pointer at `123`, and its
`open` takes the text of a document where `fs.open` takes a path.

### The set operations take the other set three ways

`src/std/anti/collection/sorted.anti:1306`, names and signatures, minor,
new.

`union`, `intersect`, `minus` and `is_subset` take `other` by value in
`Set`, `HashSet` and `BitSet`, at `set.anti:81`, `275` and `577`, as the
decision on sets says. `SortedSet` takes `o: *SortedSet<T>`, a pointer
under another name, at lines 1306, 1329, 1350 and 1371. `SyncSet` takes
`own other: set.Set<T>` at `synchronized.anti:1033`, for the reason its
decision gives.

### The byte forms of `anti.regex` put `bytes` first and last

`src/std/anti/regex.anti:980`, names and signatures, minor, new.

The functions of a `ByteRegex` start with `bytes_`, from
`bytes_matches` at line 652 on, as the header says. `patch_bytes` and
`patch_bytes_fixed` at lines 980 and 993 put the word last, and `patch`
at line 904 takes a `ByteRegex` with no mark.
