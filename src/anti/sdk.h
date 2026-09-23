#ifndef ANTI_SDK_H
#define ANTI_SDK_H

/* Pack the .tbd stubs and the version of a MacOSX.sdk into
   <out>/apple-sdk-<version>.tar.xz. sdk names the SDK, or NULL for the
   newest of the Command Line Tools that ld64.lld reads. A Mac alone runs
   it. Returns the exit status of anti. */
int sdk_export(const char *sdk, const char *out);

/* Unpack a bundle of sdk_export into sdk/ of macos-arm64 and macos-x86_64
   under sysroot, and record the SHA-256 digest of the bundle there. It
   reads the file and fetches nothing. Returns the exit status of anti. */
int sdk_import(const char *bundle, const char *sysroot);

#endif
