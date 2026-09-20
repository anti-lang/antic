# Anti language additions, round three

Addendum to `docs/anti-language-additions.md`. It adds eight sections and changes four existing lines. Apply it by editing that document, then delete the addendum. Settled on 2026-09-21.

Nothing in this addendum is work for a session. It becomes work when Eddie names it in a work order or a session message. When this addendum is applied, recording it in the additions document is the whole task. A session that finds one of these features missing while doing other work notes it in its report. It does not build it.

## What to change in the additions document

- Timing. After "sum types, locking and channels" insert "then `may fail` with tuples, error origins and stack traces, then the symbols tooling". The small items of round two and the simd structs stay where round two put them. The CPU levels and `none` go before the first release. The first changes what a release is built for, and the second is a rename.
- Nullable pointers. One sentence says a failing function returns `?*Error` with results through out pointers. Replace it, wherever it appears, with a reference to [Failing functions](#failing-functions). The out-pointer form stays legal for bindings and is no longer the spelling the standard library uses.
- Keywords. Add the keywords `fail`, `here`, `fallthrough`. Add the contextual words `may fail` after a signature, `simd` before `struct`. Add the prefixes `rf` and `x` to the string forms.
- Errors, in `docs/anti-object-model.md`. `Error` gains the fields of [Error origin and stack traces](#error-origin-and-stack-traces). Replace `anti.error.NullPointer` with `anti.error.NoneDereference`, and `null` with `none` throughout, as already decided.

## Failing functions

`may fail` replaces the hand-written convention. The convention stays the ABI.

- `fn divide(a: int, b: int) -> (int, int) may fail` declares a function with two channels. `return v;` leaves on the result channel. `fail e;` leaves on the error channel with `e`, a `*Error`. `fn close(f: *File) may fail` has no result and returns success at its closing brace.
- `fail "text";` is sugar for `fail Error.new(0, "text");`. Code zero means "no code", and `fatal` turns it into exit status 1.
- A call to a `may fail` function must be handled with `catch`, `try`, `catch fatal` or a `try` block, which is the existing rule. `try` inside a `may fail` function forwards the error. Inside any other function `try` is refused.
- `undo` runs on the `fail` path and not on `return`.
- A `may fail` function with no `fail` and no `try` is a warning from `anti check`, not an error, since an interface function may fail in one implementation and not another.
- The ABI is the old convention: `?*Error f(args, R *out)`, with `out` absent for a function without a result. The header writes that form and the doc comment says the function may fail. A `.antl` records the flag, so a caller in another module handles it.
- A function written by hand as `-> ?*Error` with out pointers stays legal and is called the same way. Bindings produce that form. The standard library uses `may fail` everywhere, and the rewrite of its signatures is one session when the parser has the form.

```anti
fn divide(a: int, b: int) -> (int, int) may fail
{
	if b == 0 { fail "division by zero"; }
	return (a / b, a % b);
}

let (q, r) = divide(7, 2) catch fatal;
let (q, r) = divide(7, 0) catch e { yield (0, 0); };
```

## Tuples

- A tuple type is an anonymous struct with C layout. `(int, str)` is a struct of two fields in that order, laid out as the target lays out structs. It is passed and returned by value under the struct rules. Two tuple types are the same when their element types are the same in order.
- A tuple value is `(a, b)`. Its elements are `t.0`, `t.1`, up to the last. Destructuring is `let (a, b) = e;` and `for i, x in items`, and nowhere else: not in a parameter list and not nested.
- `for i, x in items` is destructuring of an `(int, T)` per element. `let (result, flags) = a + b;` is destructuring of a `(T, Flags)` result. Both special forms are removed from the additions document and described here instead.
- The C header writes a tuple as a named struct, `struct anti_tuple_int_str { int64_t _0; const struct anti_str *_1; }`, one per distinct tuple type used in an exported signature.
- Tuples are for functions that have two answers and no name for the pair: `divmod`, `min_max`, a coordinate pair, the flags. A tuple of more than three elements is a struct that has not been named yet. So is one that crosses a module boundary. The site says so.

## Error origin and stack traces

- `Error` gains `pub at: SourceLocation`, set by `fail` at the position of the `fail` statement when the error has no location yet. The first `fail` wins, so an error forwarded through `try` keeps its origin, and the `cause` chain shows the path it took.
- `Error` gains `own frames: ?*StackTrace`, captured by the same `fail` when backtraces are on. On in dev mode, off in release. `--anti.backtrace` and `backtrace = true` in the runtime configuration turn it on for a release build. When off the field is `none` and `fail` costs what it costs today.
- `e.text()` prints `file:line:column: message`, then the cause chain, then the trace when there is one. `print` and `fatal` inherit it.
- `Error.new` takes no location. `fail` supplies it.
- `anti.debug.StackTrace`: `StackTrace.capture(skip: int = 0) -> *StackTrace` walks frame pointers and stores the return addresses with, per frame, the module the address is in, that module's build id and its load base. `t.frames` is the raw form. `t.text()` prints one address per line with the modules on top. `t.symbolize() -> []Frame` fills `Frame { address, function: str, file: str, line: int }` from the symbol table for the function and from the line table for file and line, so a release build gives functions and a `-g` build gives everything. Symbolising is lazy and never runs for an error that was handled.
- `anti.debug.backtrace(n)` is the short form returning a `StackTrace`.
- Every Anti executable and shared library carries a build id, the digest of its code, in `anti_licenses` beside the version.

## Source locations

- `here` is a keyword whose value is a `SourceLocation` for the position it is written at: `file`, `line`, `column`, `function`, `module`. `file` is the root-relative path the checks use. `function` is the full name, `module.Class.f`. The value is constant data, so it costs the loads.
- `here` as a default parameter value is evaluated at the call site, so `fn log(level: Level, msg: str, at: SourceLocation = here)` sees the caller's position. That is how a logger, `show` and a traced error get the caller's line without a macro.
- `here` in an ordinary expression gives the position of that expression, which is rarely what a message wants. The site says so.

## Symbols tooling

The release binary carries no symbol data. Every deliverable ships a symbols archive beside it. `anti symbols` manages those archives.

- `anti build --release` writes, beside `prog`, `prog-symbols.zip` holding `prog.debug`, the same link with the debug sections kept, and `prog.map`, a text map of address ranges to function, file and line. Both carry the build id. A shared library or a plugin is its own deliverable with its own archive. Zip, because every host opens one without a tool.
- `anti symbols inventory --conf config.toml [--from dir] [--out symbols.zip]` reads the runtime configuration, finds the program, the `plugins` directories and the `[injections]` libraries, reads every Anti binary's build id, and folds each one's symbols archive into one `symbols.zip` for the deployment, keyed by id, with an `index.toml` of module, id, version and source. It reports what it could not find.
- `anti symbols check --conf config.toml [--symbols symbols.zip]` walks the same binaries and reports per module whether its symbols are present, stale because the binary changed, or missing. Non-zero exit when anything is missing, so a deployment script can stop a rollout.
- `anti symbols resolve trace.txt --symbols symbols.zip` turns a raw trace into function names and lines, matching each frame to an archive by build id. More than one `--symbols` is allowed. An archive whose id matches no frame is ignored. A frame whose id matches no archive is printed raw.
- The `.debug` twin is preferred over the map when both are present, because it carries inlining. The twin holds root-relative paths and names and nothing else, so keeping it is a matter of size, not secrecy.
- The release layout on the site and on GitHub keeps `symbols/` beside each release, never in the package a user downloads.

## CPU levels

- The x86_64 baseline for everything Anti ships and for release builds is x86-64-v3. `--cpu v1` and `--cpu v2` stay available for a program that must run older hardware. The runtime archive's native libraries are built for v3.
- The ARM64 baseline is per operating system. `macos-arm64` is `armv8.5`, since every Apple Silicon Mac is an M1 or later. `linux-arm64` is `armv8.0`, for the Pi 4 and older boards. `windows-arm64` is `armv8.2`, since every Windows-on-ARM machine sold is a Snapdragon 8cx or later. `--cpu` overrides on every target. `armv8.2` and above make atomics one instruction and add half-precision conversion and the dot products.
- `rt/start.c` checks the processor once at start. When the machine has less than the program needs, it exits with a message naming the level. The message reads "this program needs a processor with AVX2 (x86-64-v3, 2013 or later)".
- A level is a code-generation setting, not a target. The six targets stay six.
- The vector byte cap of [Simd structs](#simd-structs) is a constant in the level table. It is the widest vector register of any level antic knows.

## Simd structs

- `simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }` declares a vector. Every field is the same primitive type and the field count is a power of two. The size is a multiple of eight bytes up to the level table's cap, 256 bytes to start. The alignment is the size or sixteen, whichever is smaller. Fields are visible and named, so `v.x` is a lane.
- `+ - * /`, the bitwise operators on integer lanes, comparisons and unary minus apply element-wise, built in. A comparison yields a mask, a `simd struct` of `bool` with the same lane count. `simd.select(mask, a, b)`, `simd.any(mask)` and `simd.all(mask)` live in `anti.simd`.
- Built-ins on the type and on values, lowered straight to instructions: `Vec4.splat(v)`, `Vec4.load(slice, i)`, `v.store(slice, i)`, `v.shuffle(...)` with constant indexes, `v.sum()`, `v.min()`, `v.max()`, `a.dot(b)`. Anything that costs more than one instruction on the native width is a named function, so a reader sees it.
- `as` between a `simd struct` and the array or plain struct of the same bytes is free, both ways. A raylib `Vector4` becomes a `Vec4` that way.
- C layout: a 16-byte `simd struct` is `float32x4_t` on ARM64 and `__m128` on x86_64, passed and returned in vector registers. The header writes the vector type.
- The back end maps every operation to the target's native width. One instruction where the width exists, two or more where it does not. The same result everywhere. A `f32x8` is one instruction on x86_64 at v3 and two on ARM64. A `f32x64` is sixteen on ARM64. The lane count is the programmer's, the instruction count is the machine's.
- `f16` lanes are storage only, as `f16` is: every operation converts to `f32` lanes and back.
- Above the cap it is an array and a loop, with a message that says which. Variable-length vectors, scatter and gather, and vectorisation of scalar loops are not part of it.

## Small items, round three

- `fallthrough;` as the last statement of a `switch` arm continues into the next arm's body without testing its values. Not in the last arm, and not into an arm that binds a variant's fields.
- `rf"..."` and `rf#"..."#`: interpolation without escape processing. `{expr}` and the format specifications work as in `f"..."`, every backslash is literal, `{{` and `}}` write a brace. `fr` is refused with a message naming `rf`.
- `x"00 AB CC"`: a `[]byte` literal of hex pairs with whitespace ignored. An odd digit count or a non-hex character is an error naming the position. No hash delimiters, since the content is hex and spaces.
- The string prefixes are `r`, `b`, `br`, `f`, `rf` and `x`, letters only, one meaning each, listed in one table. No word-form prefixes.
- `f16` is a storage type: sixteen bits in a field, an array or a slice, read as `f32`, written with `as f16`. No arithmetic on it. The conversion is one instruction on ARM64 and on x86_64 at v3, and a runtime routine at `v1`.
- `anti check` compiles every constant pattern passed to `regex.compile` with PCRE2. It reports a syntax error with the pattern's file, line and the position PCRE2 names. A pattern built at run time is not checked. The check is skipped with a note when the runtime archive is absent.
- `none`, replacing `null`: the value of a `?*T` or `?fn` that points at nothing. On every current target it is represented as address 0, so that C's `NULL` and Anti's `none` are one value across a call. The language does not promise that representation, and a target where address 0 is memory may choose another.
- A static function is namespaced by its class and may share a name with a static in the chain. The redeclaration rule covers fields, functions that take `self`, and constants.
- Considered and declined: a power operator `**`. It would be the one arithmetic operator that compiles to a library call, and `math.pow` says that it is one. Considered and declined: multiple names in one `let`, `let a, b = e;`, since a `let` binds one name and the two-name form is tuple destructuring.

## Messages added

- `` `divide` may fail and its error is not handled ``
- `` `try` outside a function that may fail ``
- `` `close` may fail and never does `` as a warning from `anti check`
- `` `fallthrough` in the last arm ``
- `` `fr"` is not a prefix, write `rf"` ``
- `` odd digit count in `x"..."` at column 7 ``
- `` `f16` has no arithmetic, convert with `as f32` ``
- `` pattern `[a-` at column 3: missing terminating ] `` from `anti check`
- `` `Vec4` lanes must share one type ``
- `` `f32x128` exceeds the vector cap, use an array ``
- `` this program needs a processor with AVX2 (x86-64-v3, 2013 or later) `` at run time
