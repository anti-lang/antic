# The messages of `anti check`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

set(check_manifest "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")

# A wrong use of the command ends with 2, as `anti doc` ends, by R160 of
# "Rules of 2026-10-13" in docs/decisions.md.
anti_message(no_source STATUS 2
    FILES anti.toml "${check_manifest}" src/notes.txt "No module.\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: no .anti file under src or test")
# Two modules that import each other have no order, by R125 of "Rules of
# 2026-10-13" in docs/decisions.md.
anti_message(modules_import_each_other
    FILES anti.toml "${check_manifest}"
          src/com/example/a.anti "//! The first.\n\nimport com.example.b;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a")
# The block names a function its module keeps private, which a user of
# the module does not reach.
anti_message(block_fails
    FILES anti.toml "${check_manifest}"
          src/com/example/hidden.anti "//! A user block reaches the public items alone.\n\n/// The doubled value of `n`.\n///\n/// ```anti\n/// let m = hidden.helper(3);\n/// ```\npub fn twice(n: int) -> int\n{\n\treturn helper(n) + helper(n);\n}\n\nfn helper(n: int) -> int\n{\n\treturn n;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "${WORK}/check/check/docs/com_example_hidden_0.anti:5:9: error: `hidden` has no public item `helper`"
        "anti: the `anti` block of `twice` at src/com/example/hidden.anti:3 failed, and its module is ${WORK}/check/check/docs/com_example_hidden_0.anti")

# The formatting class. Two files that each hold a line `anti fmt` writes
# otherwise are two findings.
anti_message(format_two_files
    FILES anti.toml "${check_manifest}"
          src/com/example/first.anti "//! A module.\n\n/// One.\npub fn one() -> int\n{\n  return 1;\n}\n"
          src/com/example/second.anti "//! A module.\n\n/// Two.\npub fn two() -> int\n{\n  return 2;\n}\n"
    OUT "anti check: front end: 2 files, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 2 files, 2 findings"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/first.anti:6:1: error: the line is not the one `anti fmt` writes"
        "src/com/example/second.anti:6:1: error: the line is not the one `anti fmt` writes")
# A file that goes on after the text `anti fmt` writes: the finding names
# the first line past it.
anti_message(format_empty_lines_at_the_end
    FILES anti.toml "${check_manifest}"
          src/com/example/long.anti "//! A module whose text ends with two empty lines.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n\n\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file, 1 finding"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/long.anti:8:1: error: the line is not the one `anti fmt` writes")

# The fence of a block. A space after the tag `anti` belongs to no tag,
# so the block compiles. A fence that no second one closes holds no block,
# and by R179 of "Rules of 2026-10-13" in docs/decisions.md it gives a doc
# warning.
anti_message(block_fence_with_a_space
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/spaced.anti "//! A module.\n\n/// One.\n///\n/// ```anti \n/// let n = spaced.one();\n/// ```\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}")
anti_message(block_fence_never_closed
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/open.anti "//! A module.\n\n/// One.\n///\n/// ```anti\n/// let n = nothing_here();\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 1"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/open.anti:3:1: warning: the doc comment of `one` holds a fence that nothing closes [doc-markup]")
anti_message(module_fence_never_closed
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/open.anti "//! A module.\n//!\n//! ```anti\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 1"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/open.anti:1:1: warning: the doc comment of the module holds a fence that nothing closes [doc-markup]")
