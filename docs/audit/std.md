# The standard library

This step audited `src/std/` at `69fc7bee`: the 35 `.anti` files, with
`LICENSE` and `.gitkeep`. The session read 26 files line by line:
`collection.anti`, the five sequences, `lang`, `text`, `mem`, `fs`, `io`,
`license`, `json`, `time`, `reflect`, `simd`, `os`, `error`, `runtime`,
`random`, `toml`, `plugin`, `debug`, `config`, `log` and `args`. Three
reviewer agents read the other nine line by line: `map`, `set`, `sorted`,
`synchronized`, `concurrent`, `pool`, `tree`, `trace` and `regex`. Every
finding of theirs that stands below was read again in the code at its lines,
and the C side in `src/rt/` or `src/antic/` where a finding names one. What
could not be settled by reading stands under "Not confirmed" and is counted
nowhere. The modules were held against "Collections" and "Standard library"
of `docs/anti-syntax-overview.md`, `docs/site/standard-library/index.md`,
`docs/notes/collections.md` and `docs/notes/concurrent-classes.md`. No build
ran and no suite ran.

The C guidelines apply where they carry over. A dev build checks every index
of an array, a slice and a `str` and traps on signed overflow. A release
build checks neither, and no build checks an index through a raw pointer. So
rules 4, 5 and 6 hold for library code as they hold for C.

Each finding names its standard, its rule or question, and its severity. It
says whether `docs/audit/2026-09-23/` or `docs/audit/2026-09-30/` reported
it. The first audit has no finding in `src/std/`.

## Counts

| Severity | Standard | Findings | New | Returned | Still open |
|---|---|---|---|---|---|
| Severe | Safety, rules 4, 5, 6, 12, 14 and 23 | 6 | 6 | 0 | 0 |
| Severe | Documentation | 1 | 1 | 0 | 0 |
| Major | Safety, rules 4, 11, 12 and 23 | 4 | 4 | 0 | 0 |
| Major | Structure | 2 | 2 | 0 | 0 |
| Major | Documentation | 4 | 4 | 0 | 0 |
| Minor | Safety, rules 4, 10, 14 and 26 | 5 | 5 | 0 | 0 |
| Minor | Structure | 11 | 8 | 0 | 3 |
| Minor | Documentation | 3 | 3 | 0 | 0 |

In all: 36 findings, 33 new and 3 still open. None returned. The second
audit had 21 findings here. `docs/audit/2026-09-30/closure.md` records 16 as
fixed and two as no defect, and the places were read again in this pass. The
three left for a decision of Eddie's stand below as still open.

## Severe

### A logger file that names an unknown kind of sink recurses without end

`src/std/anti/log.anti:404`, safety, rule 14, severe, new.

`made` reads the registered kinds with `Log.get().kinds.load()` for every
kind other than `stderr` and `file`. It runs inside `construct` of `Log`,
through `from_configuration` and `first_sink`. `lower_singleton_get` of
`src/antic/lower_class.c:192` stores the instance only after `construct`
returns. The inner `Log.get()` therefore finds no instance, builds a second
`Log` and reads the file again, and so does every level below it. A file with
`kind = "syslog"`, or a misspelt `stderr`, ends the program by exhausting the
stack. Lines 376 and 377 promise "one line to standard error and the entry is
left out".

`register` at line 378 calls `Log.get()` as well, which reads the file before
the kind is linked into the list. A registered kind is therefore never found
by the file in either order of the calls. No test names a kind other than
`file`, and no test calls `log.register`. The fix is to keep the kinds
outside the object that `construct` fills.

### Three trace handlers write plain tables from every thread

`src/std/anti/trace.anti:230`, safety, rules 23 and 6, severe, new.

`src/rt/hooks.c` calls the installed handler on whichever thread reaches a
hook, with no lock. The header of the module says "a handler counts with
atomic fields" at line 28. `Profiler`, `ErrorMonitor` and `ChangeJournal` do
not.

- `Profiler.slot` at line 230 tests `self.count >= PROFILE_SLOTS`, writes
  `self.names[at]` and adds one to `count`, all on plain fields. Two threads
  that enter at `count == 63` both pass the test and leave `count` at 65.
  `find` at line 216 then reads `names[64]`, outside the array, in a release
  build. `self.runs[at] += 1` at line 262 loses counts, against "The counts
  stay right" at line 135.
- `ErrorMonitor.failed` at line 445 writes `code`, the two words of `name`
  and the bytes of `message` with no guard, while `report` reads them.
- `ChangeJournal.changed` at line 652 writes the two words of a `str` slot
  that `class_of` and `field_of` read with no order between them.

`LeakTracker`, `CallLogger` and `ThreadMonitor` count with atomic fields.
The fix is one rule for all seven: atomic counters, and a `Mutex` around
every table of more than one word.

### The five place functions are public and check no index

`src/std/anti/collection.anti:761`, safety, rule 6, severe, new.

`docs/anti-object-model.md:140` makes every `abstract fn` and `concrete fn`
public. `first_place`, `next_place`, `lend_place`, `copy_place` and
`take_place` of `Collection<T>` are therefore functions of every collection
that a program may call. `Indexed.lend_place`, `copy_place` and `take_place`
at lines 761 to 802 index `items` through a raw pointer with the `at` they
are given, which no build checks. `Slots` does the same at `pool.anti:276`
and `281`, and `slot_element` at `pool.anti:571` indexes the raw `blocks`.
`tests/std/collection_pool.anti:271` calls `hs.copy_place(6)` from a
program.

`take_place` leaves the collection wrong as well. Its comment at line 142
says "`count` still counts the element, and the caller takes it off". On a
free slot of a `Pool`, `take_place` at `pool.anti:513` runs `release` again
and links the slot into the free list twice, so two later `add` calls share
one slot. The functions serve the base alone. A way to keep them out of the
public table, or a check of `at` in each, would close it.

### The size of a room is a product that nothing checks

`src/std/anti/collection.anti:714`, safety, rule 5, severe, new.

`room_from` asks for `n * size_of(T)` bytes, and `n` comes from the
`capacity` a caller gives `new` or `reserve`. A release build wraps where a
dev build traps. The other places:

- `grid.anti:46`, `let cells = made.width * made.height;`. A release build
  makes `Grid<int>.new(1 << 32, 1 << 32, 0)` a grid of no cells that keeps
  both sizes. `get(3, 3)` at line 77 then passes its test and `copy_place`
  reads outside the room.
- `map.anti:297` to `299`, where `n * size_of(E)`, `8 * n` and
  `16 * buckets` add up to the block of an `EntryTable`, and `map.anti:612`
  for a `SlotTable`.
- `sorted.anti:719`, `n * NODE_MAX * size_of(T)`, and line 731.
- `pool.anti:451`, the bytes of a block, with the shift of `prepare` at
  line 122 that never ends for a capacity above 2^62.
- `concurrent.anti:924`, `Snapshot.grow`, through `room_from`.

`ArenaAllocator.grow` at `mem.anti:160` tests `size > LARGEST - start`
before it adds, so the library already holds the check in one place. A
capacity that large comes from a fault of the caller or from a file. One
checked product in `room_from`, and the same test at the four blocks of
mixed layout, would refuse it.

### `args` writes an option into room it did not get

`src/std/anti/args.anti:273`, safety, rule 4, severe, new.

`reserve` at line 317 gives up without a word when `alloc` gives nothing:
`let room = alloc(Option, size) else { return; };`. `declare` then writes
`self.options[self.count] = Option {` at line 274 whatever `reserve` did.
The slice is as long as before, so a release build writes past it, or
through a `ptr` of `none` at the first declaration. `keep` at line 292
returns before its write and drops the argument instead. Memory has to run
out for either, which is why the second major finding of "Structure" below
asks for one answer.

### `dup` of a `Snapshot` frees its copies twice

`src/std/anti/collection/concurrent.anti:878`, safety, rule 12, severe,
new.

`Snapshot<T>` holds its room in `items: ?*T`, which is not `own`. Its
`destruct` at line 934 tears down each copy and gives the room back. The
class replaces no `copy`. `dup` of a value copies a plain pointer as an
address, as "Ownership and copies" of `docs/anti-object-model.md` says, so
`dup(m.keys())` leaves two snapshots that both free one room.
`docs/decisions.md:2537` says "Every collection replaces `copy`", and the
reason of line 2435 names this fault for the sequences. `=` is refused,
since the class has a `destruct`. No test copies a snapshot, and the finding
was read from the two documents and not run.

The same shape, with the same cause:

- `mem.anti:91`, `ArenaAllocator`. Its comment at line 89 says "A copy would
  give the blocks back twice", and nothing refuses the `dup`.
- `log.anti:59`, `FileSink`, whose `destruct` closes a `file` that a copy
  would close again.
- `collection.anti:59` and `pool.anti:66`, `transient made` of `NotOne` and
  `Changed`. `dup` writes `none` into the copy, whose `message` still points
  at the bytes the original frees. The comment "A copy made with `dup`
  shares them" holds only while the original lives.

### The header of `anti.log` describes a file the code does not read

`src/std/anti/log.anti:15`, documentation, a file whose documented behaviour
differs from the code, severe, new.

The header says the file "holds `level`, `format` and `[[sink]]` entries
with `kind`, `path` and `level`". `first_sink` at line 450 reads `kind` and
`path` of an entry and no `level`. It returns the sink of the first entry it
can make and never reads the others, so a file with two sinks writes to one.
It stops at the eighth entry through a bare `while i < 8`. `from_configuration`
reads a `name` at line 441 that the header leaves out. A user who writes
what the header describes gets one sink and no level per sink, with no
message. A `file` sink that will not open writes two lines, the second of
them "no sink of kind file" from line 471, which is wrong.

## Major

### `reserve` and `shrink` move the room and count no change

`src/std/anti/collection/list.anti:189`, safety, a use after free that the
check of a walk exists to stop, major, new.

An iterator holds the room it walks: `IndexedWalk` keeps `items` at
`collection.anti:914`, and the walks of the maps keep `entries` and
`hashes`. `resized` is what a dev build compares before every turn of a
`for`. `reserve` and `shrink` replace the room through `resize` or `rebuild`
and never call `resized`. A loop that calls `l.shrink()` on the list it
walks then reads freed room in a dev build too, with no trap. The places:
`list.anti:189` and `197`, `deque.anti:95` and `103`, `map.anti:80`, `88`,
`382` and `394`, `set.anti:483` and `492`, `sorted.anti:211` and `224`. The
entry of `docs/decisions.md` on `Changes` counts "every change of its size",
which a move of the room is not. The fix is to count a change wherever the
room moves.

### A text taken from a builder that got no memory is a literal

`src/std/anti/text.anti:200`, safety, rules 4 and 11, major, new.

`take` and `take_in` tell the caller to give the text back with
`free(s.ptr)` or `from.free(s.ptr)`. When no memory came,
`anti_rt_builder_take` of `src/rt/text.c:147` gives the literal `""`, and
`take_in` says so at line 200: "the text is the empty literal". A caller
that follows the comment then frees a literal. `SinkLogger.new` does it
inside the library: `made.held = kept.ptr;` at `log.anti:197` puts what
`take` gave into an `own` field, which `delete` of the logger frees. Every
default logger takes that path with an empty name. An empty text that the
caller can tell apart, a `ptr` of `none`, would keep the one rule.

### Memory that runs out is answered five ways

`src/std/anti/fs.anti:170`, structure, one way to report an error to the
user, major, new.

- The collections stop the program: `catch fatal` on every room, as
  `collection.anti:844`.
- `fs.read_file` at `fs.anti:170` and `json.unquote` at `json.anti:92` fail
  with `lang.Error.new(1, "out of memory")`.
- `text.Builder` drops the bytes and goes on, through `reserve` of
  `src/rt/text.c`. The closure of the second audit left that as a question
  for Eddie.
- `args.Parser` drops the option or the argument, at `args.anti:292` and
  `317`, and `StackTrace.capture` and `symbolize` give an empty trace, at
  `lang.anti:134` and `180`.
- `take` gives a literal, as the finding above records.

A reader cannot say what a call of the library does when memory runs out
without reading its body. Only `Copies.new`, `Ring.new`,
`PriorityQueue.new`, `Grid.new` and `SpscRing.new` say it in their comment.
`List.new`, `Deque.new`, `Pool.new`, `Tree.new` and every `add` and `push`
stop the program and do not say so. One rule, written once in the header of
`anti.mem`, would settle the five.

### `ConcurrentMap` stamps the key after the change, `SyncMap` before it

`src/std/anti/collection/concurrent.anti:352`, structure, duplicated logic
that has drifted, major, new.

`lend_entry` of `map.anti:1402` puts a changed key back only after the
function it lent to returns. `SyncMap.update_all` at
`synchronized.anti:726` therefore copies the key first:
`let k = dup(e.0); change(e); self.stamp(k);`. `docs/decisions.md:2523`
gives the reason and the rule, "one rule for both maps". `ConcurrentMap`
does not follow it: `change(e); changed(part, e.0);` at
`concurrent.anti:352`, and the same in `update_in` at line 850. A `change`
that rewrites the key stamps a key the part does not hold. The version of
the real entry stays, and a later `set_if_version` succeeds on an entry that
changed. No test rewrites a key in either map. The version store itself is
written three times, as the minor finding on duplication lists.

### State that threads reach is guarded in part

`src/std/anti/random.anti:89`, safety, rule 23, major, new.

- `SharedRandom` takes its `Mutex` in `next` at `random.anti:146`. It
  inherits the public `reseed` of `Random` at line 89, which writes the four
  words of state with no lock. `SharedRandom.get().reseed(1)` on one thread
  races every draw of another.
- `FileConfig.read` at `config.anti:74` swaps the document and deletes the
  old one. `has` and `text_of` at lines 82 and 88 load the pointer and then
  call through it. A thread between the two reads a document that another
  thread deleted. The field is `atomic`, which guards the pointer and not
  what it points at.
- `Logger` has the plain public fields `name`, `level` and `format` at
  `log.anti:124`. `SinkLogger.log` reads all three on every thread, and the
  class comment invites a program to write them. `name` is two words.

### `install` loses the handlers past the eighth

`src/std/anti/trace.anti:873`, safety, rule 12, major, new.

`handlers_of` calls `c.add(held); c.add(one);` and `several.add(one);` and
reads no result. `Composite.add` at line 676 gives `false` once its
`COMPOSITE_SLOTS`, 8, are taken. A `trace` value of nine names builds the
ninth handler, which nothing owns and nothing deletes, and writes no line.
The comment of `install` names no limit.

### The growth of a `Map` is not what its DESIGN comment and the decision say

`src/std/anti/collection/map.anti:47`, documentation, a DESIGN comment
whose statement the code no longer keeps, major, new.

The comment says the room is rebuilt "in place when a quarter of it holds
removed entries and twice as large otherwise", and `docs/decisions.md:2460`
says the same. `make_room` at line 272 calls
`self.grown(self.room_size, self.count + 1)` in the other case. With one
removed entry `count + 1` is at most `room_size`, so `grown` gives the same
size back. A full `Map` or `Set` that removes one entry and adds one then
rebuilds its whole room on every `add`. The room doubles only when no entry
was ever removed. "In place" is not what `rebuild` does either: it takes a
new block at line 300. `SlotTable.put` at line 443 doubles by its count of
live entries and has no such case.

### Public items without a `///` comment

`src/std/anti/log.anti:40`, documentation, a public item without a comment
that says what it takes and gives, major, new.

`docs/site/standard-library/index.md:21` says each module has "a `///`
comment on every `pub` item". These have none:

- `log.anti:40`, `abstract fn write(self, line: str);` of `Sink`, which
  every sink of a program fills.
- `reflect.anti:70` to `74` and `80` to `82`, the five fields of `Field` and
  the three of `Function`.
- `pool.anti:538` to `543`, the six fields of `SlotsWalk`, and
  `queue.anti:257` to `259`, the three of `QueueWalk`.
- `log.anti:368` to `370`, `mem.anti:197` and `198` and `lang.anti:421`,
  the public fields of the private classes `Kind` and `Cell` and of `Fatal`.

The walks are the worse case. Their fields are public so that another class
can write the literal, and `next` at `pool.anti:549` indexes with them. A
program that writes `used` or `at` of a `SlotsWalk` reads outside the
blocks. `IndexedWalk` keeps its fields private behind `over`.

### Comments that leave out the owner, the failure or the lifetime

`src/std/anti/os.anti:138`, documentation, a public function whose comment
does not say what it gives and refuses, with rule 11, major, new.

- `os.anti:138`, `151` and `164`: the three user directories give a text
  from `Builder.take`, and no comment says the caller frees it.
  `tests/std/userdirs.anti` frees none of the texts it takes.
- `lang.anti:124` and `debug.anti:10`: `capture` and `backtrace` give a new
  `StackTrace`, and neither says the caller deletes it.
- `lang.anti:231`: `Error.new` keeps `message` without a copy and does not
  say the text must outlive the error. `args.anti:249` and `258` rely on
  it: the message of a `parse` error points into the parser, which the
  private `say` at line 184 says and the comment of `parse` does not.
- `error.anti:24`: the message of a `SystemError` is storage of the C
  library that stays valid "until the next call", by `src/rt/std.h:153`.
  `plugin.anti:103` says "as the message of a `SystemError` does", and
  `SystemError` itself says nothing.
- `regex.anti:654`, `676`, `700`, `763` and `806`: five `bytes_` forms are
  `may fail` and name no failure, where each `str` form names
  `TooExpensive`. `regex.anti:828` to `879`: a group of a `ByteMatch` is
  found by the search run again over the live bytes, and
  `src/rt/patterns.c:521` stops the program when it finds the match no
  longer. `patch` exists to change those bytes, and no comment warns.
- `regex.anti:114` and `133`: `compile` and `compile_bytes` do not say that
  the handle is never given back, while `src/rt/regex.h:31` tells the caller
  to free it with a function no Anti code can call.
- `map.anti:792` and `1135` and `sorted.anti:1023`: `to_text` of the three
  maps does not say who frees the text. The base says it at
  `collection.anti:287`.
- `json.anti:85`: `unquote` says it "fails with code 0" and also fails with
  code 1 when memory runs out, at line 92.
- `reflect.anti:137` and `151`: `get` and `set` do not say that `d` has to
  describe the class of `object` or a base of it. Another descriptor reads
  the object at offsets of another class.

### Three notes describe a handler design the code does not have

`docs/notes/trace-handlers.md:22`, documentation, a notes file that
describes a design the code no longer has, major, new.

- `trace-handlers.md:22` says "A handler counts with atomic fields". Three
  of the seven do not, as the second severe finding records.
- `trace-handlers.md:70` says `start` "reads the `trace` key with
  `anti_rt_conf_get`". `trace.anti:832` calls `runtime.get("trace")`, since
  `30b82c77` of the second audit moved the binding into `anti.runtime`.
- `docs/notes/hooks.md:105` says the `trace` key "names one of them". The
  key names one of six, `Composite` not among them, or a list of them
  separated by commas, as `trace.anti:887` reads it.
- `docs/notes/concurrent-classes.md:80` says "The format is version 57".
  `CLAUDE.md` gives 81.

`docs/notes/collections.md` and the rest of `concurrent-classes.md` hold
against the files, sentence by sentence. Every test either note names
exists.

## Minor

### Dead code

`src/std/anti/collection/map.anti:71`, safety, rule 26, minor, new.

- `entry_hash` is abstract at `map.anti:71` and `373` and filled at
  `map.anti:780` and `1124` and `set.anti:139` and `334`. Nothing in
  `src/std/` or `tests/` calls it.
- `set.anti:24` declares `anti_rt_element_text` and never calls it.
- `trace.anti:385`, `message_len` of `ErrorMonitor`, is written at line 461
  and never read.
- `regex.anti:966`, `stop_into`, takes `into` and never reads it. Its
  message comes from `anti_rt_regex_stop_template` with the text `into`, so
  it speaks of a template where there is none.
- `log.anti:252`: the `else` of `current` stands "for a logger that `set`
  replaced with `none`", and `set` at line 273 takes a `*Logger`, which
  never holds `none`. `self.sink == none` at line 205 tests an
  `own sink: *Sink` the same way.
- `collection.anti:390`, `moved`, has no caller in the library. Two tests
  call it, and `docs/decisions.md` names it as the helper of `reserve`.

### Duplicated code

`src/std/anti/args.anti:284`, safety, rule 26, minor, new.

- `args.anti:284` to `299` and `305` to `324`: `keep` and `reserve` grow a
  slice by the same lines, one of them inline.
- `regex.anti:120` and `139` build a `BadPattern` twice, and lines 393 and
  731 a `MissingGroup`, beside the helpers `too_expensive`,
  `length_mismatch` and `missing_into`.
- `toml.anti:101` and `114`: `read` and `open` differ in `alloc` alone.
- The version store of a thread-safe map stands three times: `stamp` and
  `forgets` at `synchronized.anti:860` and `850`, again at lines 1205 and
  1195, and `changed` and `forgets` at `concurrent.anti:837` and `856`.
  `pool.anti:406` calls the same act `bump`.
- `Snapshot.grow` at `concurrent.anti:918` repeats the growth of
  `Indexed<T>` with a literal 8 where `collection.anti:27` has
  `FIRST_CAPACITY`. The second audit named it inside its finding on
  `Snapshot`.

### A walk is built three ways

`src/std/anti/collection/queue.anti:75`, structure, one way of doing each
thing, minor, new.

`IndexedWalk` has private fields and `pub fn over` at `collection.anti:922`.
The six walks of `map.anti` and the four of `sorted.anti` follow it.
`QueueWalk` and `SlotsWalk` have public fields and are built by a literal,
at `queue.anti:75` and `pool.anti:257`. `Snapshot` is built by `new` and
`add`. The twelve `over` functions are `pub` and called from their own file
alone.

### A lent element whose key changes is handled three ways

`src/std/anti/collection/set.anti:144`, structure, one way of doing each
thing, minor, new.

`Map` and `HashMap` lend the entry and put the key back, at the cost of a
`dup` and an `==` per lend, in `lend_entry` of `map.anti:1402`. `Set` and
`HashSet` lend a copy and drop the change, at `set.anti:144` and `339`.
`SortedMap` and `SortedSet` lend in place and stop the program when the
order broke, through `check_order` at `sorted.anti:1011`. The headers of
`set.anti` and `map.anti` each give their own rule. No document says why
the three families differ.

### Helpers of the library are `pub`

`src/std/anti/collection.anti:575`, structure, a name whose reach is wider
than its use, minor, new.

`text_keys`, `open_entries`, `close_entries`, `write_entry`,
`write_entry_texts` and `write_entry_text` at `collection.anti:575` to `631`
serve the maps alone. `int_value` and `bool_value` at `toml.anti:126` and
`135` serve `anti.config`. Each is part of the public interface of a module
a program imports. `Stamp`, `block_align`, `room_from` and `sort_stable` of
the same file are `internal`, which `edc892e6` of the second audit chose for
the helpers of `map.anti`.

### `destruct` is declared two ways

`src/std/anti/lang.anti:192`, structure, one way of doing each thing, minor,
new.

Five classes write `concrete fn destruct(self)`: `lang.anti:192` and `317`,
`collection.anti:76`, `trace.anti:809` and `pool.anti:83`. Fourteen write
`fn destruct(self)`, the form of "Classes" in the overview. Both compile.

### Functions that never return are not declared `-> never`

`src/std/anti/io.anti:36`, structure, one way of doing each thing, minor,
new.

`io.exit` calls `anti_rt_exit`, which is `-> never` at line 7, and has no
result type itself. `Error.fatal` of `lang.anti:304` is `-> never`. The
`stop` functions of `regex.anti:48` to `76` are `_Noreturn` in
`src/rt/patterns.c:846` and declared without it, and `patch_fixed` at line
949 relies on their not returning.

### A number that has one definition is spelled again

`src/std/anti/lang.anti:126`, structure, each concern in one place, minor,
new.

- `lang.anti:126`: `let addresses: [64]u64 = [0; 64];` beside
  `TRACE_DEPTH` of line 30, which the call of the next line passes as the
  room. Raising the constant alone lets the walk write past the array.
- `concurrent.anti:637`: `(h >> 60) as int` is 64 less the four bits of
  `PARTS`, 16, of line 31. `part` at line 641 checks no index.
- `sorted.anti:1175`: `anti_rt_type_arg(self as ?*byte, 3, 0)` where
  `map.anti:38` has `TYPE_DEPTH`.
- `set.anti:385`: `marks[i] > 1` where `map.anti` writes `> REMOVED`.
- `log.anti:453`: the eight entries `first_sink` reads have no name.

### `anti.regex` holds two functions that use no pattern

`src/std/anti/regex.anti:982`, structure, one purpose per module, minor,
new.

`patch_bytes` and `patch_bytes_fixed` search a sequence of bytes through
`anti_rt_bytes_patch`. A program that patches by a sequence alone imports
`anti.regex` for them and links PCRE2. The closure of the second audit ruled
on their names, not on their place.

### `args` takes two forms its header does not list

`src/std/anti/args.anti:251`, safety, rule 14, minor, new.

`--flag=value` on a flag sets it and drops the value, with no error.
`-n=value` is read as `-n value`, and the header at lines 7 to 9 lists
three forms without it. `-5` as a positional argument fails as an unknown
option. The codes 1 and 2 of `parse` are bare numbers, where `anti.reflect`
and `anti.plugin` name theirs.

### The walk order of a `HashMap` falls back to one fixed order

`src/std/anti/collection/map.anti:475`, safety, rule 4, minor, new.

`shuffle` calls `anti_rt_entropy` and reads no result. When the system gives
no random bytes, `drawn` stays 0 and `walk_seed` is 1 on every run. The
DESIGN comment above it promises an order no program can come to depend on.
`src/rt/hash.c` handles the same failure for the seed of the hashes.

### `os.variable` reads a `str` as a C string

`src/std/anti/os.anti:53`, safety, rule 10, minor, new.

`getenv(name.ptr)` passes the bytes without their length. The function is
private and every caller passes a literal, so it holds today. A `str` that
is a slice of a longer text would name another variable.

### `Changed` and `Snapshot` stand in sibling modules

`src/std/anti/collection/synchronized.anti:29`, structure, placement, minor,
still open.

The second audit reported it, and the closure left it for Eddie.
`concurrent.anti:17` imports `anti.collection.pool` for `Changed` alone, and
`synchronized.anti:29` imports `anti.collection.concurrent` for `Snapshot`
alone. The header of `pool.anti` names no `Changed`, and the header of
`concurrent.anti` no `Snapshot`.

### Size and presence each have three names

`src/std/anti/text.anti:174`, structure, names, minor, still open.

The second audit reported it at line 141, and the closure left it for Eddie.
`Builder.len()`, `Document.len()`, `Parser.len()` and `Profiler.len()`
stand beside `count` and `count()`. `has` and `given` stand beside
`contains`.

### `args.text_of` and `args.at` give `""` for what is not there

`src/std/anti/args.anti:104`, structure, names, minor, still open.

The second audit reported it, and the closure left it for Eddie. The comment
of `text_of` says "or its fallback" and does not say that an undeclared name
gives the empty text.

### `src/std/.gitkeep` stands beside 36 files

`src/std/.gitkeep`, structure, something left of a step that is over,
minor, new.

The empty file kept the directory in `6fb5ffd4`, before it held a module.

### Sentences in headers and comments that the code contradicts

`src/std/anti/random.anti:5`, documentation, a stale statement in a
comment, minor, new.

- `random.anti:5`: xoshiro256** "needs eight words of state". The class
  holds four, `s0` to `s3`.
- `fs.anti:5`: "the error is a `SystemError`". `read_file` also fails with
  a plain `Error` at line 170. `fs.anti:42`: "Both modes are binary", of
  the three of `Mode`.
- `lang.anti:4`: the header names `Object` and `Job` as what the compiler
  declares. It declares `Flags`, `Mutex`, `FieldDescriptor`, `Regex` and
  `Match` there as well. `lang.anti:16`: "returns `?*T` or `bool`" predates
  `?T`, which every collection gives.
- `pool.anti:44`: a handle has no `==`, and the reason given is
  `since the == of this module is that of Pool<T>`. Other modules declare
  more than one `operator fn eq`, so the reason no longer holds. `pool.anti:45`: "It never finds the wrong
  element" holds inside one pool. A handle of another pool with the same
  slot and generation finds that slot's element.
- `tree.anti:29`: the forms "are those of `pool.Pool<T>`". `pool.Slots<T>`
  declares them.
- `trace.anti:828`: with no key "the call then installs nothing".
  `install` at line 841 stores `none`, which removes a handler that was
  installed.
- `error.anti:54` and `synchronized.anti:510` each give a defect of antic as
  their reason. Whether either still stands needs a build.

### The site page and the overview against the modules

`docs/site/standard-library/index.md:27`, documentation, a document that
describes the files by an older state, minor, new.

- Line 27 of the site page says `anti.runtime` "holds `configure`". The
  module holds `get` as well, at `runtime.anti:43`.
- The table of line 17 leaves out `from_bytes` of `anti.text` and
  `read_file`, `write_file` and `FileSystem` of `anti.fs`.
- Line 21 lists the failing functions beyond `anti.text` and `anti.fs` as
  six. `toml.Document.open`, `config.read`, the three directories of
  `anti.os`, `plugin.load`, `Library.unload` and the functions of
  `anti.regex` fail as well. `CLAUDE.md` carries the same list.
- `docs/anti-syntax-overview.md:1615` says of the thread-safe collections
  "every one has versioned `set`". Line 1630 of the same page says the
  versioned `set` of `SpscRing` is not built.

### Two banned phrases in comments

`src/std/anti/collection/queue.anti:17`, documentation, rule 27, minor,
new.

`queue.anti:17` writes `a number of steps logarithmic in the count`, and
`sorted.anti:482` ends a list with `and so on up the tree`. Both phrases
stand in `tools/docs-style/banned-terms.md`.

## The tool pass in this area

`docs/audit/tool-pass.md` reported the eleven lines that `list.anti:189` and
`deque.anti:95` share: `reserve`, `shrink`, `iter` and `copy`. It stands,
still open, and is not counted again. The one line of
`data/back-end-mentions.txt` in `src/std/`, `simd.anti:3`, uses `select` as
the name of a function and is no finding. `data/unnamed-files.txt` lists 28
files of `src/std/` that no document names by path. `docs/audit/structure.md`
judged the directory and found that "State" of `CLAUDE.md` lists every
module, which holds.

## Not confirmed

These came out of the reading and need a build or a run. None is counted.

- The recursion of `anti.log` and the double free of a copied `Snapshot`
  were read from the code and the specification and not run.
- Whether `dup` of a `str` key copies its bytes. `Map.set` says it takes "a
  copy of the key", and a key built with `f"..."` that the caller frees
  would then dangle.
- Whether a function lent an element may grow the same collection, which
  would leave the lent pointer in freed room.
- Whether `anti_rt_regex_patch` can match differently in its second pass
  than in its count, for a pattern with a lookbehind.
- Whether `--memory-checks` reports the handle of `Regex.compile` and the
  `Kind` nodes of `log.register` as leaks.

## The sentence of each file

Each line gives the purpose as the code shows it. A mark follows where the
`//!` header says something else.

| File | Its sentence |
|---|---|
| `LICENSE` | The 0BSD text of the library. |
| `.gitkeep` | Nothing. A leftover, as the finding above says. |
| `anti/lang.anti` | The root that imports nothing: the types the compiler knows by name, `Error` with its text, the trace of a stack, the hook handler and the hash seed. The header names `Object` and `Job` alone. |
| `anti/io.anti` | Console output and the exit of a program. |
| `anti/text.anti` | Functions on `str`, and `Builder`, which collects bytes and formats values. |
| `anti/license.anti` | The licence notice of the program. |
| `anti/error.anti` | `SystemError`, `on_fatal` and `check`, beside the `Error` of `anti.lang`. |
| `anti/time.anti` | `Duration`, the two clocks, and the interface `Clock` with its default. |
| `anti/os.anti` | Signals, and the three per-user directories of the platform. Two subjects under one title. |
| `anti/fs.anti` | Files by handle and by whole file, and the interface `FileSystem` with its default. The header lists no `read_file` and no `write_file`. |
| `anti/reflect.anti` | Reading the descriptor of a class: fields, functions, `get`, `set`, `new`, `call` and `Value`. |
| `anti/random.anti` | The generator `Random`, the interface `Source` and the shared default. The header gives eight words of state for four. |
| `anti/collection.anti` | The interfaces of walking, the base `Collection<T>` and `Indexed<T>`, `Copies<T>`, the hashes, the writers of a map entry and the stable sort. The header names the first three alone. |
| `anti/toml.anti` | Reading the TOML subset into a flat `Document`. |
| `anti/config.anti` | The settings of the program: the interface `Config` and `FileConfig`. |
| `anti/args.anti` | A command line declared once, parsed and read by name. |
| `anti/json.anti` | Writing and reading the members of the JSON that `serialize` writes. |
| `anti/log.anti` | Levelled logging: sinks, the interface `Logger`, the logger of the program and the file that configures it. The header describes a file the code does not read. |
| `anti/debug.anti` | `backtrace`, the short form of `StackTrace.capture`. |
| `anti/mem.anti` | The interface `Allocator`, its two implementations, and `Shared<T>`. |
| `anti/runtime.anti` | `configure` and `get` of the runtime configuration. |
| `anti/simd.anti` | A header alone: the three mask functions the compiler knows by name. |
| `anti/trace.anti` | The seven stock handlers of the hooks, `install`, `start` and their tests. The header claims atomic counts for all seven and names no `install`. |
| `anti/plugin.anti` | Loading a shared library and asking it for the class it provides. |
| `anti/regex.anti` | The run-time half of patterns: the errors, `compile`, the iterators, every function the pattern methods call, and `patch`. The first line of the header names the first two alone. |
| `anti/collection/list.anti` | `List<T>`, with `sort`, `sort_by` and `map`. |
| `anti/collection/deque.anti` | `Deque<T>`, a ring that grows at both ends. |
| `anti/collection/ring.anti` | `Ring<T, N>`, a ring of fixed room. |
| `anti/collection/grid.anti` | `Grid<T>`, a rectangle of cells that never changes its size. |
| `anti/collection/queue.anti` | `PriorityQueue<T>`, a binary heap, and its walk. The header opens with "Ordered retrieval" and no class. |
| `anti/collection/map.anti` | `Map` and `HashMap`, their two tables for any element, their walks and the sort of serialized texts. The header names no sort. |
| `anti/collection/set.anti` | `Set` and `HashSet` on the tables of the maps, and `BitSet`. |
| `anti/collection/sorted.anti` | `SortedMap` and `SortedSet` over one B-tree in one block. |
| `anti/collection/pool.anti` | `Slots<T>`, `Handle<T>`, `Pool<T>`, the failure `Changed` and the walk. The header names no `Changed`. |
| `anti/collection/tree.anti` | `Tree<T>`, a hierarchy on the slots of the pool. |
| `anti/collection/synchronized.anti` | Four synchronized classes around a plain collection each, with a version per element. The header names no `sort`, `sort_by` and `map`. |
| `anti/collection/concurrent.anti` | `ConcurrentMap`, `SpscRing` and the walk `Snapshot<T>`. The header names no `Snapshot`. |
