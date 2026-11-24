[CmdletBinding()]
param(
    [string]$ToolchainBin = 'C:\msys64\mingw64\bin'
)

$ErrorActionPreference = 'Stop'
$compiler = Join-Path $ToolchainBin 'g++.exe'
$ninja = Join-Path $ToolchainBin 'ninja.exe'
if (!(Test-Path -LiteralPath $compiler) -or !(Test-Path -LiteralPath $ninja)) {
    throw 'Set -ToolchainBin to an MSYS2 MinGW bin directory containing g++.exe and ninja.exe.'
}
$build = Join-Path $PSScriptRoot 'build-release-windows'
$dist = Join-Path $PSScriptRoot 'dist'
$previousPath = $env:Path
try {
    $env:Path = "$ToolchainBin;$previousPath"
    & cmake -S $PSScriptRoot -B $build -G Ninja "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release -DALLFETCH_PACKAGE_RELEASE=ON
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & cmake --build $build --config Release
    if ($LASTEXITCODE) { throw 'Release build failed.' }
    & cpack --config (Join-Path $build 'CPackConfig.cmake') -C Release -B $dist
    if ($LASTEXITCODE) { throw 'ZIP packaging failed.' }
}
finally {
    $env:Path = $previousPath
}
