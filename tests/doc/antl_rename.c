/* The hostile library file of the doc test. It copies a file and
   replaces every occurrence of one byte string with another. The two are
   of one length, so every length and offset of the file stays valid. A
   library file holds NUL bytes, which a CMake script cannot write. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *in;
    FILE *out;
    char *bytes;
    size_t size = 0;
    size_t capacity = 1 << 16;
    size_t from_length;
    size_t n;
    size_t i;

    if (argc != 5 || strlen(argv[3]) != strlen(argv[4]) || argv[3][0] == '\0') {
        fputs("usage: antl_rename <in> <out> <from> <to>, with <from> and <to> "
              "of one length\n", stderr);
        return 2;
    }
    from_length = strlen(argv[3]);
    in = fopen(argv[1], "rb");
    bytes = malloc(capacity);
    if (in == NULL || bytes == NULL) {
        fprintf(stderr, "antl_rename: cannot read %s\n", argv[1]);
        return 1;
    }
    while ((n = fread(bytes + size, 1, capacity - size, in)) > 0) {
        size += n;
        if (size == capacity) {
            char *grown = realloc(bytes, capacity * 2);
            if (grown == NULL) {
                fputs("antl_rename: out of memory\n", stderr);
                return 1;
            }
            bytes = grown;
            capacity *= 2;
        }
    }
    fclose(in);
    for (i = 0; i + from_length <= size; i++) {
        if (memcmp(bytes + i, argv[3], from_length) == 0) {
            memcpy(bytes + i, argv[4], from_length);
            i += from_length - 1;
        }
    }
    out = fopen(argv[2], "wb");
    if (out == NULL || fwrite(bytes, 1, size, out) != size ||
        fclose(out) != 0) {
        fprintf(stderr, "antl_rename: cannot write %s\n", argv[2]);
        return 1;
    }
    free(bytes);
    return 0;
}
