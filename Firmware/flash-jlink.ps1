# 通过 J-Link 更新实车主控。命令文件决定写入范围，退出码原样交给调用者。

param([string]$CommandFile = "$PSScriptRoot/tools/update-app.jlink")

$ErrorActionPreference = 'Stop'
$commands = (Resolve-Path -LiteralPath $CommandFile).Path
$appCommands = (Resolve-Path -LiteralPath "$PSScriptRoot/tools/update-app.jlink").Path
$commander = Join-Path $env:ProgramFiles 'SEGGER/JLink/JLink.exe'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    # 使用默认更新脚本时先重建合并应用并回归，避免将上次残留的 HEX 写到板上。
    if ($commands -eq $appCommands) {
        & cmake --preset debug -S $PSScriptRoot
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & cmake --build "$PSScriptRoot/build/debug" --target competition_image
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    # 默认命令文件会回读校验应用与元数据，再通过 RAM 标记让新应用接手步进抱持。
    & $commander -NoGui 1 -ExitOnError 1 -USB 69730331 -Device STM32C5A3VG -If SWD -Speed 4000 -AutoConnect 1 -CommandFile $commands
    $result = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $result
