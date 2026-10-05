#!/usr/bin/env python3
# Ablation wrapper: replaces the declaration of anti_rt_hash_seeded with an
# internal definition of mix(hash ^ mix(with)), the finalizer of MurmurHash3
# that src/rt/hash.h names, and passes the text on.
#
# antic runs a wrapper as `--opt`, with the options of opt and the path of
# the LLVM text last. The pinned opt is bin/opt of the runtime directory in
# ANTI_ABLATE_RUNTIME. ANTI_ABLATE_NEXT lists further wrappers, separated
# by the path separator, that run before it. run.py sets both. The edited
# text goes to the next program on its standard input, so no file is left
# behind.
import os
import subprocess
import sys

DEFINITION = """define internal i64 @anti_rt_hash_seeded(i64 %h, i64 %w) nounwind willreturn memory(none) {
  %a1 = lshr i64 %w, 33
  %a2 = xor i64 %w, %a1
  %a3 = mul i64 %a2, -49064778989728563
  %a4 = lshr i64 %a3, 33
  %a5 = xor i64 %a3, %a4
  %a6 = mul i64 %a5, -4265267296055464877
  %a7 = lshr i64 %a6, 33
  %a8 = xor i64 %a6, %a7
  %x = xor i64 %h, %a8
  %b1 = lshr i64 %x, 33
  %b2 = xor i64 %x, %b1
  %b3 = mul i64 %b2, -49064778989728563
  %b4 = lshr i64 %b3, 33
  %b5 = xor i64 %b3, %b4
  %b6 = mul i64 %b5, -4265267296055464877
  %b7 = lshr i64 %b6, 33
  %b8 = xor i64 %b6, %b7
  ret i64 %b8
}
"""
DECLARATION = "declare i64 @anti_rt_hash_seeded(i64, i64) #1\n"


def next_program():
    """The next wrapper of the chain, or the pinned opt."""
    chain = os.environ.get("ANTI_ABLATE_NEXT", "")
    if chain:
        first, _, rest = chain.partition(os.pathsep)
        os.environ["ANTI_ABLATE_NEXT"] = rest
        return first
    return os.path.join(os.environ["ANTI_ABLATE_RUNTIME"], "bin", "opt")


def main():
    args = sys.argv[1:]
    source = args[-1]
    text = sys.stdin.read() if source == "-" else open(source).read()
    if text.count(DECLARATION) != 1:
        sys.exit("opt_hash.py: no declaration of anti_rt_hash_seeded")
    text = text.replace(DECLARATION, DEFINITION)
    run = subprocess.run([next_program()] + args[:-1] + ["-"],
                         input=text, text=True)
    sys.exit(run.returncode)


main()
