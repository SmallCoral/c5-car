param(
    [ValidateSet('debug', 'release')]
    [string]$Preset = 'debug'
)

$ErrorActionPreference = 'Stop'
& cmake --preset $Preset -S $PSScriptRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cmake --build (Join-Path $PSScriptRoot "build/$Preset") --target wireless_flash
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
