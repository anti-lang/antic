/* The commands of anti in this process, as anti runs them, on the
   project that `anti new` writes: `anti check`, `anti doc`, `anti build`
   and `anti run` of src/anti/check.c, doc.c and build.c, and `anti test`
   of test.c. Each compiles through driver_run of antic, so the search of
   the library files, the run of opt and llc and the link of a program
   run as well. A plugin built through driver_run writes its library and
   the index beside it. ANTIC_TEST_RUNTIME is the runtime archive of the
   build. */

#include <stdio.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/anti/check.h"
#include "build.h"
#include "check.h"
#include "cpu.h"
#include "doc.h"
#include "driver.h"
#include "files.h"
#include "project.h"
#include "target.h"
#include "test.h"
#include "text.h"

/* The project of the run, which `anti new` writes again each time. */
#define PROJECT "cmd"
#define SOURCE PROJECT "/src/com/example/cmd.anti"
#define TEST_SOURCE PROJECT "/test/com/example/cmd_test.anti"

/* Write text to path. */
static void write_text(const char *path, const char *text)
{
    struct text bytes = {0};

    text_append(&bytes, text);
    CHECK(files_write(path, &bytes));
    text_free(&bytes);
}

/* `anti check` and `anti doc` over the module of `anti new`. */
static void checks_and_docs(void)
{
    static const char *const sources[] = {SOURCE};
    static const char *const roots[] = {PROJECT "/src"};
    struct check_request c;
    struct doc_request d;

    memset(&c, 0, sizeof c);
    c.sources = sources;
    c.source_count = 1;
    c.roots = roots;
    c.root_count = 1;
    c.work = "cmd-check";
    c.runtime = ANTIC_TEST_RUNTIME;
    CHECK(check_run(&c) == 0);
    memset(&d, 0, sizeof d);
    d.sources = sources;
    d.source_count = 1;
    d.roots = roots;
    d.root_count = 1;
    d.out = "cmd-doc";
    d.work = "cmd-doc-work";
    d.package = "com.example.cmd";
    d.runtime = ANTIC_TEST_RUNTIME;
    d.form = DOC_MARKDOWN;
    CHECK(doc_run(&d) == 0);
    CHECK(files_exists("cmd-doc/com.example.cmd.md"));
}

/* `anti build` and `anti run` of the project in dev mode, and `anti
   test` of a module of its test directory with a `tests` block. */
static void builds_and_tests(void)
{
    static const char *const sources[] = {TEST_SOURCE};
    static const char *const roots[] = {PROJECT "/test"};
    struct build_request b;
    struct test_request t;

    memset(&b, 0, sizeof b);
    b.root = PROJECT;
    b.runtime = ANTIC_TEST_RUNTIME;
    CHECK(build_run(&b) == 0);
    b.run = true;
    CHECK(build_run(&b) == 0);
    CHECK(files_make_dirs(PROJECT "/test/com/example"));
    write_text(TEST_SOURCE, "fn twice(n: int) -> int { return n * 2; }\n"
                            "tests {\n"
                            "    fn doubles() {\n"
                            "        let n = twice(2);\n"
                            "        assert(n == 4);\n"
                            "    }\n"
                            "}\n");
    memset(&t, 0, sizeof t);
    t.sources = sources;
    t.source_count = 1;
    t.roots = roots;
    t.root_count = 1;
    t.work = "cmd-test";
    t.package = "com.example.cmd";
    t.runtime = ANTIC_TEST_RUNTIME;
    CHECK(test_run(&t) == 0);
}

/* A plugin: a library linked without the runtime, whose index names the
   interface it provides. */
static void plugin(void)
{
    struct options o;
    struct text index = {0};

    write_text("greeter.anti",
               "//! A plugin that provides its own interface.\n"
               "provides Greeter as Plain;\n"
               "/// A greeting.\n"
               "pub abstract class Greeter\n"
               "{\n"
               "    /// The words.\n"
               "    pub abstract fn words(self) -> str;\n"
               "}\n"
               "/// The plain greeting.\n"
               "pub class Plain\n"
               "{\n"
               "    implements g: Greeter,\n"
               "    /// The words.\n"
               "    pub concrete fn words(self) -> str { return \"hi\"; }\n"
               "}\n");
    memset(&o, 0, sizeof o);
    o.input = "greeter.anti";
    o.output = "greeter-plugin";
    o.runtime = ANTIC_TEST_RUNTIME;
    o.lib = LIB_SHARED;
    o.no_runtime = true;
    CHECK(target_host(&o.target));
    o.cpu = cpu_default(o.target);
    CHECK(driver_run(&o) == 0);
    CHECK(files_read("anti-plugins.toml", &index));
    CHECK(strstr(text_cstr(&index), "interfaces = ['greeter.Greeter']") !=
          NULL);
    text_free(&index);
}

void test_commands(void)
{
    CHECK(files_remove_tree(PROJECT));
    CHECK(project_new("com.example.cmd") == 0);
    checks_and_docs();
    builds_and_tests();
    plugin();
}
