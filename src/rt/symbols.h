/* The readers of the symbol table and the line table that
   anti.lang.StackTrace.symbolize uses. They read bytes that a caller
   holds, an image in memory or a file read whole. They call nothing of
   the system, so one test reads the tables of every target on one
   host. */
#ifndef ANTI_SYMBOLS_H
#define ANTI_SYMBOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What a lookup found. Each text points into the bytes the reader was
   given, and stays valid while they do. */
struct anti_found {
    const char *function;
    size_t function_length;
    const char *file;
    size_t file_length;
    int64_t line;
};

/* The name of an Anti function from its COFF symbol, which
   `_A4anti4lang_StackTrace.capture` spells for
   `anti.lang.StackTrace.capture`. Returns the length it writes to out, or
   0 when name is not of that form or needs more than room bytes. */
size_t anti_coff_demangle(const char *name, size_t length, char *out,
                          size_t room);

/* One function of a walk over a symbol table. It carries the name
   without the prefix a format adds, the address and the size. The size
   is 0 where the table names none, and a walk that returns true
   stops. */
typedef bool (*anti_function_fn)(void *context, const char *name,
                                 uint64_t vaddr, uint64_t size);

/* Every function of the symbol table of an ELF file. `anti build`
   reads them for the map of a symbols archive. */
bool anti_elf_functions(const uint8_t *file, size_t size,
                        anti_function_fn fn, void *context);

/* The function of an ELF file that holds vaddr, from its symbol table. */
bool anti_elf_function(const uint8_t *file, size_t size, uint64_t vaddr,
                       struct anti_found *out);
/* The file and the line of vaddr, from the line table of an ELF file. */
bool anti_elf_line(const uint8_t *file, size_t size, uint64_t vaddr,
                   struct anti_found *out);
/* The address of the symbol name in an ELF file. */
bool anti_elf_symbol(const uint8_t *file, size_t size, const char *name,
                     uint64_t *vaddr);

/* The bytes from vaddr to the end of the readable PT_LOAD segment that
   holds it, or 0 when none holds it. headers holds the count program
   headers of a mapped 64-bit ELF module. Each is 56 bytes, little-endian,
   as the dynamic loader gives them. */
uint64_t anti_elf_loaded_room(const uint8_t *headers, size_t count,
                              uint64_t vaddr);

/* The symbol table of a Mach-O image or file. */
struct anti_macho_table {
    const uint8_t *symbols;         /* the nlist_64 records */
    uint32_t count;
    const char *strings;
    uint32_t strings_size;
};

/* The symbol table of the Mach-O header. A mapped image reads it at the
   address its segment has after slide, and a file at its offset, where
   size bounds every read. */
bool anti_macho_table(const uint8_t *header, size_t size, bool mapped,
                      intptr_t slide, struct anti_macho_table *out);
/* Every function of a Mach-O symbol table, as anti_elf_functions gives
   the functions of an ELF file. */
bool anti_macho_functions(const struct anti_macho_table *t,
                          anti_function_fn fn, void *context);

/* The function that holds vaddr, an address before the slide. */
bool anti_macho_function(const struct anti_macho_table *t, uint64_t vaddr,
                         struct anti_found *out);
/* The address of the symbol name, without the `_` that Mach-O adds. */
bool anti_macho_symbol(const struct anti_macho_table *t, const char *name,
                       uint64_t *vaddr);
/* The object file that the debug map names for vaddr, the symbol of the
   function that holds it and the start of that function. */
bool anti_macho_debug_map(const struct anti_macho_table *t, uint64_t vaddr,
                          const char **object, const char **symbol,
                          uint64_t *start);
/* Resolve the relocations of the line table of a Mach-O object file in
   place, so it holds the addresses of the object. */
void anti_macho_relocate(uint8_t *file, size_t size);
/* The file and the line of offset bytes into the function symbol, from
   the line table of a Mach-O object file. */
bool anti_macho_object_line(const uint8_t *file, size_t size,
                            const char *symbol, uint64_t offset,
                            struct anti_found *out);

#endif
