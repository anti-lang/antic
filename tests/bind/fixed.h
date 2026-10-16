/* Enums with a fixed base, which C lays out as that type on every
   target. */
enum Small : unsigned char { SMALL_A, SMALL_B };
enum Wide : unsigned long long { WIDE_A, WIDE_B };
struct HoldSmall { enum Small s; char c; };
struct HoldWide { enum Wide w; char c; };
void take(struct HoldSmall *s, struct HoldWide *w);
