#ifndef ANTI_PROJECT_H
#define ANTI_PROJECT_H

/* `anti new <name>`: a project of the default layout, with a starter
   manifest and one module that prints and returns. name is the package
   name, a module path of at least two segments, and the directory takes
   its last segment. Returns the exit status of anti. */
int project_new(const char *name);

#endif
