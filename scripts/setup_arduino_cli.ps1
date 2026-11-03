# Install/configure Arduino CLI for Nesso N1 builds.
$ErrorActionPreference = "Stop"

function Ensure-ArduinoCli {
    if (Get-Command arduino-cli -ErrorAction SilentlyContinue) {
        Write-Host "arduino-cli: $(arduino-cli version)"
        return
    }
    $tools = Join-Path (Split-Path $PSScriptRoot -Parent) "tools\arduino-cli"
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $env:TEMP "arduino-cli.zip"
    Write-Host "Downloading arduino-cli..."
    Invoke-WebRequest -Uri "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $tools -Force
    $env:Path = "$tools;$env:Path"
    if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
        throw "arduino-cli not found after download. Add $tools to PATH."
    }
    Write-Host "arduino-cli: $(arduino-cli version)"
}

Ensure-ArduinoCli

Write-Host "Updating core index..."
arduino-cli config init --overwrite 2>$null | Out-Null
arduino-cli core update-index

Write-Host "Installing esp32 core (>= 3.3.5 required for Nesso)..."
arduino-cli core install esp32:esp32@3.3.12
if ($LASTEXITCODE -ne 0) {
    Write-Host "3.3.12 failed; trying latest esp32:esp32"
    arduino-cli core install esp32:esp32
}

Write-Host "Installing libraries..."
arduino-cli lib install "Arduino_Nesso_N1@1.0.0"
arduino-cli lib install "M5GFX"

Write-Host "Done. Example compile:"
Write-Host '  powershell -File scripts\build_firmware.ps1'
