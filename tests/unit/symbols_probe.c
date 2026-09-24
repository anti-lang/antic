/* Look a function up in a binary of another target with the readers of
   src/rt/symbols.c, which the runtime of that target uses on itself.

   The ELF form finds the address of the symbol in the file and adds the
   offset. It prints the function, the file and the line there. The
   Mach-O form reads an object of a -g build, as the debug map of an
   executable names it. It prints the file and the line. The two forms are
   `symbols_probe elf <file> <symbol> <offset>` and
   `symbols_probe macho <object> <symbol> <offset>`. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "symbols.h"

static unsigned char *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *bytes = NULL;
    size_t room = 0;
    size_t n;

    *size = 0;
    if (f == NULL) {
        return NULL;
    }
    for (;;) {
        if (*size == room) {
            unsigned char *more = realloc(bytes, room == 0 ? 65536 : 2 * room);
            if (more == NULL) {
                free(bytes);
                fclose(f);
                return NULL;
            }
            bytes = more;
            room = room == 0 ? 65536 : 2 * room;
        }
        n = fread(bytes + *size, 1, room - *size, f);
        if (n == 0) {
            break;
        }
        *size += n;
    }
    fclose(f);
    return bytes;
}

int main(int argc, char **argv)
{
    struct anti_found found;
    unsigned char *file;
    size_t size;
    unsigned long long offset;
    uint64_t vaddr;
    int status = 1;

    if (argc != 5) {
        fputs("usage: symbols_probe elf|macho <file> <symbol> <offset>\n",
              stderr);
        return 2;
    }
    file = read_all(argv[2], &size);
    offset = strtoull(argv[4], NULL, 0);
    memset(&found, 0, sizeof found);
    if (file == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[2]);
        return 1;
    }
    if (strcmp(argv[1], "elf") == 0) {
        if (anti_rt_elf_symbol(file, size, argv[3], &vaddr) &&
            anti_rt_elf_function(file, size, vaddr + offset, &found)) {
            printf("%.*s", (int)found.function_length, found.function);
            if (anti_rt_elf_line(file, size, vaddr + offset, &found)) {
                printf(" %.*s:%lld", (int)found.file_length, found.file,
                       (long long)found.line);
            }
            printf("\n");
            status = 0;
        }
    } else if (strcmp(argv[1], "macho") == 0) {
        anti_rt_macho_relocate(file, size);
        if (anti_rt_macho_object_line(file, size, argv[3], offset, &found)) {
            printf("%.*s:%lld\n", (int)found.file_length, found.file,
                   (long long)found.line);
            status = 0;
        }
    }
    if (status != 0) {
        fprintf(stderr, "%s: no answer for %s+%s\n", argv[2], argv[3],
                argv[4]);
    }
    free(file);
    return status;
}
