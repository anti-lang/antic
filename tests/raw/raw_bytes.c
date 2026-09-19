/* The C side of the raw-bytes test. A C program writes a CR, an LF and a
   NUL, and the test scripts read the bytes it wrote. */
#include "../binary_stdio.h"
#include <stdio.h>

int main(void)
{
    static const char bytes[] = "one\r\ntwo\nthree\0four\n";

    fwrite(bytes, 1, sizeof bytes - 1, stdout);
    return 0;
}
