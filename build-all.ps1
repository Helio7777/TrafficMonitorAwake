$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $Root 'build.ps1') -Arch x64 -Config Release
& (Join-Path $Root 'build.ps1') -Arch Win32 -Config Release
