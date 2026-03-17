param(
    [string]$BuildDir = "build-vcpkg",
    [string]$Config = "Release",
    [string]$OutDir = "dist\release"
)

$ErrorActionPreference = "Stop"

function Copy-IfExists {
    param(
        [string]$SourcePath,
        [string]$DestDir
    )
    if (Test-Path $SourcePath) {
        Copy-Item -Path $SourcePath -Destination $DestDir -Force
        return $true
    }
    return $false
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildConfigDir = Join-Path $repoRoot (Join-Path $BuildDir $Config)
$outDirAbs = Join-Path $repoRoot $OutDir

if (!(Test-Path $buildConfigDir)) {
    throw "Build output directory not found: $buildConfigDir"
}

if (Test-Path $outDirAbs) {
    Remove-Item -Path $outDirAbs -Recurse -Force
}
New-Item -ItemType Directory -Path $outDirAbs -Force | Out-Null

$binDir = Join-Path $outDirAbs "bin"
$configDir = Join-Path $outDirAbs "config"
$docsDir = Join-Path $outDirAbs "docs"

New-Item -ItemType Directory -Path $binDir -Force | Out-Null
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
New-Item -ItemType Directory -Path $docsDir -Force | Out-Null

# Core executable
$requiredBin = "MeshDenoiser.exe"
$requiredSrc = Join-Path $buildConfigDir $requiredBin
if (!(Test-Path $requiredSrc)) {
    throw "Required binary missing: $requiredSrc"
}
Copy-Item -Path $requiredSrc -Destination $binDir -Force

# Runtime DLLs to ship if present
$runtimeDllPatterns = @(
    "*.dll"
)
foreach ($pattern in $runtimeDllPatterns) {
    Get-ChildItem -Path $buildConfigDir -Filter $pattern -File -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item -Path $_.FullName -Destination $binDir -Force }
}

# Default option files
$defaultConfigFiles = @(
    "DenoisingOptions.txt"
)
foreach ($cfg in $defaultConfigFiles) {
    $src = Join-Path $repoRoot $cfg
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $configDir -Force
    }
}

# Documentation and licenses
$docFiles = @(
    "README",
    "LICENSE",
    "TUNING_GUIDE.md",
    "MIGRATION_GUIDE.md"
)
foreach ($doc in $docFiles) {
    $src = Join-Path $repoRoot $doc
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $docsDir -Force
    }
}

# Bundle third-party license snapshots when available
$thirdPartyDir = Join-Path $docsDir "third_party"
New-Item -ItemType Directory -Path $thirdPartyDir -Force | Out-Null

Copy-IfExists -SourcePath (Join-Path $repoRoot "build\_deps\rapidobj-src\LICENSE") -DestDir $thirdPartyDir | Out-Null
Copy-IfExists -SourcePath (Join-Path $repoRoot "build\_deps\eigen-src\COPYING.README") -DestDir $thirdPartyDir | Out-Null
Copy-IfExists -SourcePath (Join-Path $repoRoot "build\_deps\eigen-src\COPYING.MPL2") -DestDir $thirdPartyDir | Out-Null

# OpenMesh license header reference file (project vendor does not include standalone LICENSE in this checkout)
$openMeshHeader = Join-Path $repoRoot "external\OpenMesh\Core\IO\MeshIO.hh"
if (Test-Path $openMeshHeader) {
    Copy-Item -Path $openMeshHeader -Destination (Join-Path $thirdPartyDir "OpenMesh_MeshIO_hh_license_header.txt") -Force
}

# Emit manifest for traceability
$manifestPath = Join-Path $outDirAbs "PACKAGE_MANIFEST.txt"
$manifestLines = @()
$manifestLines += "Package generated: $(Get-Date -Format o)"
$manifestLines += "Build directory: $buildConfigDir"
$manifestLines += "Output directory: $outDirAbs"
$manifestLines += ""
$manifestLines += "Files:"

Get-ChildItem -Path $outDirAbs -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($outDirAbs.Length).TrimStart('\')
        $manifestLines += "$relative"
    }

$manifestLines | Set-Content -Path $manifestPath -Encoding ASCII

Write-Host "Release package created at: $outDirAbs"
Write-Host "Manifest: $manifestPath"
