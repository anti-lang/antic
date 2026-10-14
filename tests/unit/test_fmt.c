/* anti fmt over sources the parser would refuse. The formatter runs the
   lexer alone, so no bound of the parser guards it: an angle list nested
   past what the stack holds, and the body of an anonymous function that
   the file ends inside. The last group holds single rules, on sources
   the parser takes and on ones it refuses. */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "fmt.h"
#include "text.h"

/* More levels than any stack holds, and far more than the parser takes. */
enum { DEEP = 300000 };

/* `fn main() { f<` and count copies of level, then close and the call. */
static void deep_source(struct text *src, const char *level, const char *close)
{
    size_t i;

    text_append(src, "fn main()\n{\n\tf<");
    for (i = 0; i < DEEP; i++) {
        text_append(src, level);
    }
    text_append(src, "int");
    for (i = 0; i < DEEP; i++) {
        text_append(src, close);
    }
    text_append(src, ">(x);\n}\n");
}

/* M39: the scan of an angle list recursed once per `chan`, `*` or `<`
   and ended the formatter on the stack. A list past the depth of the
   parser is no list, so its `<` keeps the spaces of a comparison. */
static void deep_angles(void)
{
    static const char *const levels[][2] = {
        {"chan ", ""},
        {"*", ""},
        {"List<", ">"},
    };
    static const char shallow[] = "fn main()\n{\n\tf<chan *List<int>>(x);\n}\n";
    struct text src = {0};
    struct text out = {0};
    size_t i;

    CHECK(fmt_source(shallow, sizeof shallow - 1, &out));
    CHECK_STR(text_cstr(&out), shallow);
    text_free(&out);
    for (i = 0; i < sizeof levels / sizeof levels[0]; i++) {
        deep_source(&src, levels[i][0], levels[i][1]);
        CHECK(fmt_source(src.data, src.length, &out));
        CHECK(strstr(text_cstr(&out), "\tf < ") != NULL);
        text_free(&src);
        text_free(&out);
    }
}

/* M46: a file that ends inside the body of an anonymous function left
   the open brackets the body saved unfreed. The sanitizer builds check
   the leak. */
static void open_anonymous(void)
{
    static const char *const sources[] = {
        "fn main()\n{\n\tf(fn() {\n",
        "fn main()\n{\n\tf([g(fn(a) {\n\t\th(fn() {\n",
        "fn main()\n{\n\tf(fn() { g(fn() {\n",
    };
    size_t i;

    for (i = 0; i < sizeof sources / sizeof sources[0]; i++) {
        struct text out = {0};
        CHECK(fmt_source(sources[i], strlen(sources[i]), &out));
        CHECK(out.length > 0);
        text_free(&out);
    }
}

/* One rule per source, and the form it gives. The parser would refuse
   the last six, and the formatter writes each as far as its rules reach. */
static void odd_sources(void)
{
    static const char *const cases[][2] = {
        /* A comment in the list of a direct import keeps the list in the
           order it was written, since a comment belongs to a name. */
        {"import a.b.{z, /* c */ a};\n", "import a.b.{z, /* c */ a};\n"},
        /* A comma before the first name goes, by R182 of "Rules of
           2026-10-13", and the names after it are sorted. */
        {"import a.c.{, y, x};\n", "import a.c.{x, y};\n"},
        {"import a.c.{,, y};\n", "import a.c.{y};\n"},
        /* The spaces at the end of a doc line go, and a doc line of
           spaces alone is an empty one. */
        {"/// text   \n///    \n/// more\nfn main()\n{\n}\n",
         "/// text\n///\n/// more\nfn main()\n{\n}\n"},
        /* A block comment of empty lines alone has no gutter to remove. */
        {"/**\n\n\n*/\nfn main()\n{\n}\n", "/**\n\n\n*/\nfn main()\n{\n}\n"},
        /* The gutter goes from a block comment that holds an empty line. */
        {"/**\n * a\n\n * b\n */\nfn other()\n{\n}\n",
         "/**\na\n\nb\n*/\nfn other()\n{\n}\n"},
        /* `snapshot fn` as the first tokens of a file, and a `}` that
           closes nothing. */
        {"snapshot fn(a) { };\n}\n", "snapshot fn(a) { };\n}\n"},
        /* A call with type arguments as the first tokens of a file. */
        {"x<int>(y);\n", "x<int>(y);\n"},
        /* `>>` closes two lists and never one and a half. */
        {"fn main()\n{\n\tf<A<int>>, int>(x);\n}\n",
         "fn main()\n{\n\tf < A < int >>, int > (x);\n}\n"},
        {"fn main()\n{\n\tf<(A<int>>, int)>(x);\n}\n",
         "fn main()\n{\n\tf < (A < int >>, int) > (x);\n}\n"},
        {"fn main()\n{\n\tlet v: A<int>> = 1;\n}\n",
         "fn main()\n{\n\tlet v: A < int >> = 1;\n}\n"},
        /* `fn` without its parameters is no type, so its `<` is none of
           a list. */
        {"fn main()\n{\n\tf<fn int>(x);\n}\n",
         "fn main()\n{\n\tf < fn int > (x);\n}\n"},
        /* A file that ends after a `<`. */
        {"fn main()\n{\n\tf<", "fn main()\n{\n\tf <\n"},
    };
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        struct text out = {0};
        CHECK(fmt_source(cases[i][0], strlen(cases[i][0]), &out));
        CHECK_STR(text_cstr(&out), cases[i][1]);
        text_free(&out);
    }
}

void test_fmt(void)
{
    deep_angles();
    open_anonymous();
    odd_sources();
}
