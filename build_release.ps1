param(
    [string]$BuildDir = "build-vcpkg",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64",
    [string]$OutDir = "dist\\release",
    [switch]$EnableCholmod = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

$enableCholmodValue = if ($EnableCholmod) { "ON" } else { "OFF" }

cmake -S $repoRoot -B (Join-Path $repoRoot $BuildDir) -G $Generator -A $Arch `
  -DENABLE_CHOLMOD=$enableCholmodValue

cmake --build (Join-Path $repoRoot $BuildDir) --config $Config --target MeshDenoiser

powershell -ExecutionPolicy Bypass -File (Join-Path $repoRoot "package_release.ps1") `
  -BuildDir $BuildDir `
  -Config $Config `
  -OutDir $OutDir

Write-Host "Build + package complete."
Write-Host "Package output: $(Join-Path $repoRoot $OutDir)"
