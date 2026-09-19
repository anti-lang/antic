# VM setup

Two virtual machines on the development Mac run the test suite on Linux ARM64 and on Windows ARM64. They cover the untested items of `docs/decisions.md` that need no x86_64 processor. The targets linux-x86_64 and windows-x86_64 stay for the GitHub Actions runners on a release day. The Mac runs macos-x86_64 programs itself, through Rosetta.

Eddie installs UTM, both images and the Windows licence. Each VM gets the tools below once. The test run then exports the committed tree of the Mac with `git archive`, as a reader's checkout holds it, and builds it in a fresh directory.

## Linux ARM64

### Image and machine

- Image: `ubuntu-24.04.5-live-server-arm64.iso` from https://cdimage.ubuntu.com/releases/24.04/release/. The runner `ubuntu-24.04-arm` of the workflow runs the same Ubuntu release, 24.04.
- UTM: a new machine with Virtualize, Linux and the ISO. Give it 4 cores, 8 GB of memory and 64 GB of disk.
- In the installer, select "Install OpenSSH server".

### Packages and tools

Run these commands in the VM once. The tools go to `~/anti`, outside the tree that each run replaces.

```sh
sudo apt update
sudo apt install -y build-essential cmake git curl xz-utils
git -C /tmp clone --depth 1 https://github.com/FoundingFuture/book-writing-a-compiler book
cmake -DDEST=$HOME/anti/clang -P /tmp/book/tools/get-clang.cmake
cmake -DDEST=$HOME/anti/toolchain -P /tmp/book/tools/get-llvm.cmake
cmake -DDEST=$HOME/anti/sysroot -DLLVM_BIN=$HOME/anti/toolchain/bin \
  -DCLANG_DIR=$HOME/anti/clang -DACCEPT_LICENSE=yes \
  -DTARGETS="linux-x86_64;linux-arm64;macos-arm64;macos-x86_64;windows-x86_64;windows-arm64" \
  -P /tmp/book/tools/get-sysroot.cmake
cmake -DDEST=$HOME/anti/raylib -P /tmp/book/tools/get-raylib.cmake
```

The clone only supplies the scripts. If the repository is private, copy `tools/` with `scp -r tools anti-linux:/tmp/book/` instead. The macOS sysroots hold Zig's stubs, which link every macOS program that names no framework. `-DACCEPT_LICENSE=yes` accepts the terms of the Microsoft CRT and Windows SDK that xwin downloads.

A program that names a framework also needs the stubs of Apple's SDK. On the Mac, `build/anti sdk export` writes `apple-sdk-<version>.tar.xz`. Copy it to the VM and install it with `anti sdk import <bundle> --sysroot $HOME/anti/sysroot`. A copy of the SDK itself works too, as `-DAPPLE_SDK=<MacOSX.sdk>` of `get-sysroot.cmake`.

### SSH from the Mac

Add the VM to `~/.ssh/config` on the Mac. UTM shows the address of the VM in its window.

```text
Host anti-linux
    HostName <address of the VM>
    User <user of the VM>
```

### Test run

Run this command in the repository on the Mac. It exports `HEAD`, builds it and runs every test.

```sh
git archive HEAD | ssh anti-linux 'rm -rf book && mkdir book && tar -x -f - -C book && cd book &&
  cmake -S . -B build -DANTIC_CLANG_DIR="$HOME/anti/clang" \
    -DANTIC_LLVM_DIR="$HOME/anti/toolchain" \
    -DANTIC_SYSROOT_DIR="$HOME/anti/sysroot" \
    -DANTIC_RAYLIB_DIR="$HOME/anti/raylib/raylib-6.0" &&
  cmake --build build -j"$(nproc)" &&
  ctest --test-dir build -j"$(nproc)" --output-on-failure'
```

### Coverage

The machine ran the whole suite on 2026-09-19 with the pinned clang of `23.1.1-anti.3` and
the sysroots of all six targets. It passes 364 of 364, and ASan and UBSan with the pinned
runtimes of the clang archive pass 363 of 363 each. The macOS programs link against Zig's
stubs, and `link_identity_macos-arm64` finds the bytes that the Mac links.

| Untested item | Tests that run it |
|---|---|
| linux-arm64 programs, which the Mac only links | `program_*`, `std_*`, `dev_modules` |
| Static PIE programs against musl | `program_*`, linked with ld.lld against `~/anti/sysroot/linux-arm64` |
| C objects built against glibc in a musl program | `program_abi_structs`, `program_abi_raymath`, `program_abi_wchar`, whose C files gcc compiles |
| The 16-aligned register rule of AAPCS64 outside Apple | `program_abi_structs` |
| `.init_array` constructors of a shared library | `clib_shared`, `clib_loader`, `clib_two` |
| The dynamic linker path `/lib/ld-linux-aarch64.so.1` | `program_platform_linker` with GNU ld |
| The ABI probe on Linux | `abi_probe` |
| `--soname` | No test links a versioned library yet. The VM can run one. |

## Windows ARM64

### Image and machine

- Image: the Windows 11 ARM64 ISO from https://www.microsoft.com/en-us/software-download/windows11arm64, with Eddie's licence. Parallels Desktop works as well as UTM.
- UTM: a new machine with Virtualize, Windows and the ISO. Give it 4 cores, 8 GB of memory and 128 GB of disk.

### Packages and tools

Run these commands in an administrator PowerShell in the VM once. The first two lines install and start the OpenSSH server, as Microsoft's page "Get started with OpenSSH Server for Windows" describes.

```powershell
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Start-Service sshd; Set-Service -Name sshd -StartupType 'Automatic'
winget install --id Git.Git -e
winget install --id Kitware.CMake -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --includeRecommended"
```

The test `program_abi_raymath` compiles a raymath binding, so the VM needs the pinned raylib release. One command downloads it, and `C:\anti\test.cmd` below passes the directory.

```powershell
cmake -DDEST=C:\anti\raylib -P C:\anti\book\tools\get-raylib.cmake
```

The same commands install the pinned clang and LLVM tools, which `tools/clang-pin` and `tools/llvm-pin` name for Windows on ARM64.

```powershell
cmake -DDEST=C:\anti\clang -P C:\anti\book\tools\get-clang.cmake
cmake -DDEST=C:\anti\toolchain -P C:\anti\book\tools\get-llvm.cmake
```

One command installs the sysroots of all six targets. The Windows ones come from the Build Tools of the VM, and the macOS ones from Zig. A program that names a framework takes Apple's SDK as on the Linux VM.

```powershell
cmake -DDEST=C:\anti\sysroot -DLLVM_BIN=C:\anti\toolchain\bin -DCLANG_DIR=C:\anti\clang -DTARGETS="linux-x86_64;linux-arm64;macos-arm64;macos-x86_64;windows-x86_64;windows-arm64" -P C:\anti\book\tools\get-sysroot.cmake
```

Create `C:\anti\test.cmd` with these lines. `vcvarsall.bat arm64` sets the MSVC environment, including the variable `LIB` that lld-link reads, and puts the Ninja of Visual Studio on the path. The pinned clang needs Ninja, because the Visual Studio generator takes the compiler of its own toolset.

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" arm64
cd /d C:\anti\book
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DANTIC_CLANG_DIR=C:/anti/clang -DANTIC_LLVM_DIR=C:/anti/toolchain -DANTIC_SYSROOT_DIR=C:/anti/sysroot -DANTIC_RAYLIB_DIR=C:/anti/raylib/raylib-6.0
cmake --build build
ctest --test-dir build --output-on-failure
```

The machine ran the whole suite on 2026-09-19 with the pinned clang and the sysroots of all
six targets. It passes 343 of 349, among them every cross link and `link_identity_macos-arm64`.
Six fail on Windows alone: `program_args` and `std_text` read UTF-8 output as the console
code page, `program_abi_wchar` expects a `wchar_t` of 32 bits, and `std_error`, `std_log` and
`std_signals` wait for a look.

### SSH from the Mac

```text
Host anti-windows
    HostName <address of the VM>
    User <user of the VM>
```

### Test run

Run these commands in the repository on the Mac. The first one replaces `C:\anti\book` with the export of `HEAD`, and the second one runs the build and the tests.

```sh
git archive HEAD | ssh anti-windows "rmdir /s /q C:\anti\book & mkdir C:\anti\book && tar -x -f - -C C:\anti\book"
ssh anti-windows C:\anti\test.cmd
```

### Coverage

| Untested item | Tests that run it |
|---|---|
| windows-arm64 programs, which the Mac only links | `program_*`, `std_*`, linked with lld-link against `LIB` |
| The Windows branch of `rt/start.c`, compiled with MSVC | every `program_*` test, through `anti_rt.lib` |
| `c_wchar` at 16 bits and `c_long` at 32 bits against MSVC | `program_abi_wchar`, `program_abi_structs` |
| Exception unwinding through an Anti frame | The runtime test that closes the unwind data of chapter 16: an exception raised in C unwinds through an Anti frame to a handler in C. `llvm-readobj` proves that the tables parse, not that Windows walks them. The test does not exist yet. |
| Shared libraries, `.def` files and `.CRT$XCU` constructors | The `clib_*` tests skip Windows until `tests/run_clib.cmake` supports MSVC. |
| Whether `link.exe` accepts the COFF symbol form | `program_platform_linker` skips Windows until it supports `link.exe`. |
| Float aggregates of one member against MSVC | No test compares them yet. The VM can run one. |
| `tools/install.ps1` | The installer has never run. The VM is the first machine that can parse it. |
| The published package of this host | `anti-<version>-windows-<cpu>.tar.xz`, once the suite passes here. |
| Pointer equality of DLL functions | Waits for the DLL-based libraries of chapter 23. |

## Runners

The workflow `.github/workflows/test.yml` covers linux-x86_64 and windows-x86_64 when Eddie starts it on a release day. They are the Linux programs of that target and the Windows x64 unwind data at run time. The Mac already runs the macos-x86_64 programs, float conversions included.

No machine here has an x86_64 processor, so neither x86_64 package runs on hardware of its
own. Both run under emulation instead, and both passed on 2026-09-16. The Linux ARM64
machine compiled and ran a linux-x86_64 program with `qemu-x86_64`. Windows 11 on ARM
emulates x64, and the installer takes that package with `ANTI_ARCH` or `--intel` into
`%USERPROFILE%\.anti-x86_64`. Its `antic.exe` reports `windows-x86_64`, compiles a program
through the emulated llvm-mc and lld-link, and `llvm-objdump -h` calls the result
`coff-x86-64`. Emulation proves that the binaries of the package run and that the programs
they compile are correct. It does not prove the timing or the behaviour of a real x86_64
processor, which the runners cover on a release day.
