#!/usr/bin/env python3
# Measurement wrapper, option A of "One NaN on every target" in
# docs/notes/math.md: the canonical NaN after every operation that can make
# a NaN, an arithmetic operation, a conversion between f32 and f64 and the
# result of a call of a function the module does not define. nan_text.py
# holds the edit.
import re

import nan_text


def edit(lines):
    own = nan_text.defined(lines)
    out = []
    for line in lines:
        m = (nan_text.OPERATION.match(line) or nan_text.CONVERSION.match(line))
        kind = None
        if m:
            kind = m.group(4)
        else:
            c = nan_text.CALL.match(line)
            if c and c.group(4) not in own and \
                    not c.group(4).startswith(nan_text.SIGN_ONLY):
                m, kind = c, c.group(3)
        if m is None:
            out.append(line)
            continue
        indent, name = m.group(1), m.group(2)
        raw = name + ".raw"
        out.append(line.replace(name + " = ", raw + " = ", 1))
        out += nan_text.canonical(indent, name, raw, kind)
    return out


nan_text.run(edit)
