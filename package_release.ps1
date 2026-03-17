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

# Bundle MSVC/OpenMP redistributables when available so the package is closer to self-contained.
$redistRoots = @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC"
)
$redistDlls = @(
    "msvcp140.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "vcomp140.dll"
)
foreach ($dll in $redistDlls) {
    foreach ($root in $redistRoots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $src = Get-ChildItem -Path $root -Recurse -File -Filter $dll -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like "*\x64\*" } |
            Sort-Object FullName -Descending |
            Select-Object -First 1

        if ($src) {
            Copy-Item -Path $src.FullName -Destination $binDir -Force
            break
        }
    }
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
    "README.md",
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

$zipPath = Join-Path (Split-Path -Parent $outDirAbs) "MeshDenoiser-release.zip"
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}
Compress-Archive -Path (Join-Path $outDirAbs "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Release package created at: $outDirAbs"
Write-Host "Manifest: $manifestPath"
Write-Host "Zip archive: $zipPath"
