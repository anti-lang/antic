# The messages of `anti check`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

set(check_manifest "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")

anti_message(no_source
    FILES anti.toml "${check_manifest}" src/notes.txt "No module.\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: no .anti file under src or test")
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
