param(
    [string]$ProjectRoot = (Get-Location).Path
)

$ErrorActionPreference = "Stop"

# Bill's 7" ESP32-S3 Aircraft Radar support bundle
# Creates a source-only ZIP in the current user's Downloads folder.
# Explicitly excludes private config, credentials, build artifacts, and VCS metadata.

$project = (Resolve-Path $ProjectRoot).Path
$downloads = [Environment]::GetFolderPath("UserProfile") + "\Downloads"
if (-not (Test-Path $downloads)) {
    New-Item -ItemType Directory -Path $downloads | Out-Null
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$zipPath = Join-Path $downloads "esp-aircraft-radar-support-$stamp.zip"
$tempRoot = Join-Path $env:TEMP "esp-aircraft-radar-support-$stamp"

if (Test-Path $tempRoot) {
    Remove-Item -Recurse -Force $tempRoot
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null

$excludeDirNames = @(
    ".git",
    ".pio",
    ".vscode",
    ".idea",
    "build",
    "dist",
    "release",
    "node_modules",
    "__pycache__"
)

$excludeExactRelative = @(
    "include\config.h"
)

$excludePatterns = @(
    "*.bin",
    "*.elf",
    "*.map",
    "*.o",
    "*.obj",
    "*.a",
    "*.pyc",
    "*.pyo",
    "*.log",
    "*.tmp",
    "*.bak",
    "*.swp",
    "*.zip",
    "*.radarota"
)

# Copy only repository/source-support file types that are useful for inspection.
$includeExtensions = @(
    ".cpp", ".cc", ".c", ".h", ".hpp",
    ".py", ".ps1", ".js", ".html", ".css",
    ".md", ".txt", ".json", ".ini", ".toml",
    ".yml", ".yaml", ".csv"
)

$includeExactNames = @(
    "platformio.ini",
    ".gitignore",
    "LICENSE",
    "LICENSE.txt"
)

Get-ChildItem -Path $project -Recurse -File -Force | ForEach-Object {
    $file = $_
    $relative = $file.FullName.Substring($project.Length).TrimStart("\","/")

    # Exclude any path containing a forbidden directory component.
    $parts = $relative -split '[\\/]'
    foreach ($dir in $excludeDirNames) {
        if ($parts -contains $dir) {
            return
        }
    }

    # Never include private config.
    if ($excludeExactRelative -contains $relative) {
        return
    }

    # Skip excluded file patterns.
    foreach ($pattern in $excludePatterns) {
        if ($file.Name -like $pattern) {
            return
        }
    }

    $include = $false
    if ($includeExactNames -contains $file.Name) {
        $include = $true
    } elseif ($includeExtensions -contains $file.Extension.ToLowerInvariant()) {
        $include = $true
    }

    if (-not $include) {
        return
    }

    $destination = Join-Path $tempRoot $relative
    $destinationDir = Split-Path $destination -Parent
    if (-not (Test-Path $destinationDir)) {
        New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null
    }
    Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
}

# Belt-and-suspenders credential check before packaging.
$forbidden = Get-ChildItem -Path $tempRoot -Recurse -File -Force | Where-Object {
    $_.FullName -match '[\\/]include[\\/]config\.h$'
}
if ($forbidden) {
    throw "Safety check failed: include\config.h was staged."
}

$manifest = @"
ESP32 AIRCRAFT RADAR SUPPORT BUNDLE

Source project:
$project

Created:
$(Get-Date -Format "yyyy-MM-dd HH:mm:ss K")

Purpose:
Complete source/support files for ChatGPT inspection and focused fixes.

Explicitly excluded:
- include\config.h
- .git
- .pio
- release/build output
- firmware binaries / .radarota
- logs and temporary files
- common IDE/build/cache directories

This ZIP is not a firmware release package.
"@

$manifestPath = Join-Path $tempRoot "SUPPORT_BUNDLE_README.txt"
Set-Content -LiteralPath $manifestPath -Value $manifest -Encoding UTF8

Compress-Archive -Path (Join-Path $tempRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal -Force

Remove-Item -Recurse -Force $tempRoot

Write-Host ""
Write-Host "Created support ZIP:" -ForegroundColor Green
Write-Host $zipPath -ForegroundColor Cyan
Write-Host ""
Write-Host "Private include\config.h was excluded." -ForegroundColor Green
