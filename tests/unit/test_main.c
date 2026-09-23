#include "../binary_stdio.h"
#include "check.h"

int check_failures = 0;

void test_target(void);
void test_cpu(void);
void test_f16(void);
void test_text(void);
void test_host_target(void);
void test_lexer(void);
void test_parser(void);
void test_types(void);
void test_sema(void);
void test_sema_cycles(void);
void test_sema_constants(void);
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
void test_json(void);
void test_bind(void);
void test_deps(void);
void test_zip(void);

int main(void)
{
    test_target();
    test_cpu();
    test_f16();
    test_text();
    test_host_target();
    test_lexer();
    test_parser();
    test_types();
    test_sema();
    test_sema_cycles();
    test_sema_constants();
    test_nullable();
    test_variant();
    test_sync();
    test_ir();
    test_layout();
    test_lower();
    test_modules();
    test_header();
    test_optimize();
    test_select();
    test_regalloc();
    test_x86_64();
    test_arm64();
    test_emit();
    test_link();
    test_userdirs();
    test_float();
    test_struct();
    test_utf();
    test_whole();
    test_sha256();
    test_arith();
    test_symbols();
    test_float_read();
    test_coff();
    test_toml();
    test_json();
    test_bind();
    test_deps();
    test_zip();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
