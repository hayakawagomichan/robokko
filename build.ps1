param(
    [switch]$Upload,
    [string]$Port
)
$ErrorActionPreference = 'Stop'
if ($Upload -and -not $Port) { throw 'Specify the device port, for example: ./build.ps1 -Upload -Port COM4' }
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
if (Test-Path -LiteralPath $python) {
    $runner = $python
    $runnerArgs = @('-m', 'platformio')
} elseif (Get-Command pio -ErrorAction SilentlyContinue) {
    $runner = (Get-Command pio).Source
    $runnerArgs = @()
} else { throw 'Install PlatformIO Core or the VS Code PlatformIO extension first.' }
# The ESP32 Windows toolchain cannot compile a project under a Japanese path.
$stage = Join-Path $env:TEMP 'robotko-atoms3r-20260926'
if ($stage -match '[^\x00-\x7F]') { throw 'Set TEMP to an ASCII-only path before running this script.' }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($name in @('src', 'include')) {
    $target = Join-Path $stage $name
    if (Test-Path -LiteralPath $target) {
        $resolved = (Resolve-Path -LiteralPath $target).Path
        $expected = [IO.Path]::GetFullPath($target)
        if ($resolved -ne $expected -or -not $resolved.StartsWith([IO.Path]::GetFullPath($stage) + [IO.Path]::DirectorySeparatorChar)) {
            throw 'Unexpected staging path.'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stage -Recurse
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'platformio.ini') -Destination $stage -Force
$pioArgs = $runnerArgs + @('run', '-d', $stage)
if ($Upload) { $pioArgs += @('-t', 'upload', '--upload-port', $Port) }
& $runner @pioArgs
if ($LASTEXITCODE -ne 0) { throw "PlatformIO failed: $LASTEXITCODE" }
$out = Join-Path $PSScriptRoot 'output/firmware'
New-Item -ItemType Directory -Path $out -Force | Out-Null
foreach ($name in @('firmware.bin','bootloader.bin','partitions.bin')) {
    Copy-Item -LiteralPath (Join-Path $stage ".pio/build/m5stack-atoms3r/$name") -Destination $out -Force
}
Write-Output "Firmware artifacts: $out"
