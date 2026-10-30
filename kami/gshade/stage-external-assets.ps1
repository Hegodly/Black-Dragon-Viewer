# Stage optional shader assets for a separate user-installed GShade/ReShade.
# Do not build, load, bundle, or activate any graphics runtime in KAMI itself.
param([string]$Output = 'build-vc170-64/newview/Release')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$out = Join-Path $root $Output
if (!(Test-Path -LiteralPath $out -PathType Container)) { throw "Compiled viewer output does not exist: $out" }
$fx = Join-Path $root 'kami/gshade/effects'
$dest = Join-Path $out 'KAMI-FX'
New-Item -ItemType Directory -Path (Join-Path $dest 'Shaders') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $dest 'Textures') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $fx 'KAMI-FX.fx') -Destination (Join-Path $dest 'Shaders/KAMI-FX.fx') -Force
foreach ($preset in @('FireGarden.ini','Cinematic.ini','Clean4K.ini','FX-LICENSE.txt')) {
    Copy-Item -LiteralPath (Join-Path $fx $preset) -Destination (Join-Path $dest $preset) -Force
}
Copy-Item -LiteralPath (Join-Path $root 'kami/gshade/README.md') -Destination (Join-Path $out 'KAMI-GSHADE-EXTERNAL-SETUP.md') -Force
Copy-Item -LiteralPath (Join-Path $root 'kami/gshade/Install-GShade-Assets.ps1') -Destination (Join-Path $out 'Install-GShade-Assets.ps1') -Force
if (Test-Path -LiteralPath (Join-Path $out 'KAMIMXAO64.dll')) { throw 'Embedded MXAO/ReShade runtime must not be shipped' }
Write-Host 'External GShade-compatible shader assets staged. No GShade or ReShade runtime installed.' -ForegroundColor Cyan
