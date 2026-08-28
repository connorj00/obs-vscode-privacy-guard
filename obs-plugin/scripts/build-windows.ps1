[CmdletBinding()]
param([switch]$SkipTests)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2022 Build Tools with the Desktop development with C++ workload.'
}

$installationPath = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $installationPath) {
    throw 'The Visual Studio 2022 x64/x86 C++ build tools were not found.'
}

$cmake = Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw 'Visual Studio CMake was not found. Add C++ CMake tools for Windows through Visual Studio Installer.'
}
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'

# Some hosts expose both Path and PATH to child .NET processes. MSBuild treats
# these as duplicate keys, so construct one canonical process-level value.
$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = "$machinePath;$userPath"

& $cmake --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

& $cmake --build --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    throw "Native plugin compilation failed with exit code $LASTEXITCODE."
}

if (-not $SkipTests) {
    & $ctest --preset windows-x64
    if ($LASTEXITCODE -ne 0) {
        throw "Native plugin tests failed with exit code $LASTEXITCODE."
    }
}

$plugin = Join-Path $PSScriptRoot '..\build_x64\RelWithDebInfo\obs-vscode-privacy-guard.dll'
Write-Host "Built OBS plugin: $([System.IO.Path]::GetFullPath($plugin))"
