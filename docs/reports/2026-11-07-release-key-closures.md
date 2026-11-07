# Release calls, the key of a program and calls of closures

Eddie answered the three questions of
`docs/reports/2026-11-07-reuse-and-nan.md` with the recommendations of the
report. The session recorded the answers and built Q2. Q1 is not built,
by its own conditions. Q3 is not built and waits for a question below.

## What changed

- `f00775dd` records the answers as "Answers of 2026-11-07" in
  `docs/decisions.md`, with the line in the entry of the feature freeze.
  No rule of the language changes, so the specifications and the overview
  stay as they are.
- `59df540c` builds Q2. The key of a program and of a runner of
  `anti test` holds a line `archive <path> <digest>` for every object and
  archive of `lib/<target>/`, of `lib/<target>/<level>/` and below, and of
  the library directory of the sysroot, in the glibc mode of Linux as
  well. The four tests `anti_reuse_<command>_<mode>_native` add a member
  to `libraylib.a` of a stand-in runtime archive. All four failed before
  the change with "the second build kept project" or "the second run kept
  obj/shapes/geometry.runner", and pass after it.
- The reads of those files go uncounted by the fault seam, through
  `anti_rt_fault_hold`, since their number follows the host. Nine fault
  cases of `tests/anti-build/messages.cmake` count one open less, because
  the old read of `libanti_rt.a` went through the seam.

The key now reads 11 files of 8.7 MB on macos-arm64 and 56 files of
40.7 MB for linux-arm64. A release build with nothing changed took
0.08 s before and takes 0.18 s on the Mac, and 0.61 s for linux-arm64.
The Debug build of `anti` hashes about 80 MB/s.

## Q1: one call for the program and its symbols

Each figure is the median of 11 runs in a scratch copy of
`build/pythagoras-tree`, with the anti and antic of `build/host`. The
change of one module adds a public function to `tree.geometry`. A comment
alone changes no library file, so the program is reused.

| Run | Time |
|---|---:|
| first `anti build --release` | 2.464 s |
| after a change of one module | 2.478 s |
| nothing changed | 0.178 s |
| the call of antic without `-g` | 0.992 s |
| the call with `-g`, for the symbols | 1.009 s |
| opt in one call, without and with `-g` | 0.169 s, 0.177 s |
| ld64.lld with full LTO, without and with `-g` | 0.774 s, 0.789 s |
| front end and lowering, `--dump-ir` | 0.023 s |
| the same and the LLVM text, `--dump-llvm` | 0.048 s |
| both calls started at once | 1.029 s |

Not built, for two reasons.

- One call cannot write the program of today from the code of the
  symbols. `-g` changes the code that the LTO of lld writes. In the tree,
  332 of the 2239 functions of macos-arm64 stand at another address in
  the debug link than in the program, and `__unwind_info` holds 8428
  bytes there against 4320. On linux-arm64, 632 of 2214 functions moved,
  and `.text` is 16 bytes longer. A program stripped from the debug link
  would differ from the program the two calls write.
- What one call can share and stay byte-identical is the front end and
  lowering, 0.023 s of 2.46 s. The two texts, opt and the two LTO links
  differ by `-g`. One percent lies inside the spread of the runs, so the
  call would not be measurably faster.

Reuse of release code per module would keep the front end and opt of the
unchanged modules. Both make up at most 0.048 s and 0.17 s of each call,
so the gain is 0.4 s of 2.48 s at most, 18 percent. The two LTO links,
1.56 s, stay.

## Q3: calls through a function value with a context

Not built. A value of two words can hold a function of C with the context
`none`. This program passes `fabsf` of C to a parameter of two words, and
`apply` calls it through the value:

```anti
extern fn fabsf(x: f32) -> f32;

fn apply(f: fn(f32) -> f32, x: f32) -> f32
{
	return f(x);
}
```

antic compiles it, and the call in `apply` reaches C. A one-word field, a
global or a result that C filled converts the same way. Built as
answered, such a call would hand C the NaN of the processor. That breaks
the rule "a float that C takes" of `docs/anti-language-additions.md`. The
provisional line on calls through an address stays as it was.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and
`-I 4301,6942` with `-j14`. The first run took 387 s, 270 s and 322 s,
and `raw_output` and `anti_interface` failed. The new runner captured the
output of llvm-ar without `ENCODING NONE`, and `cache.c` included
`linker.h`. The second run, 382 s, 271 s and 320 s, failed `raw_output`
once more, for the name of the variable of llvm-ar. The third run took
376 s, 270 s and 313 s and passed all 6942 tests, with the two skips of
`mimalloc_environment`.

Each sanitizer suite ran its 6940 tests at `59df540c` in six parts,
`ctest -I 1,1150` to `-I 6001,6940`: `asan` 249 s, 188 s, 257 s, 194 s,
445 s and 456 s, `ubsan` 257 s, 189 s, 259 s, 201 s, 471 s and 509 s.
Every test passed, with the same two skips.

The VMs stayed untouched, so no suite ran on Linux or Windows.

## Provisional lines

- The key names the objects and archives of `lib/<target>/`, of
  `lib/<target>/<level>/` and below, and of the library directory of the
  sysroot, in both modes of Linux. Shared libraries and stubs of a
  sysroot, `.so` and `.tbd`, are not named. A library for C links on
  every build.
- The reads of the key go uncounted by the fault seam, through
  `anti_rt_fault_hold`.

## Questions for Eddie

1. Q3: a value of two words may hold a function of C, as `fabsf` above.
   Should the call through it keep the select, which takes a test of the
   context word at each float? Should a plain function of C reach the
   form of two words through a thunk that takes the select? Or should C
   lose the canonical NaN there?
2. The program of a release build and the debug link of its symbols
   archive differ in code, as Q1 shows. The map comes from the debug link.
   A frame of a moved function would then resolve to what stands at its
   address in the debug link. The session did not run that case. Is it a
   defect to record and fix? A program stripped
   from the debug link would fix it, and it changes the bytes of every
   release program.
3. Both release calls started at once took 1.03 s against 2.0 s one
   after the other, with the same bytes. Should `anti build --release`
   run the two at once?
4. The key reads 40.7 MB for a Linux target, 0.61 s in the Debug build
   with nothing changed. Should it keep the digest of each archive by its
   size and time of change, and hash a file again only when those change?
