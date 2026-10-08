# mimalloc without statistics, and the other forms of `anti license`

Eddie's answers to the two questions of `2026-10-08-mimalloc-notices.md`.

## 1. No detailed statistics (`ff252d4c`)

`ANTIC_MIMALLOC_DEFINES` passes `-DMI_STATS=0` where mimalloc's Release build gives 1.
The entry of `docs/decisions.md` on the definitions of `src/static.c` lost the
sentence that kept them on, and a new entry records the decision.
`musl_allocator_<target>` now refuses a program of musl that holds `binned` or
`malloc req~`. `mi_stats_print` writes these two labels for the detailed statistics
alone. Before the change both labels stood in every program the test links, and the
test failed on both targets. After it, it passes.

`MI_STATS=0` keeps the few counters that `include/mimalloc/types.h` names for level 0
and drops the detailed ones. `MI_PROFILE=1` stays. Profiling starts only once a program
sets a profiler on a heap through mimalloc's API, which no Anti program calls.

The last report said the change saves about 5 KB per program. It does not. The five
programs of `tests/bench/` at linux-arm64, linked against the old and the new library:

| Program | Before | After |
|---|---:|---:|
| `scalar_loop` | 246,688 B | 248,200 B |
| `objects` | 284,624 B | 286,072 B |
| `builder` | 288,136 B | 289,584 B |
| `simd_loop` | 246,816 B | 248,264 B |
| `map_work` | 447,280 B | 448,792 B |

`_mi_page_update_stats` (1,636 B) goes, `_mi_stats_print` shrinks by 864 B and the
printers of the counters by about 420 B. With that much less code, the inliner takes
about 4.3 KB more into `_mi_thread_done`, so `.text` grows by 1,744 B. The 5 KB of
the last report was the difference of the object, not of a linked program.

## 2. The other forms of `anti license` (`dfc3e2ed`)

The plain form, `--project`, `--project --notice` and `--from-archive` are item 37 of
"First sessions" in `CLAUDE.md` and item 11 of
`2026-09-27-distribution-target.md`. An entry in `docs/decisions.md` records that
they wait for the distribution work.

## Questions for Eddie

1. The statistics are gone and the programs are 1.5 KB larger. Should the size be
   looked at, for example by checking why the inliner grows `_mi_thread_done`, or does
   it stand?
2. `MI_PROFILE=1` keeps sampled profiling. A program can reach it only through
   mimalloc's API, which no Anti program calls. That is the same kind of unreachable
   code as the statistics. Should it go too?

## Gates

At `dfc3e2ed` the host suite passed 1692 of 1692 in 232 s, ASan 1691 of 1691 in 465 s
and UBSan 1691 of 1691 in 321 s, with `-j14` and no warnings from our code. A first
ASan run took 554 s while anti-linux ran its suite on the same Mac, and the run alone
took 465 s. anti-linux passed 1622 of 1622 in 251 s, `musl_allocator_<target>` and
`mimalloc_environment_<target>` among them.

## Decisions

Eddie decided on 2026-10-08. The size stands, since 1.5 KB on a program of 250 KB is not worth a study of the inliner. `MI_PROFILE` goes the way of `MI_STATS`, with `-DMI_PROFILE=0` in `ANTIC_MIMALLOC_DEFINES` and a check of `musl_allocator_<target>` that no symbol of `src/sample-profile.c` stays in a program of musl. Both stand in `docs/decisions.md` after the entry on `MI_STATS`. The change itself is a step of a driver.
