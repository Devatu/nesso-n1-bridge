$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bridge = Join-Path $root "nesso_n1_bridge"
$outDir = Join-Path $PSScriptRoot "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "test_host.exe"

$sources = @(
    (Join-Path $PSScriptRoot "test_host.cpp"),
    (Join-Path $bridge "log.cpp"),
    (Join-Path $bridge "log_ring.cpp"),
    (Join-Path $bridge "shelly_decoder.cpp"),
    (Join-Path $bridge "device_registry.cpp"),
    (Join-Path $bridge "authorization.cpp"),
    (Join-Path $bridge "event_dedup.cpp"),
    (Join-Path $bridge "action_table.cpp"),
    (Join-Path $bridge "action_dispatcher.cpp"),
    (Join-Path $bridge "heatpump_config.cpp"),
    (Join-Path $bridge "portal_api.cpp"),
    (Join-Path $bridge "ota_manifest.cpp"),
    (Join-Path $bridge "ota_manager.cpp")
)

Write-Host "g++ -std=c++17 -DHOST_TEST -> $exe"
& g++ -std=c++17 -Wall -Wextra -DHOST_TEST -DNESSO_ENABLE_HW=0 -I $bridge -o $exe @sources
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
& $exe
exit $LASTEXITCODE
