# Provisional entries for review

`docs/decisions.md` holds 188 `[provisional]` entries at commit `50aebbe`, all added since the review of 2026-09-22 and of the `anti.mem` entries. This report sorts them and changes none.

- Review, a choice a user could observe that a reasonable person could have made differently: 83.
- Settled, a built detail a user could observe that agrees with the specifications: 88.
- Internal, seen by a compiler contributor alone: 17.
- Stale, made so by a later step: 4, marked in their lists.

Each entry names its line in `docs/decisions.md` at that commit and the section it stands in, and quotes the full text. An entry that fitted two lists went into Review.

## Stale entries

- S24, line 811. The third sentence is stale. Plugins are built, and the entry on `plugin:<path>` and `discover` under "Plugins" (line 882) has the runtime fill such a slot before `main`. `src/rt/conf.c` calls `anti_rt_plugin_provider` for it.
- S26, line 815. The last sentence is stale. Plugins are built, and discovery reads the `plugins` key (line 884).
- S41, line 857. Stale. `--closed` is built: `src/antic/main.c` takes it, and the entry under "Plugins" (line 880) gives what it turns off.
- B8, line 879. The sentence "A program that loads nothing links no loader, no digest and no reader of an index" is stale. The entry under "Versions" (line 903) says the runtime archive puts the loader in every program, because the reader of the configuration file calls it. `src/rt/start.c` reaches `anti_rt_plugin_provider` through `anti_rt_conf_start`.

| Section | Review | Settled | Internal |
|---|---|---|---|
| Compiler behaviour | 1 | 0 | 0 |
| Tests and fixtures | 0 | 1 | 0 |
| Wrapping and saturating operators and `Flags` | 7 | 2 | 1 |
| Sum types | 7 | 6 | 0 |
| Locking and channels | 11 | 5 | 0 |
| Simd structs | 6 | 8 | 0 |
| Runtime configuration | 4 | 4 | 0 |
| Hooks and tracing | 10 | 8 | 3 |
| Injection | 2 | 7 | 2 |
| Standard interfaces | 7 | 0 | 0 |
| Plugins | 8 | 7 | 3 |
| Versions | 2 | 6 | 3 |
| The check command | 3 | 7 | 0 |
| The build command | 3 | 2 | 2 |
| The symbols command | 2 | 5 | 0 |
| The doc command | 3 | 5 | 0 |
| The formatter | 2 | 6 | 0 |
| The bind command | 5 | 9 | 1 |
| Repository layout | 0 | 0 | 2 |

## Review

**R1**, line 441, "Compiler behaviour".

> [provisional] antic alone never takes a framework from a library file. A program built with antic and not with `anti` names its frameworks with `--framework`. Reason: the entry under "SDK and frameworks" in `docs/anti-language-additions.md` has `anti` pass the names, and the entry above says `anti build` passes what a binding declares.

**R2**, line 739, "Wrapping and saturating operators and `Flags`".

> [provisional] `<<%` gives 0 for a count at or above the width. A negative count counts as one above it, as the dev-mode check of `<<` compares a count. A constant count outside the width is no error. Reason: the specification gives `<<%` the modulo result in every build and no trap, and the value times 2 to such a count is 0 modulo 2^N.

**R3**, line 740, "Wrapping and saturating operators and `Flags`".

> [provisional] `mul_high` is a name the checker knows, as it knows `Object`, and a function or a local of that name in scope wins over it. The upper half is signed for a signed type and unsigned for an unsigned one. On `c_long`, `c_ulong` or `c_wchar` it is no constant expression. Reason: the specification lists `mul_high` among the built-ins and not among the keywords, and the upper half of a target-sized product has another value at each width.

**R4**, line 741, "Wrapping and saturating operators and `Flags`".

> [provisional] `zero` and `negative` read the wrapped result, `negative` its top bit. `+` sets `carry` to the carry out and `-` to the borrow, and both set `overflow` when the signed result leaves the range. Unary `-` is `0 - a`. `*` sets `carry` when the unsigned product does not fit and `overflow` when the signed one does not. `<<` and `>>` set `carry` to the last bit shifted out, which is false for a count of 0. `<<` sets `overflow` when the signed value does not fit, and `>>` never does. Reason: the four fields are the four flags both processors keep, and each takes the meaning that x86_64 gives it where an instruction defines it: `add`, `sub`, `neg`, `mul` and `imul`, and `shl` and `shr` for one bit.

**R5**, line 744, "Wrapping and saturating operators and `Flags`".

> [provisional] The carry into `+` and the borrow into `-` is the `carry` field of any Flags value, behind a pointer as well. `x + f.carry` after any other operand adds 0 and the carry. A plain one keeps the dev-mode check of a signed `+` or `-`, over the whole operation. `Flags` has no field `borrow`, and `a - b - f.carry` takes the borrow. Reason: the specification calls the borrow the `carry` field read the way subtraction uses it, and names four fields.

**R6**, line 745, "Wrapping and saturating operators and `Flags`".

> [provisional] `Flags` is a struct of `anti.lang` that the compiler declares, as it declares `Object`, and a type named `Flags` in the module wins over it. It has no descriptor, as a tuple has none. The C header writes it once as `struct anti_Flags` of four `bool`, before the first exported signature or struct that names it. Reason: the specification makes `Flags` a built-in struct that C sees as such, and no module declares it.

**R7**, line 746, "Wrapping and saturating operators and `Flags`".

> [provisional] The flags name assigns a local or a parameter of type `Flags` in any enclosing block. A name of another type in an enclosing block makes a new variable, and one in the same block is refused as declared already. `(result, flags) = e;` assigns two names and takes the flags form alone. Reason: the specification gives the flags name these two forms and the plain form two existing names.

**R8**, line 748, "Wrapping and saturating operators and `Flags`".

> [provisional] The wrapping and saturating operators have no compound assignment. Reason: the keywords of the specification add their tokens and no `+%=`.

**R9**, line 756, "Sum types".

> [provisional] A field of a case is a name and a type, with no default, no bitfield width and no modifier. Reason: the specification gives a case a name and fields, and a struct field has no default either.

**R10**, line 757, "Sum types".

> [provisional] The header writes `enum Shape_tag` before the typedef of the variant, with the value `Shape_Circle = 0` for each case. The field `tag` keeps its integer type. Reason: the specification names an enum and a `uint8_t` field, an enum of C is as wide as an int, and the enumerators of C share one namespace.

**R11**, line 759, "Sum types".

> [provisional] The name an arm binds holds a copy of the fields of its case, in that arm alone. The arm may change the copy and the variant. `Circle =>` binds nothing, and a name on a case without fields is refused. Reason: a variant is a value type, and a copy is what a `let` of a value gives.

**R12**, line 760, "Sum types".

> [provisional] `if let Circle c = s { } else { }` is a `switch` with one arm and an `else`, which is empty without one. `else if` continues the chain, and `if let Empty = s` binds nothing. The missing-return rule reads it as an `if` with an `else`. The access sentence of the specification names `if let` beside `switch`. Reason: the syntax overview writes `if let` over one case, and the specification names `switch` as the one access. A switch of one arm keeps the two one rule.

**R13**, line 761, "Sum types".

> [provisional] `switch`, `if let` and `is` take a variant value, and a pointer `p` is written `*p`. `p.tag` reads through the pointer as a field does. Reason: the specification writes all three on a value.

**R14**, line 763, "Sum types".

> [provisional] A literal of a variant is no constant expression, as a literal of a union is none. A constant, a field default and a parameter default of variant type are therefore refused. Reason: the smallest option, since a constant of a union has no form yet.

**R15**, line 764, "Sum types".

> [provisional] A variant has no descriptor. A field of variant type has the type id `None`, so `serialize` and `reflect.get` pass over it. Reason: a union has no descriptor either, and the type ids name no variant.

**R16**, line 770, "Locking and channels".

> [provisional] `Mutex` is a struct of `anti.lang` that the compiler declares, as it declares `Flags`, with one field that holds the handle of the mutex the runtime made. A type named `Mutex` in the module wins over it. A copy of a Mutex names the same mutex. Reason: the specification makes it a struct that wraps the platform's mutex, whose size differs per system, and a handle keeps the C layout and the IR free of sizes.

**R17**, line 771, "Locking and channels".

> [provisional] `m.destroy()` takes a Mutex in a place or a pointer to one, releases the mutex and clears the handle, so a second `destroy` releases nothing. A `sync` on a Mutex whose handle is clear stops the program with `` `sync` on a Mutex that holds no mutex ``. Reason: a handle that nothing wrote is zero, as in the memory of `alloc(T, n)`, and a message says more than a fault.

**R18**, line 774, "Locking and channels".

> [provisional] `recv(c)` gives `?*T`, a pointer to a slot of the frame of the function that holds the call, or `none` once the channel is closed and empty. Each `recv` has a slot of its own, which it fills again each time it runs. Reason: the specification writes `-> ?T`, and `?` stands before `*` and `fn` alone. A slot of the caller's frame needs no allocation and leaves the IR free of sizes.

**R19**, line 775, "Locking and channels".

> [provisional] `chan T` is a struct of `anti.lang` per element type with one field that holds the handle of the channel, and a copy names the same channel. `delete(c)` ends a channel and frees what the runtime holds for it, and a worker cannot `delete` one, as it cannot delete an object. Reason: the specification gives the channel no way to release it, and `delete` is the word that frees what the program made. A worker shares the channel with the others as it shares an object.

**R20**, line 776, "Locking and channels".

> [provisional] A `Mutex` and a `chan T` count as pointer-free for the rule of `parallel` and `dispatch`, so a worker takes them beside its values and a channel may carry a channel. Reason: both exist to be shared between threads, as `str` counts as pointer-free because it never changes.

**R21**, line 777, "Locking and channels".

> [provisional] `close(c)` is a name the checker knows, as it knows `mul_high`, and a function or a local of that name in scope wins over it. A second `close` changes nothing. Reason: the specification lists `close` among neither the keywords nor the reserved words.

**R22**, line 778, "Locking and channels".

> [provisional] The capacity of `chan T(n)` is an `int`, and a channel holds one value or more. A capacity below 1 stops the program with a message. Reason: the specification makes the channel a bounded queue and names no channel without room.

**R23**, line 779, "Locking and channels".

> [provisional] A `send` on a closed channel stops the program with `` `send` on a closed channel ``, a waiting one included. Reason: the value would never be received, and the specification gives `send` no error.

**R24**, line 780, "Locking and channels".

> [provisional] `select` is written `select { a x => stmt, b => stmt }`. Each arm names a channel and, before the arrow, the name that takes what the channel gives, which may be left out. The name holds what `recv` of that channel would give, a `?*T`, in that arm alone. A `select` has one arm or more and no `else`. Reason: the specification writes it like `switch` over the channels and names no non-blocking form, and an arm binds as an arm of a `switch` on a variant does.

**R25**, line 781, "Locking and channels".

> [provisional] `select` waits until one of its channels holds a value or is closed, and a closed and empty channel counts as ready. Among the channels that are ready it takes the first from a place that moves one arm further for each `select` of the thread. Reason: an arm on a closed channel gives `none` as `recv` does, and a place that moves keeps a channel that is always ready from hiding the others.

**R26**, line 782, "Locking and channels".

> [provisional] `send`, `recv`, `close` and `select` take a channel value, and a pointer `p` to one is written `*p`. Reason: the specification writes them on a value, as it does `switch` on a variant.

**R27**, line 792, "Simd structs".

> [provisional] The mask of a comparison has no spelling. It is a simd struct of `bool` with the field names of the type compared, one per type, and a program names it through the value a comparison gives. `simd.select`, `simd.any` and `simd.all` take any simd struct of `bool`, `select` with the lane count of its two values, so a declared one serves where the section allows it. Reason: the section names the mask by its lanes alone and gives no form that writes the type.

**R28**, line 793, "Simd structs".

> [provisional] The operators of a simd struct are the ones the section names: `+ - * /`, the bitwise `& | ^ << >>` and `~` on integer lanes, unary `-`, and the comparisons. `%`, the wrapping and saturating operators, `&&`, `||` and `!` are refused, and an `operator fn` whose first parameter is a simd struct is refused as well. Reason: the section lists the operators that apply element-wise, and an `operator fn` beside a built-in operator would never be called.

**R29**, line 794, "Simd structs".

> [provisional] `+`, `-` and `*` on integer lanes wrap in every build and have no dev-mode check. `/` and the shifts keep the rules of the scalar operator, the checks of a dev build among them. Reason: the dev-mode checks name the operators on integers, and a check per lane would take a vector apart. Neither a division nor a shift of lanes has an instruction on either architecture, so both are already one operation per lane.

**R30**, line 796, "Simd structs".

> [provisional] `v.sum()`, `v.min()`, `v.max()` and `a.dot(b)` fold the upper half of the lanes onto the lower half until one lane is left. The least keeps the upper lane where it is less than the lower one by `<`, and the greatest where it is greater. On `f16` lanes they compute in `f32`, and the result is the `f32` of the `f16` it rounds to. Reason: the section gives one result on every target, and a float sum depends on the order. Halving is the order that splits a wide vector at its native width, and the comparison of the pair is the one instruction both architectures have. `f16` is storage, so a value of it reads as an `f32`.

**R31**, line 798, "Simd structs".

> [provisional] A function of the module that the method syntax would call wins over a built-in of that name. A function named `close` wins over `close(c)` the same way. Reason: the built-ins take no form that a function of Anti could take, and a program keeps its own names.

**R32**, line 800, "Simd structs".

> [provisional] `anti.simd` declares nothing. The compiler knows `select`, `any` and `all` there by name. A module that calls one imports the module, as one that writes `here` imports `anti.lang`. `select` may follow `.` and stays a keyword everywhere else, as `alloc` and `free` do. Reason: no function of Anti takes a value of any simd struct, and the section puts the three in that module.

**R33**, line 808, "Runtime configuration".

> [provisional] `rt.configure(path)` is `configure` of the module `anti.runtime`, which a program imports as `import anti.runtime as rt;`. Reason: the specification writes the call with the prefix `rt`, and the entry under "Modules and packages" makes `anti.rt` the C runtime, which no compilation may define. A module a program imports under the name the specification writes is the smallest form that keeps both.

**R34**, line 809, "Runtime configuration".

> [provisional] The file the command line names wins over the file the environment names. Both win over the file `rt.configure` names, which the runtime then does not read at all. Reason: the precedence of the keys puts the command line above the file, and the program is the layer below both. One file layer holds the keys, so the file a higher layer named is the one that is read.

**R35**, line 812, "Runtime configuration".

> [provisional] A cycle of includes is found by the path each include resolved to. An include nested deeper than thirty-two files counts as one, and both name the path in the message. Reason: the specification makes a cycle a startup error naming the path. Two spellings of one file are one file to a reader and two paths to a comparison.

**R36**, line 813, "Runtime configuration".

> [provisional] `--anti.inspect` prints one line per key, `<key> = <value> (<layer>)`, where the layer is `command line`, the path of the file that set it, or `build`. A key no layer set prints an empty value, because the value of the build is the program as it was compiled and the runtime holds no reader of it. The version is the one antic wrote into the notice for the package `anti.rt`. Reason: the specification asks for the effective value of every key with the layer it came from, and the backtrace default reaches the program only where a `fail` pulls it in.

**R37**, line 825, "Hooks and tracing".

> [provisional] `created` fires after the `construct` bodies of the whole chain, once per object, and a `construct` that failed fires none. `destroyed` fires at the head of the teardown the compiler writes for the class, before the first `destruct` body. `copied` fires after the `dup` operator with the object it was made from. `dispatched` fires after the pool took the object, and `joined` after `join` has the result. Reason: one object gives one hook whatever its chain declares, a failed `construct` leaves no object behind, and every route into a teardown goes through the entry that the compiler wrote.

**R38**, line 828, "Hooks and tracing".

> [provisional] `--trace <pattern>` names the class itself, its module path or a package above it. It instruments that class in any mode, and it does not turn the code marked `trace` on. The bare `--trace` and the mode decide that. Reason: the specification gives the pattern the job of reaching code that did not ask, and the bare option the job of the mode.

**R39**, line 830, "Hooks and tracing".

> [provisional] `enter`, `leave` and `failed` stand in the body of the function, so the build of the module that holds the body decides them. A `--trace` on a program does not reach a function whose body came from a library file. `changed` stands at the write, so it follows the build where the write is written. Reason: a library file carries the IR of a body, which the program links as it was lowered. `hooks_modules_release` and `hooks_modules_dev` pin both halves.

**R40**, line 831, "Hooks and tracing".

> [provisional] `anti.lang.FieldDescriptor` is a built-in struct that the compiler declares, as it declares `Flags`, with the five fields of one record of a field list: `name`, `offset`, `type_id`, `owned` and `descriptor`. Reason: `changed` takes a pointer to a record, `anti.lang` imports nothing and cannot name `anti.reflect.Field`, and a class that replaces `changed` then names the type without an import.

**R41**, line 832, "Hooks and tracing".

> [provisional] The `failed` hook of the root takes `e: *Error` where the compilation carries `anti.lang`, and `e: *Object` where it does not. Reason: the compiler declares the root before it knows the module, and a program without `anti.lang` cannot name `Error` in a replacement either.

**R42**, line 834, "Hooks and tracing".

> [provisional] `anti.trace.start()` reads the `trace` key and installs the handler it names. The program calls it, as it calls `rt.configure(path)`, and a program that does not import `anti.trace` links none of the handlers. Reason: the specification says the key selects one by name at start, and gives no form that would install one without a call. Nothing runs between the runtime's own start and `main`, and the handlers are Anti code of a module that only an import pulls in. A call as the first statement of `main` is the smallest form that keeps the key in charge.

**R43**, line 835, "Hooks and tracing".

> [provisional] The names of the key are `leaks`, `profile`, `calls`, `errors`, `threads` and `writes`, for the six handlers in that order. Two or more separated by commas give a `Composite` of them, in the order they stand. A name that no handler has writes one line to standard error and is passed over, and the rest of the key still installs. Reason: the specification names the seven classes and leaves the spelling of the key open. `--anti.trace=leaks` already stands in `tests/conf`, so the names are short and lower case, and `Composite` is the handler that a list of names asks for. An unknown name is no reason to refuse a program, because the key is a debug aid and not a part of the program.

**R44**, line 836, "Hooks and tracing".

> [provisional] A handler allocates no object inside a hook and counts with atomic fields. The reports are written by the program and go to standard error. Reason: an `alloc` under `created` calls `created` again without end, and a hook runs wherever the object it stands for runs. A report that ran from a hook would count itself.

**R45**, line 838, "Hooks and tracing".

> [provisional] `Profiler` keeps one slot per function name, with the calls, the nanoseconds and the depth of that name. `enter` reads the clock where the depth is 0. `leave` adds the span where it falls back to 0, so a function that calls itself is measured once per outermost call. The spans of a function that two threads run at once overlap in the one slot. The counts stay right. Reason: the hooks carry the name and the object and no thread of their own. A stack per thread needs a thread-local that the language does not have yet.

**R46**, line 842, "Hooks and tracing".

> [provisional] The fixed sizes are `PROFILE_SLOTS` 64, `JOURNAL_SLOTS` 64, `MESSAGE_BYTES` 128 and `COMPOSITE_SLOTS` 8, each a `pub const` of the module. A `Profiler` counts the calls it had no slot for, and a `ChangeJournal` keeps the newest writes and drops the rest. Reason: a handler allocates nothing inside a hook, so every table is a field of the object and has a size at compile time. A program that needs another size reads the constants and knows what it gets.

**R47**, line 850, "Injection".

> [provisional] One slot serves every field of an interface, so `inject final` on any field of the program makes the slot final. Reason: the run-time configuration replaces the slot and not a field, and a replacement one field refuses is refused for all.

**R48**, line 852, "Injection".

> [provisional] The provider graph follows the direct calls alone. An edge from one interface to another stands where the provider of the first, or a function it calls, reads the slot of the second. A cycle is refused, and the message names the interfaces of it. Reason: the specification asks for a cycle through the providers that allocate, detected on the provider graph at link. A call through a table names no function in the IR. A graph that guessed at those would refuse a program that has no cycle.

**R49**, line 862, "Standard interfaces".

> [provisional] The default provider of an interface is a static function `default` of the interface itself, `fn default() -> *Interface`. The link takes it where the build's table names no provider for that interface, and the table overrides it where it does. An interface with neither is the error it was, which now names both ways to give one. Reason: the specification says that the six ship with a default provider, and that a library ships its own interface with one. The default therefore belongs to the interface and not to a table inside the compiler. A provider is already resolved by path, so `anti.log.Logger.default` needs no new form.

**R50**, line 863, "Standard interfaces".

> [provisional] `anti.log.Logger` is the interface and keeps the name, the level and the format. `log(level, message)` is its one abstract function, `trace` to `fatal` are bodies over it, and `SinkLogger` writes the line and owns the `Sink`. `Logger.default` gives the logger of the program, the one `log.info` writes through. Reason: `log.logger().level` and `log.set` are the module's interface to the logger, and both keep working where the three fields stand in the abstract class. A provider that gave a second logger would write somewhere else than the module functions do.

**R51**, line 864, "Standard interfaces".

> [provisional] `anti.time.Clock` has `now`, `wall` and `sleep` abstract and `since(reading)` as a body. `SystemClock`, a singleton over the three calls of the runtime, is the default. Reason: the module's three functions are the readings a program takes, and a class that takes a `*Clock` runs under a fake that holds them still.

**R52**, line 865, "Standard interfaces".

> [provisional] `anti.random.Source` has `next` abstract and `below`, `between`, `boolean` and `fraction` as bodies over it. `Random` gives the draw and is a `Source`. `SharedRandom`, a singleton that inherits `Random`, seeds itself from the wall clock and takes a mutex per draw, and it is the default. Reason: every derived draw is written once where it stands in the interface, so a fake counts out `next` alone. One generator of the program serves every thread, which is what a lock is for, and a program that wants one sequence again makes its own `Random` from a seed it holds.

**R53**, line 866, "Standard interfaces".

> [provisional] `anti.fs.FileSystem` works in whole files: `read_file`, `write_file`, `list`, `remove` and `rename`. `anti.fs` gains `read_file` and `write_file` as module functions, and `SystemFileSystem` is the default over them. Reason: `File` holds the C stream of a file, which a fake cannot make, so an interface over `open` and `read` could not be faked at all. A fake of whole files holds the bytes of each path, which is what the specification asks a fake for. A program that reads a file in pieces calls `open` and `read` of the module.

**R54**, line 867, "Standard interfaces".

> [provisional] `anti.config.Config` is the program's own settings, as keys with text values. `has` and `text_of` are abstract, `int_of` and `bool_of` are bodies over them, and `FileConfig` is the default, a singleton over one TOML file that `config.read(path)` names. A program that names no file answers every key with the fallback of the call. Reason: the `[runtime]` table of `--anti.conf` is the runtime's configuration and holds five keys of its own, so it is no place for the program's settings. `anti.toml` already reads the subset, and the key of a value is its path with a dot between the parts, as that reader gives it.

**R55**, line 868, "Standard interfaces".

> [provisional] `anti.toml.Document.open(source)` gives a `*Document` the caller deletes, and `anti.text.from_bytes(bytes)` gives the bytes as text. Reason: `Document.read` gives a value, which ends with the block that made it, and `FileConfig` holds its document in a field. The bytes of a file are a `[]byte` and the TOML reader takes a `str`.

**R56**, line 875, "Plugins".

> [provisional] The factory of an entry is the function that prepares an object, the one the registry holds. The loader allocates the size the class descriptor gives, calls it and moves the pointer to the interface sub-object. A class whose `construct` takes arguments, or that holds a class field no default fills, is refused at `instance` and not at the link. Reason: the registry already builds a class that way for `reflect.new`, and the flags it records answer the same question. The loader then writes no code of its own.

**R57**, line 878, "Plugins".

> [provisional] `load` checks two things before the version checks. The runtime version of the library must match the host's exactly. Every interface descriptor of its table must lie in the host's image. Reason: the specification asks for the version and for each interface against the host. A library that carried an interface of its own would give objects that no `is` of the program answers for. The image an address lies in is the one fact that tells the two apart.

**R58**, line 880, "Plugins".

> [provisional] The runtime is compiled without hidden visibility. A program that injects an interface or calls the loader makes every name of its assembly global and links with `-export_dynamic`. Reason: a plugin resolves the runtime, the descriptors and the standard library against its host. A symbol that is hidden or local is not there to resolve. `--closed` turns both off, and a program built with it takes no provider from a library: the link refuses a `plugin:` or `discover` entry, and the runtime finds no place to put an object. The rule of `docs/libraries-for-c.md`, that a shared library for C shows its export functions alone, is kept by the link instead: an `-exported_symbols_list` on macOS, `--exclude-libs ALL` on Linux and the `.def` file on Windows.

**R59**, line 883, "Plugins".

> [provisional] antic writes `anti-plugins.toml` beside a plugin. Each `[[library]]` entry holds a file name, the runtime version, the digest of the bytes and the interfaces that library provides. The entries of the other libraries of the directory stay as they stand. Reason: the specification gives the file to `anti build --lib shared` and `anti publish`, and neither is built. The build that writes the library knows what it provides.

**R60**, line 885, "Plugins".

> [provisional] A program loads a library where it is linked dynamically, which is a macOS host today. Reason: a Linux program of this runtime archive is static against musl on purpose and has no loader. A Windows DLL resolves nothing at load without an import library of its host. The test of a plugin therefore runs on a macOS host, and `--no-runtime` writes a library for every target.

**R61**, line 886, "Plugins".

> [provisional] `anti.plugin.Library` holds the handle of an open library. `load(path) -> *Library may fail` opens one and `live` counts its objects. `unload` closes it and fails with `IN_USE` while an object is alive. `instance_at` and `supports_at` are the two functions the compiler writes for `lib.instance(I)` and `lib.supports(I, "f")`. Reason: the two calls name an interface where a value stands, and no type of Anti carries one. The compiler puts the descriptor of the interface in the place of the name. `instance` then has the type `?*I`, which the `catch fatal` of the overview narrows.

**R62**, line 887, "Plugins".

> [provisional] `lib.supports(I, "f")` answers whether the class the library provides for the interface carries a public function of that name. Reason: the specification asks whether a slot is filled, and a complete class of Anti fills every slot of an interface it carries. The question a host has is whether the class knows the function at all, which the function list of its descriptor answers.

**R63**, line 890, "Plugins".

> [provisional] A program holds at most sixteen libraries open at once, and at most sixteen interfaces come from a library. Reason: the loader allocates nothing for its own table, and a program that needs more says so.

**R64**, line 900, "Versions".

> [provisional] `compatible <version>;` takes the source text of the number the lexer read, with any further `.<integer>` parts after it. `1.1.0` is no number of Anti. The comparison is part by part, and a part that is missing counts as zero. The line stands with the fields of the class body, before the functions. Reason: the specification writes `compatible 1.1;` without quotes, and a version is no value of the language. The source text is what the programmer wrote, and it is the one spelling a float's rounding cannot change.

**R65**, line 902, "Versions".

> [provisional] Every class descriptor carries the version of the build that wrote it, `--package-version`, and `0.0.0` where the build names none. The module that declares a class writes its descriptor. Every other module refers to the one it wrote. Reason: the specification says every class descriptor carries the version of the package that declared the class. A library file carries the descriptor of its module with the value of that build, so an importing module needs none of its own.

**R66**, line 913, "The check command".

> [provisional] The doc-warning class reports and fails nothing, so the front end, the doc blocks and the formatting decide the status. Reason: the specification calls every finding of that class a warning. A warning of the checker is not always a defect. A backtick holds the name of an environment variable, or of a key of a configuration file, as often as a name of the program. A `may fail` function that never fails is a warning for the reason the specification gives. The standard library of this checkout reports 36 of them, and none is a defect.

**R67**, line 914, "The check command".

> [provisional] A backtick name is one identifier, or a path of identifiers with an optional `()` at the end. Everything else between backticks is code and no name. The name resolves against eight tables. They are the items of the module and the names they declare, the parameters of the documented item, the imports and the module itself. The other four are the items of every loaded library, the members of those items, the names the compiler declares and the keywords. An import resolves by alias, by last segment, by first segment and by whole path. A path of two segments or more resolves through its first segment. Reason: the specification asks for a warning on an unresolved backtick name. The deeper lookup of a path needs the type tables of the checker, which the doc pass does not carry.

**R68**, line 915, "The check command".

> [provisional] Markup outside the subset is one warning per kind per doc comment. The kinds are a heading, a table, emphasis, a numbered list, an image and HTML. The lines of a fenced block hold what they like. A heading, a table and a numbered list are read at the start of a line. An image and a tag are read anywhere outside inline code. Emphasis is read where a delimiter has text after it and its partner has text before it. Reason: the specification names the forms the subset leaves out and no message for each. One warning per kind keeps a long comment from reporting one finding ten times. The flanking rule keeps `a * b` and a pointer type in prose out.

**R69**, line 936, "The build command".

> [provisional] A dependency of a path whose directory holds a manifest is a project. It is built for the host first, in the mode of the build that names it, and its `dist/` is read. The chain of such builds stops after eight. Reason: `docs/tooling.md` gives that shape and names no depth. A cycle of path dependencies then reports rather than running out of stack.

**R70**, line 938, "The build command".

> [provisional] `anti new` refuses a package name of one segment. Reason: antic warns on a module path of one segment, which is for a program's own files. The name of a package is the root of every module of it.

**R71**, line 940, "The build command".

> [provisional] The map holds one line per function, `<start>-<end> <name>` and the file and the line where the debug information gives them. It reads the program with the readers of `src/rt/symbols.c`, which gained `anti_elf_functions` and `anti_macho_functions` for the walk. Reason: those readers are what `anti.lang.StackTrace.symbolize` reads a running program with, so the map is what a trace of that program would name. A Windows program carries no symbol table, and the PDB of the archive holds its functions.

**R72**, line 946, "The symbols command".

> [provisional] The program of a deployment is every executable of Anti in the directory of the runtime configuration, a file with no suffix or with `.exe` whose header is an executable and whose notice carries a build id. A relative path of `plugins` and of `[injections]` is read against that directory. `--from` names the directory of the archives, which otherwise stand beside each binary. Reason: the configuration names no program, and `check` takes no `--from`, so the binaries have to be found from the file alone. The runtime reads the same paths against the directory a deployment runs its program from.

**R73**, line 951, "The symbols command".

> [provisional] `check` and `inventory` write one line per binary, `present`, `stale` or `missing`, then the path and the build id. `stale` is an archive of the same stem with another id. `check` exits with 1 when a binary is stale or missing, or when a file that the configuration names is not there. `inventory` folds what is present, reports the rest and exits with 0 once it wrote the archive. Reason: the symbols of a stale archive are not the symbols of the binary, so a rollout stops on it as it stops on a missing one. The specification gives the gate to `check`.

**R74**, line 961, "The doc command".

> [provisional] The fixed set of class names of the HTML is `doc`, `index`, `item`, `signature`, `fields`, `members`, `internals` and `code`. `doc` is the `<main>` of a page, `index` the list of modules, `item` the section of one item, `signature` its declaration, `fields` the fields, the values or the cases of a body, `members` the functions of a class body, `internals` the section a `//#` or a `//#!` text fills and `code` a fenced block. Reason: the entry under "Names and publication" asks for a fixed small set and names none of them. Eight is what the highlighters carry, and the stylesheet of the site then addresses these and nothing else.

**R75**, line 962, "The doc command".

> [provisional] A signature carries the visibility, the form word, the name, the name and type of every parameter, the result and `may fail`. It carries no default value, no `own` mark and no body. Reason: the library file keeps the first set for every function and the rest for some, and the two pages have to read alike. A value and an `own` mark are the next thing to add, once the file carries them for a function of a class body as well.

**R76**, line 967, "The doc command".

> [provisional] The command writes the interface file of every source into a work directory first, in the order the imports ask for, and reads them back through a search root of its own. `--work` names the directory and `build/doc-work` stands without one. Reason: a module of the project that imports another needs that module's interface file, and `antic -c` is what writes one. `anti check` writes the same files for the same reason, and `src/anti/units.c` holds the list both commands read.

**R77**, line 974, "The formatter".

> [provisional] The canonical form keeps the line breaks of the author and re-flows no expression. The rules move the breaks they name and no others. The brace of an item body moves onto a line of its own. The brace of a statement block moves onto the line of its statement. `} else {` and `} while cond` each come to one line. The second statement of a line goes onto its own. Reason: the addendum names the indent, the braces, the parentheses of a condition, one statement per line and the width of a doc comment. None of them names a width for code. A formatter that re-flowed an expression would need one.

**R78**, line 978, "The formatter".

> [provisional] A run of empty lines becomes one, and an empty line at the start or the end of a file goes. Reason: the rules name no separator between two items, so the empty lines of the author stand. One is what every file of the checkout writes.

**R79**, line 993, "The bind command".

> [provisional] The clang versions accepted are major 23, the major of the pinned clang. Reason: the pinned clang is the one the suite tests, and the JSON of the AST dump is no stable interface.

**R80**, line 996, "The bind command".

> [provisional] The module of a binding is `anti.<name>`, the file name without its extension and without `_api`, unless `--module` names another. The files go into `-o`, `.` without it, as `<last segment>.anti`, `shim_<last segment>.c` when the header has inline functions, and with `--probe` `probe_<last segment>.c` and `.anti`. Reason: the addendum names `anti.raylib` for `raylib_api.json` and `anti.miniaudio` for `miniaudio.h`, and no file names.

**R81**, line 999, "The bind command".

> [provisional] Every pointer of a binding is `?*T`, and `char *` and `void *` are `?*byte`. A pointer to a type Anti cannot name, a variadic function pointer and a `va_list` parameter are `?*byte`. Reason: C lets every pointer be NULL, and every pointer passes alike on the six targets, a `va_list` parameter included.

**R82**, line 1000, "The bind command".

> [provisional] A typedef stands for the type it names, and a record takes the name of its tag or of the typedef that names it. `intN_t` and `uintN_t` are `iN` and `uN`, `intptr_t`, `ptrdiff_t`, `ssize_t` and `intmax_t` are `i64`, and `uintptr_t` and `uintmax_t` are `u64`. Reason: Anti has no alias, and all six targets are 64-bit.

**R83**, line 1001, "The bind command".

> [provisional] A field or a parameter whose name is a word of Anti takes a trailing `_`. A function whose name is one is left out with a warning. A record, a function or a constant that names a type without an Anti form is left out with a warning. The command still succeeds. Reason: a field name changes no layout, and a function name is its symbol.

## Settled

**S1**, line 600, "Tests and fixtures".

> [provisional] `anti test` writes an object of every library file the module under test needs, the transitive imports included, and links them into the runner. `driver_libraries` of `src/antic/driver.c` gives the list. Reason: a dev build compiles one module into its own object and links the objects of the modules it imports. The tool passed the object of the module alone, so only a module that imported nothing could carry a `tests` block, and every module of the standard library imports something.

**S2**, line 742, "Wrapping and saturating operators and `Flags`".

> [provisional] The flags form never traps, the shift count included. A count at or above the width gives a result and flags that no rule fixes, as a plain shift in release does. Reason: the specification says no trap in any build.

**S3**, line 743, "Wrapping and saturating operators and `Flags`".

> [provisional] A `-` directly before a literal is refused in the flags form with `` a `-` before a literal forms a constant, which has no flags ``. Reason: the numeric rules make it one constant rather than an operation.

**S4**, line 753, "Sum types".

> [provisional] The tag numbers the cases from 0 in the order of the declaration. `v.tag` has an enum that messages call `Shape.tag` and that no program writes. Reason: the specification gives the tag as an enum and names no numbers, and C numbers an enumerator the same way.

**S5**, line 754, "Sum types".

> [provisional] A case without fields has no member in the union. A variant whose cases have no fields is its tag alone, without `u`. Reason: the C view of the specification leaves `Empty` out of the union, and C has no empty union.

**S6**, line 755, "Sum types".

> [provisional] A variant has one case or more, and `variant V { }` is refused. Reason: a variant of no case holds no value, so nothing could build one.

**S7**, line 758, "Sum types".

> [provisional] `packed` covers the structs of the cases and the union as well as the variant, and `align(N)` applies to the variant. The header writes `#pragma pack` around the typedef and `_Alignas` on the tag. Reason: `#pragma pack` in C covers the definitions inside it, and the header follows the struct rules.

**S8**, line 762, "Sum types".

> [provisional] The `tag` of a variant is read-only, and `v.u` is refused. Reason: the specification gives `v.tag` and no other access, and a tag written alone would name a case whose fields nothing wrote.

**S9**, line 765, "Sum types".

> [provisional] Another module names a case as `geo.Shape.Circle { }`, `geo.Shape.Empty` and `v is geo.Shape.Circle`. An arm names the case alone, as in the module that declares it. Reason: the specification gives the unqualified forms, and the module adds its name as it does for a struct.

**S10**, line 772, "Locking and channels".

> [provisional] `sync` takes a `Mutex` or a pointer to one and reads the handle once, before the block. Reason: the specification writes `sync m` on a name, and a mutex held by a class is reached through a pointer.

**S11**, line 773, "Locking and channels".

> [provisional] Two `sync` blocks of one function hold the same mutex when their operands name one place. That is the same variable, or the same path of fields from it, with `&` and `*` taken off. The message names both operands as written. Reason: that is the sameness one function can prove. Two places that hold copies of one handle deadlock at run time, as the specification says of the case it does not refuse.

**S12**, line 783, "Locking and channels".

> [provisional] A field of type `Mutex` or `chan T` has the type id none, and neither type has a descriptor, so `serialize` and `reflect.get` pass over it. Reason: no module declares either type, and a handle has nothing to write.

**S13**, line 784, "Locking and channels".

> [provisional] An export item refuses both types, since C cannot represent them. Reason: the header has no declaration of the objects the runtime makes, and the specification gives C no view of them.

**S14**, line 785, "Locking and channels".

> [provisional] A `sync` counts for the missing-return rule when the last statement of its block returns. Reason: its block runs once, as a plain block does.

**S15**, line 790, "Simd structs".

> [provisional] A lane is `bool`, `char`, an integer of a fixed width, `f16`, `f32` or `f64`. `c_long`, `c_ulong` and `c_wchar` are refused. Reason: the section gives the size of a simd struct as a multiple of eight bytes up to the cap, which is a property of the declaration, and the target decides the width of those three.

**S16**, line 791, "Simd structs".

> [provisional] A `simd struct` takes no `packed`, no `align(N)` and no bitfield. Reason: the section gives the alignment as the size or sixteen, whichever is smaller, and a lane is a whole value.

**S17**, line 795, "Simd structs".

> [provisional] `T.load(slice, i)` and `v.store(slice, i)` take a slice of the lane type and an index. The dev-mode check covers the first element and the last, and the message names the one that lies outside. Reason: the section writes both on a slice, and the bounds rule checks every index into a slice.

**S18**, line 797, "Simd structs".

> [provisional] `v.shuffle(...)` takes one constant index per lane, from 0 to the lane count less one. Reason: the section gives the indexes as constants and the result the type of the value.

**S19**, line 799, "Simd structs".

> [provisional] `as` converts between a simd struct and an array or a plain struct of the same bytes, in both directions. A class, a union, a variant and another simd struct are refused. The checker computes the bytes of both sides by the C rules, which are the same on every target. It refuses a type that holds a width the target decides. Reason: the section names the array and the plain struct of the same bytes. A conversion whose sides differ per target would hold on one and fail on another.

**S20**, line 801, "Simd structs".

> [provisional] A simd struct of 16 bytes is the vector type of C. It passes and returns in one vector register on ARM64 and in one xmm register under System V. Windows x64 takes a pointer to a copy and returns it in xmm0, as MSVC passes and returns `__m128`. A simd struct of another size passes as the plain struct of its lanes. The header writes that struct with the alignment of the simd struct. Reason: the section gives the vector type of 16 bytes and the register, and the point of the C layout is that C and Anti agree.

**S21**, line 802, "Simd structs".

> [provisional] Every operation on `f16` lanes converts each lane to an `f32` and the result back. Each is written as one scalar operation per lane. Reason: `f16` is storage, and the section says every operation on such lanes goes through `f32`.

**S22**, line 803, "Simd structs".

> [provisional] A simd struct above the vector cap is accepted with a warning at its declaration. Every operation on it is then a loop over its lanes. Reason: the section says it is an array and a loop with a message that says which. A warning is the message the compiler has.

**S23**, line 810, "Runtime configuration".

> [provisional] A key of `[runtime]` that the runtime does not know, a key outside `include`, `[runtime]` and `[injections]`, and a value a key does not take are startup errors naming the file and the line. Reason: the specification makes an unknown `--anti.` name a startup error listing the keys, and the keys of the file are the same keys.

**S24**, line 811, "Runtime configuration".

Stale: The third sentence is stale. Plugins are built, and the entry on `plugin:<path>` and `discover` under "Plugins" (line 882) has the runtime fill such a slot before `main`. `src/rt/conf.c` calls `anti_rt_plugin_provider` for it.

> [provisional] A line of `[injections]` and a `--anti.inject` that names an interface the program does not inject is a startup error naming the ones it does have. One that names an `inject final` field is a startup error that says so. One that names an interface the program injects and that is not final is a startup error too, because the value is the path of a library and plugins are not built. Reason: the specification gives the first two messages. The third waits for `plugin.load`, which nothing has yet, and a line that cannot be carried out is not passed over in silence.

**S25**, line 814, "Runtime configuration".

> [provisional] `--anti.help` answers before the file is read, and `--anti.inspect` after it, so `--anti.help` prints the options of a program whose file is broken and `--anti.inspect` prints what the file gave. A command line with both prints the help. Reason: the options of the runtime are the same in every program, and the effective configuration is not.

**S26**, line 815, "Runtime configuration".

Stale: The last sentence is stale. Plugins are built, and discovery reads the `plugins` key (line 884).

> [provisional] The value of `plugins` from a file is its array joined with `:`, the separator of `--anti.plugins=dir:dir`. Reason: the specification writes the option with that separator and the key as an array, and the two forms then print alike under `--anti.inspect`. Nothing reads the value yet, because plugins are not built.

**S27**, line 826, "Hooks and tracing".

> [provisional] `trace` before a module-level `fn` is refused with `` `trace` marks a class or a function of one ``. Reason: `enter`, `leave` and `failed` take the object, and a module function has none.

**S28**, line 827, "Hooks and tracing".

> [provisional] A hook is never instrumented, whatever its signature: a function under one of the nine names takes no `enter` and no `leave`. Reason: `enter` around a body of `enter` calls itself without end, and a handler's nine are hooks as much as a class's own.

**S29**, line 829, "Hooks and tracing".

> [provisional] `--trace writes` calls `changed` after every write to a field of an instrumented class, wherever the write stands. It takes the record of the field from the field list the class's descriptor carries. The list is written for the hook even under `--no-reflect`, which leaves the descriptor without it. Reason: the record is the data reflection already holds, so the hook adds none. A write from another module answers the same question, because the library file carries the marking of the class. The format rose to version 47 with it.

**S30**, line 833, "Hooks and tracing".

> [provisional] The report of an abstract class that no class fills runs for a build that writes a program, and not for `-c` or `--lib`. Reason: a library is filled by the code that links it. `anti.lang` declares `TraceHandler` for the program that installs a handler, and the report named it on every build of the standard library.

**S31**, line 837, "Hooks and tracing".

> [provisional] `LeakTracker` counts `created` and `copied` as one object made each, and `destroyed` as one freed. Reason: `dup` fires `copied` alone, because it runs no `construct`, and the object it makes is one more object to free.

**S32**, line 839, "Hooks and tracing".

> [provisional] `ErrorMonitor` copies the message of the error into itself, cut to `MESSAGE_BYTES` bytes, and keeps the code and the name of the function. It never keeps the error. Reason: the error a `catch` binds is deleted when the handler exits, so a `str` into it points at freed memory once the hook has returned.

**S33**, line 840, "Hooks and tracing".

> [provisional] `ChangeJournal` keeps the name of the class and the name of the field of each write, and never the object. Reason: both names stand in read-only data, the field record in the class descriptor and the class name in the descriptor itself. The object may be deleted before the journal is read.

**S34**, line 841, "Hooks and tracing".

> [provisional] `Composite` owns the handlers it holds and deletes them with itself. It reaches them in the order they were added, and on `leave` in the other order, so the handlers nest around a call as the handler and the object's own hook do. Reason: `install` builds the handlers it adds, so one `delete` frees the whole group. The reversal on `leave` is the rule the hook site already follows.

**S35**, line 847, "Injection".

> [provisional] `inject` and `inject final` are contextual words before the name of a field, in a class body alone. `inject: int` is still a field named `inject`, and `inject final: *L` a field named `final` that a provider fills. Reason: the specification calls `inject` a field marker like `own`, and every field marker of Anti is contextual. A struct has no `construct`, so no provider could fill a field of one.

**S36**, line 848, "Injection".

> [provisional] The type of an `inject` field is `*Interface` of an abstract class, never `?*T`. No default stands beside it, it is not `own`, and no literal may name it. A literal that leaves it out is complete. Reason: the specification gives the field a pointer to an abstract class. It says that no literal may set it, and that the provider is never `none`. A default would be overwritten by the provider, and the provider owns what it gives.

**S37**, line 851, "Injection".

> [provisional] A provider names a function of the program that takes no arguments and gives a pointer. The path is split at each dot, and the module and the name that answer name the function. A path that names no function names a class, and `get` is appended to it. Where the name is `C.f` and the module holds a class `C`, the class must be the interface, inherit it or implement it. A sub-object of the interface gets a thunk that calls the provider and moves the pointer to the sub-object. Reason: the specification gives the two forms, a static function of the interface's type and the `get` of a singleton. The IR carries no Anti types, so the result of a module function is checked as a pointer alone. The class records carry what a class is.

**S38**, line 854, "Injection".

> [provisional] The providers of a library for C are resolved as a program's are, and its slots are written into it. Reason: its host is C and cannot fill a slot. The used-slot bitmaps are the host program's, because a plugin fills them, and a provider is not.

**S39**, line 855, "Injection".

> [provisional] `anti test` reads `anti.toml` of the directory it runs in, which is the project root, and passes the `[inject]` table with `[inject.test]` over it, per interface. A project without a manifest injects nothing, which is no error. A quoted key of `[inject]` whose path starts with `test.` is read as a key of `[inject.test]`. Reason: TOML itself nests `[inject.test]` under `[inject]` and the reader gives a flat list of paths, so the two spellings are one key.

**S40**, line 856, "Injection".

> [provisional] `alloc` and `free` name a field after `inject` and after `inject final`, and nowhere else among the fields. Reason: the example under "Injection" in `docs/anti-syntax-overview.md` writes `inject final alloc: *Allocator`. A name must stand after `inject`, so the position has one reading, as the positions after `fn` and after `.` have. The entry under "`anti.mem`" left this field to the session that built `inject`.

**S41**, line 857, "Injection".

Stale: Stale. `--closed` is built: `src/antic/main.c` takes it, and the entry under "Plugins" (line 880) gives what it turns off.

> [provisional] `--closed`, which builds a program without the exports of its runtime symbols and its descriptors, is not built. Nothing loads a plugin yet, so nothing reads those exports. Reason: the option belongs with `plugin.load`, and the specification gives it for a program that refuses a run-time replacement.

**S42**, line 873, "Plugins".

> [provisional] The interface of a `provides` line is one dotted path. The last name is the class, and the names before it are its module, written as the alias an `import` declared or as the whole module path. `lib.instance(I)` and `lib.supports(I, "f")` read the same form. Reason: the overview writes `provides anti.log.Logger as FancyLogger;` and `lib.instance(anti.log.Logger)`, and a type expression takes the alias alone. One rule that reads both spellings keeps the two documents and the language together.

**S43**, line 874, "Plugins".

> [provisional] A `provides` line names a complete class of the module that is no singleton. Reason: a singleton has one instance, which its `get` makes, and `instance` would hand the host a second.

**S44**, line 876, "Plugins".

> [provisional] A plugin is emitted as the object of its own module alone, as a dev build is. It links no runtime and no standard library. Every name it uses is left to the loader: `-undefined dynamic_lookup` on macOS and the default of a shared object on Linux. Reason: the specification says the library uses the host's runtime, heap, pool and registry. One copy of each is what "the host's" means.

**S45**, line 882, "Plugins".

> [provisional] A provider written `plugin:<path>` or `discover` leaves the slot empty at the link. The runtime fills it before `main`. A failure to fill it is a startup error naming the interface and the reason. The command line wins over the configuration file, and both win over the build. Reason: the specification gives the two providers to the manifest and the replacement to the configuration. The precedence of the layers is the one the keys already follow.

**S46**, line 884, "Plugins".

> [provisional] Discovery searches the directories of the `plugins` key in their order and nothing else. Reason: the specification adds the libraries that ship with Anti, and none ships. The runtime holds no path of the install, so that directory joins the search when there is something in it.

**S47**, line 888, "Plugins".

> [provisional] The static atomics of a loaded library are its own globals and no work of this step. Reason: the specification says they belong to that load. A global of the library is defined in the library, and the host defines its own.

**S48**, line 889, "Plugins".

> [provisional] The message of a failed call of the loader belongs to the runtime and stands until the next failure. Reason: `anti.error.SystemError` keeps the message of `strerror` the same way, and an error of the language holds a `str` and not an owned copy.

**S49**, line 896, "Versions".

> [provisional] The chain of an abstract class is the hash of every prefix of its table, the empty prefix first. Entry k is then the hash of the version whose table held k entries. Reason: the specification says the chain holds the hashes of the earlier versions with the slots each had. An interface stays compatible while every new function is added at the end, which the same section says. An earlier version is therefore the prefix of today's table with that many entries. The chain is then the one thing a build can compute from the source it has, and it needs no record of earlier builds.

**S50**, line 897, "Versions".

> [provisional] The load compares the two chains once, at the length of the shorter. Reason: the hash is rolled over the entries in order. Equal there means equal at every shorter length as well. A newer plugin in an older program and an older plugin in a newer one are then the same comparison. That is what the specification asks for with "a newer plugin in an older program passes when the same rules held between the two versions".

**S51**, line 898, "Versions".

> [provisional] "No field was added" is the field count and the size of the interface compared. A plugin records both from the descriptor of the interface it was built against. The loader reads the host's from the descriptor it is bound to. Reason: a field added to an abstract class moves every field of the classes below it and widens the sub-object. The count alone misses a field that replaced another, and the size alone misses one that padding hides. The size stays symbolic in the IR, so the back end lays it out per target.

**S52**, line 899, "Versions".

> [provisional] Without a `compatible` line there is no floor of its own, and the chain alone decides. Reason: the specification says the floor without one is where the chain says the structure last changed incompatibly. A chain built from the source of one version holds no such point. A change that is not an addition at the end gives a chain that diverges, which the comparison above already refuses. Every version whose hash stands in the chain is therefore compatible, and there is nothing below it to name.

**S53**, line 901, "Versions".

> [provisional] A plugin records the version of the package that declared the interface, which is the one the floor is compared against. Reason: the floor names the lowest version a plugin may have been built for. The version a plugin was built for is the interface's, not its own.

**S54**, line 905, "Versions".

> [provisional] `--anti.inspect` prints the version of each injectable interface and the count of the slots the program reaches through it. The table of the injectables carries the descriptor of the interface for it. Reason: the specification says the option prints the injectable interfaces with versions and used slots. The descriptor is where both stand.

**S55**, line 911, "The check command".

> [provisional] The front-end class writes the interface file of every module into the work directory, in the order the imports of the project ask for, and passes `--front-end` together with `-c`. Reason: a module that imports another of the project needs that module's interface, and `antic -c` is what writes one. The interface files are the only thing the command writes, and no assembly, object or executable is produced. The two rules of producing a library, the reserved module root and the module path of one segment, stay quiet under `--front-end`, because a check of `src/main.anti` would otherwise warn on every run.

**S56**, line 912, "The check command".

> [provisional] A module under `--front-end` is checked as a library and not as a program. Reason: a check of one file cannot know which module of a project fills an abstract class. The whole-program report of an abstract class that nothing fills would then fire on every interface of a project.

**S57**, line 916, "The check command".

> [provisional] The doc warnings of `--warn-undocumented` cover the `pub` items of a module and the `pub` members of a class body. An item the compiler wrote, such as the `get` of a singleton or a function of a `tests` block, is no item a reader documents. Reason: the specification says every `pub` item, and a member of a class body is one.

**S58**, line 917, "The check command".

> [provisional] The implied `import <module>;` of a user doc block is left out where the block writes it itself. Reason: two imports of one module are a redeclaration, and the module doc of `anti.plugin` and of `anti.trace` writes the import in the block.

**S59**, line 918, "The check command".

> [provisional] A developer doc block without `fn main` is wrapped in a function whose name is the module path with `_` for every dot and `_block_<n>` after it, and a user block is wrapped in `fn main` as the specification says. Reason: a developer block compiles inside the module, which may define `main` already.

**S60**, line 920, "The check command".

> [provisional] Without a file argument the command takes every `.anti` file under the source and the test directory that `[layout]` names, sorted by path. Both directories are search roots of the run, and the package name of `[package]` rides on every call. Reason: the directories under the source root mirror the module paths, so the source root is where a module path starts. The modules of one package share an `internal` item, and the interface files the run writes carry the name.

**S61**, line 921, "The check command".

> [provisional] `--targets all` runs the front end of every source once per target. The pass of the host writes the interface files and the other five write nothing. Reason: a library file is target-independent, so one pass writes it, and the specification asks for the front end once per target.

**S62**, line 934, "The build command".

> [provisional] A dev build writes an object of every module of the standard library that the program reaches. They stand under `build/<target>/dev/obj/`. Reason: a dev build compiles one module into its own object, so the link needs an object of each. `anti test` writes the same objects for the same reason, and `driver_libraries` gives the list from the imports.

**S63**, line 939, "The build command".

> [provisional] `anti build --release` writes `<program>-symbols.zip` beside the program in `dist/`. It holds `<program>.debug` and `<program>.map`, with the PDB of a Windows link beside them. `src/anti/zip.c` writes it with stored entries and no compression, and every field that would carry a clock or a machine is fixed. Reason: "Symbols tooling" in `docs/anti-language-additions.md` asks for the archive and for zip. A stored archive needs no library and gives one file from one input on every host.

**S64**, line 947, "The symbols command".

> [provisional] The libraries of a `plugins` directory are the ones its `anti-plugins.toml` lists, and an empty line of `[injections]`, which is `discover`, adds none. A library two keys reach counts once. Reason: discovery reads the index and nothing else of the directory, so a library it does not list is never loaded.

**S65**, line 948, "The symbols command".

> [provisional] The archive of a binary is `<stem>-symbols.zip`, where the stem is its file name without `.exe`, `.dylib`, `.so` or `.dll`. Reason: `anti build --release` names the archive of a program that way, and a library of any target then follows the same rule.

**S66**, line 949, "The symbols command".

> [provisional] The archive of a deployment holds `index.toml` and one directory per build id with the entries of that build's archive in it. Each `[[module]]` of the index holds `module`, the file name of the binary, `id`, `version` and `source`, the path of the archive it came from. The version is the one of the last `package` line of the binary's notice. Reason: the specification asks for an archive keyed by id with an index of those four. The notice is the one place a binary names its version, and the package of the compiled module is its last line, which the entry below makes hold.

**S67**, line 952, "The symbols command".

> [provisional] `resolve` prints every line of the trace as it came in. A frame whose module an archive holds gains the function and the `file:line` after it. The debug link answers first, and the map fills the function or the line where the link gives none. A Mach-O link keeps its line table in the objects it names, and the map answers where those are gone. A frame of a Windows program stays raw, because its functions stand in the PDB and only DbgHelp reads one. Reason: the specification prints a frame without an archive raw. A line that is no frame, such as the message before a trace, passes the same way. The map carries the line of the declaration and the debug link the line of the call.

**S68**, line 953, "The symbols command".

> [provisional] The reader of an archive takes stored entries and entries compressed with deflate, and refuses every other method. Reason: `anti build` stores its entries, and an archive a user packed again with another tool is deflated. Every host packs zip that way.

**S69**, line 963, "The doc command".

> [provisional] A constant carries its value on the page where the value is an integer, a float, a bool or `none`. An array, a struct, a character and a value computed from `size_of` carry none. Reason: the four print in one line from the value the library file stores, and the others need a form of their own.

**S70**, line 964, "The doc command".

> [provisional] `anti.lang.Object` is left out after `inherits`. Reason: it is the root of every class chain, so naming it says nothing a reader does not know. A class that names a base of its own writes it, with the module of the base.

**S71**, line 965, "The doc command".

> [provisional] User docs take every field of a `struct` or a `union` and the `pub` fields of a class. Reason: a struct body carries no visibility marker and every field of one is readable and writable everywhere, which the object model says. A class body marks its fields.

**S72**, line 966, "The doc command".

> [provisional] A backtick name becomes a link where it resolves against the items of the module and the modules it imports. An item of the documented module links to its place on the page. A `module.Item` links to the place on that module's page, and a module path to the page itself. A member of an item links to the item, which is the anchor the page has. Everything else stays inline code. Reason: `docs/tooling.md` asks that a name resolved against the module table become a link. The tables of a library file are the items and the imports, so a page built from one writes the links of the source.

**S73**, line 968, "The doc command".

> [provisional] Without a file the command takes every `.anti` file under the source directory that `[layout]` names, and not the test directory. Reason: a module of the tests is no module a reader of the library documents.

**S74**, line 975, "The formatter".

> [provisional] A body whose `}` stands on the line of its `{` keeps to that line. The braces of one that spans lines each take a line of their own, so the first element of the body moves off the `{` and the `}` stands alone. Reason: the addendum keeps the one-line body of `concrete fn joined(self, o: *Object) { }` and of `pub enum Mode: u8 { Read, Write }`. It asks as well that a closing brace stand on its own line. A body that already spans lines reads the same way at its opening.

**S75**, line 976, "The formatter".

> [provisional] An item body is the body of `fn`, `struct`, `union`, `class`, `enum`, `variant`, `tests` and `fixtures`. Reason: the entry above says an item body opens `{` on its own line and names none of them. The addendum names `fn`, `struct` and `union` as the three it illustrates. The eight are the declarations that carry a body, so the rule reaches each of them.

**S76**, line 977, "The formatter".

> [provisional] A brace that no word of its statement governs builds a value, and a brace with no statement before it opens a block of its own line. The words that govern are `if`, `else`, `while`, `do`, `for`, `switch`, `sync`, `select`, `catch`, `defer`, `parallel`, `undo` and `=>`. Reason: a token stream tells a struct literal from a statement block by the word that opens the statement, and `let got = read(f) catch e { }` holds one of each.

**S77**, line 979, "The formatter".

> [provisional] A space stands inside an empty brace, `{ }`, and between the type of a bitfield and the colon of its width, `layer: u32 : 4`. Reason: both are the forms the addendum writes, the first in the hook it names and the second in the listing of "Bitfields".

**S78**, line 980, "The formatter".

> [provisional] A doc comment is filled to 80 columns, where a tab counts the four columns `anti html` and `anti tex` render it as. A fenced block is written as it stands and an empty line separates two paragraphs. A `- ` item carries its continuation two columns in, and a line of a deeper indent opens a paragraph of its own. Reason: the addendum asks for 80 columns and for the markup subset. A fill that crossed a fence or joined two paragraphs would write markup the subset has not.

**S79**, line 981, "The formatter".

> [provisional] A source the lexer refuses stays as it is, and `anti fmt` names it and ends with a non-zero status. Reason: the canonical form of a source that does not lex is unknown, and the front-end class of `anti check` is what reports the error.

**S80**, line 992, "The bind command".

> [provisional] A macro may be a compound literal of a bound struct, with one constant per field in field order. It becomes a constant of the struct. A leading call of a macro of one parameter is expanded first when its body is that parameter, bare or in parentheses. `CLITERAL(Color)` is one. Reason: raylib writes its colours so, and the literal is a constant the generator can evaluate. The header then gives the 26 colours that the JSON description gave.

**S81**, line 995, "The bind command".

> [provisional] clang reads the header for the host target unless `--target` names another, with the headers of that target's sysroot in the runtime archive and `-x c`. `-I` and `-D` reach clang, and the shim repeats each `-D`. Reason: the addendum names no target, and the sysroot of the archive gives one set of headers to every host.

**S82**, line 998, "The bind command".

> [provisional] An anonymous enum becomes one `const` of `c_int` per enumerator. Reason: C uses an enum without a name for its constants alone, and no type has its name.

**S83**, line 1002, "The bind command".

> [provisional] `#pragma pack(1)` and `__attribute__((packed))` give `packed`. A pack above one byte is refused. `aligned(N)` on a record gives `align(N)`, and on the first field it raises the record's alignment. On another field it is refused. Reason: the addendum refuses every layout directive other than the two it names, and an aligned first field lays out as an aligned record.

**S84**, line 1003, "The bind command".

> [provisional] The shim renames a `static` function with a macro around the include and wraps it under its name. A C99 `inline` definition gets `extern __typeof__(f) f;`, which makes the header's definition the symbol. A variadic inline function is left out. Reason: the addendum asks for a wrapper of the same name, and neither form copies a body.

**S85**, line 1004, "The bind command".

> [provisional] The macros are the object-like `#define` lines of the header in the output of `clang -E -dD`, evaluated as `docs/notes/bind.md` describes. A define of rlparser of the kind `FLOAT` is a `c_float`, and `COLOR` is a struct literal. Reason: the AST holds no macro, and raylib writes its floats with the suffix f.

**S86**, line 1005, "The bind command".

> [provisional] The frameworks of a binding come from a table in `src/anti/bindtype.c`: raylib takes Cocoa, CoreVideo, IOKit and OpenGL, and miniaudio takes AudioToolbox, CoreAudio and CoreFoundation. Reason: those are the frameworks raylib's GLFW back end and miniaudio's Core Audio back end link on macOS.

**S87**, line 1006, "The bind command".

> [provisional] `volatile` leaves a `// volatile in C` line before its field, and a line before a function whose parameter is volatile. Reason: the entry under "Core language" asks for a comment in the binding.

**S88**, line 1007, "The bind command".

> [provisional] A binding and a probe in Anti pass through the formatter of `anti fmt` before they are written. Reason: a binding committed into `src/std/` stands in the canonical form that `fmt_canonical` checks.

## Internal

**B1**, line 747, "Wrapping and saturating operators and `Flags`".

> [provisional] A Flags variable reads the fields that its function names after `.`, and any other use of it reads all four. The flags form writes those alone. Reason: the specification makes unused fields cost nothing, and a use of the whole value may read any of them.

**B2**, line 822, "Hooks and tracing".

> [provisional] A table entry is keyed by its name and by the count of its parameters, `self` among them. Reason: `anti.lang.TraceHandler` declares the nine hooks again with the object after `self`, and a key of the name alone gave those bodies the entries of the root's nine. A handler object would then have called its own two-argument `created` where a hook site dispatches the one-argument hook of an object. Anti has no overloading, so every other name in a chain has one signature and the key changes nothing there.

**B3**, line 823, "Hooks and tracing".

> [provisional] A hook site is one call of the runtime with the object and the number of the hook. `src/rt/hooks.c` loads the handler and calls it when there is one. It then dispatches the object's own hook, unless that entry still holds the root's empty body. Reason: the order of the two calls, the reversal on `leave` and the compare against the empty body then stand in one place. The compiler writes no branch. The cost the specification gives is one load and one compare per creation, destruction, copy, dispatch and join. A site is a call today, so that form is work.

**B4**, line 824, "Hooks and tracing".

> [provisional] The compiler writes the sites of `created`, `destroyed`, `copied` and `dispatched`, and `join` and `join_all` call `anti_rt_join_hooked` and `anti_rt_join_all_hooked` for `joined`. Reason: a `Job` holds the handle alone, so the site of a `join` cannot name the object and the runtime must. A second pair of entry points keeps `--no-hooks` whole, which drops every site the compiler writes.

**B5**, line 849, "Injection".

> [provisional] The slot of an interface is one global of the runtime module, named `inject.` and the path of the interface. It holds the provider, and every site loads it and calls through it. Reason: the specification gives one static atomic pointer per interface, initialised to the compiled-in provider. A relaxed load of a pointer is a plain load on all six targets, so the site is one load and one call. The passes over the whole program write the data of the runtime module, which a dev build keeps in the object that links.

**B6**, line 853, "Injection".

> [provisional] Every program carries `anti_rt_injectable`, a table of the interfaces it injects with the class and the field that need each and whether it is final. The table is empty where nothing injects. A library for C carries one where it carries the runtime. Reason: the runtime reads the table before `main`, to answer `[injections]`, `--anti.inject` and `--anti.inspect`, so the symbol stands in every program that links the configuration.

**B7**, line 877, "Plugins".

> [provisional] A plugin brings no constructor of its own, and the loader registers what it carries. Reason: the host has run the runtime's start, and a second `anti_rt_init` would run over it. The table the library exports holds its classes. The loader adds them to the host's registry, so `reflect.new` finds a class of a library by name.

**B8**, line 879, "Plugins".

Stale: The sentence "A program that loads nothing links no loader, no digest and no reader of an index" is stale. The entry under "Versions" (line 903) says the runtime archive puts the loader in every program, because the reader of the configuration file calls it. `src/rt/start.c` reaches `anti_rt_plugin_provider` through `anti_rt_conf_start`.

> [provisional] The objects of a library are counted in the `created` and `destroyed` hooks, by the image the table of the object's class lies in. `src/rt/loaded.c` holds the slots and the count of the open libraries, apart from the loader. A program that loads nothing links no loader, no digest and no reader of an index. It pays one load and one compare per object. Reason: the specification counts through those two hooks. A plugin may make an object of any class it brought, beside the one it provides.

**B9**, line 881, "Plugins".

> [provisional] Every injectable interface carries one holder, a pointer of the runtime module, and one thunk that gives what the holder holds. A provider that comes from a library is an object. The runtime stores it in the holder and puts the thunk in the slot. Reason: a slot holds a function, and the runtime writes no code. Every site of the program then calls through one function, whatever layer filled it.

**B10**, line 895, "Versions".

> [provisional] The structural hash of an abstract class is an FNV-1a over the entries of its table in order. Each entry adds its name, a colon, its signature and a semicolon. An entry whose signature no `reflect.Value` carries adds the count of its parameters instead. Reason: the specification says the hash covers the entries in order with their names and signatures. The signature text is the one the trampolines of `reflect.call` already key on. The count of parameters tells two entries of one name apart where that text does not exist.

**B11**, line 903, "Versions".

> [provisional] `anti_rt_slots` holds the slots the program's calls reach. A flag `reflect` says that the program calls `reflect.call` and may therefore reach any slot. Every program carries the table, empty where its calls reach none. Reason: the specification counts a program that uses `reflect.call` as reaching every slot. It asks for a stub where such a slot is missing and for a refusal where a reached one is. The two answers need the two kinds of reach apart. The runtime archive puts the loader in every program, because the reader of the configuration file calls it, so the table has to be there for the link.

**B12**, line 904, "Versions".

> [provisional] The loader fills a slot that `reflect.call` alone may reach with a stub. It writes a table of its own for the sub-object of the object it builds. The library's entries come first and the stub fills the rest. The stub reads that table back through the object it was called with. It names the class the library provides and its version. Reason: the table of the library is read-only data of another image, and the specification asks for a stub in the slot. A table of the loader's own is the one place a stub can stand. `anti.reflect` refuses an index past the function list of the object's own class, so no program reaches the stub through that module today. The stub keeps a call at a slot the host knows and the library lacks from reading past the library's table.

**B13**, line 933, "The build command".

> [provisional] The module that carries `main` is compiled from its source on every dev build, and every other module from its library file. Reason: antic stops a dev build of a library file at its object and never links one. That module is therefore the link, and `docs/tooling-addendum.md` says every build relinks.

**B14**, line 937, "The build command".

> [provisional] The cache key of a module carries the processor level and a mark for `-g` beside the three that `docs/tooling-addendum.md` names. Reason: both decide the instructions and the lines of the output, so an object of one level would otherwise stand for another.

**B15**, line 994, "The bind command".

> [provisional] The pinned clang of a development tree is `deps/clang/bin/clang` under the directory above the one that holds `anti`. Reason: the configure step installs the pinned clang into `build/deps/clang` of the checkout, and every build tree stands in `build/` beside `deps/`. No path is written into the binary, which `no_paths` requires.

**B16**, line 1014, "Repository layout".

> [provisional] `tools/deps-dir.cmake` names `build/deps` once, and the configure step and the `get-*.cmake` scripts include it. Reason: a directory spelled at more than one call site drifts, and the scripts run alone with `-P`, where no cache variable reaches them.

**B17**, line 1016, "Repository layout".

> [provisional] `repo_layout` reads the tree from the disk, without `build/`, when the root is no git work tree. Reason: step 2 of `./r` runs the suite in an export of the commit, which holds the tracked files and no `.git`.

## Session

This step changed no decision and no entry. It read every line of
`docs/decisions.md` that carries the tag, sorted each into one list and
checked the four stale ones against the code they name. The script that wrote
the lists is `build/prov/provgen.py`, and the lines it read are in
`build/drive/logs/prov-lines.txt`. The step added no `[provisional]` entry.
This commit changes one file under `docs/`, so the docs-style checker is its
one gate, and it reports nothing on the report.

At the start of the step, before this report:

```text
50aebbe Tag the proof block of the restructure report
01d1e75 Report the restructure
1ac6185 Count repo_layout in the suite totals
$ git status --short
?? docs/reports/2026-09-23-provisional-review.md
$ git rev-parse HEAD origin/main
50aebbee58c0d486b5add4b7edacd1c12d0fbc75
50aebbee58c0d486b5add4b7edacd1c12d0fbc75
```
