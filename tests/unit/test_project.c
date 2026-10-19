/* `anti new` and the sources of a project, of src/anti/project.c: the
   layout and the manifest of a new project, the names it refuses, and the
   files a command reads from the source and the test directory. */

#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "manifest.h"
#include "modpath.h"
#include "project.h"

void test_project(void)
{
    struct text manifest = {0};
    struct files_list sources = {0};

    /* The directory of the run keeps the project of the run before. */
    CHECK(files_remove_tree("starter"));
    CHECK(project_new("com.example.starter") == 0);
    CHECK(files_read("starter/" MANIFEST_FILE, &manifest));
    CHECK_STR(text_cstr(&manifest), "[package]\n"
                                    "name = \"com.example.starter\"\n"
                                    "version = \"0.1.0\"\n");
    CHECK(files_exists("starter/src/com/example/starter" SOURCE_SUFFIX));
    CHECK(files_exists("starter/test"));
    text_free(&manifest);

    /* A directory that is there, one segment and a name with a capital
       letter are refused. */
    CHECK(project_new("com.example.starter") == 1);
    CHECK(project_new("starter") == 1);
    CHECK(project_new("com.Example.starter") == 1);

    /* The sources: the one module of src/, and none under test/ alone. */
    CHECK(project_sources("starter/src", "starter/test", &sources));
    CHECK(sources.count == 1);
    if (sources.count == 1) {
        CHECK(files_ends_with(text_cstr(&sources.items[0]),
                              "starter" SOURCE_SUFFIX));
    }
    files_list_free(&sources);
    CHECK(!project_sources("starter/test", NULL, &sources));
    CHECK(sources.count == 0);
    files_list_free(&sources);
}
