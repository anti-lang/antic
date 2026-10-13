# The messages of `anti doc`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

# Without a file the command takes the source directory, which holds no
# module here.
anti_message(no_source STATUS 2
    FILES src/notes.txt "No module.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
    ERR "anti: doc takes a source or a library file")
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
anti_message(source_imports_no_module
    FILES one.anti "//! A module.\n\nimport com.example.gone;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "one.anti:3:8: error: cannot find module `com.example.gone`")
