<!-- docs-style:ignore-file: this file is the banned-term data itself -->

# Banned terms

Every term here is banned in comments, docstrings, and `.md` files. The list is the same
one `scripts/check_docs.py` enforces. Groups exist for reading, not for exceptions.

Contents:

- [Importance and emphasis](#importance-and-emphasis)
- [Marketing adjectives](#marketing-adjectives)
- [Verbs with plain replacements](#verbs-with-plain-replacements)
- [Connectives and transitions](#connectives-and-transitions)
- [Hedges, softeners, reader address](#hedges-softeners-reader-address)
- [Intensifiers without numbers](#intensifiers-without-numbers)
- [Minimizers](#minimizers)
- [Filler openers](#filler-openers)
- [Metaphors and abstractions](#metaphors-and-abstractions)
- [Quantity vagueness](#quantity-vagueness)
- [Self-praise](#self-praise)
- [Phrases](#phrases)
- [Context-dependent terms](#context-dependent-terms)

## Importance and emphasis

crucial, vital, paramount, essential, imperative, pivotal, integral, indispensable,
cornerstone, critical (as a value judgment), important to note, importantly, notably,
significantly (as emphasis), key (as an adjective)

Replacement: state the consequence. `Skipping this drops the index` beats `This step is
crucial`.

## Marketing adjectives

robust, seamless, seamlessly, powerful, comprehensive, elegant, sleek, intuitive,
delightful, flexible, extensible, scalable, performant, blazing fast, blazingly fast,
lightning-fast, production-ready, enterprise-grade, battle-tested, industry-leading,
best-in-class, world-class, cutting-edge, state-of-the-art, next-level, top-notch,
turnkey, first-class, out of the box, game-changer, game-changing, holistic, meticulous,
meticulously, painstaking, intricate, nuanced, multifaceted

Replacement: a number, a benchmark, or nothing.

## Verbs with plain replacements

| Banned | Use |
|---|---|
| leverage | use |
| utilize | use |
| employ | use |
| optimize | speed up, shrink, tune |
| facilitate | let, run |
| enable (as a synonym for let) | let |
| harness | use |
| foster | cause |
| bolster | add to |
| augment | add to |
| showcase | show |
| underscore | show |
| highlight (meaning emphasize) | show |
| spotlight | show |
| delve, dive deep, deep dive | read, see |
| navigate (as a metaphor) | find, move through |
| embark | start |
| ensure that | so, or state the effect |
| streamline | shorten, cut |
| empower | let |
| elevate | raise, improve |
| unlock, unleash | let, expose |
| supercharge | speed up |

## Connectives and transitions

moreover, furthermore, additionally, in addition, in conclusion, in summary, to summarize,
overall, to wrap up, in closing, that being said, with that said, having said that, at the
end of the day, when it comes to, in today's, in the world of, ultimately, consequently
(prefer `so`), thus (prefer `so`), hence

Replacement: nothing. Put the sentences next to each other.

## Hedges, softeners, reader address

please note, it is worth noting, it is important to note, it is important to remember,
keep in mind, bear in mind, note that, remember that, worth mentioning, rest assured,
feel free to, don't hesitate to, as we can see, as you can see, let's, let us, we'll,
we will (in docs), you'll want to, one can, the user should, happy coding, hope this
helps, as mentioned above, as stated earlier, as previously discussed

Replacement: delete the clause and keep the fact.

## Intensifiers without numbers

significantly, dramatically, drastically, vastly, exponentially, immensely, incredibly,
remarkably, considerably, substantially, greatly, highly, extremely, quite, very

Replacement: the measurement. `Cuts p99 from 800ms to 120ms`.

## Minimizers

simply, just (as a softener), easily, effortlessly, trivially, merely, straightforward,
simple (as reassurance), quick and easy, all you need to do

Replacement: delete. If a step is short, its shortness is visible.

## Filler openers

basically, essentially, fundamentally, at its core, in essence, put simply, in other
words, think of it as, imagine

Replacement: delete and start with the fact.

## Metaphors and abstractions

tapestry, beacon, landscape, realm, arena, sphere, frontier, journey, ecosystem (unless
literally about a package ecosystem), testament, backbone, bedrock, lifeblood, under the
hood, behind the scenes, the magic of, the beauty of, heavy lifting, secret sauce, silver
bullet, glue (as a metaphor)

Replacement: name the mechanism.

## Quantity vagueness

myriad, plethora, wealth of, a wide range of, a variety of, a number of, several (when the
count is known), wide array, countless, numerous

Replacement: the count, or the enumerated list.

## Self-praise

clean, beautiful, nice, neat, clever, smart (about your own code), gracefully (as in
"handles errors gracefully"), properly (as filler), correctly (as filler)

Replacement: the behavior. `Returns 409 on a duplicate key` beats `handles duplicates
gracefully`.

## Phrases

These are banned as whole phrases, in any tense or person.

- `this function`, `this method`, `this class`, `this module`, `this script`, `this file`
  as the opening of a docstring or comment
- `is responsible for`
- `serves as`, `acts as`
- `is designed to`, `aims to`, `seeks to`
- `allows you to`, `makes it easy to`, `provides a way to`, `gives you the ability to`
- `in order to`
- `not only ... but also`
- `whether you are ...`
- `here is`, `here are`, `here's how`, `let's take a look`, `let's dive`
- `this ensures that`, `this means that`, `this way`
- `may potentially`, `could possibly`, `might be able to`
- `it should be noted`
- `for your convenience`
- `and more`, `and so on`, `etc.` at the end of a list you could finish

## Context-dependent terms

These are banned when used as filler and allowed when they carry a technical meaning. The
checker flags them as warnings, not errors.

| Term | Allowed when | Banned when |
|---|---|---|
| optimize | naming a compiler or query optimizer | describing your own work |
| ensure | a guarantee the code enforces | rounding off a paragraph |
| granular | describing data granularity | describing detail level of prose |
| ecosystem | npm, PyPI, or a named package ecosystem | describing a codebase |
| enable | a flag, feature toggle, or capability bit | a synonym for let |
| key | a map key, API key, or sort key | an adjective meaning important |
| critical | a severity level or critical path | a value judgment |
| simple | a named pattern, as in simple queue service | reassuring the reader |
| performance | a measured quantity | a vague virtue |
| clean | `git clean`, a clean build, a clean shutdown | praising code |
