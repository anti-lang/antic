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

/* DESIGN: antic writes the modules a plugin reaches into its library as
   the bytes of ANTI_PLUGIN_MODULES_SYMBOL: this text, then each module
   path after one space, ended by a NUL. A library whose load fails
   because the host lacks a symbol shows no table, so the loader finds
   the list in the bytes of the file and names the module of the symbol,
   by R133 of "Rules of 2026-10-13". */
#define ANTI_PLUGIN_MODULES "anti-plugin-modules:"
#define ANTI_PLUGIN_MODULES_SYMBOL "anti_rt_plugin_modules"

#endif
