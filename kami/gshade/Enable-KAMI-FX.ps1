# Enables the *bundled*, original KAMI effects using the single optional
# ReShade FX-compatible runtime already built with KAMI.
param([Parameter(Mandatory=$true)][string]$ViewerDirectory)
$ErrorActionPreference='Stop'
$base=(Resolve-Path -LiteralPath $ViewerDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $base 'KAMIMXAO64.dll') -PathType Leaf)) { throw 'Complete KAMI FX runtime missing. Extract the full portable ZIP.' }
if (!(Test-Path -LiteralPath (Join-Path $base 'KAMI-FX/Shaders/KAMI-FX.fx') -PathType Leaf)) { throw 'Built-in KAMI shader kit missing. Extract the full portable ZIP.' }
if (Get-Process -Name KAMIViewer,BlackDragonViewer,BlackDragon -ErrorAction SilentlyContinue) { throw 'Close KAMI before enabling bundled effects. Restart afterward.' }
if (Test-Path -LiteralPath (Join-Path $base 'opengl32.dll')) { throw 'External GShade/ReShade opengl32.dll detected. Do not stack two graphics hooks.' }
if (Test-Path -LiteralPath (Join-Path $base 'KAMI_MXAO_PRIVATE.enabled')) { throw 'Private MXAO already has a runtime configuration. Choose a clean KAMI portable folder for the bundled effect kit.' }
if (Test-Path -LiteralPath (Join-Path $base 'KAMI_GSHADE_USER.enabled')) { throw 'You already imported a GShade-compatible preset. Use the overlay to select effects from that installation.' }
if (Test-Path -LiteralPath (Join-Path $base 'ReShade.ini')) { throw 'ReShade.ini already exists. Not overwriting your shader paths or presets.' }

$config=@'
[GENERAL]
EffectSearchPaths=.\KAMI-FX\Shaders\**
TextureSearchPaths=.\KAMI-FX\Textures\**
PresetPath=.\KAMI-FX\FireGarden.ini
SkipLoadingDisabledEffects=0
PerformanceMode=0
PreprocessorDefinitions=RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0,RESHADE_DEPTH_INPUT_IS_REVERSED=0,RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0
[INPUT]
KeyOverlay=36,0,0,0
'@
$config | Set-Content -LiteralPath (Join-Path $base 'ReShade.ini') -Encoding UTF8
'User enabled bundled KAMI-FX. Disable/remove this marker while KAMI is closed to avoid loading the FX runtime.' |
    Set-Content -LiteralPath (Join-Path $base 'KAMI_EFFECTS_BUNDLED.enabled') -Encoding UTF8
Write-Host 'KAMI built-in FX is installed. Restart KAMI, turn on GShade-compatible FX in Display and press Home for techniques.' -ForegroundColor Cyan
Write-Host 'The starter preset enables zero effects until the user selects one. FPS remains uncapped.'
