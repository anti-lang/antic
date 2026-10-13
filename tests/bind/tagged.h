/* An enum without a name that is the type of a field, for
   `anti bind --clang`. Its enumerators are names of the file, as those
   of an enum without a name outside a struct are. */
#ifndef TAGGED_H
#define TAGGED_H

struct Tagged
{
    enum
    {
        TAGGED_X,
        TAGGED_Y
    } which;
    int n;
};

void tagged_take(struct Tagged *t);

#endif
