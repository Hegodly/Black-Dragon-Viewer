# OPTIONAL: Copies KAMI-authored effects into an already installed external
# GShade/ReShade shader directory. Does not install or inject a GShade runtime.
param([string]$ViewerDirectory = $PSScriptRoot, [string]$ShaderDirectory = '')
$ErrorActionPreference='Stop'
$base=(Resolve-Path -LiteralPath $ViewerDirectory).Path
$source=Join-Path $base 'KAMI-FX'
if (!(Test-Path -LiteralPath (Join-Path $source 'Shaders/KAMI-FX.fx') -PathType Leaf)) {
    throw 'Missing KAMI-FX assets. Extract the entire KAMI portable package.'
}
if (!(Test-Path -LiteralPath (Join-Path $base 'opengl32.dll') -PathType Leaf)) {
    throw 'Install your own trusted GShade/ReShade OpenGL runtime for KAMIViewer.exe first. No OpenGL runtime is bundled.'
}
if (!$ShaderDirectory) {
    $candidates=@((Join-Path $base 'gshade-shaders/Shaders'),(Join-Path $base 'reshade-shaders/Shaders'))
    $ShaderDirectory=$candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -First 1
}
if (!$ShaderDirectory) {
    throw 'Choose your installed GShade/ReShade shader folder with -ShaderDirectory. Your effects search path must include that folder.'
}
$target=(Resolve-Path -LiteralPath $ShaderDirectory).Path
$destination=Join-Path $target 'KAMI-FX.fx'
if (Test-Path -LiteralPath $destination) {
    throw "Refusing to overwrite an existing user shader: $destination"
}
Copy-Item -LiteralPath (Join-Path $source 'Shaders/KAMI-FX.fx') -Destination $destination -ErrorAction Stop
$presets=Join-Path $base 'KAMI-FX-Presets'
New-Item -ItemType Directory -Path $presets -Force | Out-Null
foreach ($preset in @('FireGarden.ini','Cinematic.ini','Clean4K.ini')) {
    $dest=Join-Path $presets $preset
    if (Test-Path -LiteralPath $dest) { throw "Refusing to overwrite an existing user preset: $dest" }
    Copy-Item -LiteralPath (Join-Path $source $preset) -Destination $dest
}
Write-Host 'KAMI-FX assets copied. Select the KAMI-FX-Presets folder in your external GShade/ReShade overlay.' -ForegroundColor Green
Write-Host 'No runtime or DLLs were installed; no existing preset, shader or settings file was overwritten.'
