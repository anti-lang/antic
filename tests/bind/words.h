/* Names of C that are words of Anti: two records, an enum, an enumerator,
   one of an enum without a name and macros. */
enum self { SELF_A, fn };
struct type { int a; enum self s; };
struct dup { int x; int fn; };
enum { none = 3 };
#define class 4
#define PICK fn
#define ONE_DUP (struct dup){ 1, 2 }
void take(struct type *t, enum self s, struct dup d);
