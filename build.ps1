param(
    [ValidateSet('x64','Win32')]
    [string]$Arch = 'x64',
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root ("build-" + $Arch)

cmake -S $Root -B $BuildDir -A $Arch
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }

$Dll = Join-Path $BuildDir "$Config/TrafficMonitorAwake.dll"

if (-not (Test-Path -LiteralPath $Dll -PathType Leaf)) {
    throw 'Build succeeded but TrafficMonitorAwake.dll was not found.'
}

$Dist = Join-Path $Root ("dist\\" + $Arch)
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Copy-Item -LiteralPath $Dll -Destination (Join-Path $Dist 'TrafficMonitorAwake.dll') -Force

Write-Host "Built: $(Join-Path $Dist 'TrafficMonitorAwake.dll')"
