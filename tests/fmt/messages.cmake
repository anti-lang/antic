# The messages of `anti fmt`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

# The file ends inside a string, so the lexer refuses it.
anti_message(does_not_lex
    FILES open.anti "fn main() -> int\n{\n\tlet s = \"open;\n\treturn 0;\n}\n"
    ARGS fmt open.anti
    ERR "anti: open.anti does not lex and stays as it is")
