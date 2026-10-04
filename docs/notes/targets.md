# Targets, object formats and ABIs

Choices made for targets, object formats and ABIs. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Chapter 11 target table in `src/antic/target.c`. Each row holds the operating system, processor, object format, calling convention, triple and file suffixes. Triples: `x86_64-unknown-linux-gnu`, `aarch64-unknown-linux-gnu`, `x86_64-apple-macos`, `arm64-apple-macos`, `x86_64-pc-windows-msvc`, `aarch64-pc-windows-msvc`. Windows uses `.obj` and `.exe`. `--print-targets` prints the matrix, and `target_mangle` chooses the symbol form by object format.
- `src/antic/llvm_target.c` holds what the LLVM back end writes per target and per level: the triple of `target.c` with the minimum macOS version appended, the data layout string, the relocation model, `pic` on Linux and macOS and `static` on Windows, and the `"target-cpu"` and `"target-features"` attributes. The data layout and the feature strings are copied from the output of the pinned clang, and `llvm_datalayout_pin` compares them with it on every run.
