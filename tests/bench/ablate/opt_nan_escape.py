#!/usr/bin/env python3
# Measurement wrapper, option B of "One NaN on every target" in
# docs/notes/math.md: the canonical NaN where a float leaves the registers
# of the program. That is a store of a float to memory that is no local
# of the function and a float argument of a function the module does not
# define, outside the runtime and the intrinsics of LLVM. A store of a
# value that a load from such memory gave is a copy of bits that stood in
# memory already, and stays as it is. A local of antic is an alloca, which
# opt turns into registers. nan_text.py holds the rest of the edit.
import re

import nan_text

ALLOCA = re.compile(r"^\s*(%[\w.]+) = alloca ")
LOAD = re.compile(r"^\s*(%[\w.]+) = load [^,]+, ptr (%[\w.]+)")
STORE = re.compile(r"^(\s*)store (float|double) (%[\w.]+), ptr (%[\w.]+)")
ARG = re.compile(r"(float|double) (%[\w.]+)")
CALL = re.compile(r"^(\s*)(?:%[\w.]+ = )?(?:tail )?call [^@]*@([^\s(]+)\((.*)\)")


def edit(lines):
    own = nan_text.defined(lines)
    out = []
    locals_ = set()
    loaded = set()
    fresh = [0]

    def fixed(indent, kind, value):
        fresh[0] += 1
        name = "%%nan.%d" % fresh[0]
        out.extend(nan_text.canonical(indent, name, value, kind))
        return name

    for line in lines:
        if line.startswith("define "):
            locals_, loaded = set(), set()
        a = ALLOCA.match(line)
        if a:
            locals_.add(a.group(1))
        ld = LOAD.match(line)
        if ld and ld.group(2) not in locals_:
            loaded.add(ld.group(1))
        s = STORE.match(line)
        if s and s.group(4) not in locals_ and s.group(3) not in loaded:
            name = fixed(s.group(1), s.group(2), s.group(3))
            line = line.replace("%s %s," % (s.group(2), s.group(3)),
                                "%s %s," % (s.group(2), name), 1)
        c = CALL.match(line)
        if c and c.group(2) not in own and \
                not c.group(2).startswith(("llvm.", "anti_rt_")):
            head, _, args = line.partition("@" + c.group(2) + "(")
            def repl(m):
                return "%s %s" % (m.group(1),
                                  fixed(c.group(1), m.group(1), m.group(2)))
            line = head + "@" + c.group(2) + "(" + ARG.sub(repl, args)
        out.append(line)
    return out


nan_text.run(edit)
