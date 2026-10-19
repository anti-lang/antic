/* The modules of a run of src/anti/units.c: the options every compile
   starts from, the path of a file of a module, one module read from its
   source, and the order of the imports with the refusal of a cycle. */

#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "units.h"

/* Write text to path, every directory above it made. */
static void write_source(const char *dir, const char *path, const char *text)
{
    struct text bytes = {0};

    CHECK(files_make_dirs(dir));
    text_append(&bytes, text);
    CHECK(files_write(path, &bytes));
    text_free(&bytes);
}

/* unit_options sets what each compile of a run carries, and nothing
   else. */
static void options(void)
{
    static const char *const roots[] = {"src", "lib"};
    struct options o;
    enum target target;
    enum cpu_level cpu;

    CHECK(unit_host(&target, &cpu));
    CHECK(cpu == cpu_default(target));
    unit_options(&o, "app", "/rt", roots, 2, target, cpu);
    CHECK_STR(o.package_name, "app");
    CHECK_STR(o.runtime, "/rt");
    CHECK(o.roots == roots && o.root_count == 2);
    CHECK(o.target == target && o.cpu == cpu);
    CHECK(o.input == NULL && o.output == NULL && !o.dev);
}

/* A module path becomes directories under dir, which unit_file makes,
   and one flat name. */
static void paths(void)
{
    struct text path = {0};
    struct text flat = {0};

    CHECK(unit_file("units-work", "com.example.geo", ".antl", &path));
    CHECK_STR(text_cstr(&path), "units-work/com/example/geo.antl");
    CHECK(files_exists("units-work/com/example"));
    unit_flat_path("com.example.geo", &flat);
    CHECK_STR(text_cstr(&flat), "com_example_geo");
    text_free(&path);
    text_free(&flat);
}

/* One module read from its source under a root: its path, its imports,
   its tests and fixtures, its main and its interface file. */
static void reads(void)
{
    static const char *const roots[] = {"units-src"};
    struct unit u;

    write_source("units-src/com/example", "units-src/com/example/app.anti",
                 "import com.example.geo;\n"
                 "fn main() -> int { return 0; }\n"
                 "tests {\n"
                 "    fn adds() {}\n"
                 "}\n"
                 "fixtures {\n"
                 "    fn setup() {}\n"
                 "}\n");
    CHECK(unit_read("units-src/com/example/app.anti", roots, 1, "units-work",
                    &u));
    CHECK(u.parsed && u.has_main && u.setup && !u.teardown);
    CHECK_STR(text_cstr(&u.path), "com.example.app");
    CHECK_STR(text_cstr(&u.library), "units-work/com/example/app.antl");
    CHECK(u.import_count == 1);
    if (u.import_count == 1) {
        CHECK_STR(text_cstr(&u.imports[0]), "com.example.geo");
    }
    CHECK(u.test_count == 1);
    if (u.test_count == 1) {
        CHECK_STR(u.tests[0], "adds");
    }
    unit_free(&u);
    CHECK(!unit_read("units-src/none.anti", roots, 1, NULL, &u));
    unit_free(&u);
}

/* A unit of path that imports each of imports. */
static void unit_of(struct unit *u, const char *path, const char *a,
                    const char *b)
{
    memset(u, 0, sizeof *u);
    text_append(&u->path, path);
    u->imports = files_array(3, sizeof *u->imports);
    if (a != NULL) {
        text_append(&u->imports[u->import_count++], a);
    }
    if (b != NULL) {
        text_append(&u->imports[u->import_count++], b);
    }
}

/* A module comes after every module of the run it imports, and an
   import of a module outside the run waits for nothing. A cycle is
   refused, and every module still gets a place. */
static void orders(void)
{
    struct unit units[3];
    size_t order[3];
    size_t i;

    unit_of(&units[0], "app", "geo", "anti.text");
    unit_of(&units[1], "geo", "base", NULL);
    unit_of(&units[2], "base", NULL, NULL);
    CHECK(unit_order(units, 3, order));
    CHECK(order[0] == 2 && order[1] == 1 && order[2] == 0);
    for (i = 0; i < 3; i++) {
        unit_free(&units[i]);
    }
    unit_of(&units[0], "app", "geo", NULL);
    unit_of(&units[1], "geo", "app", NULL);
    unit_of(&units[2], "base", NULL, NULL);
    CHECK(!unit_order(units, 3, order));
    CHECK(order[0] == 2 && order[1] == 0 && order[2] == 1);
    for (i = 0; i < 3; i++) {
        unit_free(&units[i]);
    }
}

void test_units(void)
{
    options();
    paths();
    reads();
    orders();
}
