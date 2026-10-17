# The messages of `anti doc`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

# Without a file the command takes the source directory, which holds no
# module here. The sources of a project come from the one function that
# `anti check` and `anti fmt` take them from, M339 of the audit.
anti_message(no_source STATUS 2
    FILES src/notes.txt "No module.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
    ERR "anti: no .anti file under src")
foreach(items dev private)
    anti_message(${items}_of_a_library
        FILES one.antl "A library file, which holds the public interface alone.\n"
        ARGS doc --${items} -o "${WORK}/pages" --runtime "${RUNTIME}" one.antl
        ERR "anti: one.antl: --${items} needs the source")
endforeach()
# A library file is read as the driver of antic reads one.
anti_message(library_damaged
    FILES one.antl "A text where the library file belongs.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.antl
    ERR "antic: one.antl is not a library file")
# A source goes through the front end, and its first mistake ends the
# command. The second case shows T123 of docs/audit/tests-findings.md.
anti_message(source_name_no_identifier
    FILES Bad-Name.anti "//! A module whose file has no name of a module.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" Bad-Name.anti
    ERR "anti: `Bad-Name` in Bad-Name.anti is not a lowercase identifier")
anti_message(source_does_not_parse
    FILES one.anti "fn main( -> int\n{\n\treturn 0;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "one.anti:1:10: error: expected identifier")
# Two modules that import each other have no order, by R125 of "Rules of
# 2026-10-13" in docs/decisions.md.
anti_message(modules_import_each_other
    FILES src/com/example/a.anti "//! The first.\n\nimport com.example.b;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
    ERR "anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a")
# R179 of "Rules of 2026-10-13" in docs/decisions.md: a fence that nothing
# closes gives a warning, and the page is written.
anti_message(fence_never_closed STATUS 0
    FILES one.anti "//! A module.\n\n/// One.\n/// ```anti\n/// let n = one();\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "doc: 1 module into ${WORK}/pages"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "anti: one.anti: warning: the doc comment of `one` holds a fence that nothing closes")
anti_message(module_fence_never_closed STATUS 0
    FILES one.anti "//! A module.\n//! ```\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "doc: 1 module into ${WORK}/pages"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "anti: one.anti: warning: the doc comment of the module holds a fence that nothing closes")
anti_message(source_imports_no_module
    FILES one.anti "//! A module.\n\nimport com.example.gone;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "one.anti:3:8: error: cannot find module `com.example.gone`")
