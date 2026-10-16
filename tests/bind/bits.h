/* A bitfield of one bit of an unsigned type that no `u` starts. */
#include <stddef.h>
struct Bits { size_t one : 1; int n; };
void take(struct Bits *b);
