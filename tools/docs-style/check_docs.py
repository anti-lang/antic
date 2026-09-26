#!/usr/bin/env python3
"""Flag AI writing patterns in Markdown documents, the `.md` files.

A file of any other kind, named or found in a directory, is not read.

Usage:
    python3 check_docs.py FILE [FILE ...]
    python3 check_docs.py --strict docs/

Exit code 0 means no errors. Exit code 1 means at least one error.
Add "docs-style:ignore" to a line to skip that line.
Add "docs-style:ignore-file" to the first 10 lines to skip the whole file.
"""

import argparse
import json
import os
import re
import sys

# Characters that never appear in documentation.
BANNED_CHARS = [
    ("—", "em dash", "period, then a new sentence"),
    ("–", "en dash", "period, or 'to' in a range"),
    ("…", "ellipsis", "finish the sentence"),
    ("‘", "curly quote", "straight '"),
    ("’", "curly apostrophe", "straight '"),
    ("“", "curly quote", 'straight "'),
    ("”", "curly quote", 'straight "'),
    (" ", "non-breaking space", "regular space"),
    ("→", "arrow glyph", "a word"),
    ("←", "arrow glyph", "a word"),
    ("✓", "check glyph", "a word"),
    ("✗", "cross glyph", "a word"),
    ("•", "bullet glyph", "a '-' list marker"),
    ("★", "star glyph", "a word"),
    ("⚠", "warning glyph", "the word WARNING"),
]

EMOJI = re.compile(
    "[\U0001f000-\U0001faff\U00002600-\U000027bf\U0001f1e6-\U0001f1ff️⬀-⯿]"
)

BANNED_WORDS = [
    "crucial", "vital", "paramount", "essential", "imperative", "pivotal", "integral",
    "indispensable", "cornerstone", "robust", "seamless", "seamlessly", "powerful",
    "comprehensive", "elegant", "sleek", "intuitive", "delightful", "extensible",
    "scalable", "performant", "turnkey", "holistic", "meticulous", "meticulously",
    "painstaking", "intricate", "nuanced", "multifaceted", "leverage", "leverages",
    "leveraging", "utilize", "utilizes", "utilizing", "facilitate", "facilitates",
    "harness", "harnesses", "foster", "fosters", "bolster", "bolsters", "augment",
    "augments", "showcase", "showcases", "underscore", "underscores", "spotlight",
    "delve", "delves", "embark", "streamline", "streamlines", "empower", "empowers",
    "elevate", "elevates", "unleash", "unleashes", "supercharge", "moreover",
    "furthermore", "additionally", "ultimately", "consequently", "hence", "thus",
    "significantly", "dramatically", "drastically", "vastly", "exponentially",
    "immensely", "incredibly", "remarkably", "considerably", "substantially",
    "greatly", "extremely", "simply", "effortlessly", "effortless", "trivially",
    "merely", "straightforward", "basically", "fundamentally", "tapestry", "beacon",
    "landscape", "realm", "arena", "frontier", "testament", "backbone", "bedrock",
    "lifeblood", "myriad", "plethora", "countless", "numerous", "gracefully",
    "seamlessness", "paradigm", "plethora", "cutting-edge", "state-of-the-art",
    "best-in-class", "world-class", "industry-leading", "top-notch", "game-changer",
    "game-changing", "production-ready", "enterprise-grade", "battle-tested",
    "lightning-fast", "first-class", "next-level", "deep-dive",
]

BANNED_PHRASES = [
    "in conclusion", "in summary", "to summarize", "to wrap up", "in closing",
    "that being said", "with that said", "having said that", "at the end of the day",
    "when it comes to", "in today's", "in the world of", "please note",
    "it is worth noting", "worth noting", "it is important to note",
    "it is important to remember", "keep in mind", "bear in mind", "note that",
    "remember that", "worth mentioning", "rest assured", "feel free to",
    "don't hesitate to", "do not hesitate to", "as we can see", "as you can see",
    "let's take a look", "let's dive", "let's look at", "you'll want to",
    "you will want to", "the user should", "happy coding", "hope this helps",
    "as mentioned above", "as mentioned earlier", "as stated earlier",
    "as previously discussed", "is responsible for", "serves as", "acts as",
    "is designed to", "aims to", "seeks to", "allows you to", "allowing you to",
    "makes it easy to", "provides a way to", "gives you the ability to",
    "in order to", "not only", "whether you are", "whether you're", "here is the",
    "here are the", "here's how", "this ensures that", "this means that",
    "may potentially", "could possibly", "might be able to", "it should be noted",
    "for your convenience", "and so on", "quick and easy", "all you need to do",
    "at its core", "in essence", "put simply", "in other words", "think of it as",
    "under the hood", "behind the scenes", "the magic of", "the beauty of",
    "heavy lifting", "secret sauce", "silver bullet", "out of the box",
    "a wide range of", "a variety of", "a number of", "wide array", "wealth of",
    "deep dive", "dive deep", "blazing fast", "blazingly fast", "this function",
    "this method", "this class", "this module", "this script", "this file",
]

# Flagged as warnings. These have a technical sense and a filler sense.
CONTEXT_WORDS = [
    "optimize", "optimizes", "ensure", "ensures", "ensuring", "granular", "ecosystem",
    "enable", "enables", "critical", "simple", "clean", "just", "easily",
    "essentially", "notably", "highly", "quite", "very", "several", "properly",
    "correctly", "flexible", "navigate", "employ", "journey",
]

# "key" is a map key far more often than an adjective, so match only the adjective sense.
KEY_ADJECTIVE = re.compile(
    r"\b(key\s+(benefit|feature|component|point|part|piece|factor|difference|insight"
    r"|takeaway|consideration|advantage|idea|concept|thing|aspect|detail|step)"
    r"|is\s+key\b|are\s+key\b)",
    re.IGNORECASE,
)

DOCSTRING_OPENERS = re.compile(
    r"^\s*(this\s+(function|method|class|module|script|file|helper|utility)\b"
    r"|the\s+purpose\s+of\s+this\b"
    r"|a\s+(function|method|class)\s+(that|which)\b)",
    re.IGNORECASE,
)

CAPS_RUN = re.compile(r"\b([A-Z]{3,}\b[ ]+){2,}[A-Z]{3,}\b")
CAPS_ALLOW = {"TODO", "FIXME", "HACK", "NOTE", "SAFETY", "WARNING", "XXX", "BUG"}
TABLE_SEP = re.compile(r"^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$")

MAX_SENTENCE_WORDS = 25
MAX_HEADING_DEPTH = 3

# DESIGN: the checker reads Markdown documents, `.md` files, and nothing
# else. Tool output such as the data of the audit stays as the tools wrote
# it. The comments of source and CMake files follow the rules for comments
# without this checker.
MARKDOWN_EXT = {".md"}
SKIP_DIRS = {".git", "node_modules", "__pycache__", ".venv", "venv", "dist", "build",
             ".next", "target", "vendor", ".tox", ".mypy_cache"}


def compile_terms(terms):
    out = []
    for term in terms:
        pattern = re.escape(term).replace(r"\ ", r"\s+").replace(r"\'", "['’]")
        out.append((term, re.compile(r"(?<!\w)" + pattern + r"(?!\w)", re.IGNORECASE)))
    return out


WORD_RE = compile_terms(BANNED_WORDS)
PHRASE_RE = compile_terms(BANNED_PHRASES)
CONTEXT_RE = compile_terms(CONTEXT_WORDS)


class Finding:
    def __init__(self, path, line, level, rule, message):
        self.path = path
        self.line = line
        self.level = level
        self.rule = rule
        self.message = message

    def format(self):
        return f"{self.path}:{self.line}: {self.level}: [{self.rule}] {self.message}"

    def as_dict(self):
        return {"file": self.path, "line": self.line, "level": self.level,
                "rule": self.rule, "message": self.message}


def strip_inline_code(text):
    return re.sub(r"`[^`]*`", " ", text)


def check_text_line(path, lineno, raw, findings, in_table=False):
    """Run the character, vocabulary, and sentence checks on one line of prose."""
    if "docs-style:ignore" in raw:
        return
    text = strip_inline_code(raw)

    for char, name, fix in BANNED_CHARS:
        if char in text:
            findings.append(Finding(path, lineno, "error", "char",
                                    f"{name} found. Use {fix}."))
    if EMOJI.search(text):
        findings.append(Finding(path, lineno, "error", "char", "emoji found. Remove it."))
    if re.search(r"\S\s--\s\S|\w--\w", text) and not TABLE_SEP.match(raw):
        findings.append(Finding(path, lineno, "error", "char",
                                "double hyphen used as a dash. Start a new sentence."))
    if "!" in re.sub(r"!=|!==|\bhttps?://\S+|!(?=\[)", "", text):
        findings.append(Finding(path, lineno, "error", "char",
                                "exclamation mark. Use a period."))
    if ";" in text and not re.search(r"&\w+;|;\s*$", text):
        findings.append(Finding(path, lineno, "warning", "char",
                                "semicolon. Use two sentences."))
    if "..." in text:
        findings.append(Finding(path, lineno, "error", "char",
                                "ellipsis. Finish the sentence."))

    caps = CAPS_RUN.search(text)
    if caps and not any(tag in caps.group(0) for tag in CAPS_ALLOW):
        findings.append(Finding(path, lineno, "error", "caps",
                                f"ALL CAPS emphasis: {caps.group(0).strip()}"))

    for term, pattern in PHRASE_RE:
        if pattern.search(text):
            findings.append(Finding(path, lineno, "error", "phrase",
                                    f"banned phrase: {term}"))
    for term, pattern in WORD_RE:
        if pattern.search(text):
            findings.append(Finding(path, lineno, "error", "word",
                                    f"banned word: {term}"))
    for term, pattern in CONTEXT_RE:
        if pattern.search(text):
            findings.append(Finding(path, lineno, "warning", "context",
                                    f"filler risk: {term}. Keep it only if it is "
                                    f"technical here."))
    if KEY_ADJECTIVE.search(text):
        findings.append(Finding(path, lineno, "error", "word",
                                "banned word: key (as an adjective)"))
    if DOCSTRING_OPENERS.match(text):
        findings.append(Finding(path, lineno, "error", "opener",
                                "subject-restating opener. Start with the verb."))

    if in_table:
        return


def check_sentence_length(path, start_line, block, findings):
    """Measure sentences across a whole paragraph, since they wrap across lines."""
    joined = " ".join(block)
    # A run of inline-code tokens is a list of identifiers, not prose.
    if len(re.findall(r"`[^`]+`", joined)) >= 4:
        return
    prose = " ".join(
        re.sub(r"^\s*([-*+]|\d+\.|>)\s*", "", strip_inline_code(line))
        for line in block
    )
    for sentence in re.split(r"(?<=[.?])\s+", prose):
        words = [w for w in sentence.split() if re.search(r"[A-Za-z]", w)]
        if len(words) > MAX_SENTENCE_WORDS:
            findings.append(Finding(path, start_line, "error", "length",
                                    f"sentence runs {len(words)} words. "
                                    f"Split at {MAX_SENTENCE_WORDS}."))


def check_markdown(path, lines, findings):
    in_fence = False
    in_frontmatter = False
    heading_seen = False
    para = []
    para_start = 0

    def flush():
        nonlocal para, para_start
        if para:
            check_sentence_length(path, para_start, para, findings)
        para = []

    for i, raw in enumerate(lines, start=1):
        stripped = raw.strip()
        if (not stripped or stripped.startswith("|") or stripped.startswith("#")
                or re.match(r"^\s*([-*+]|\d+\.)\s", raw)):
            flush()
        if i == 1 and stripped == "---":
            in_frontmatter = True
            continue
        if in_frontmatter:
            if stripped == "---":
                in_frontmatter = False
            continue
        fence = re.match(r"^\s*(```|~~~)(.*)$", raw)
        if fence:
            flush()
            if not in_fence:
                in_fence = True
                if not fence.group(2).strip():
                    findings.append(Finding(path, i, "error", "fence",
                                            "code fence has no language tag."))
            else:
                in_fence = False
            continue
        if in_fence:
            continue
        if stripped in ("---", "***", "___") and heading_seen:
            findings.append(Finding(path, i, "error", "rule",
                                    "decorative horizontal rule. Use a heading."))
            continue

        heading = re.match(r"^(#{1,6})\s+(.*)$", raw)
        if heading:
            heading_seen = True
            depth = len(heading.group(1))
            title = heading.group(2).strip()
            if depth > MAX_HEADING_DEPTH:
                findings.append(Finding(path, i, "error", "heading",
                                        f"heading depth {depth}. Split the file."))
            if title.endswith((".", ":", "!", "?")):
                findings.append(Finding(path, i, "error", "heading",
                                        "heading ends with punctuation."))
            if "?" in title:
                findings.append(Finding(path, i, "error", "heading",
                                        "question heading. Use a noun phrase."))
            words = [w for w in re.sub(r"[`*_]", "", title).split() if w.isalpha()]
            capped = [w for w in words[1:] if w[:1].isupper() and not w.isupper()]
            if len(words) > 2 and len(capped) >= len(words) - 1:
                findings.append(Finding(path, i, "error", "heading",
                                        "Title Case heading. Use sentence case."))
            if re.match(r"^(introduction|overview|conclusion|summary|final thoughts"
                        r"|wrapping up|closing thoughts)$", title, re.IGNORECASE):
                findings.append(Finding(path, i, "error", "heading",
                                        f"filler section: {title}"))
        if stripped and not heading and not stripped.startswith("|"):
            if not para:
                para_start = i
            para.append(raw)
        check_text_line(path, i, raw, findings, in_table=stripped.startswith("|"))
    flush()


def check_file(path, findings):
    if os.path.splitext(path)[1].lower() not in MARKDOWN_EXT:
        return
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            lines = handle.read().splitlines()
    except OSError as exc:
        findings.append(Finding(path, 0, "error", "io", str(exc)))
        return
    if any("docs-style:ignore-file" in line for line in lines[:10]):
        return
    check_markdown(path, lines, findings)


def collect(targets):
    paths = []
    for target in targets:
        if os.path.isfile(target):
            paths.append(target)
            continue
        for root, dirs, files in os.walk(target):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
            for name in sorted(files):
                ext = os.path.splitext(name)[1].lower()
                if ext in MARKDOWN_EXT:
                    paths.append(os.path.join(root, name))
    return paths


def main():
    parser = argparse.ArgumentParser(description="Flag AI writing patterns in docs.")
    parser.add_argument("targets", nargs="+", help="files or directories")
    parser.add_argument("--strict", action="store_true", help="treat warnings as errors")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument("--quiet", action="store_true", help="print only the count")
    args = parser.parse_args()

    findings = []
    for path in collect(args.targets):
        check_file(path, findings)
    findings.sort(key=lambda f: (f.path, f.line))

    errors = sum(1 for f in findings if f.level == "error")
    warnings = len(findings) - errors

    if args.json:
        print(json.dumps([f.as_dict() for f in findings], indent=2))
    elif not args.quiet:
        for finding in findings:
            print(finding.format())
    print(f"{errors} error(s), {warnings} warning(s)", file=sys.stderr)

    if errors or (args.strict and warnings):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
