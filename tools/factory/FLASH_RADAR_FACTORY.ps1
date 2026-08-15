param(
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"
$PackageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ManifestPath = Join-Path $PackageRoot "factory-manifest.json"

$AllowedHardware = "waveshare-esp32-s3-touch-lcd-7"
$AllowedChip = "esp32s3"
$AllowedFlashSize = "16MB"
$AllowedFlashMode = "dio"
$AllowedFlashFreq = "80m"
$DistributionMarker = "RADAR-DISTRIBUTION-BUILD"
$ExpectedLayout = [ordered]@{
    "bootloader.bin" = 0x00000000
    "partitions.bin" = 0x00008000
    "boot_app0.bin" = 0x0000E000
    "firmware.bin" = 0x00010000
}

function Fail([string]$Message) {
    throw $Message
}

function Find-Python {
    foreach ($candidate in @(
        "$env:USERPROFILE\.platformio\penv\Scripts\python.exe",
        "python",
        "py"
    )) {
        try {
            & $candidate --version *> $null
            if ($LASTEXITCODE -eq 0) { return $candidate }
        } catch {}
    }
    Fail "Python was not found. Use the browser installer when available, or install Python/esptool for this offline recovery path."
}

function Find-Esptool([string]$Python) {
    $pioTool = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptoolpy\esptool.py"
    if (Test-Path $pioTool) {
        return @($Python, $pioTool)
    }

    & $Python -m esptool version *> $null
    if ($LASTEXITCODE -eq 0) {
        return @($Python, "-m", "esptool")
    }

    Fail "esptool was not found. Use the browser installer when available, or install esptool for this offline recovery path."
}

function Run-Esptool([array]$Prefix, [array]$Arguments) {
    $exe = $Prefix[0]
    $prefixArgs = @()
    if ($Prefix.Count -gt 1) {
        $prefixArgs = $Prefix[1..($Prefix.Count - 1)]
    }

    & $exe @prefixArgs @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "esptool failed with exit code $LASTEXITCODE."
    }
}

function Test-AsciiMarker([string]$Path, [string]$Marker) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $text = [Text.Encoding]::ASCII.GetString($bytes)
    return $text.Contains($Marker)
}

if (-not (Test-Path $ManifestPath)) {
    Fail "Missing factory-manifest.json. Refusing destructive factory install."
}

$manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1) { Fail "Unsupported factory manifest schema." }
if ($manifest.hardware -ne $AllowedHardware) { Fail "Factory manifest hardware mismatch." }
if ($manifest.chip -ne $AllowedChip) { Fail "Factory manifest chip mismatch." }
if ($manifest.flash_size -ne $AllowedFlashSize) { Fail "Factory manifest flash-size mismatch." }
if ($manifest.flash_mode -ne $AllowedFlashMode) { Fail "Factory manifest flash-mode mismatch." }
if ($manifest.flash_freq -ne $AllowedFlashFreq) { Fail "Factory manifest flash-frequency mismatch." }
if (-not $manifest.build_id.StartsWith("7IN-")) { Fail "Factory manifest build ID is invalid." }
if ($manifest.files.Count -ne $ExpectedLayout.Count) { Fail "Factory manifest file count is invalid." }

$seen = @{}
foreach ($entry in $manifest.files) {
    $name = [string]$entry.name
    if (-not $ExpectedLayout.Contains($name)) { Fail "Unexpected factory file in manifest: $name" }
    if ($seen.ContainsKey($name)) { Fail "Duplicate factory file in manifest: $name" }
    $seen[$name] = $true

    $expectedAddress = [uint32]$ExpectedLayout[$name]
    $actualAddress = [Convert]::ToUInt32(([string]$entry.address).Replace("0x", ""), 16)
    if ($actualAddress -ne $expectedAddress) {
        Fail "Unexpected flash address for $name. Refusing destructive install."
    }

    $path = Join-Path $PackageRoot $name
    if (-not (Test-Path $path)) { Fail "Missing required factory file: $name" }

    $size = (Get-Item $path).Length
    if ($size -ne [int64]$entry.size) { Fail "Size mismatch for $name. Refusing to flash." }

    $actualHash = (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne ([string]$entry.sha256).ToLowerInvariant()) {
        Fail "SHA-256 mismatch for $name. Refusing to flash."
    }
}

foreach ($name in $ExpectedLayout.Keys) {
    if (-not $seen.ContainsKey($name)) { Fail "Required factory file is absent from manifest: $name" }
}

$firmwarePath = Join-Path $PackageRoot "firmware.bin"
if (-not (Test-AsciiMarker $firmwarePath $DistributionMarker)) {
    Fail "firmware.bin is not marked as a distribution build. Refusing destructive install."
}

if (-not (Test-AsciiMarker $firmwarePath ([string]$manifest.build_id))) {
    Fail "firmware.bin does not contain the factory manifest build ID. Refusing to flash."
}

Write-Host ""
Write-Host "============================================================"
Write-Host " BILL'S AIRCRAFT RADAR - FACTORY INSTALLER"
Write-Host " Waveshare ESP32-S3-Touch-LCD-7 ONLY"
Write-Host "============================================================"
Write-Host ""
Write-Host "Product: $($manifest.version_label)"
Write-Host "Build:   $($manifest.build_id)"
Write-Host ""
Write-Host "THIS IS A DESTRUCTIVE FACTORY INSTALL."
Write-Host "It erases the ENTIRE 16 MB flash chip before reinstalling firmware."
Write-Host "Wi-Fi, location, MQTT/Home Assistant settings, airport database,"
Write-Host "OTA state, and every other owner-specific flash value will be removed."
Write-Host ""
Write-Host "Factory package verification: PASS"

$python = Find-Python
$esptool = Find-Esptool $python

if (-not $Port) {
    $ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty DeviceID)

    if ($ports.Count -eq 1) {
        $Port = $ports[0]
        Write-Host "Detected serial port: $Port"
    } else {
        Write-Host ""
        Write-Host "Available COM ports:"
        if ($ports.Count -eq 0) {
            Write-Host "  No COM ports were automatically identified."
        } else {
            $ports | ForEach-Object { Write-Host "  $_" }
        }
        $Port = Read-Host "Enter the radar COM port (example COM10)"
    }
}

if ($Port -notmatch '^COM\d+$') { Fail "Invalid COM port: $Port" }

Write-Host ""
Write-Host "Probing $Port..."

$probeFile = Join-Path $env:TEMP ("radar-factory-probe-" + [guid]::NewGuid() + ".txt")
try {
    $exe = $esptool[0]
    $prefixArgs = @()
    if ($esptool.Count -gt 1) {
        $prefixArgs = $esptool[1..($esptool.Count - 1)]
    }

    & $exe @prefixArgs --chip esp32s3 --port $Port flash_id 2>&1 |
        Tee-Object -FilePath $probeFile

    if ($LASTEXITCODE -ne 0) { Fail "Could not communicate with an ESP32-S3 on $Port." }

    $probe = Get-Content $probeFile -Raw
    if ($probe -notmatch '(?im)^Chip is ESP32-S3\b') {
        Fail "Connected chip was not positively identified as ESP32-S3."
    }
    if ($probe -notmatch '(?im)^(?:Auto-detected |Detected )?Flash size:\s*16MB\s*$') {
        Fail "Detected flash-size line did not positively report 16MB. Refusing destructive erase."
    }
}
finally {
    Remove-Item $probeFile -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Hardware guard: ESP32-S3 with detected 16 MB flash confirmed."
Write-Host ""
$confirmation = Read-Host "Type ERASE RADAR to permanently erase $Port and install $($manifest.version_label)"
if ($confirmation -cne "ERASE RADAR") { Fail "Confirmation did not match. Nothing was erased." }

Write-Host ""
Write-Host "Erasing ENTIRE flash chip..."
Run-Esptool $esptool @(
    "--chip","esp32s3","--port",$Port,
    "--before","default_reset","--after","no_reset",
    "erase_flash"
)

Write-Host ""
Write-Host "Writing verified distribution factory image..."
Run-Esptool $esptool @(
    "--chip","esp32s3","--port",$Port,
    "--baud","921600",
    "--before","no_reset","--after","hard_reset",
    "write_flash",
    "-z",
    "--flash_mode","dio",
    "--flash_freq","80m",
    "--flash_size","16MB",
    "0x00000000",(Join-Path $PackageRoot "bootloader.bin"),
    "0x00008000",(Join-Path $PackageRoot "partitions.bin"),
    "0x0000E000",(Join-Path $PackageRoot "boot_app0.bin"),
    "0x00010000",(Join-Path $PackageRoot "firmware.bin")
)

Write-Host ""
Write-Host "============================================================"
Write-Host " FACTORY INSTALL COMPLETE"
Write-Host "============================================================"
Write-Host ""
Write-Host "Expected first-boot state:"
Write-Host "  - no saved Wi-Fi"
Write-Host "  - no saved owner location"
Write-Host "  - MQTT/Home Assistant unconfigured"
Write-Host "  - no installed regional airport database"
Write-Host "  - clean OTA state"
Write-Host ""
