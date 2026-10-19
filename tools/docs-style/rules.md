<!-- docs-style:ignore-file: this file quotes the characters and the phrases it bans -->

# Documentation and comment style

The docs-style rules of the repository, pinned beside the checker as the
checker is, so they do not change when the skill they came from syncs.
`CLAUDE.md` and rule 27 of `docs/c-guidelines.md` hold every comment and
every `.md` file to them. `check_docs.py` reads `.md` files alone, so a
comment of a source or a CMake file is checked by reading these rules.

Every rule here is mechanically checkable. There are no judgment calls to make. If a rule
does not clearly apply, the text passes.

Apply the rules to inline comments, docstrings, commit bodies, and every `.md` file.

## Workflow

1. Write the text.
2. Run `tools/docs-style/check_docs.py` on the files you touched.
3. Fix every reported line.
4. Re-run until the checker reports zero violations.

```bash
python3 tools/docs-style/check_docs.py docs/notes/llvm.md README.md
python3 tools/docs-style/check_docs.py --strict docs   # walks a directory
```

The checker catches banned terms, punctuation, sentence length, heading form, and
docstring openers. It does not catch the structural rules in section 5. Check those
by reading.

## 1. Characters and punctuation

Banned characters, no exceptions:

| Character | Name | Replacement |
|---|---|---|
| `—` | em dash | Period, then a new sentence |
| `–` | en dash | Period, or `to` in ranges |
| `--` | double hyphen as a dash | Period, then a new sentence |
| `…` `...` | ellipsis | Nothing, or finish the sentence |
| `!` | exclamation mark | Period |
| `'` `'` `"` `"` | curly quotes | Straight `'` and `"` |
| `→` `←` `✓` `✗` `✅` `❌` `⚠` `•` `★` | decorative glyphs | Words, or a `-` bullet |
| Any emoji | | Nothing |
| U+00A0 | non-breaking space | Regular space |

More punctuation rules:

- No parenthetical asides longer than five words. Split into a sentence.
- No semicolons. Use two sentences.
- No ALL CAPS for emphasis. Acronyms and the tags `TODO`, `FIXME`, `HACK`, `NOTE`,
  `SAFETY`, `WARNING` are allowed.
- No bold or italics inside a sentence for emphasis. Bold is allowed only for a table
  header or a definition-list term.
- No `---` horizontal rules as decoration between sections. Headings already separate
  sections.
- No trailing punctuation on headings. No question marks in headings.

## 2. Banned vocabulary

Never use these. The full list, grouped by category, is in
`tools/docs-style/banned-terms.md`. Read that file before writing anything longer than a few
lines. The checker enforces the same list.

The highest-frequency offenders:

- Importance words: `crucial` `vital` `paramount` `essential` `imperative` `pivotal`
  `integral` `indispensable` `cornerstone` `key` (as an adjective)
- Marketing adjectives: `robust` `seamless` `powerful` `comprehensive` `elegant`
  `intuitive` `flexible` `scalable` `blazing fast` `production-ready` `enterprise-grade`
  `battle-tested` `best-in-class` `cutting-edge` `out of the box` `first-class`
- Verbs with plain replacements: `leverage` `utilize` `employ` (use), `facilitate`
  `enable` (let), `harness` (use), `foster` (cause), `bolster` `augment` (add to),
  `showcase` `underscore` (show), `delve` (read), `navigate` (as a metaphor)
- Connectives: `moreover` `furthermore` `additionally` `in conclusion` `in summary`
  `overall` `that being said` `at the end of the day` `when it comes to` `ultimately`
- Hedges and softeners: `please note` `it is worth noting` `keep in mind` `bear in mind`
  `note that` `rest assured` `feel free to` `it is important to remember`
- Unquantified intensifiers: `significantly` `dramatically` `drastically` `vastly`
  `exponentially` `incredibly` `remarkably` `notably` `considerably`
- Minimizers: `simply` `just` `easily` `effortlessly` `trivially` `merely`
  `straightforward`
- Filler openers: `basically` `essentially` `fundamentally` `at its core` `in essence`
- Metaphors: `tapestry` `beacon` `landscape` `realm` `journey` `ecosystem` `testament`
  `backbone` `under the hood` `magic of` `beauty of`

## 3. Banned sentence patterns

Each pattern below is banned in every form.

| Pattern | Banned form | Write instead |
|---|---|---|
| Subject-restating opener | `This function parses the header` | `Parse the header` |
| Contrastive framing | `While X seems Y, it actually does Z` | `Does Z` |
| Negation then correction | `This is not a cache. It is an index` | `Indexes rows by primary key` |
| Rule of three | `Fast, reliable, and scalable` | One measured fact |
| Additive pairing | `Not only validates, but also logs` | `Validates and logs` |
| Reader-type framing | `Whether you are new or experienced` | Delete the sentence |
| Preamble | `Here is the code that` | Delete the sentence |
| Postamble | `This ensures that the cache stays warm` | `Keeps the cache warm` |
| Purpose padding | `is responsible for` `serves as` `acts as` `is designed to` `aims to` | The plain verb |
| Permission padding | `allows you to` `makes it easy to` `provides a way to` | `lets you`, or the plain verb |
| Purpose clause padding | `In order to reset the index` | `To reset the index` |
| Stacked hedges | `may potentially` `could possibly` `might be able to` | One modal, or none |
| Reader address | `As we can see` `Let us take a look` `You will want to` | Delete the sentence |
| Backreference | `As mentioned above` `As stated earlier` | Link to the section |
| Sign-off | `Happy coding` `Hope this helps` | Delete the line |

## 4. Sentences

- Maximum 25 words per sentence. Split anything longer.
- Present tense. Active voice. Passive voice is allowed only when the actor is unknown or
  irrelevant, as in `The row is deleted on rollback`.
- Command mood for instructions. Write `Set TIMEOUT to 30`.
  Never `the user should set it` or `one can set it`.
- Second person for the reader. Never `we`, `let's`, or `one`.
- Every adjective must be falsifiable. `Fast` is banned. `Runs in 40ms on a 10k-row table`
  is fine.
- Name the concrete thing. Write the exact file path, function name, env var, flag, error
  string, or command. Never `the relevant config` or `the appropriate handler`.
- One idea per sentence. If a sentence needs a conjunction to hold two ideas, split it.

## 5. Structure

Read these rules. The checker only catches some of them.

### Code comments

- A comment states why, not what. If the comment restates the line below it, delete the
  comment.
- Start with a command verb or a plain present verb.
  Write `Cache the session to skip the lookup`. Never `This caches the session`.
- Three lines maximum. Longer explanations go in a `.md` file next to the code.
- Place the comment on the line above. Trailing comments are allowed only under 40
  characters.
- No banner comments. `# ====== HELPERS ======` and `//////////` are banned.
- No commented-out code. Git holds the history.
- No author names, dates, ticket numbers, or changelog entries in comments. Git holds
  those too.
- `TODO` and `FIXME` need an owner and a condition: `TODO(eddie): remove after the v3
  migration ships`.

### Docstrings

- First line: one command sentence, under 80 characters, ends with a period.
- Blank line, then the body, if a body is needed at all.
- Never repeat the type annotation in prose. `user_id (int): The user ID` carries nothing.
  Document the constraint instead: `user_id: must be a live account, not a soft-deleted
  one`.
- Document only what the signature cannot say. Units, ranges, side effects, raised
  exceptions, thread safety, ordering guarantees.
- Omit `Args`, `Returns`, and `Raises` sections that hold nothing beyond the signature.

### Markdown files

- Headings are sentence case noun phrases. `Error handling`, not `Error Handling` and not
  `How do I handle errors?`.
- Maximum heading depth is three. Deeper nesting means the file should be split.
- No bold text standing in for a heading.
- No `Introduction` or `Overview` section that restates the title.
- No `Conclusion`, `Summary`, or `Final thoughts` section.
- Every code fence carries a language tag.
- Code samples run as written. No `...` placeholders, no pseudo-code presented as real.
- Lists are grammatically parallel. If one item starts with a verb, all do.
- Prerequisites sections list versions and commands, not knowledge claims. Delete
  `Basic knowledge of Python is assumed`.
- A table of contents belongs in files with more than six sections.

## 6. Verification

Before returning any file:

1. Run `tools/docs-style/check_docs.py` on it.
2. Fix every line it reports.
3. Read the file once more for the section 5 rules the checker cannot see.

The checker exits non-zero when it finds violations. It drops into a pre-commit hook or a
CI step with no wrapper.
