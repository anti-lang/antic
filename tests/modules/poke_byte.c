/* Copy a file with one byte replaced: poke_byte <in> <out> <offset>
   <value>, both numbers in decimal. The tests of a damaged library file
   write it with this, since a CMake script cannot write a NUL byte. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *in;
    FILE *out;
    unsigned long offset;
    unsigned long at = 0;
    int value;
    int c;
    int status = 0;

    if (argc != 5) {
        fputs("usage: poke_byte <in> <out> <offset> <value>\n", stderr);
        return 2;
    }
    offset = strtoul(argv[3], NULL, 10);
    value = (int)strtoul(argv[4], NULL, 10);
    in = fopen(argv[1], "rb");
    if (in == NULL) {
        fprintf(stderr, "poke_byte: cannot read %s\n", argv[1]);
        return 1;
    }
    out = fopen(argv[2], "wb");
    if (out == NULL) {
        fprintf(stderr, "poke_byte: cannot write %s\n", argv[2]);
        fclose(in);
        return 1;
    }
    while ((c = fgetc(in)) != EOF) {
        if (fputc(at == offset ? value : c, out) == EOF) {
            status = 1;
            break;
        }
        at++;
    }
    if (at <= offset) {
        fprintf(stderr, "poke_byte: %s holds %lu bytes\n", argv[1], at);
        status = 1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        status = 1;
    }
    return status;
}
