/* A struct packed to two bytes, which Anti has no form for. anti bind
   refuses the header and names the struct. */
#pragma pack(push, 2)
struct PackTwo
{
    char a;
    int b;
};
#pragma pack(pop)
