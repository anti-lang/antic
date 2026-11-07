#!/usr/bin/env python3
# Measurement wrapper of the built form of "One NaN on every target" in
# docs/notes/math.md, form B of Q1 of "Answers of 2026-11-06, fourth set".
# It takes the canonical NaN out of the LLVM text that antic writes: each
# select of the canonical NaN gets the condition false, so opt folds it to
# the value it guards and drops its fcmp. The baseline of run.py is then
# the program with the rule, and this build the program without it.
import re

import nan_text

SCALAR = re.compile(r"^(\s*%[\w.]+ = select )i1 %[\w.]+(, (?:float|double) "
                    + nan_text.CANONICAL + r", .*)$")
VECTOR = re.compile(r"^(\s*%[\w.]+ = select )<(\d+) x i1> %[\w.]+(, <\d+ x "
                    r"(?:float|double)> <(?:float|double) "
                    + nan_text.CANONICAL + r".*)$")


def edit(lines):
    out = []
    for line in lines:
        m = SCALAR.match(line)
        if m:
            line = m.group(1) + "i1 false" + m.group(2)
        m = VECTOR.match(line)
        if m:
            line = m.group(1) + "<" + m.group(2) + " x i1> zeroinitializer" \
                + m.group(3)
        out.append(line)
    return out


nan_text.run(edit)
