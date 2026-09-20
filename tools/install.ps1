# Install Anti for the user who runs it, in the directories of Windows.
#
#   irm https://anti-lang.com/install.ps1 | iex
#   $env:ANTI_ARCH = "x86_64"; irm https://anti-lang.com/install.ps1 | iex
#
# It downloads the package of this host and SHA256SUMS from the GitHub
# release of the version, and SHA256SUMS.sig from anti-lang.com. It checks
# the signature first, then the SHA-256 of the package against the
# manifest, unpacks it and offers the sysroot of Windows. The C runtime and
# the Windows SDK belong to Microsoft, so the script asks before xwin
# fetches them. Unpacking uses tar.exe, which Windows 10 and later carry.
#
# ANTI_ARCH takes the package of another processor, which Windows on ARM
# runs under emulation. A copy of the file on disk takes --arm or --intel
# on its command line instead, because iex passes no arguments.
#
#   ANTI_VERSION    the version to install, default the newest, which the
#                   latest-release API of GitHub names
#   ANTI_BASE       a staging area to install from rather than the
#                   release, in the layout <base>\anti\<version>\<file>
#                   that the packer writes, as a local directory or with a
#                   file:// prefix. A release checks its own packages
#                   through it before they are published. It takes
#                   ANTI_VERSION with it, because a staging area names no
#                   newest version
#   ANTI_GITHUB     the release area, default the releases of
#                   anti-lang/antic. The tests of the installer stand a
#                   fake one on disk, and a fork names its own
#   ANTI_GITHUB_API the latest-release API that names the newest version,
#                   default the one of anti-lang/antic
#   ANTI_SITE_BASE  the site that serves SHA256SUMS.sig, default
#                   anti-lang.com. The tests of the installer stand a fake
#                   one on disk
#   ANTI_STAGING    the base is the staging area of a release, whose
#                   manifest step 6 of ./r signs after the checks of
#                   step 5. It takes a manifest without a signature, and
#                   never one whose signature is wrong
#   ANTI_ARCH       arm64 or x86_64, default the processor of this machine
#   ANTI_HOME       one tree to install everything under, executables
#                   included. Without it the install follows Windows:
#                   the executables in %LOCALAPPDATA%\Programs\anti\bin,
#                   the toolchain and the runtime archive in
#                   %LOCALAPPDATA%\anti. The package of another processor
#                   takes %LOCALAPPDATA%\anti-<cpu> and keeps its
#                   executables there.
#   ANTI_REPLACE    yes or no to replacing an install that is there
#   ANTI_PATH       yes or no to the entry in the PATH of the account
$ErrorActionPreference = "Stop"

# DESIGN: the two halves of a release stand on two hosts, and a forged
# release needs both of them. The GitHub release of the tag holds the
# packages, the symbols archives and SHA256SUMS. anti-lang.com holds
# SHA256SUMS.sig, the public key, the two installers and the downloads
# page. Whoever takes GitHub changes binaries the signature no longer
# covers. Whoever takes the site signs nothing, because the private key is
# on neither host. tools/release-base and tools/site-base of the
# repository name these addresses, and the test installer_github pins them
# against this copy.
$github = if ($env:ANTI_GITHUB) { $env:ANTI_GITHUB } else { "https://github.com/anti-lang/antic/releases/download" }
$github_api = if ($env:ANTI_GITHUB_API) { $env:ANTI_GITHUB_API } else { "https://api.github.com/repos/anti-lang/antic/releases/latest" }
$site = if ($env:ANTI_SITE_BASE) { $env:ANTI_SITE_BASE } else { "https://anti-lang.com" }
$base = $env:ANTI_BASE

# Read one file of the download area. A base that names a directory of
# this machine is copied rather than fetched, which is how a release
# checks its packages before anyone can download them.
function Get-AntiFile($source, $destination) {
    if ($source -match '^file://') {
        Copy-Item ($source -replace '^file:///?', '') $destination -Force
    } elseif ($source -match '^[A-Za-z]:\\|^\\\\') {
        Copy-Item $source $destination -Force
    } else {
        Invoke-WebRequest $source -OutFile $destination
    }
}

function Get-AntiText($source) {
    $work_file = [System.IO.Path]::GetTempFileName()
    Get-AntiFile $source $work_file
    $text = Get-Content $work_file -Raw
    Remove-Item $work_file -Force
    return $text
}

# DESIGN: the installer carries the public key that checks SHA256SUMS.sig of
# the package and of the LLVM tools, and neither carries a key of its own.
# anti-lang.com serves this script, and GitHub serves both downloads. A key
# that travelled with them could be replaced with them. anti-lang.com serves
# the same key as keys/release.pem.
$release_key = @'
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEao0Di9RL8gvG6oA9x7gIDJ7/zLn6
/J5i5dgtCf82Hvpro/4umWhaPA8APgrIJKLD4XDvTqhLijckvFxj0f3Bhg==
-----END PUBLIC KEY-----
'@

$version = $env:ANTI_VERSION

# The processor of the package. A file on disk takes --arm or --intel,
# and a run through iex takes ANTI_ARCH.
$arch = $env:ANTI_ARCH
foreach ($argument in $args) {
    switch ($argument) {
        "--arm"    { $arch = "arm64" }
        "--arm64"  { $arch = "arm64" }
        "--intel"  { $arch = "x86_64" }
        "--x86_64" { $arch = "x86_64" }
        "--x64"    { $arch = "x86_64" }
        default    { throw "anti: unknown option $argument" }
    }
}
if ($arch -eq "arm") { $arch = "arm64" }
if ($arch -eq "intel" -or $arch -eq "x64") { $arch = "x86_64" }

function Say($text) { Write-Host "anti: $text" }
function Fail($text) { throw "anti: $text" }

# The openssl of this machine, or the one Git for Windows carries.
function Find-AntiOpenssl {
    $found = (Get-Command openssl -ErrorAction SilentlyContinue).Source
    foreach ($dir in "clangarm64\bin", "mingw64\bin", "usr\bin") {
        if (-not $found -and (Test-Path "$env:ProgramFiles\Git\$dir\openssl.exe")) {
            $found = "$env:ProgramFiles\Git\$dir\openssl.exe"
        }
    }
    return $found
}

# Answer whether $sigfile holds the signature of $file by the release key.
function Test-AntiSignature($openssl, $file, $sigfile) {
    Set-Content -Path "$work\release.pem" -Value $release_key -Encoding ascii
    # openssl writes to stderr on a failure, which Stop would turn into an
    # error before the exit code is read.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $openssl dgst -sha256 -binary -out "$file.sha256" $file 2>$null
    $hashed = $LASTEXITCODE
    & $openssl pkeyutl -verify -pubin -inkey "$work\release.pem" `
        -in "$file.sha256" -sigfile $sigfile 2>$null | Out-Null
    $verified = $LASTEXITCODE
    $ErrorActionPreference = $previous
    return ($hashed -eq 0 -and $verified -eq 0)
}

# Ask one yes or no question. The variable of its name answers it, and a
# plain return accepts.
function Ask($name, $question) {
    $given = [Environment]::GetEnvironmentVariable("ANTI_$name")
    if ($given -match "^(y|Y|yes|Yes)$") { return $true }
    if ($given -match "^(n|N|no|No)$") { return $false }
    $answer = Read-Host "$question [Y/n]"
    return $answer -notmatch "^(n|N|no|No)$"
}

# Windows PowerShell 5.1 does not load RuntimeInformation, so the
# processor comes from the environment. A 32-bit shell on a 64-bit Windows
# reports the real processor in PROCESSOR_ARCHITEW6432.
$cpu = $env:PROCESSOR_ARCHITECTURE
if ($env:PROCESSOR_ARCHITEW6432) { $cpu = $env:PROCESSOR_ARCHITEW6432 }
switch ($cpu) {
    "AMD64" { $native = "x86_64" }
    "ARM64" { $native = "arm64" }
    default { Fail "no package for the processor $cpu" }
}
if (-not $arch) { $arch = $native }
if ($arch -ne "arm64" -and $arch -ne "x86_64") {
    Fail "the processor $arch has no package, only arm64 and x86_64"
}
$host_name = "windows-$arch"

# DESIGN: an install of Anti is local to the user and follows the
# conventions of Windows. LOCALAPPDATA is the root of the per-user files
# of an application that is not roamed. The toolchain and the runtime
# archive go in %LOCALAPPDATA%\anti, and the two executables under
# Programs\ of it, which is where a user-local install goes. "Names and
# publication" in docs/decisions.md holds the rule, src/userdirs.c builds
# the same paths for antic, and the test installer_options pins the
# spellings together.
#
# A package of the other processor runs under emulation and stands beside
# the native one. Its executables stay in the bin\ of that tree, because
# the PATH names one antic and it is the one this machine runs without
# emulation.
$local = $env:LOCALAPPDATA
if (-not $local) { throw "anti: LOCALAPPDATA names no directory of this account" }
$app = if ($arch -eq $native) { "anti" } else { "anti-$arch" }
$bin = "$local\Programs\anti\bin"
# DESIGN: ANTI_HOME names one tree that carries everything, the two
# executables included, and nothing outside it is written. A release
# checks a package that way on each VM.
$one_tree = [bool]$env:ANTI_HOME
$home_dir = if ($env:ANTI_HOME) { $env:ANTI_HOME } else { "$local\$app" }
if ($arch -ne $native) {
    Say "installing the $arch package, which this machine runs under emulation"
}

# The newest version is the tag of the newest release, which the
# latest-release API names. A staging area holds one version and no API,
# so a run against one takes the version from the caller.
if (-not $version) {
    if ($base) {
        Fail "ANTI_BASE names a staging area, which names no newest version. Set ANTI_VERSION with it."
    }
    $answer = Get-AntiText $github_api
    $found = [regex]::Match($answer, '"tag_name"\s*:\s*"v?([^"]+)"')
    if (-not $found.Success) {
        Fail "$github_api names no newest version of Anti"
    }
    $version = $found.Groups[1].Value
}

# The assets of a release stand under its tag, and the signature of the
# manifest stands on the site. A staging area of a release holds both in
# the layout the packer writes, one directory per version, because step 5
# installs before step 9 publishes anything.
$area = if ($base) { "$base/anti/$version" } else { "$github/v$version" }
$signature = if ($base) { "$area/SHA256SUMS.sig" } else { "$site/downloads/anti/$version/SHA256SUMS.sig" }
$asset = "anti-$version-$host_name.tar.xz"
$work = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Force $work | Out-Null

try {
    Say "downloading $asset"
    Get-AntiFile "$area/$asset" "$work\$asset"
    Get-AntiFile "$area/SHA256SUMS" "$work\SHA256SUMS"

    # DESIGN: SHA256SUMS says which bytes are the package, so no line of
    # it is read before openssl has checked SHA256SUMS.sig against the key
    # above. The digest of the download proves nothing on its own:
    # whoever serves the package serves the manifest beside it. The
    # signature comes from the other host for that reason. openssl checks
    # it against the key this file carries, never against one fetched
    # beside it. That is why a missing openssl stops the install here and
    # only warns for the LLVM tools, whose digest tools/llvm-pin of the
    # package carries as well.
    $signed = $true
    try {
        Get-AntiFile $signature "$work\SHA256SUMS.sig"
    } catch {
        $signed = $false
    }
    if ($signed) {
        $openssl = Find-AntiOpenssl
        if (-not $openssl) {
            Fail "openssl is missing, and without it SHA256SUMS.sig is no signature of anything. Install Git for Windows, which carries one."
        }
        if (-not (Test-AntiSignature $openssl "$work\SHA256SUMS" "$work\SHA256SUMS.sig")) {
            Fail "SHA256SUMS of $version carries no signature of the key of Anti"
        }
        Say "SHA256SUMS carries the signature of the key of Anti, from $signature"
    } elseif ($env:ANTI_STAGING -match "^(y|Y|yes|Yes)$") {
        Say "warning: the staging area of a release holds no SHA256SUMS.sig, so this installer checked the digest and not the signature"
    } else {
        Fail "$signature answered nothing, and an unsigned manifest names no package"
    }

    $sums = Get-Content "$work\SHA256SUMS" -Raw
    $want = ($sums -split "`n" | Where-Object { $_ -match [regex]::Escape($asset) }) -split "\s+" | Select-Object -First 1
    $got = (Get-FileHash "$work\$asset" -Algorithm SHA256).Hash.ToLower()
    if ($want -ne $got) { Fail "$asset has SHA-256 $got, expected $want" }

    if ((Test-Path $home_dir) -and -not (Ask REPLACE "$home_dir exists. Replace it?")) {
        Fail "nothing installed"
    }
    Remove-Item -Recurse -Force $home_dir -ErrorAction SilentlyContinue
    tar.exe -xJf "$work\$asset" -C $work
    Move-Item "$work\anti" $home_dir
    # The marker that tells a tree this installer wrote from any other
    # directory. The uninstaller removes nothing without it.
    Set-Content -Path "$home_dir\.anti-install" -Value $version
    Say "installed $version in $home_dir"

    # DESIGN: the site serves this installer, so it is always the
    # newest. The CMake scripts it drives come from the package, which is
    # as old as the version installed. The number rises when the options
    # it passes change, and a package older than that fails here rather
    # than halfway.
    $PACKAGE_API = 1
    $api = 0
    if (Test-Path "$home_dir\tools\package-api") {
        $api = [int]((Get-Content "$home_dir\tools\package-api" -Raw).Trim())
    }
    if ($api -lt $PACKAGE_API) {
        Fail "Anti $version speaks version $api of the installer interface, and this installer needs $PACKAGE_API. Install a newer version of Anti."
    }

    # DESIGN: the LLVM tools come from the release that tools/llvm-pin of
    # the package names. The pinned digest decides, and the openssl of Git
    # for Windows checks the signature of SHA256SUMS against the key of this
    # installer. A package without the pin carries the tools itself.
    $llvm_pin = "$home_dir\tools\llvm-pin"
    if (Test-Path $llvm_pin) {
        $rows = Get-Content $llvm_pin
        function Row($key) {
            (($rows | Where-Object { $_ -match "^$key=" }) -split "=", 2)[1]
        }
        $llvm_version = (Get-Content "$home_dir\tools\llvm-version" -Raw).Trim()
        $tag = (Row "tag").Replace("@VERSION@", $llvm_version)
        $release = (Row "release").Replace("@TAG@", $tag)
        $tools = (Row "file").Replace("@TAG@", $tag).Replace("@HOST@", $host_name)
        $digest = Row "$host_name-digest"
        if (-not $digest) { Fail "tools/llvm-pin names no LLVM tools for $host_name" }
        Say "downloading $tools"
        Invoke-WebRequest "$release/$tools" -OutFile "$work\$tools"
        Invoke-WebRequest "$release/SHA256SUMS" -OutFile "$work\llvm-sums"
        Invoke-WebRequest "$release/SHA256SUMS.sig" -OutFile "$work\llvm-sums.sig"
        $got = (Get-FileHash "$work\$tools" -Algorithm SHA256).Hash.ToLower()
        if ($got -ne $digest) { Fail "$tools has SHA-256 $got, expected $digest" }
        $line = Get-Content "$work\llvm-sums" | Where-Object { $_.EndsWith("  $tools") }
        $listed = ($line -split "\s+")[0]
        if ($listed -ne $digest) { Fail "SHA256SUMS of $tag lists '$listed' for $tools, and the pin $digest" }
        $openssl = Find-AntiOpenssl
        if ($openssl) {
            if (-not (Test-AntiSignature $openssl "$work\llvm-sums" "$work\llvm-sums.sig")) {
                Fail "SHA256SUMS of $tag carries no signature of the key of Anti"
            }
        } else {
            Say "warning: openssl is missing, so this installer checked the digest of $tools and not the signature"
        }
        tar.exe -xJf "$work\$tools" -C $home_dir bin licenses
        if ($LASTEXITCODE -ne 0) { Fail "tar.exe failed to unpack $tools" }
    }

    # lld answers to its four names through argv[0], and the archive
    # carries one copy of it.
    foreach ($name in "ld.lld", "ld64.lld", "lld-link") {
        if (-not (Test-Path "$home_dir\bin\$name.exe")) {
            Copy-Item "$home_dir\bin\lld.exe" "$home_dir\bin\$name.exe"
        }
    }

    # CMake installs the sysroot. The package holds the script and the
    # pins, and the pinned CMake stands in when the host has none.
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if (-not $cmake) {
        $pins = Get-Content "$home_dir\tools\cmake-pin"
        $cmake_version = (Get-Content "$home_dir\tools\cmake-version").Trim()
        $url = (($pins | Where-Object { $_ -match "^$host_name-url=" }) -split "=", 2)[1]
        $url = $url.Replace("@VERSION@", $cmake_version)
        $digest = (($pins | Where-Object { $_ -match "^$host_name-digest=" }) -split "=", 2)[1]
        Say "installing CMake $cmake_version, which the sysroot step needs"
        Invoke-WebRequest $url -OutFile "$work\cmake.zip"
        $got = (Get-FileHash "$work\cmake.zip" -Algorithm SHA256).Hash.ToLower()
        if ($digest -ne $got) { Fail "CMake has SHA-256 $got, expected $digest" }
        Expand-Archive "$work\cmake.zip" -DestinationPath "$home_dir\tools\cmake"
        $cmake = (Get-ChildItem "$home_dir\tools\cmake" -Recurse -Filter cmake.exe |
                  Select-Object -First 1).FullName
    }

    # The script takes the CRT and the SDK of the Build Tools when this
    # machine has them, which downloads nothing and needs no privilege. It
    # stops and asks for the licence when the machine has neither, because
    # xwin then fetches them from Microsoft.
    Say "laying out the sysroot of $host_name"
    $sysroot = @("-DDEST=$home_dir\sysroot", "-DLLVM_BIN=$home_dir\bin",
                 "-DTARGETS=$host_name")
    $script = "$home_dir\tools\get-sysroot.cmake"
    & $cmake $sysroot -P $script
    if ($LASTEXITCODE -ne 0) {
        Say "this machine holds no Microsoft CRT, which a Windows program"
        Say "links against. xwin fetches about 1 GB of it from Microsoft."
        if (Ask MICROSOFT "Let xwin fetch the CRT and the Windows SDK?") {
            # The progress bar of xwin draws only on a terminal of its
            # own, so CMake writes the command and this shell runs it.
            & $cmake $sysroot "-DACCEPT_LICENSE=yes" "-DSPLAT=script" -P $script
            $splat = "$home_dir\sysroot\.download\splat.cmd"
            if ($LASTEXITCODE -eq 0 -and (Test-Path $splat)) {
                & cmd /c $splat
                & $cmake $sysroot "-DACCEPT_LICENSE=yes" "-DSPLAT=done" -P $script
            }
        } else {
            Say "a Developer Command Prompt of Visual Studio needs neither,"
            Say "because lld-link reads the LIB variable it sets."
        }
    }

    # DESIGN: the PATH names one antic, and it is the one this machine runs
    # without emulation. A package of the other processor is therefore
    # called by its path, and so is an install that ANTI_HOME put in one
    # tree.
    if (($arch -ne $native) -or $one_tree) {
        Say "run it with: $home_dir\bin\antic.exe hello.anti -o hello.exe"
        return
    }

    # The two executables go in the bin directory of the user, and the
    # rest of the archive stays where it is. antic finds it by the same
    # rule that src/userdirs.c holds.
    New-Item -ItemType Directory -Force -Path $bin | Out-Null
    foreach ($program in @("antic", "anti")) {
        Copy-Item "$home_dir\bin\$program.exe" "$bin\$program.exe" -Force
    }
    Say "put antic.exe and anti.exe in $bin"

    # The PATH of the account lives under HKCU and needs no administrator.
    # The installer writes it only with a yes, and the uninstaller removes
    # what it wrote.
    $path = [Environment]::GetEnvironmentVariable("PATH", "User")
    if ($path -and ($path -split ";" | Where-Object { $_ -eq $bin })) {
        Say "$bin is already in the PATH of your account"
    } elseif (Ask PATH "Add $bin to the PATH of your account?") {
        $joined = if ($path) { "$path;$bin" } else { $bin }
        [Environment]::SetEnvironmentVariable("PATH", $joined, "User")
        $env:PATH = "$bin;$env:PATH"
        Say "added $bin to the PATH of your account and to this shell"
    } else {
        Say "add this to your PATH yourself:"
        Write-Host "    $bin"
    }
    Say "then compile a program with: antic hello.anti -o hello.exe"
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
