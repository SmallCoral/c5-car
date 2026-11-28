# 按所选配置构建当前比赛固件并通过 HC-08 下载。

param(
    [ValidateSet('debug', 'release')]
    [string]$Preset = 'debug',
    [switch]$PipeSupported
)

$ErrorActionPreference = 'Stop'
& cmake --preset $Preset -S $PSScriptRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
# 托稳开关选择允许解除抱持的升级入口，传输完成后由工具核对运行镜像。
$flashTarget = if ($PipeSupported) { 'wireless_flash_supported' } else { 'wireless_flash' }
& cmake --build (Join-Path $PSScriptRoot "build/$Preset") --target $flashTarget
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
