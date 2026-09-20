# Setup and the first executable

Choices made for the setup and the first executable. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- The sources of `anti_rt` live in `rt/` at the repository root. The main CMake build compiles `anti_rt` for the host into `build/runtime/lib/<target>/libanti_rt.a`, the layout of the runtime archive, with hidden visibility, and copies `rt/LICENSE` to `build/runtime/licenses/anti_rt.txt`.
- `rt/start.c` called a `main` without parameters until `strings.md` added strings and slices.
- Program tests: `tests/programs/NAME.anti` with `NAME.expected`. The expected file starts with the line `exit N`, and the bytes after it are the expected standard output. A test fails when antic prints anything.
