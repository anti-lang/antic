# The messages of `anti test`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

anti_message(no_source STATUS 2
    ARGS test --work "${WORK}/test" --runtime "${RUNTIME}"
    ERR "anti: test takes one or more .anti files")

# Fault injection, through the variable ANTI_FAULT of src/rt/fault.h. T153:
# the work directory, which anti test leaves without a message. The work
# directory is relative, so the count of its directories is the same on
# every machine.
anti_message(fault_work_not_made
    FILES com/example/twice.anti "//! Doubling.\n\n/// Twice n.\npub fn twice(n: int) -> int\n{\n\treturn n * 2;\n}\n\ntests\n{\n\tfn doubles()\n\t{\n\t\tlet n = twice(2);\n\t\tassert(n == 4);\n\t}\n}\n"
    FAULT mkdir:2:EACCES
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "anti: cannot make ../test")
