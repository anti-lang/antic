# The release link, and a context that C passes

Eddie answered the three questions of
`docs/reports/2026-11-07-symbols-closures.md` with the recommendations of
that report. The session recorded the answers and built them. Q1 stopped at
the check of its route. The second link does not give the code of the debug
link on four of the six targets. Q2 builds nothing, and Q3 is built.

## What changed

- `7c444e74` records the answers as "Answers of 2026-11-07, third set" in
  `docs/decisions.md`, with the line in the entry of the feature freeze.
- `68f2f6a1` builds Q3: an exported function wraps each callback of two
  words that C passes in a thunk of its frame.

## Q1: the second link of the LTO object

Not built. T211 stays open, and `symbols_moved_frame` stays on the list.

A prototype took the debug link of `anti build --release` for each target
and added the option that keeps the LTO object. A second link of the kept
object, with the same other inputs and `-S` or `--strip-debug`, wrote the
program. A script read both with `llvm-readobj --sections` and compared the
address, the size and the bytes of every section that the image loads.
Mach-O sections count by segment and name, without `__LINKEDIT`. ELF takes
the sections of `SHF_ALLOC`, and COFF every section but `.buildid`, which
names the PDB of each link. Two programs ran: `tests/anti-symbols/moved`,
which hosts plugins, and the same program without its class.

| Target | `moved` | plain |
|---|---|---|
| macos-arm64 | identical | identical |
| macos-x86_64 | moved by 0x50 | identical |
| linux-arm64, linux-x86_64 | the PLT in another order | other members of musl |
| windows-arm64, windows-x86_64 | the export directory | no LTO object |

- macos-x86_64: the LTO link of `moved` writes no `__eh_frame`, and the
  link of the kept object writes 0xc78 bytes of it. Its section header
  moves `__text` and every section after it by 0x50 bytes.
- Linux, `moved`, which links against glibc: every section keeps its
  address and size. The PLT, `.dynsym` and the hash tables hold the
  symbols in another order. A call of `fwrite@plt` at 0x25c48 goes to
  0x4dd10 in the debug link and to 0x4dd00 in the program.
- Linux, the plain program, which links musl statically. The bitcode
  names functions of `libc.a` that LTO then removes, and the LTO link has
  taken their members by then. The link of the object takes the weak
  stand-ins: `__tl_lock` is 128 bytes in the debug link and 4 in the
  program. `.text` is 0x3018c bytes against 0x30090 on arm64, and every
  section after it moves.
- Windows, `moved`: a program that hosts plugins links without LTO, so
  both links take one object, and the code is identical. The export
  directory names the file of each `.def`, so `.rdata` differs there.
- Windows with LTO: `/lto-obj-path:` of the pinned lld-link writes no
  object. `/lldsavetemps` writes `<output>.lto.obj` beside four bitcode
  files, and its link differs in `.text` as musl does.

A third form was measured for the question below. The LTO link keeps its
object, and both the debug link and the program are links of that object.
One keeps the debug information and one drops it. Those two were identical
on macos-x86_64, linux-arm64 and linux-x86_64, for both programs. No test
was written, since nothing is built, and `anti build --release` was not
measured again. The figures before stand in the last report: 2.483 s,
2.493 s and 0.192 s.

## Q2: both release calls at once

Recorded. Nothing is built.

## Q3: a callback of C with its own context

- `store_params` of `src/antic/lower_function.c` stores the callback and
  the context of each parameter of two words of an exported function in a
  slot of the frame. The parameter then holds the thunk `callback.N`,
  written by `lower_c_callback` of `src/antic/lower.c`, and the address of
  the slot.
- The thunk loads both words and calls the callback through its address,
  which takes the canonical NaN of each float. A context that is not null
  then never reaches C inside Anti.
- The checker refuses every way out of the call for such a parameter: a
  store, a `keep` parameter, a local that is stored, a return, an `own`
  field, a snapshot and a `concurrent` parameter without its mark. Line
  755 to 758 of `docs/anti-language-additions.md` give the rule. The frame
  is enough.
- `program_abi_nan` gains the lines `context` and `context-apply`. C
  passes `nan_record` with the address of a local, and `anti_nan_pass`
  passes a NaN to it directly and through `apply`. Before the change both
  lines gave `ffd00000`, and `ffc00000` under Rosetta. All four runs pass
  now: release, dev, `--memory-checks` and `v1` under Rosetta.
- `dump_llvm_c_callback_macos-arm64` holds the form. The header does not
  change, so `tests/clib/headers/` does not either.
- `guide_examples` failed on the excerpt of `call_it` in chapter 5 of the
  guide, which is exported. opt inlines the thunk there, and only the
  names of the text changed. The excerpt and its prose follow.

The cost. The thunk allocates nothing and takes 16 bytes of the frame per
parameter. opt inlines it, so no call is added. A loop of C called an
exported function that calls a callback of C 200 million times, each
argument the result of the call before. The median of 11 runs per call:

| Build | arm64 | x86_64 at `v1` |
|---|---:|---:|
| before, a context of C | 2.15 ns | 2.39 ns |
| after, a context of C | 4.34 ns | 9.21 ns |
| before, a null context | 3.87 ns | none |
| after, a null context | 4.76 ns | none |

The time is the latency of a select on the argument and of one on the
result. The thunk returns the canonical NaN as every function whose address
the module takes does. `float_work` and `pythagoras_geometry` against the same
programs without the selects, the median of 21 runs: 1.01 and 1.00, 1.10
and 1.22. They were 1.01 and 1.00, 1.10 and 1.23.

## Suites

The host suite ran in three parts with `-j14` before `68f2f6a1`. The first
run, 381 s, 266 s and 284 s up to test 6953, failed `guide_examples` on the
excerpt above. After the fix the whole suite ran again, 395 s, 266 s and
326 s, and passed. The two `mimalloc_environment` tests skipped each time.

Each sanitizer suite ran its 6951 tests at `68f2f6a1` in six parts,
`ctest -I 1,1150` to `-I 6001,6951`: `asan` 250 s, 190 s, 261 s, 194 s,
447 s and 448 s, `ubsan` 252 s, 187 s, 260 s, 211 s, 465 s and 468 s.
Every test passed, with the two skips of `mimalloc_environment`. The VMs
stayed untouched, so no suite ran on Linux or Windows.

## Provisional lines

- An exported function that wraps a callback of C is an `export fn` or a
  public function of an `export class`. A call from Anti takes the thunk as
  well, and the thunk returns the canonical NaN of a float result.

## Questions for Eddie

1. T211: route (b) gives the code of the debug link on macos-arm64 alone,
   and on macos-x86_64 for a program that exports nothing. Which route now?
   (a) A release of `anti-lang/llvm-tools` with `llvm-objcopy`, as
   `llvm-strip` too, and `dsymutil`, which travel in every package.
   (b') The LTO link keeps its object, and the debug link of the archive
   and the program are both links of that object, with and without the
   debug information. That form gave identical code on macOS and Linux in
   the probe. Windows needs `/lldsavetemps` to keep the object, which
   writes four bitcode files beside it, and the `.def` of both links must
   name the shipped program. The LTO link then writes a program nobody
   ships, and each release build links three times.
2. Q3: the thunk returns the canonical NaN of a float result, which costs
   a second select per call. C reaches the thunk only through a callback of
   C that takes a value of two words. Should the thunk keep that select?
