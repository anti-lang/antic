#ifndef ANTIC_SHARED_H
#define ANTIC_SHARED_H

/* The names and the records that more than one part of antic shares,
   and anti through antic.h: the suffixes of the files antic writes, the
   layout of the runtime archive, the linker and the mode of LTO, the
   package record of a library file and the paths of the modules of the
   standard library that the compiler names. */

#include <stddef.h>

/* The suffix of a library file. */
#define ANTL_SUFFIX ".antl"

/* The suffix of the C header that a library for C carries. antic writes
   the file and `anti build` copies it into `dist/`, so the name stands
   here once. */
#define HEADER_SUFFIX ".h"

/* The runtime archive keeps the runtime of each target and processor
   level in <runtime>/RUNTIME_LIB_DIR/<target>/<level>/. A program built
   with --cpu below the target's default then links a runtime of its own
   level. The native libraries stay in <runtime>/RUNTIME_LIB_DIR/<target>/
   and are built for the default level alone. The sysroot that lld links
   against is <runtime>/RUNTIME_SYSROOT_DIR/<target>/, and the pinned LLVM
   tools are in <runtime>/RUNTIME_BIN_DIR/. A macOS sysroot names the version of
   its stubs in the file SYSROOT_SDK_VERSION. The stubs of Apple's SDK for
   a program that names a framework lie in SYSROOT_APPLE_SDK_DIR of it. */
#define RUNTIME_LIB_DIR "lib"
#define RUNTIME_SYSROOT_DIR "sysroot"
#define RUNTIME_BIN_DIR "bin"
/* The library files of the standard library, a search root of antic. */
#define RUNTIME_STD_DIR "std"
/* The licence texts of the runtime archive, one file per component in
   <runtime>/RUNTIME_LICENSES_DIR/. CMakeLists.txt writes the text of the
   runtime under RUNTIME_LICENSE_NAME, tools/get-sysroot.cmake the text of
   musl as <MUSL_PACKAGE>.txt and src/native/mimalloc.cmake the text of
   mimalloc as <MIMALLOC_PACKAGE>.txt. The test license_notice reads the
   two of a program of musl. */
#define RUNTIME_LICENSES_DIR "licenses"
/* The text of a component is <name>RUNTIME_LICENSE_SUFFIX, and the name
   of the runtime's is RUNTIME_LICENSE_NAME. */
#define RUNTIME_LICENSE_SUFFIX ".txt"
#define RUNTIME_LICENSE_NAME "anti_rt"
/* The licence of antic and anti in the root of a package, which the
   CMake build writes into the root of the runtime archive as well. `anti
   license` prints it before the components. */
#define RUNTIME_OWN_LICENSE "LICENSE"
/* The record of the upstream source of every pinned component, beside
   the texts: one line per component with the name of its text, its
   version and the URL of the exact source package, which
   tools/upstream-sources.cmake writes from the pins. `anti license`
   prints the line of a component after its text. */
#define RUNTIME_SOURCES_FILE "sources.txt"
/* The packages of the C library of musl and of its allocator in the
   notice of a program of musl. */
#define MUSL_PACKAGE "musl"
#define MIMALLOC_PACKAGE "mimalloc"
/* The object beside the runtime library that a bundled archive carries
   instead of the licence text of src/rt/license.c. */
#define RUNTIME_LICENSE_STUB "anti_rt_license_stub"
/* The member of a static library for C that holds the copy of the
   package header is <name>LIBRARY_PACKAGE_SUFFIX and the object suffix of
   the target. `anti license --from-archive` finds the copy by it. */
#define LIBRARY_PACKAGE_SUFFIX ".package"
/* The symbol that the marker member of every bundled Mach-O library
   defines, and with _<package> after it the one that the library object
   of that package refers to. See bundle in driver_library.c. */
#define RUNTIME_BUNDLE_MARKER "anti_rt_bundle"
/* DESIGN: the Linux link mode against glibc takes the sysroot and the
   runtime of the target name with this suffix, linux-arm64-glibc beside
   linux-arm64. tools/get-sysroot.cmake and CMakeLists.txt spell the same
   names. */
#define LINUX_GLIBC_SUFFIX "-glibc"
#define SYSROOT_SDK_VERSION "sdk-version"
#define SYSROOT_APPLE_SDK_DIR "sdk"

/* DESIGN: lld of the pinned LLVM release links for every target, and
   --linker platform selects the linker of the host's own toolchain. */
enum linker { LINKER_LLD, LINKER_PLATFORM };

/* DESIGN: --lto full|thin links the bitcode of a program in release mode
   with the runtime as bitcode through the LTO of lld, choice D7 of
   docs/work-order-llvm-optimization.md. Eddie decided on 2026-10-07 that
   a release build links through full LTO by default, since it gives the
   finer control over what code goes into a program, and --lto none links
   the program and the runtime as objects. The runtime archive keeps the bitcode of a target
   and level in <runtime>/RUNTIME_LIB_DIR/<target>/<level>/
   RUNTIME_BITCODE_DIR/<mode>/, named by the word of the option, since
   ThinLTO imports nothing from the bitcode of full LTO and ld64.lld
   cannot choose the mode of one kind of bitcode for both. CMakeLists.txt
   and tools/pack-anti.cmake spell the same names. */
#define RUNTIME_BITCODE_DIR "bitcode"
enum lto { LTO_NONE, LTO_FULL, LTO_THIN };

/* A dependency in the package header of a library file. */
struct package_dependency {
    const char *name;               /* a module path */
    const char *constraint;         /* a version constraint, such as ^1.2 */
    const char *url;                /* the repository */
};

/* The version of a package whose build names none. It stands in the
   package header of a library file and in every class descriptor. */
#define PACKAGE_VERSION_DEFAULT "0.0.0"

/* The package header of a library file. antic alone writes the module
   path as the name, version 0.0.0 and empty licence fields. */
struct package {
    const char *name;
    const char *version;
    const struct package_dependency *dependencies;
    size_t dependency_count;
    const char *license;            /* an SPDX identifier */
    const char *license_text;       /* the full text */
    const char *const *attribution; /* lines copied verbatim */
    size_t attribution_count;
};

/* The module of the patterns, which a program that holds a pattern
   literal links with its native library. */
#define REGEX_MODULE "anti.regex"
/* `Object.deserialize` takes its memory from an allocator of this
   class, and `delete(p, from)` and `destroy(p, from)` give memory back
   to one. */
#define MEM_MODULE "anti.mem"
#define MEM_ALLOCATOR "Allocator"

#endif
