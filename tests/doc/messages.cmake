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
