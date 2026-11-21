param(
    [ValidateSet('debug', 'release')]
    [string]$Preset = 'debug',
    [switch]$PipeSupported
)

$ErrorActionPreference = 'Stop'
& cmake --preset $Preset -S $PSScriptRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$flashTarget = if ($PipeSupported) { 'wireless_flash_supported' } else { 'wireless_flash' }
& cmake --build (Join-Path $PSScriptRoot "build/$Preset") --target $flashTarget
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
