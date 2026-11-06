# The edits of the LLVM text that the wrappers opt_nan_ops.py,
# opt_nan_escape.py and opt_nan_dn.py make, the three forms of one NaN on
# every target that "One NaN on every target" in docs/notes/math.md
# measures. Each wrapper is a measurement and no part of antic.
#
# The canonical NaN is the quiet NaN with a clear sign and no payload,
# 0x7fc00000 in f32 and 0x7ff8000000000000 in f64. The LLVM text writes
# both as the hex of a double.
import os
import re
import subprocess
import sys

CANONICAL = "0x7FF8000000000000"
FLOATS = ("float", "double")

# A result that can be a new NaN: an arithmetic operation, a conversion
# between the two widths and a call of a function that antic does not
# define, such as the libm of the runtime. A negation, an absolute value
# and a copysign change the sign alone, which is the same on every
# target, so they stay as they are.
OPERATION = re.compile(
    r"^(\s*)(%[\w.]+) = (fadd|fsub|fmul|fdiv|frem) (float|double) ")
CONVERSION = re.compile(
    r"^(\s*)(%[\w.]+) = (fptrunc|fpext) \w+ [^ ]+ to (float|double)$")
CALL = re.compile(r"^(\s*)(%[\w.]+) = (?:tail )?call (float|double) "
                  r"(?:\([^)]*\) )?@([^\s(]+)\(")
SIGN_ONLY = ("llvm.fabs.", "llvm.copysign.", "anti_rt_fabs", "anti_rt_copysign")
DEFINE = re.compile(r"^define .*@([^\s(]+)\(")
DECLARE = re.compile(r"^declare .*@([^\s(]+)\(")


def next_program():
    """The next wrapper of the chain, or the pinned opt."""
    chain = os.environ.get("ANTI_ABLATE_NEXT", "")
    if chain:
        first, _, rest = chain.partition(os.pathsep)
        os.environ["ANTI_ABLATE_NEXT"] = rest
        return first
    return os.path.join(os.environ["ANTI_ABLATE_RUNTIME"], "bin", "opt")


def defined(lines):
    """The functions that the text defines."""
    return {m.group(1) for m in map(DEFINE.match, lines) if m}


def canonical(indent, name, raw, kind):
    """The lines that give name the canonical NaN where raw is a NaN."""
    return ["%s%s.uno = fcmp uno %s %s, 0.0" % (indent, name, kind, raw),
            "%s%s = select i1 %s.uno, %s %s, %s %s"
            % (indent, name, name, kind, CANONICAL, kind, raw)]


def run(edit):
    """Edit the text of the last argument and pass it to the next program."""
    args = sys.argv[1:]
    source = args[-1]
    text = sys.stdin.read() if source == "-" else open(source).read()
    lines = edit(text.split("\n"))
    done = subprocess.run([next_program()] + args[:-1] + ["-"],
                          input="\n".join(lines), text=True)
    sys.exit(done.returncode)
