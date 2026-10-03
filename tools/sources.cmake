# The sources of antic and anti, relative to the root of the repository.
# CMakeLists.txt builds from these lists and tools/pack-anti.cmake compiles
# the programs of a package from them, so the two compile the same files.
# DESIGN: the packer once took a glob of src/antic and src/anti. The anti
# it compiled lacked src/rt/json.c, src/rt/symbols.c and src/rt/toml.c,
# which the CMake build had added, and did not link. One list leaves
# nothing to fall out of step.

# The compiler, which the unit tests, antic and anti link. The files of
# src/rt/ in it are the code that the runtime and the host share.
set(ANTIC_CORE_SOURCES
    src/antic/alloc.c
    src/antic/antic.c
    src/antic/antl.c
    src/antic/antl_io.c
    src/antic/antl_read.c
    src/antic/antl_read_ir.c
    src/antic/antl_tree.c
    src/antic/antl_verify.c
    src/antic/antl_write.c
    src/antic/applesdk.c
    src/antic/arith.c
    src/antic/arm64.c
    src/antic/coff.c
    src/antic/arena.c
    src/antic/ast_dump.c
    src/antic/ast_walk.c
    src/antic/cpu.c
    src/antic/debug.c
    src/antic/diagnostic.c
    src/antic/docpage.c
    src/antic/driver.c
    src/antic/driver_index.c
    src/antic/driver_library.c
    src/antic/driver_link.c
    src/antic/driver_search.c
    src/antic/emit.c
    src/antic/expand.c
    src/antic/header.c
    src/antic/ir.c
    src/antic/ir_fold.c
    src/antic/ir_print.c
    src/antic/ir_verify.c
    src/antic/layout.c
    src/antic/lexer.c
    src/antic/linker.c
    src/antic/llvm_target.c
    src/antic/lower.c
    src/antic/lower_check.c
    src/antic/lower_class.c
    src/antic/lower_desc.c
    src/antic/lower_eq.c
    src/antic/lower_error.c
    src/antic/lower_expr.c
    src/antic/lower_function.c
    src/antic/lower_hash.c
    src/antic/lower_hook.c
    src/antic/lower_owning.c
    src/antic/lower_pattern.c
    src/antic/lower_place.c
    src/antic/lower_simd.c
    src/antic/lower_stmt.c
    src/antic/lower_sync.c
    src/antic/lower_worker.c
    src/antic/mach.c
    src/antic/memcheck.c
    src/antic/modpath.c
    src/antic/notice.c
    src/antic/optimize.c
    src/antic/parser.c
    src/antic/parser_clause.c
    src/antic/parser_decl.c
    src/antic/parser_expr.c
    src/antic/parser_item.c
    src/antic/parser_stmt.c
    src/antic/parser_type.c
    src/antic/pattern.c
    src/antic/platform.c
    src/antic/reach.c
    src/antic/regalloc.c
    src/antic/rt_abi.c
    src/antic/select.c
    src/antic/sha256.c
    src/antic/userdirs.c
    src/antic/ptrset.c
    src/antic/sema.c
    src/antic/sema_call.c
    src/antic/sema_chain.c
    src/antic/sema_class.c
    src/antic/sema_generic.c
    src/antic/sema_hash.c
    src/antic/sema_copies.c
    src/antic/sema_const.c
    src/antic/sema_doc.c
    src/antic/sema_export.c
    src/antic/sema_expr.c
    src/antic/sema_operator.c
    src/antic/sema_pattern.c
    src/antic/sema_safety.c
    src/antic/sema_simd.c
    src/antic/sema_stmt.c
    src/antic/sema_thread.c
    src/antic/sema_value.c
    src/antic/target.c
    src/antic/text.c
    src/antic/types.c
    src/antic/warnings.c
    src/antic/whole.c
    src/antic/whole_tables.c
    src/antic/x86_64.c
    src/rt/digest.c
    src/rt/regex.c
    src/rt/toml.c
    src/rt/utf.c)
set(ANTIC_CORE_INCLUDE_DIRS src/antic)

set(ANTIC_MAIN_SOURCES src/antic/main.c)

# anti reads anti.toml, the runtime configuration, JSON and the symbols of
# a binary with the readers of the runtime in src/rt. src/rt/toml.c stands
# in the compiler, which reads the index of plugins with it. The unit tests
# link ANTI_CORE_SOURCES, every file of anti but its main.
set(ANTI_MAIN_SOURCES src/anti/main.c)
set(ANTI_CORE_SOURCES
    src/anti/bind.c
    src/anti/bindapi.c
    src/anti/bindclang.c
    src/anti/bindexpr.c
    src/anti/bindmodel.c
    src/anti/bindtype.c
    src/anti/bindwrite.c
    src/anti/build.c
    src/anti/check.c
    src/anti/cursor.c
    src/anti/deps.c
    src/anti/doc.c
    src/anti/files.c
    src/anti/fmt.c
    src/anti/jsontree.c
    src/anti/manifest.c
    src/anti/memreport.c
    src/anti/platform.c
    src/anti/project.c
    src/anti/repo.c
    src/anti/sdk.c
    src/anti/symmap.c
    src/anti/syms.c
    src/anti/test.c
    src/anti/units.c
    src/anti/zip.c
    src/rt/json.c
    src/rt/symbols.c)
set(ANTI_SOURCES ${ANTI_MAIN_SOURCES} ${ANTI_CORE_SOURCES})
set(ANTI_INCLUDE_DIRS src/rt)
