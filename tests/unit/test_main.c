#include "../../src/antic/platform.h"
#include "../binary_stdio.h"
#include "check.h"

int check_failures = 0;

void test_target(void);
void test_cpu(void);
void test_llvm_target(void);
void test_abi(void);
void test_llvm_emit(void);
void test_f16(void);
void test_text(void);
void test_diagnostic_cut(void);
void test_host_target(void);
void test_lexer(void);
void test_parser(void);
void test_types(void);
void test_ptr_tables(void);
void test_sema(void);
void test_sema_cycles(void);
void test_sema_constants(void);
void test_sema_generic_copies(void);
void test_sema_chains(void);
void test_sema_copy_bounds(void);
void test_sema_shared_parts(void);
void test_sema_members(void);
void test_nullable(void);
void test_variant(void);
void test_sync(void);
void test_ir(void);
void test_layout(void);
void test_lower(void);
void test_modules(void);
void test_header(void);
void test_optimize(void);
void test_select(void);
void test_regalloc(void);
void test_x86_64(void);
void test_arm64(void);
void test_emit(void);
void test_link(void);
void test_userdirs(void);
void test_float(void);
void test_struct(void);
void test_utf(void);
void test_whole(void);
void test_sha256(void);
void test_arith(void);
void test_symbols(void);
void test_float_read(void);
void test_coff(void);
void test_toml(void);
void test_rt_bounds(void);
void test_fs(void);
void test_json(void);
void test_bind(void);
void test_deps(void);
void test_zip(void);
void test_syms(void);
void test_fmt(void);
void test_files(void);
void test_tool_platform(void);
void test_interface(void);

/* Each group of checks under the name of its file, tests/unit/test_<name>.c.
   ctest runs one file per test, unit_<name>, so a failure names its file. */
struct group {
    const char *name;
    void (*run)(void);
};

static const struct group groups[] = {
    {"target", test_target},
    {"cpu", test_cpu},
    {"llvm_target", test_llvm_target},
    {"abi", test_abi},
    {"llvm_emit", test_llvm_emit},
    {"f16", test_f16},
    {"text", test_text},
    {"text", test_diagnostic_cut},
    {"target", test_host_target},
    {"lexer", test_lexer},
    {"parser", test_parser},
    {"types", test_types},
    {"types", test_ptr_tables},
    {"sema", test_sema},
    {"sema", test_sema_cycles},
    {"sema", test_sema_constants},
    {"sema", test_sema_generic_copies},
    {"sema", test_sema_chains},
    {"sema", test_sema_copy_bounds},
    {"sema", test_sema_shared_parts},
    {"sema", test_sema_members},
    {"nullable", test_nullable},
    {"variant", test_variant},
    {"sync", test_sync},
    {"ir", test_ir},
    {"layout", test_layout},
    {"lower", test_lower},
    {"modules", test_modules},
    {"header", test_header},
    {"optimize", test_optimize},
    {"select", test_select},
    {"regalloc", test_regalloc},
    {"x86_64", test_x86_64},
    {"arm64", test_arm64},
    {"emit", test_emit},
    {"link", test_link},
    {"userdirs", test_userdirs},
    {"float", test_float},
    {"struct", test_struct},
    {"utf", test_utf},
    {"whole", test_whole},
    {"sha256", test_sha256},
    {"arith", test_arith},
    {"symbols", test_symbols},
    {"float_read", test_float_read},
    {"coff", test_coff},
    {"toml", test_toml},
    {"rt_bounds", test_rt_bounds},
    {"fs", test_fs},
    {"json", test_json},
    {"bind", test_bind},
    {"deps", test_deps},
    {"zip", test_zip},
    {"syms", test_syms},
    {"fmt", test_fmt},
    {"files", test_files},
    {"tool_platform", test_tool_platform},
    {"interface", test_interface},
};

/* The name of the file whose groups run, or NULL for every group, and
   whether one group has that name. */
struct selection {
    const char *name;
    int found;
};

/* Run the groups selected. Each runs on the thread of
   platform_run_on_stack, as the passes of antic do, so a deep case meets
   the stack it meets in antic and not the 8 MB of the main thread. */
static void run_groups(void *context)
{
    struct selection *selection = context;
    size_t i;

    for (i = 0; i < sizeof groups / sizeof groups[0]; i++) {
        if (selection->name == NULL ||
            strcmp(selection->name, groups[i].name) == 0) {
            groups[i].run();
            selection->found = 1;
        }
    }
}

/* Run the groups of the file the one argument names, or every group
   without one. */
int main(int argc, char **argv)
{
    struct selection selection;

    if (argc > 2) {
        fprintf(stderr, "usage: antic_unit_tests [<file>]\n");
        return 2;
    }
    selection.name = argc == 2 ? argv[1] : NULL;
    selection.found = 0;
    if (!platform_run_on_stack(run_groups, &selection)) {
        return 1;
    }
    if (!selection.found) {
        fprintf(stderr, "no unit test file is named %s\n", argv[1]);
        return 2;
    }
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
