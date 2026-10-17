# The messages of `anti fmt`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

# Without a file the command takes the source and the test directory,
# which hold no module here. M339 of the audit: the command formatted
# none in silence, where `anti check` refuses the project.
anti_message(no_source STATUS 2
    FILES src/notes.txt "No module.\n"
    ARGS fmt
    ERR "anti: no .anti file under src or test")
# The file ends inside a string, so the lexer refuses it.
anti_message(does_not_lex
    FILES open.anti "fn main() -> int\n{\n\tlet s = \"open;\n\treturn 0;\n}\n"
    ARGS fmt open.anti
    ERR "anti: open.anti does not lex and stays as it is")
