#!/usr/bin/env python3
# Measures candidate facts of the LLVM text, as "How a fact is measured" of
# docs/work-order-llvm-optimization.md describes. It builds one program of
# tests/bench, or of this directory, in release mode once per build, runs
# every build and the C twin alternately, and prints the median of each in
# a table, then the object size, the executable size and the median release
# compile of each build.
#
#   tests/bench/ablate/run.py map_work 'opt-inline --no-hooks'
#   tests/bench/ablate/run.py map_work --table D
#   tests/bench/ablate/run.py mixed_work -- --no-hooks
#
# A build is a list of words. A word that starts with `-` is an option of
# antic, any other word names a wrapper of this directory, and `+` joins
# two wrappers as a space does. The wrappers run in the order given, each
# passing the text it edited to the next, and the last one to the pinned
# opt of the runtime directory. A build of one antic option alone stands
# after `--`, which ends the options of the script. The baseline, the build with neither, is
# always the first and the base of the column "To baseline".
#
# Every build must print the line of the C twin, bench_<program> of the C
# directory, since the two otherwise do different work. A program without
# a twin, such as mixed_work, must print the line of its baseline.
#
# The shell wrappers need a POSIX shell, so the script runs on macOS and
# Linux.
import argparse
import os
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BENCH = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(BENCH))
WINDOWS = sys.platform == "win32"
EXE = ".exe" if WINDOWS else ""
OBJECT = ".obj" if WINDOWS else ".o"

# The builds of run A to run D of the work order, after the baseline, and
# the C programs each run times beside the twin.
TABLES = {
    "A": (["--no-hooks", "--no-checks", "opt-inline",
           "opt-inline --no-hooks"], []),
    "B": (["opt_hash.py", "opt-inline", "opt-inline opt_hash.py"], []),
    "C": (["opt_m1.sh", "opt-inline", "opt-inline opt_m1.sh"],
          ["map_work_rtseed"]),
    "D": (["opt_noreturn.py", "opt_noreturn.py opt_hash.py", "opt-inline",
           "opt-inline opt_hash.py"], []),
}


class Build:
    """One way to compile the program: its wrappers and antic options."""

    def __init__(self, spec, index):
        self.options = []
        self.wrappers = []
        for word in spec.split():
            if word.startswith("-"):
                self.options.append(word)
                continue
            for name in word.split("+"):
                path = os.path.join(HERE, name)
                if not os.path.isfile(path):
                    sys.exit("run.py: no wrapper %s in %s" % (name, HERE))
                self.wrappers.append(path)
        words = [os.path.basename(w) for w in self.wrappers] + self.options
        self.label = " and ".join("`%s`" % w for w in words) or "baseline"
        self.index = index
        self.compiles = []
        self.times = []

    def compile(self, args, source, output):
        command = [args.antic] + self.options
        env = dict(os.environ)
        if self.wrappers:
            command += ["--opt", self.wrappers[0]]
            env["ANTI_ABLATE_RUNTIME"] = args.runtime
            env["ANTI_ABLATE_NEXT"] = os.pathsep.join(self.wrappers[1:])
        command += ["--runtime", args.runtime, "-o", output, source]
        start = time.perf_counter_ns()
        run = subprocess.run(command, env=env, capture_output=True)
        took = time.perf_counter_ns() - start
        if run.returncode != 0:
            sys.stderr.buffer.write(run.stdout + run.stderr)
            sys.exit("run.py: build %s failed with %d"
                     % (self.label, run.returncode))
        self.compiles.append(took)


def timed(executable):
    """The time a run of executable took in nanoseconds, and its output."""
    start = time.perf_counter_ns()
    run = subprocess.run([executable], capture_output=True)
    took = time.perf_counter_ns() - start
    if run.returncode != 0:
        sys.exit("run.py: %s exited with %d" % (executable, run.returncode))
    return took, run.stdout


def milliseconds(times):
    return "%.1f ms" % (statistics.median(times) / 1e6)


def ratio(times, base):
    return "%.2f" % (statistics.median(times) / statistics.median(base))


def size(path):
    return "{:,} B".format(os.path.getsize(path))


def find_program(name):
    for directory in (BENCH, HERE):
        path = os.path.join(directory, name + ".anti")
        if os.path.isfile(path):
            return path
    sys.exit("run.py: no program %s.anti in %s or %s" % (name, BENCH, HERE))


def main():
    build_dir = os.path.join(ROOT, "build", "host")
    parser = argparse.ArgumentParser(
        description="Time builds of a program of tests/bench.")
    parser.add_argument("program", help="name of the program, as map_work")
    parser.add_argument("builds", nargs="*",
                        help="wrappers and antic options of a build")
    parser.add_argument("--table", choices=sorted(TABLES),
                        help="the builds of run A to D of the work order")
    parser.add_argument("--antic", default=os.path.join(build_dir, "antic" + EXE))
    parser.add_argument("--runtime", default=os.path.join(build_dir, "runtime"),
                        help="the runtime directory, which holds bin/opt")
    parser.add_argument("--c-dir", default=os.path.join(build_dir, "tests"),
                        help="the directory of bench_<program>")
    parser.add_argument("--twin", action="append", default=[],
                        help="another C program bench_<name> to time")
    parser.add_argument("--work", help="a directory under build/ for builds")
    parser.add_argument("--runs", type=int, default=15)
    parser.add_argument("--compiles", type=int, default=3)
    parser.add_argument("--output", help="a file that receives the tables")
    args = parser.parse_args()
    args.antic = os.path.abspath(args.antic)
    args.runtime = os.path.abspath(args.runtime)
    source = find_program(args.program)
    work = os.path.abspath(args.work or os.path.join(
        ROOT, "build", "ablate", args.program))
    os.makedirs(work, exist_ok=True)

    specs = list(args.builds)
    twins = list(args.twin)
    if args.table:
        specs = TABLES[args.table][0] + specs
        twins = TABLES[args.table][1] + twins
    builds = [Build("", 0)]
    builds += [Build(spec, i + 1) for i, spec in enumerate(specs)]

    outputs = {}
    for _ in range(args.compiles):
        for b in builds:
            output = os.path.join(work, "%d" % b.index, args.program)
            os.makedirs(os.path.dirname(output), exist_ok=True)
            b.compile(args, source, output)
            outputs[b.index] = output

    twin_path = os.path.join(args.c_dir, "bench_" + args.program + EXE)
    has_twin = os.path.isfile(twin_path)
    others = [("C twin", twin_path, [])] if has_twin else []
    for name in twins:
        path = os.path.join(args.c_dir, "bench_" + name + EXE)
        if not os.path.isfile(path):
            sys.exit("run.py: no C program %s" % path)
        others.append(("C `%s`" % name, path, []))
    # The first run of a new executable on macOS waits for the check of its
    # signature and takes more than twice as long, so each program runs
    # once untimed, which checks its line as well.
    _, reference = timed(twin_path if has_twin else outputs[0] + EXE)
    paths = [path for _, path, _ in others]
    paths += [outputs[b.index] + EXE for b in builds]
    for path in paths:
        _, line = timed(path)
        if line != reference:
            sys.exit("run.py: %s prints %r, not %r" % (path, line, reference))

    for _ in range(args.runs):
        for b in builds:
            took, line = timed(outputs[b.index] + EXE)
            if line != reference:
                sys.exit("run.py: build %s prints %r, not %r"
                         % (b.label, line, reference))
            b.times.append(took)
        for _, path, times in others:
            took, _ = timed(path)
            times.append(took)

    base = builds[0].times
    c_times = others[0][2] if has_twin else None
    lines = ["%s, %d runs each, prints %s" % (
        args.program, args.runs, reference.decode().strip()), ""]
    if c_times is not None:
        lines += ["| Build | Median | Anti / C | To baseline |",
                  "|---|---:|---:|---:|"]
    else:
        lines += ["| Build | Median | To baseline |", "|---|---:|---:|"]
    rows = [(b.label, b.times) for b in builds]
    rows += [(label, times) for label, _, times in others]
    for label, times in rows:
        cells = [label, milliseconds(times)]
        if c_times is not None:
            cells.append(ratio(times, c_times))
        cells.append(ratio(times, base))
        lines.append("| " + " | ".join(cells) + " |")
    lines += ["", "| Build | Object | Executable | Release compile |",
              "|---|---:|---:|---:|"]
    for b in builds:
        output = outputs[b.index]
        lines.append("| %s | %s | %s | %s |" % (
            b.label, size(output + OBJECT), size(output + EXE),
            milliseconds(b.compiles)))
    text = "\n".join(lines) + "\n"
    sys.stdout.write(text)
    if args.output:
        with open(args.output, "w") as out:
            out.write(text)


main()
