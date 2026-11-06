#!/usr/bin/env python3
# Measurement wrapper, the ARM64 half of option C of "One NaN on every
# target" in docs/notes/math.md: the bit DN of FPCR set at the start of
# main, so every operation of the processor gives the default NaN, which
# is the canonical one. The program of the measurement runs on one thread,
# and a build of the option sets the bit on every thread of the runtime.
import re

import nan_text

MAIN = re.compile(r"^define .*@[^\s(]+\.main\(")
SET_DN = ('  call void asm sideeffect "mrs x9, fpcr\\0Aorr x9, x9, #0x2000000'
          '\\0Amsr fpcr, x9", "~{x9}"()')


def edit(lines):
    out = []
    pending = False
    for line in lines:
        out.append(line)
        if MAIN.match(line):
            pending = True
        elif pending and line.endswith(":"):
            out.append(SET_DN)
            pending = False
    return out


nan_text.run(edit)
