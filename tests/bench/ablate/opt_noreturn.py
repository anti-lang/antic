#!/usr/bin/env python3
# Ablation wrapper: adds `noreturn cold` to anti_rt_exit and
# anti_rt_out_of_memory, and writes `unreachable` after each call of
# Error.fatal, slot 168 of the table, that ends a block which builds an
# anti.lang.NoneDereference. The text then passes on.
#
# antic runs a wrapper as `--opt`, with the options of opt and the path of
# the LLVM text last. The pinned opt is bin/opt of the runtime directory in
# ANTI_ABLATE_RUNTIME. ANTI_ABLATE_NEXT lists further wrappers, separated
# by the path separator, that run before it. run.py sets both. The edited
# text goes to the next program on its standard input, so no file is left
# behind.
import os
import re
import subprocess
import sys

# In each block that builds a NoneDereference, the indirect call through
# slot 168 is followed by a branch back to the normal path.
BLOCK = re.compile(
    r"(= call ptr \(\) @anti\.lang\.NoneDereference\.new\(\)\n(?:  [^\n]*\n)*?"
    r"  call void \(ptr\) %v\d+\(ptr %v\d+\)\n)  br label %[\w.]+\n")


def next_program():
    """The next wrapper of the chain, or the pinned opt."""
    chain = os.environ.get("ANTI_ABLATE_NEXT", "")
    if chain:
        first, _, rest = chain.partition(os.pathsep)
        os.environ["ANTI_ABLATE_NEXT"] = rest
        return first
    return os.path.join(os.environ["ANTI_ABLATE_RUNTIME"], "bin", "opt")


def end_block(match):
    if "i64 168" not in match.group(1):
        sys.exit("opt_noreturn.py: a NoneDereference block calls no slot 168")
    return match.group(1) + "  unreachable\n"


def main():
    args = sys.argv[1:]
    source = args[-1]
    text = sys.stdin.read() if source == "-" else open(source).read()
    declarations = 0
    for name, kind in (("anti_rt_exit", "i32"),
                       ("anti_rt_out_of_memory", "i64")):
        pattern = re.compile(r"^(declare void @%s\(%s\))" % (name, kind), re.M)
        text, k = pattern.subn(r"\1 noreturn cold", text)
        declarations += k
    if declarations != 2:
        sys.exit("opt_noreturn.py: %d of the 2 declarations found"
                 % declarations)
    text, sites = BLOCK.subn(end_block, text)
    sys.stderr.write("opt_noreturn.py: %d fatal sites\n" % sites)
    run = subprocess.run([next_program()] + args[:-1] + ["-"],
                         input=text, text=True)
    sys.exit(run.returncode)


main()
