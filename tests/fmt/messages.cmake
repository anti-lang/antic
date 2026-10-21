# The messages of `anti fmt`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

# Without a file the command takes the source and the test directory,
# which hold no module here. M339 of the audit: the command formatted
# none in silence, where `anti check` refuses the project.
anti_message(no_source STATUS 2
    FILES src/notes.txt "No module.\n"
    ARGS fmt
    ERR "anti: no .anti file under src or test")
# A name that starts with a dot is hidden, and the walk of the source
# directory takes no such file, walk of files.c.
anti_message(no_source_hidden STATUS 2
    FILES src/com/example/.draft.anti "fn main() -> int\n{\n\treturn 0;\n}\n"
    ARGS fmt
    ERR "anti: no .anti file under src or test")
# The file ends inside a string, so the lexer refuses it.
anti_message(does_not_lex
    FILES open.anti "fn main() -> int\n{\n\tlet s = \"open;\n\treturn 0;\n}\n"
    ARGS fmt open.anti
    ERR "anti: open.anti does not lex and stays as it is")

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h.
# files_replace of files.c writes the formatted text beside the file,
# gives it the permissions of the file and its name. The name of the file
# stands on the standard output before the write.
set(unformatted "fn main() -> int\n{\n  return 0;\n}\n")
anti_message(fault_new_not_written
    FILES a.anti "${unformatted}"
    OUT "a.anti"
    FAULT open:2:EACCES
    ARGS fmt a.anti
    ERR "anti: cannot write a.anti.new")
anti_message(fault_permissions_not_copied
    FILES a.anti "${unformatted}"
    OUT "a.anti"
    FAULT write:2:EACCES
    ARGS fmt a.anti
    ERR "anti: cannot replace a.anti")
anti_message(fault_not_replaced
    FILES a.anti "${unformatted}"
    OUT "a.anti"
    FAULT rename:1:EACCES
    ARGS fmt a.anti
    ERR "anti: cannot replace a.anti")
# Without a file the command reads every source under the source
# directory, fmt_run of fmt.c, and lists the directory first,
# project_sources of main.c.
set(fmt_manifest "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")
set(formed "//! The smallest library.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
anti_message(fault_source_unread
    FILES anti.toml "${fmt_manifest}" src/com/example/a.anti "${unformatted}"
          src/com/example/one.anti "${formed}"
    FAULT open:5:EACCES
    ARGS fmt
    ERR "anti: cannot read src/com/example/a.anti")
anti_message(fault_listing_unread
    FILES anti.toml "${fmt_manifest}" src/com/example/a.anti "${unformatted}"
          src/com/example/one.anti "${formed}"
    FAULT read:6:EIO
    ARGS fmt
    ERR "anti: cannot read src")
anti_message(manifest_no_toml
    FILES anti.toml "name = \n" src/com/example/a.anti "${unformatted}"
    ARGS fmt
    ERR "anti: anti.toml is no TOML that anti reads")
