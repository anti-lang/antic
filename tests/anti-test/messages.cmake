# The messages of `anti test`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

anti_message(no_source STATUS 2
    ARGS test --work "${WORK}/test" --runtime "${RUNTIME}"
    ERR "anti: test takes one or more .anti files")
