# Configures and builds Keys with MSVC + Ninja.
# Usage:  .\build.ps1 [-BuildType Debug|Release|RelWithDebInfo] [-Test]
param(
    [ValidateSet("Debug","Release","RelWithDebInfo")]
    [string]$BuildType = "Debug",
    [switch]$Test
)

$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Visual Studio Build Tools 2022 with the C++ workload is required."
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    Write-Error "No Visual Studio installation with the C++ toolset was found."
}

# Enter the MSVC environment so cl.exe, the Windows SDK and the linker are on PATH.
Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

$qt = $env:KEYS_QT_DIR
if (-not $qt) { $qt = "C:/Qt/6.10.3/msvc2022_64" }

# Arguments are built as an array so PowerShell passes each -D intact.
$cmakeArgs = @(
    "-S", ".",
    "-B", "build",
    "-G", "Ninja",
    "-DCMAKE_PREFIX_PATH=$qt",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    ctest --test-dir build --output-on-failure
    exit $LASTEXITCODE
}
