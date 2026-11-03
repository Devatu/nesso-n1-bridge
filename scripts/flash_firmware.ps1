param(
    [Parameter(Mandatory = $true)]
    [string]$Port
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$sketch = Join-Path $root "firmware\nesso_n1_bridge"
$fqbn = "esp32:esp32:arduino_nesso_n1:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr"

$localCli = Join-Path $root "tools\arduino-cli"
if (Test-Path (Join-Path $localCli "arduino-cli.exe")) {
    $env:Path = "$localCli;$env:Path"
}
if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw "arduino-cli not in PATH. Run scripts\setup_arduino_cli.ps1 first."
}

Write-Host "Upload to $Port with $fqbn"
arduino-cli upload -p $Port --fqbn $fqbn $sketch
exit $LASTEXITCODE
