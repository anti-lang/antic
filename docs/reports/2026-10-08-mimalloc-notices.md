# The notices of musl and mimalloc, and the environment of mimalloc

The three follow-ups of `2026-10-08-allocator-and-alignment.md` that Eddie decided on
2026-10-08.

## 1. musl and mimalloc in `anti_licenses` (`371a717c`)

The notice of every program of musl names `musl 1.2.6 MIT` and `mimalloc 3.5.3 MIT`
after the runtime, with their texts from `licenses/` of the runtime archive.
`tools/libc-versions.cmake` reads the two versions from `tools/sysroot-pins` and
`tools/mimalloc-pin` for antic, the packer and the tests. A program of the glibc mode,
any other target and every shared library name neither. The back end now builds the
notice, since whether a program hosts plugins, and so links glibc, is known only after
lowering.

`anti license` and `NOTICE.txt` did not exist. CLAUDE.md counts an unbuilt rule of the
specifications as work, so this step builds the form `anti license --from <binary>`. It
prints the notice without the markers and the build id. `anti build` writes the same text
as `NOTICE.txt` beside a program or a shared library in `dist/<target>/<mode>/`.
The test `license_notice` checks both on a program of musl, one of glibc and one of
macOS. On a Linux host `std_license` takes `license.musl.expected`, and
`run_licenses.cmake` expects the two packages.

## 2. No `MIMALLOC_*` variables (`08e2dd75`)

`-DMI_NO_GETENV=1` joins `ANTIC_MIMALLOC_DEFINES`. The sentence on `MIMALLOC_ALLOW_THP`
left the comment and the entry in `docs/decisions.md`. Before the define,
`mimalloc_environment_linux-arm64` and `_linux-x86_64` failed on anti-linux with the
options and statistics of mimalloc on standard error. After it both pass.

`tests/bench/ablate/run.py` on anti-linux at linux-arm64 after the change, 15 runs each,
beside the mimalloc column of the last report:

| Program | Before | After | Executable after |
|---|---:|---:|---:|
| `scalar_loop` | 314.8 ms | 314.1 ms | 246,688 B |
| `objects` | 106.0 ms | 105.5 ms | 284,624 B |
| `builder` | 48.6 ms | 48.9 ms | 288,136 B |
| `simd_loop` | 130.0 ms | 128.7 ms | 246,816 B |
| `map_work` | 155.3 ms | 160.0 ms | 447,280 B |
| `mixed_work` | 174.5 ms | 176.9 ms | 1,524,216 B |

Five programs stay within 1.4 percent. `map_work` is 3.0 percent slower. The define
changes no default, and the last report measured the same build at 157 ms, so the
difference is likely noise. Each executable grows by about 3.5 KB. The two licence texts
add 7.3 KB to the notice, and this step did not find where the difference goes.

## 3. The VMs (`27cee303`)

anti-linux has 6 cores, 16 GB of memory and a 256 GB disk, and its line said 4, 8 and
64. anti-windows has 4 cores and 16 GB, and its line said 8 GB. Both lines now say what
the machines have. The disk of anti-windows reads 518 GiB, a size UTM gives no setting
for, so its line keeps 128 GB.

## Provisional entries

`anti license --from` prints the text between the build id and the end marker, and
`NOTICE.txt` of `anti build` holds that text, written beside a program or a shared
library alone.

## Questions for Eddie

1. mimalloc keeps `MI_STATS=1`, and with `MI_NO_GETENV` no program can print the
   statistics any more. Should they go? Turning them off saves about 5 KB per program
   of musl.
2. Should the other forms of `anti license` follow: the plain form, `--project`,
   `--project --notice` and `--from-archive`? `NOTICE.txt` now comes from the notice of
   the binary rather than from `anti.lock`.

## Gates

At `27cee303` the host suite passed 1692 of 1692 in 274 s, ASan 1691 of 1691 in 463 s
and UBSan 1691 of 1691 in 318 s, with `-j14` and no warnings from our code. The six
`-Wattribute-alias` warnings stay in mimalloc's own `alloc-override.c`. anti-linux passed
1622 of 1622 with six skips, and anti-windows 1610 of 1610 with fourteen skips in 797 s
of `ctest -j4`, the two `mimalloc_environment` tests among them.

The first run of anti-linux failed `std_license`, `program_licenses` and
`program_licenses_std`, whose programs of the host are programs of musl there. Their
expectations are part of `371a717c`.
