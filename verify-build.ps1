param(
    [ValidateSet('x64','Win32')]
    [string]$Arch = 'x64'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Dll = Join-Path $Root ("dist\\" + $Arch + "\\TrafficMonitorAwake.dll")
if (-not (Test-Path $Dll)) {
    throw "DLL not found: $Dll. Run .\\build.ps1 -Arch $Arch -Config Release first."
}

$Dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not $Dumpbin) {
    throw 'dumpbin.exe not found. Run this from a Visual Studio Developer PowerShell.'
}

$Exports = & $Dumpbin.Source /nologo /exports $Dll
if ($LASTEXITCODE -ne 0) { throw "dumpbin failed: $LASTEXITCODE" }
if (-not ($Exports | Select-String -SimpleMatch 'TMPluginGetInstance')) {
    throw 'Required export TMPluginGetInstance was not found.'
}

Write-Host "OK: $Dll"
Write-Host 'OK: TMPluginGetInstance export exists.'
Write-Host 'Runtime check after installing/enabling: powercfg /requests'
