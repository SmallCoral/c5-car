param([string]$CommandFile = "$PSScriptRoot/tools/update-app.jlink")

$ErrorActionPreference = 'Stop'
$commands = (Resolve-Path -LiteralPath $CommandFile).Path
$commander = Join-Path $env:ProgramFiles 'SEGGER/JLink/JLink.exe'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    & $commander -NoGui 1 -ExitOnError 1 -USB 69730331 -Device STM32C5A3VG -If SWD -Speed 4000 -AutoConnect 1 -CommandFile $commands
    $result = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $result
