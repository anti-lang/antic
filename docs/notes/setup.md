# Setup and the first executable

Choices made for the setup and the first executable. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- The sources of `anti_rt` live in `src/rt/`. The main CMake build compiles `anti_rt` for every target whose sysroot it holds, the host's own target among them, through `anti_cross_runtime` of `CMakeLists.txt`. Each level of a target gets `build/host/runtime/lib/<target>/<level>/libanti_rt.a`, or `anti_rt.lib` for Windows, in the layout of the runtime archive. No rule passes `-fvisibility`, so the symbols keep the default visibility that `docs/notes/plugins.md` gives the reason of. The build copies `src/rt/LICENSE` to `build/host/runtime/licenses/anti_rt.txt`.
- `src/rt/start.c` called a `main` without parameters until `strings.md` added strings and slices.
- Program tests: `tests/programs/NAME.anti` with `NAME.expected`. The expected file starts with the line `exit N`, and the bytes after it are the expected standard output. A test fails when antic prints anything.
