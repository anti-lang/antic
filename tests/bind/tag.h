/* A struct whose tag is the name of a function of the header, as
   struct stat beside stat() of <sys/stat.h>. */

struct stat {
    int size;
};

int stat(const char *path, struct stat *out);
