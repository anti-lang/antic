/* The name of the index of plugins, shared by antic, the runtime and
   anti.

   DESIGN: one definition read from three sides. `antic --lib shared
   --no-runtime` writes the index beside a plugin, the runtime reads it
   to discover a provider and opens no library to find out what is inside
   one, and `anti symbols` reads it to find the libraries of a
   deployment. src/antic/target.h includes this header rather than
   repeating the name. */
#ifndef ANTI_RT_PLUGIN_INDEX_H
#define ANTI_RT_PLUGIN_INDEX_H

#define ANTI_PLUGIN_INDEX "anti-plugins.toml"

#endif
