#ifndef ANTIC_RT_ABI_H
#define ANTIC_RT_ABI_H

/* The contract between the code antic writes and the runtime of src/rt/:
   the runtime functions that generated code calls and the records that
   antic writes for the runtime to read.

   DESIGN: the contract stands here once. A row of RT_FUNCTIONS names a
   function, its result and its parameters, each type as a pair of its
   IR type and its C type. A row of a record names a field the same way,
   by the member of the C struct. antic declares every call and lays out
   every record from the IR column alone. The tests runtime_functions and
   runtime_records of tests/unit/test_lower.c read the C column against
   the declarations of src/rt/, the prototype of each function and the
   offset of each member, so a change on one side that the other lacks
   fails the unit tests on the host. */

#include <stddef.h>
#include <stdint.h>

#include "ir.h"

/* The runtime calls the program's main through the symbol of function
   RUNTIME_ENTRY in module RUNTIME_MODULE, anti.rt.main. That module path
   is reserved. */
#define RUNTIME_MODULE "anti.rt"
#define RUNTIME_ENTRY "main"

/* The runtime defines the functions of the root class anti.lang.Object,
   its descriptor and its ancestors under this C prefix. */
#define RUNTIME_ROOT "anti_lang_Object_"

/* DESIGN: the slot of an injectable interface is one global per
   interface, of the runtime module. Its name is this prefix and the
   path of the interface. The passes over the whole program write data
   of the runtime module, and that data belongs to the object that
   links. No Anti identifier holds a dot, so no module of the standard
   library takes such a name. */
#define INJECT_SLOT_PREFIX "inject."

/* The runtime function that `anti.plugin.load` calls. A program that
   holds it can host a plugin, so the link exports its symbols. */
#define PLUGIN_LOAD "anti_rt_plugin_load"

/* The two providers of the manifest that name a library rather than a
   function of the program. `plugin:` carries the path after it, and
   `discover` searches the directories of the `plugins` key. */
#define PROVIDER_PLUGIN "plugin:"
#define PROVIDER_DISCOVER "discover"

/* The most parameters a runtime function in RT_FUNCTIONS takes. */
#define RT_PARAMS_MAX 9

/* The count of parameters of a row, the arguments after its result. */
#define RT_COUNT(...) RT_COUNT_(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1)
#define RT_COUNT_(r, a1, a2, a3, a4, a5, a6, a7, a8, a9, n, ...) n

#define RT_CAT(a, b) RT_CAT_(a, b)
#define RT_CAT_(a, b) a##b

/* m applied to the result and to each parameter of a row, with commas
   between. */
#define RT_EACH(m, ...) RT_CAT(RT_EACH_, RT_COUNT(__VA_ARGS__))(m, __VA_ARGS__)
#define RT_EACH_0(m, r) m r
#define RT_EACH_1(m, r, a) m r, m a
#define RT_EACH_2(m, r, a, b) m r, m a, m b
#define RT_EACH_3(m, r, a, b, c) m r, m a, m b, m c
#define RT_EACH_4(m, r, a, b, c, d) m r, m a, m b, m c, m d
#define RT_EACH_5(m, r, a, b, c, d, e) m r, m a, m b, m c, m d, m e
#define RT_EACH_6(m, r, a, b, c, d, e, f)                                  \
    m r, m a, m b, m c, m d, m e, m f
#define RT_EACH_7(m, r, a, b, c, d, e, f, g)                               \
    m r, m a, m b, m c, m d, m e, m f, m g
#define RT_EACH_8(m, r, a, b, c, d, e, f, g, h)                            \
    m r, m a, m b, m c, m d, m e, m f, m g, m h
#define RT_EACH_9(m, r, a, b, c, d, e, f, g, h, i)                         \
    m r, m a, m b, m c, m d, m e, m f, m g, m h, m i

/* The runtime functions that generated code calls, in the order of
   their names. A row is X(id, name, result, parameters...). */
#define RT_FUNCTIONS(X)                                                    \
    X(RT_FN_ASSERT_FAILED, anti_rt_assert_failed, (VOID, void),            \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_ATOMIC_ADD, anti_rt_atomic_add, (I64, int64_t), (PTR, void *), \
      (I64, int64_t), (I64, int64_t))                                      \
    X(RT_FN_ATOMIC_AND, anti_rt_atomic_and, (I64, int64_t), (PTR, void *), \
      (I64, int64_t), (I64, int64_t))                                      \
    X(RT_FN_ATOMIC_COMPARE_SWAP, anti_rt_atomic_compare_swap,              \
      (I8, int8_t), (PTR, void *), (I64, int64_t), (I64, int64_t),         \
      (I64, int64_t))                                                      \
    X(RT_FN_ATOMIC_LOAD, anti_rt_atomic_load, (I64, int64_t),              \
      (PTR, const void *), (I64, int64_t))                                 \
    X(RT_FN_ATOMIC_OR, anti_rt_atomic_or, (I64, int64_t), (PTR, void *),   \
      (I64, int64_t), (I64, int64_t))                                      \
    X(RT_FN_ATOMIC_STORE, anti_rt_atomic_store, (VOID, void),              \
      (PTR, void *), (I64, int64_t), (I64, int64_t))                       \
    X(RT_FN_ATOMIC_SUB, anti_rt_atomic_sub, (I64, int64_t), (PTR, void *), \
      (I64, int64_t), (I64, int64_t))                                      \
    X(RT_FN_ATOMIC_SWAP, anti_rt_atomic_swap, (I64, int64_t),              \
      (PTR, void *), (I64, int64_t), (I64, int64_t))                       \
    X(RT_FN_BACKTRACE_ON, anti_rt_backtrace_on, (I8, bool))                \
    X(RT_FN_CAST_FAILED, anti_rt_cast_failed, (VOID, void),                \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_CHAN_CLOSE, anti_rt_chan_close, (VOID, void), (PTR, void *))   \
    X(RT_FN_CHAN_DELETE, anti_rt_chan_delete, (VOID, void), (PTR, void *)) \
    X(RT_FN_CHAN_NEW, anti_rt_chan_new, (PTR, void *), (I64, int64_t),     \
      (I64, int64_t))                                                      \
    X(RT_FN_CHAN_RECV, anti_rt_chan_recv, (PTR, void *), (PTR, void *),    \
      (PTR, void *))                                                       \
    X(RT_FN_CHAN_SEND, anti_rt_chan_send, (VOID, void), (PTR, void *),     \
      (PTR, const void *))                                                 \
    X(RT_FN_CHECK_FAILED, anti_rt_check_failed, (VOID, void),              \
      (PTR, const unsigned char *), (I64, int64_t), (I32, int32_t),        \
      (I64, int64_t), (I64, int64_t))                                      \
    X(RT_FN_COMPARE_BYTES, anti_rt_compare_bytes, (I64, int64_t),          \
      (PTR, const unsigned char *), (I64, int64_t),                        \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_COPY_BUFFER, anti_rt_copy_buffer, (PTR, void *),               \
      (PTR, const void *), (I64, int64_t))                                 \
    X(RT_FN_DELETE, anti_rt_delete, (VOID, void), (PTR, void *),           \
      (PTR, const struct anti_descriptor *))                               \
    X(RT_FN_DELETE_FROM, anti_rt_delete_from, (VOID, void), (PTR, void *), \
      (PTR, const struct anti_descriptor *), (PTR, struct anti_object *))  \
    X(RT_FN_DESCRIPTOR, anti_rt_descriptor,                                \
      (PTR, const struct anti_descriptor *), (PTR, const void *))          \
    X(RT_FN_DESTROY, anti_rt_destroy, (VOID, void), (PTR, void *),         \
      (PTR, const struct anti_descriptor *))                               \
    X(RT_FN_DESTROY_FROM, anti_rt_destroy_from, (VOID, void),              \
      (PTR, void *), (PTR, const struct anti_descriptor *),                \
      (PTR, struct anti_object *))                                         \
    X(RT_FN_DISPATCH, anti_rt_dispatch, (PTR, void *), (PTR, void *),      \
      (I64, int64_t), (PTR, anti_rt_dispatch_body *), (PTR, void *))       \
    X(RT_FN_DUP, anti_rt_dup, (PTR, void *), (PTR, void *),                \
      (PTR, const struct anti_descriptor *))                               \
    X(RT_FN_F16_TO_F32, anti_rt_f16_to_f32, (F32, float), (I32, uint32_t)) \
    X(RT_FN_F32_TO_F16, anti_rt_f32_to_f16, (I32, uint32_t), (F32, float)) \
    X(RT_FN_GIVE, anti_rt_give, (VOID, void), (PTR, struct anti_object *), \
      (PTR, void *))                                                       \
    X(RT_FN_GROW, anti_rt_grow, (PTR, void *), (PTR, void *),              \
      (I64, int64_t))                                                      \
    X(RT_FN_HASH_BYTES, anti_rt_hash_bytes, (I64, uint64_t),               \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_HOOK, anti_rt_hook, (VOID, void), (PTR, struct anti_object *), \
      (I64, int64_t))                                                      \
    X(RT_FN_HOOK_CALL, anti_rt_hook_call, (VOID, void),                    \
      (PTR, struct anti_object *), (I64, int64_t),                         \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_HOOK_CHANGED, anti_rt_hook_changed, (VOID, void),              \
      (PTR, struct anti_object *), (PTR, const struct anti_field *))       \
    X(RT_FN_HOOK_COPIED, anti_rt_hook_copied, (VOID, void),                \
      (PTR, struct anti_object *), (PTR, struct anti_object *))            \
    X(RT_FN_HOOK_FAILED, anti_rt_hook_failed, (VOID, void),                \
      (PTR, struct anti_object *), (PTR, const unsigned char *),           \
      (I64, int64_t), (PTR, struct anti_object *))                         \
    X(RT_FN_INIT, anti_rt_init, (VOID, void))                              \
    X(RT_FN_JOIN, anti_rt_join, (VOID, void), (PTR, void *),               \
      (I64, int64_t), (PTR, void *))                                       \
    X(RT_FN_JOIN_ALL, anti_rt_join_all, (VOID, void),                      \
      (PTR, void *const *), (I64, int64_t))                                \
    X(RT_FN_JOIN_ALL_HOOKED, anti_rt_join_all_hooked, (VOID, void),        \
      (PTR, void *const *), (I64, int64_t))                                \
    X(RT_FN_JOIN_HOOKED, anti_rt_join_hooked, (VOID, void),                \
      (PTR, void *), (I64, int64_t), (PTR, void *))                        \
    X(RT_FN_MUTEX_DESTROY, anti_rt_mutex_destroy, (VOID, void),            \
      (PTR, void *))                                                       \
    X(RT_FN_MUTEX_LOCK, anti_rt_mutex_lock, (VOID, void), (PTR, void *))   \
    X(RT_FN_MUTEX_LOCK_AT, anti_rt_mutex_lock_at, (VOID, void),            \
      (PTR, void *), (PTR, const char *))                                  \
    X(RT_FN_MUTEX_UNLOCK, anti_rt_mutex_unlock, (VOID, void),              \
      (PTR, void *))                                                       \
    X(RT_FN_MUTEX_UNLOCK_AT, anti_rt_mutex_unlock_at, (VOID, void),        \
      (PTR, void *))                                                       \
    X(RT_FN_OBJECT_LOCK, anti_rt_object_lock, (VOID, void), (PTR, void *)) \
    X(RT_FN_OBJECT_LOCK_AT, anti_rt_object_lock_at, (VOID, void),          \
      (PTR, void *), (PTR, const char *))                                  \
    X(RT_FN_OBJECT_LOCK_PAIR, anti_rt_object_lock_pair, (VOID, void),      \
      (PTR, void *), (PTR, void *))                                        \
    X(RT_FN_OBJECT_LOCK_PAIR_AT, anti_rt_object_lock_pair_at, (VOID, void), \
      (PTR, void *), (PTR, void *), (PTR, const char *))                   \
    X(RT_FN_OBJECT_OF, anti_rt_object_of, (PTR, void *), (PTR, void *))    \
    X(RT_FN_OBJECT_UNLOCK, anti_rt_object_unlock, (VOID, void),            \
      (PTR, void *))                                                       \
    X(RT_FN_OBJECT_UNLOCK_AT, anti_rt_object_unlock_at, (VOID, void),      \
      (PTR, void *))                                                       \
    X(RT_FN_OBJECT_UNLOCK_PAIR, anti_rt_object_unlock_pair, (VOID, void),  \
      (PTR, void *), (PTR, void *))                                        \
    X(RT_FN_OBJECT_UNLOCK_PAIR_AT, anti_rt_object_unlock_pair_at,          \
      (VOID, void), (PTR, void *), (PTR, void *))                          \
    X(RT_FN_OUT_OF_MEMORY, anti_rt_out_of_memory, (VOID, void),            \
      (I64, int64_t))                                                      \
    X(RT_FN_PARALLEL, anti_rt_parallel, (VOID, void), (PTR, const void *), \
      (I64, int64_t), (I64, int64_t), (I64, int64_t), (I64, int64_t),      \
      (PTR, anti_rt_parallel_body *), (PTR, void *), (PTR, void **),       \
      (PTR, int64_t *))                                                    \
    X(RT_FN_PATTERN_HASH, anti_rt_pattern_hash, (I64, uint64_t),           \
      (PTR, const void *))                                                 \
    X(RT_FN_PATTERN_SAME, anti_rt_pattern_same, (I8, int8_t),              \
      (PTR, const void *), (PTR, const void *))                            \
    X(RT_FN_REGEX_LITERAL, anti_rt_regex_literal, (PTR, void *),           \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_REGEX_LITERAL_BYTES, anti_rt_regex_literal_bytes, (PTR, void *), \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_SAME_BYTES, anti_rt_same_bytes, (I32, int),                    \
      (PTR, const unsigned char *), (I64, int64_t),                        \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_SELECT, anti_rt_select, (I64, int64_t), (PTR, void *const *),  \
      (PTR, void *const *), (I64, int64_t), (PTR, void **))                \
    X(RT_FN_SNAPSHOT_DUP, anti_rt_snapshot_dup, (PTR, void *),             \
      (PTR, const void *))                                                 \
    X(RT_FN_SNAPSHOT_FREE, anti_rt_snapshot_free, (VOID, void),            \
      (PTR, void *))                                                       \
    X(RT_FN_SNAPSHOT_NEW, anti_rt_snapshot_new, (PTR, void *),             \
      (I64, int64_t))                                                      \
    X(RT_FN_SNAPSHOT_TEXT, anti_rt_snapshot_text, (VOID, void),            \
      (PTR, void *), (I64, int64_t), (PTR, const unsigned char *),         \
      (I64, int64_t))                                                      \
    X(RT_FN_TABLE_UNSET, anti_rt_table_unset, (VOID, void),                \
      (PTR, const unsigned char *), (I64, int64_t))                        \
    X(RT_FN_WALK_CHANGED, anti_rt_walk_changed, (VOID, void),              \
      (PTR, const unsigned char *), (I64, int64_t),                        \
      (PTR, const unsigned char *), (I64, int64_t), (I64, int64_t))

#define RT_FUNCTION_ENUM(id, name, ...) id,
enum rt_function { RT_FUNCTIONS(RT_FUNCTION_ENUM) RT_FUNCTION_COUNT };
#undef RT_FUNCTION_ENUM

/* The IR form of one row of RT_FUNCTIONS. types[0] is the result and
   the parameters follow it. */
struct rt_signature {
    const char *name;
    size_t param_count;
    enum ir_type types[1 + RT_PARAMS_MAX];
};

const struct rt_signature *rt_signature(enum rt_function f);
const char *rt_name(enum rt_function f);

/* The records, each a list of rows F(id, member, IR type), in the order
   of the members of its C struct. */

/* struct anti_descriptor of src/rt/object.h, the record at entry 0 of the
   table of a class. A struct, a variant and a `?T` have one as well. */
#define RT_DESCRIPTOR_RECORD(F)                                            \
    F(RT_DESCRIPTOR_NAME, name, PTR)                                       \
    F(RT_DESCRIPTOR_NAME_LENGTH, name_length, I64)                         \
    F(RT_DESCRIPTOR_PARENT, parent, PTR)                                   \
    F(RT_DESCRIPTOR_SIZE, size, I64)                                       \
    F(RT_DESCRIPTOR_DEPTH, depth, I64)                                     \
    F(RT_DESCRIPTOR_ANCESTORS, ancestors, PTR)                             \
    F(RT_DESCRIPTOR_FIELD_COUNT, field_count, I64)                         \
    F(RT_DESCRIPTOR_FIELDS, fields, PTR)                                   \
    F(RT_DESCRIPTOR_DESTRUCT, destruct, PTR)                               \
    F(RT_DESCRIPTOR_OFFSET, offset, I64)                                   \
    F(RT_DESCRIPTOR_FUNCTION_COUNT, function_count, I64)                   \
    F(RT_DESCRIPTOR_FUNCTIONS, functions, PTR)                             \
    F(RT_DESCRIPTOR_VERSION, version, PTR)                                 \
    F(RT_DESCRIPTOR_VERSION_LENGTH, version_length, I64)                   \
    F(RT_DESCRIPTOR_VERSIONS, versions, PTR)                               \
    F(RT_DESCRIPTOR_TYPE_ARG_COUNT, type_arg_count, I64)                   \
    F(RT_DESCRIPTOR_TYPE_ARGS, type_args, PTR)

/* struct anti_field of src/rt/object.h, one field of a descriptor. */
#define RT_FIELD_RECORD(F)                                                 \
    F(RT_FIELD_NAME, name, PTR)                                            \
    F(RT_FIELD_NAME_LENGTH, name_length, I64)                              \
    F(RT_FIELD_OFFSET, offset, I64)                                        \
    F(RT_FIELD_TYPE, type, I64)                                            \
    F(RT_FIELD_OWNED, owned, I64)                                          \
    F(RT_FIELD_DESCRIPTOR, descriptor, PTR)

/* struct anti_function of src/rt/object.h, one public function of the
   chain of a class. */
#define RT_FUNCTION_RECORD(F)                                              \
    F(RT_FUNCTION_NAME, name, PTR)                                         \
    F(RT_FUNCTION_NAME_LENGTH, name_length, I64)                           \
    F(RT_FUNCTION_SLOT, slot, I64)                                         \
    F(RT_FUNCTION_PARAM_COUNT, param_count, I64)                           \
    F(RT_FUNCTION_SIGNATURE, signature, PTR)

/* struct anti_versions of src/rt/object.h, the chain and the floor of an
   abstract class. */
#define RT_VERSIONS_RECORD(F)                                              \
    F(RT_VERSIONS_CHAIN, chain, PTR)                                       \
    F(RT_VERSIONS_CHAIN_LENGTH, chain_length, I64)                         \
    F(RT_VERSIONS_FLOOR, floor, PTR)                                       \
    F(RT_VERSIONS_FLOOR_LENGTH, floor_length, I64)

/* struct anti_class of src/rt/registry.h, one class of the registry. */
#define RT_CLASS_RECORD(F)                                                 \
    F(RT_CLASS_DESCRIPTOR, descriptor, PTR)                                \
    F(RT_CLASS_INIT, init, PTR)                                            \
    F(RT_CLASS_MODULE, module, PTR)                                        \
    F(RT_CLASS_MODULE_LENGTH, module_length, I64)                          \
    F(RT_CLASS_FLAGS, flags, I64)

/* struct anti_registry of src/rt/registry.h. */
#define RT_REGISTRY_RECORD(F)                                              \
    F(RT_REGISTRY_COUNT, count, I64)                                       \
    F(RT_REGISTRY_CLASSES, classes, PTR)

/* struct anti_backtrace_default of src/rt/trace.h. */
#define RT_BACKTRACE_DEFAULT_RECORD(F)                                     \
    F(RT_BACKTRACE_DEFAULT_ON, on, I64)

/* struct anti_trampoline of src/rt/reflect.h. */
#define RT_TRAMPOLINE_RECORD(F)                                            \
    F(RT_TRAMPOLINE_SIGNATURE, signature, PTR)                             \
    F(RT_TRAMPOLINE_CALL, call, PTR)

/* struct anti_trampolines of src/rt/reflect.h. */
#define RT_TRAMPOLINES_RECORD(F)                                           \
    F(RT_TRAMPOLINES_COUNT, count, I64)                                    \
    F(RT_TRAMPOLINES_ITEMS, items, PTR)

/* struct anti_slots of src/rt/plugin.h. */
#define RT_SLOTS_RECORD(F)                                                 \
    F(RT_SLOTS_DESCRIPTOR, descriptor, PTR)                                \
    F(RT_SLOTS_SLOT_COUNT, slot_count, I64)                                \
    F(RT_SLOTS_BITS, bits, PTR)

/* struct anti_slot_table of src/rt/plugin.h. */
#define RT_SLOT_TABLE_RECORD(F)                                            \
    F(RT_SLOT_TABLE_COUNT, count, I64)                                     \
    F(RT_SLOT_TABLE_INTERFACES, interfaces, PTR)                           \
    F(RT_SLOT_TABLE_REFLECT, reflect, I64)

/* struct anti_inject_slot of src/rt/conf.h. */
#define RT_INJECT_SLOT_RECORD(F)                                           \
    F(RT_INJECT_SLOT_PROVIDER, provider, PTR)

/* struct anti_injectable of src/rt/conf.h. */
#define RT_INJECTABLE_RECORD(F)                                            \
    F(RT_INJECTABLE_NAME, name, PTR)                                       \
    F(RT_INJECTABLE_OWNER, owner, PTR)                                     \
    F(RT_INJECTABLE_FIELD, field, PTR)                                     \
    F(RT_INJECTABLE_FINAL, final, I64)                                     \
    F(RT_INJECTABLE_SLOT, slot, PTR)                                       \
    F(RT_INJECTABLE_LIBRARY, library, PTR)                                 \
    F(RT_INJECTABLE_LIBRARY_LENGTH, library_length, I64)                   \
    F(RT_INJECTABLE_DISCOVER, discover, I64)                               \
    F(RT_INJECTABLE_HOLDER, holder, PTR)                                   \
    F(RT_INJECTABLE_THUNK, thunk, PTR)                                     \
    F(RT_INJECTABLE_DESCRIPTOR, descriptor, PTR)

/* struct anti_injectables of src/rt/conf.h. */
#define RT_INJECTABLES_RECORD(F)                                           \
    F(RT_INJECTABLES_COUNT, count, I64)                                    \
    F(RT_INJECTABLES_INTERFACES, interfaces, PTR)

/* struct anti_provides of src/rt/plugin.h. */
#define RT_PROVIDES_RECORD(F)                                              \
    F(RT_PROVIDES_PATH, path, PTR)                                         \
    F(RT_PROVIDES_PATH_LENGTH, path_length, I64)                           \
    F(RT_PROVIDES_DESCRIPTOR, descriptor, PTR)                             \
    F(RT_PROVIDES_CLASS_OF, class_of, PTR)                                 \
    F(RT_PROVIDES_INIT, init, PTR)                                         \
    F(RT_PROVIDES_OFFSET, offset, I64)                                     \
    F(RT_PROVIDES_FLAGS, flags, I64)                                       \
    F(RT_PROVIDES_CHAIN, chain, PTR)                                       \
    F(RT_PROVIDES_CHAIN_LENGTH, chain_length, I64)                         \
    F(RT_PROVIDES_FIELDS, fields, I64)                                     \
    F(RT_PROVIDES_SIZE, size, I64)                                         \
    F(RT_PROVIDES_BUILT, built, PTR)                                       \
    F(RT_PROVIDES_BUILT_LENGTH, built_length, I64)

/* struct anti_provided of src/rt/plugin.h. */
#define RT_PROVIDED_RECORD(F)                                              \
    F(RT_PROVIDED_COUNT, count, I64)                                       \
    F(RT_PROVIDED_ENTRIES, entries, PTR)                                   \
    F(RT_PROVIDED_VERSION, version, PTR)                                   \
    F(RT_PROVIDED_VERSION_LENGTH, version_length, I64)                     \
    F(RT_PROVIDED_CLASS_COUNT, class_count, I64)                           \
    F(RT_PROVIDED_CLASSES, classes, PTR)

/* Every record, as R(id, IR name, C struct, list, count). The IR name
   names the aggregate in the IR and in an array of the record. */
#define RT_RECORDS(R)                                                      \
    R(RT_RECORD_DESCRIPTOR, "anti.rt.Descriptor", anti_descriptor,         \
      RT_DESCRIPTOR_RECORD, RT_DESCRIPTOR_ITEM_COUNT)                      \
    R(RT_RECORD_FIELD, "anti.rt.Field", anti_field, RT_FIELD_RECORD,       \
      RT_FIELD_ITEM_COUNT)                                                 \
    R(RT_RECORD_FUNCTION, "anti.rt.Function", anti_function,               \
      RT_FUNCTION_RECORD, RT_FUNCTION_ITEM_COUNT)                          \
    R(RT_RECORD_VERSIONS, "anti.rt.Versions", anti_versions,               \
      RT_VERSIONS_RECORD, RT_VERSIONS_ITEM_COUNT)                          \
    R(RT_RECORD_CLASS, "anti.rt.Class", anti_class, RT_CLASS_RECORD,       \
      RT_CLASS_ITEM_COUNT)                                                 \
    R(RT_RECORD_REGISTRY, "anti.rt.Registry", anti_registry,               \
      RT_REGISTRY_RECORD, RT_REGISTRY_ITEM_COUNT)                          \
    R(RT_RECORD_BACKTRACE_DEFAULT, "anti.rt.BacktraceDefault",             \
      anti_backtrace_default, RT_BACKTRACE_DEFAULT_RECORD,                 \
      RT_BACKTRACE_DEFAULT_ITEM_COUNT)                                     \
    R(RT_RECORD_TRAMPOLINE, "anti.rt.Trampoline", anti_trampoline,         \
      RT_TRAMPOLINE_RECORD, RT_TRAMPOLINE_ITEM_COUNT)                      \
    R(RT_RECORD_TRAMPOLINES, "anti.rt.Trampolines", anti_trampolines,      \
      RT_TRAMPOLINES_RECORD, RT_TRAMPOLINES_ITEM_COUNT)                    \
    R(RT_RECORD_SLOTS, "anti.rt.Slots", anti_slots, RT_SLOTS_RECORD,       \
      RT_SLOTS_ITEM_COUNT)                                                 \
    R(RT_RECORD_SLOT_TABLE, "anti.rt.SlotTable", anti_slot_table,          \
      RT_SLOT_TABLE_RECORD, RT_SLOT_TABLE_ITEM_COUNT)                      \
    R(RT_RECORD_INJECT_SLOT, "anti.rt.InjectSlot", anti_inject_slot,       \
      RT_INJECT_SLOT_RECORD, RT_INJECT_SLOT_ITEM_COUNT)                    \
    R(RT_RECORD_INJECTABLE, "anti.rt.Injectable", anti_injectable,         \
      RT_INJECTABLE_RECORD, RT_INJECTABLE_ITEM_COUNT)                      \
    R(RT_RECORD_INJECTABLES, "anti.rt.Injectables", anti_injectables,      \
      RT_INJECTABLES_RECORD, RT_INJECTABLES_ITEM_COUNT)                    \
    R(RT_RECORD_PROVIDES, "anti.rt.Provides", anti_provides,               \
      RT_PROVIDES_RECORD, RT_PROVIDES_ITEM_COUNT)                          \
    R(RT_RECORD_PROVIDED, "anti.rt.Provided", anti_provided,               \
      RT_PROVIDED_RECORD, RT_PROVIDED_ITEM_COUNT)

#define RT_ITEM_ENUM(id, member, type) id,
#define RT_RECORD_ITEM_ENUM(id, ir_name, c_struct, list, count)            \
    enum { list(RT_ITEM_ENUM) count };
RT_RECORDS(RT_RECORD_ITEM_ENUM)
#undef RT_RECORD_ITEM_ENUM
#undef RT_ITEM_ENUM

#define RT_RECORD_ENUM(id, ir_name, c_struct, list, count) id,
enum rt_record { RT_RECORDS(RT_RECORD_ENUM) RT_RECORD_COUNT };
#undef RT_RECORD_ENUM

/* The name of record r in the IR, and its count of items. */
const char *rt_record_name(enum rt_record r);
size_t rt_record_items(enum rt_record r);

/* The aggregate of record r in m, added on its first use. */
uint32_t rt_record_agg(struct ir_module *m, enum rt_record r);

#endif
