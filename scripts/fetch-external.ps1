<#
.SYNOPSIS
    Downloads the prebuilt third-party packages (OpenCASCADE 8.0.1 and its
    3rdparty bundle) that CMakeLists.txt expects under External/.

.DESCRIPTION
    External/opencascade-8.0.1-vc14-64 and External/3rdparty-vc14-64 are large
    prebuilt binaries (~1.6 GB) and are intentionally not tracked by git.
    This script downloads the official archives from GitHub, verifies their
    SHA-256, and unpacks them into External/.

    Already-present packages are skipped unless -Force is used.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\fetch-external.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\fetch-external.ps1 -Force
#>
[CmdletBinding()]
param(
    [string]$ExternalDir = '',
    [string]$CacheDir = '',
    [switch]$Force,
    [switch]$SkipHashCheck
)

$ErrorActionPreference = 'Stop'

# $PSScriptRoot is not available while default parameter values are evaluated.
if ([string]::IsNullOrEmpty($ExternalDir)) {
    $ExternalDir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\External'
}
if ([string]::IsNullOrEmpty($CacheDir)) {
    $CacheDir = Join-Path $env:LOCALAPPDATA 'myCAD\external-cache'
}

# Pinned packages. Url + Sha256 point at immutable GitHub release assets.
$packages = @(
    @{
        Name   = 'OpenCASCADE 8.0.1 (release + debug, vc14 x64)'
        File   = 'occt-combined-with-debug-no-pch.zip'
        Url    = 'https://github.com/Open-Cascade-SAS/OCCT/releases/download/V8.0.1/occt-combined-with-debug-no-pch.zip'
        Sha256 = 'a16b14501275bc404025691968d64cc64ee51e2de1ae7f4820ec48cd21994938'
        Dirs   = @('opencascade-8.0.1-vc14-64')
    },
    @{
        Name   = 'OpenCASCADE 8.0.1 3rdparty bundle (vc14 x64)'
        File   = '3rdparty-vc14-64.zip'
        Url    = 'https://github.com/Open-Cascade-SAS/OCCT/releases/download/V8.0.1/3rdparty-vc14-64.zip'
        Sha256 = '12d04f688c583b8e896758f65cd7012458fee921cedb792ec6caaf469b372598'
        Dirs   = @('3rdparty-vc14-64')
    }
)

# Layout CMakeLists.txt relies on.
$requiredPaths = @(
    'opencascade-8.0.1-vc14-64/cmake/OpenCASCADEConfig.cmake',
    'opencascade-8.0.1-vc14-64/win64/vc14/bin/TKernel.dll',
    'opencascade-8.0.1-vc14-64/win64/vc14/bind/TKernel.dll',
    'opencascade-8.0.1-vc14-64/win64/vc14/lib/TKernel.lib',
    'opencascade-8.0.1-vc14-64/win64/vc14/libd/TKernel.lib',
    'opencascade-8.0.1-vc14-64/data/step/linkrods.step',
    '3rdparty-vc14-64/tbb-2021.13.0-x64/lib/cmake/TBB/TBBConfig.cmake',
    '3rdparty-vc14-64/openvr-1.14.15-64/bin/win64/openvr_api.dll'
)

$ExternalDir = [System.IO.Path]::GetFullPath($ExternalDir)
New-Item -ItemType Directory -Force -Path $ExternalDir, $CacheDir | Out-Null

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Test-PackagePresent($Package) {
    foreach ($dir in $Package.Dirs) {
        if (-not (Test-Path -LiteralPath (Join-Path $ExternalDir $dir))) {
            return $false
        }
    }
    return $true
}

foreach ($pkg in $packages) {
    if ((Test-PackagePresent $pkg) -and -not $Force) {
        Write-Host "[skip] $($pkg.Name) is already present in $ExternalDir"
        continue
    }

    $archive = Join-Path $CacheDir $pkg.File

    if ((Test-Path -LiteralPath $archive) -and -not $Force) {
        Write-Host "[cache] $($pkg.File)"
    }
    else {
        Write-Host "[download] $($pkg.Name)"
        Write-Host "           $($pkg.Url)"
        # Invoke-WebRequest keeps the progress bar; disable it to avoid very slow
        # rendering on PS 5.1 for large files.
        $prevProgress = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue'
        try {
            Invoke-WebRequest -Uri $pkg.Url -OutFile $archive -UseBasicParsing
        }
        finally {
            $ProgressPreference = $prevProgress
        }
    }

    if ($SkipHashCheck) {
        Write-Warning "SHA-256 check skipped for $($pkg.File)"
    }
    else {
        Write-Host "[verify] $($pkg.File)"
        $actual = Get-Sha256 $archive
        if ($actual -ne $pkg.Sha256.ToLowerInvariant()) {
            Remove-Item -LiteralPath $archive -Force
            throw "SHA-256 mismatch for $($pkg.File)`n  expected: $($pkg.Sha256)`n  actual:   $actual"
        }
    }

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("mycad-extract-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
    try {
        Write-Host "[extract] $($pkg.File)"
        Expand-Archive -LiteralPath $archive -DestinationPath $tempDir -Force

        foreach ($dir in $pkg.Dirs) {
            $source = Join-Path $tempDir $dir
            if (-not (Test-Path -LiteralPath $source)) {
                # Fall back to searching one level deep (archive layout may vary).
                $source = Get-ChildItem -LiteralPath $tempDir -Directory -Recurse -Filter $dir |
                    Select-Object -First 1 -ExpandProperty FullName
            }
            if (-not $source) {
                throw "Archive $($pkg.File) does not contain '$dir'"
            }

            $target = Join-Path $ExternalDir $dir
            if (Test-Path -LiteralPath $target) {
                Remove-Item -LiteralPath $target -Recurse -Force
            }
            Write-Host "[install] $dir -> $ExternalDir"
            Move-Item -LiteralPath $source -Destination $target
        }
    }
    finally {
        Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$missing = @()
foreach ($rel in $requiredPaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $ExternalDir $rel))) {
        $missing += $rel
    }
}

if ($missing.Count -gt 0) {
    Write-Host ''
    Write-Host 'Missing expected files:' -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

Write-Host ''
Write-Host "External dependencies are ready in $ExternalDir" -ForegroundColor Green
Write-Host 'Next: cmake -S . -B build && cmake --build build --config Debug'