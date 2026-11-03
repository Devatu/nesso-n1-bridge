# Build Nesso N1 bridge with Zigbee ZCZR + OTA A/B (8MB table via sketch partitions.csv).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$sketch = Join-Path $root "firmware\nesso_n1_bridge"
$out = Join-Path $root "build\firmware"
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Sketch partitions.csv = official zigbee_zczr_8MB (A/B ~3.3MB); WiFiProv+Zigbee past 1.25MB.
$fqbn = "esp32:esp32:arduino_nesso_n1:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr"
$appMax = 3407872  # 0x340000 from zigbee_zczr_8MB.csv

$localCli = Join-Path $root "tools\arduino-cli"
if (Test-Path (Join-Path $localCli "arduino-cli.exe")) {
    $env:Path = "$localCli;$env:Path"
}
if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw "arduino-cli not in PATH. Run scripts\setup_arduino_cli.ps1 first."
}

Write-Host "FQBN: $fqbn (custom partitions.csv = zigbee_zczr_8MB)"
Write-Host "Sketch: $sketch"
arduino-cli compile --fqbn $fqbn --output-dir $out --build-property "upload.maximum_size=$appMax" $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Artifacts in $out"
Get-ChildItem $out | Format-Table Name, Length
