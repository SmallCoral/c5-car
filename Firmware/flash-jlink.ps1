param([string]$CommandFile = "$PSScriptRoot/tools/update-app.jlink")

$ErrorActionPreference = 'Stop'
$commands = (Resolve-Path -LiteralPath $CommandFile).Path
$appCommands = (Resolve-Path -LiteralPath "$PSScriptRoot/tools/update-app.jlink").Path
$commander = Join-Path $env:ProgramFiles 'SEGGER/JLink/JLink.exe'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    if ($commands -eq $appCommands) {
        & cmake --preset debug -S $PSScriptRoot
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & cmake --build "$PSScriptRoot/build/debug" --target competition_image
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    & $commander -NoGui 1 -ExitOnError 1 -USB 69730331 -Device STM32C5A3VG -If SWD -Speed 4000 -AutoConnect 1 -CommandFile $commands
    $result = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $result
