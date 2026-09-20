# Install Anti for the user who runs it, under $HOME\.anti.
#
#   irm https://anti-lang.com/install.ps1 | iex
#   $env:ANTI_ARCH = "x86_64"; irm https://anti-lang.com/install.ps1 | iex
#
# It downloads the package of this host, checks its SHA-256 against the
# SHA256SUMS beside it, unpacks it and offers the sysroot of Windows. The
# C runtime and the Windows SDK belong to Microsoft, so the script asks
# before xwin fetches them. Unpacking uses tar.exe, which Windows 10 and
# later carry.
#
# ANTI_ARCH takes the package of another processor, which Windows on ARM
# runs under emulation. A copy of the file on disk takes --arm or --intel
# on its command line instead, because iex passes no arguments.
#
#   ANTI_VERSION    the version to install, default the newest
#   ANTI_BASE       where the packages are served from, default
#                   anti-lang.com. A release checks its own packages
#                   through it, before they are published, with a local
#                   directory or a file:// prefix
#   ANTI_ARCH       arm64 or x86_64, default the processor of this machine
#   ANTI_HOME       where to install, default $HOME\.anti, and
#                   $HOME\.anti-<cpu> for another processor
#   ANTI_REPLACE    yes or no to replacing an install that is there
#   ANTI_PATH       yes or no to the entry in the PATH of the account
$ErrorActionPreference = "Stop"

$base = if ($env:ANTI_BASE) { $env:ANTI_BASE } else { "https://anti-lang.com/downloads/resources" }

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
# the LLVM tools, and the package carries none. anti-lang.com serves this
# script, and GitHub serves the tools. A key that travelled with them could
# be replaced with them. anti-lang.com serves the same key as keys/release.pem.
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

# DESIGN: a package of the other processor runs under emulation and is
# installed beside the native one, never over it. Both directories then
# work, and the one on the PATH stays the native one.
if ($arch -eq $native) {
    $home_dir = if ($env:ANTI_HOME) { $env:ANTI_HOME } else { "$HOME\.anti" }
} else {
    $home_dir = if ($env:ANTI_HOME) { $env:ANTI_HOME } else { "$HOME\.anti-$arch" }
    Say "installing the $arch package, which this machine runs under emulation"
}

if (-not $version) {
    $version = (Get-AntiText "$base/anti/latest").Trim()
}
$asset = "anti-$version-$host_name.tar.xz"
$work = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Force $work | Out-Null

try {
    Say "downloading $asset"
    Get-AntiFile "$base/anti/$version/$asset" "$work\$asset"
    $sums = Get-AntiText "$base/anti/$version/SHA256SUMS"
    $want = ($sums -split "`n" | Where-Object { $_ -match [regex]::Escape($asset) }) -split "\s+" | Select-Object -First 1
    $got = (Get-FileHash "$work\$asset" -Algorithm SHA256).Hash.ToLower()
    if ($want -ne $got) { Fail "$asset has SHA-256 $got, expected $want" }

    if ((Test-Path $home_dir) -and -not (Ask REPLACE "$home_dir exists. Replace it?")) {
        Fail "nothing installed"
    }
    Remove-Item -Recurse -Force $home_dir -ErrorAction SilentlyContinue
    tar.exe -xJf "$work\$asset" -C $work
    Move-Item "$work\anti" $home_dir
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
        $openssl = (Get-Command openssl -ErrorAction SilentlyContinue).Source
        foreach ($dir in "clangarm64\bin", "mingw64\bin", "usr\bin") {
            if (-not $openssl -and (Test-Path "$env:ProgramFiles\Git\$dir\openssl.exe")) {
                $openssl = "$env:ProgramFiles\Git\$dir\openssl.exe"
            }
        }
        if ($openssl) {
            # openssl writes to stderr on a failure, which Stop would turn
            # into an error before the exit code is read.
            Set-Content -Path "$work\release.pem" -Value $release_key -Encoding ascii
            $previous = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            & $openssl dgst -sha256 -binary -out "$work\llvm-sums.sha256" "$work\llvm-sums" 2>$null
            $hashed = $LASTEXITCODE
            & $openssl pkeyutl -verify -pubin -inkey "$work\release.pem" -in "$work\llvm-sums.sha256" -sigfile "$work\llvm-sums.sig" 2>$null | Out-Null
            $verified = $LASTEXITCODE
            $ErrorActionPreference = $previous
            if ($hashed -ne 0 -or $verified -ne 0) {
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
    # called by its path.
    if ($arch -ne $native) {
        Say "run it with: $home_dir\bin\antic.exe hello.anti -o hello.exe"
        return
    }

    # The PATH of the account lives under HKCU and needs no administrator.
    # The installer writes it only with a yes, and the uninstaller removes
    # what it wrote.
    $bin = "$home_dir\bin"
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
