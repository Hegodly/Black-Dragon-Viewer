param([Parameter(Mandatory=$true)][string]$ViewerDirectory)
$ErrorActionPreference = 'Stop'
$base = (Resolve-Path $ViewerDirectory).Path
if (!(Test-Path "$base/KAMIMXAO64.dll")) { throw 'This build lacks KAMIMXAO64.dll. Extract the complete KAMI portable ZIP first.' }
if (Test-Path "$base/opengl32.dll") { throw 'An external ReShade/GShade installation exists. Use that installation instead.' }
if (Test-Path "$base/ReShade.ini") { throw 'Existing ReShade.ini found. Back it up and review your installation; this installer will not overwrite it.' }
if (Get-Process -Name KAMIViewer,BlackDragonViewer,BlackDragon -ErrorAction SilentlyContinue) {
    Write-Host 'Close KAMI Viewer to continue installing MXAO.' -ForegroundColor Yellow
    while (Get-Process -Name KAMIViewer,BlackDragonViewer,BlackDragon -ErrorAction SilentlyContinue) {
        Start-Sleep -Milliseconds 500
    }
}
$revision = 'f57d3afa1ebe5d1fd6152d4f6fb9a2e75bd1d1cb'
$cache = Join-Path ([IO.Path]::GetTempPath()) ('KAMI-MXAO-' + [guid]::NewGuid().ToString())
function Check { if ($LASTEXITCODE -ne 0) { throw "Download command failed: $LASTEXITCODE" } }
try {
    # An optional personal install must work for players without Git installed.
    # Fetch the author's official repository at an immutable commit, on demand.
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $archive = Join-Path $cache 'iMMERSE.zip'
    Invoke-WebRequest -UseBasicParsing -Uri ("https://codeload.github.com/martymcmodding/iMMERSE/zip/" + $revision) -OutFile $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $cache -Force
    $source = Join-Path $cache ("iMMERSE-" + $revision)
    if (!(Test-Path "$source/Shaders/MartysMods_MXAO.fx")) { throw 'Official MXAO shader not present in downloaded source.' }
    $target = Join-Path $base 'KAMI-MXAO'
    if (Test-Path $target) { throw 'KAMI-MXAO directory exists; refusing to overwrite it.' }
    New-Item -ItemType Directory "$target/Shaders" -Force | Out-Null
    Copy-Item "$source/Shaders/MartysMods_MXAO.fx" "$target/Shaders/"
    Copy-Item "$source/Shaders/MartysMods" "$target/Shaders/" -Recurse
    Copy-Item "$source/Textures" "$target/" -Recurse
    Copy-Item "$source/LICENSE" "$target/AUTHOR-LICENSE.txt"
    # Select the author's existing pixel-shader fallback; runtime GL support still applies.
    $fx = "$target/Shaders/MartysMods_MXAO.fx"
    $code = [IO.File]::ReadAllText($fx)
    if (!$code.Contains('//#undef _COMPUTE_SUPPORTED')) { throw 'MXAO source changed; manual review required.' }
    [IO.File]::WriteAllText($fx, $code.Replace('//#undef _COMPUTE_SUPPORTED', "#undef _COMPUTE_SUPPORTED`n#define _COMPUTE_SUPPORTED 0"))
    @'
[GENERAL]
EffectSearchPaths=.\KAMI-MXAO\Shaders\**
TextureSearchPaths=.\KAMI-MXAO\Textures\**
PresetPath=.\KAMI-MXAO\Private.ini
SkipLoadingDisabledEffects=0
PerformanceMode=0
PreprocessorDefinitions=RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0,RESHADE_DEPTH_INPUT_IS_REVERSED=0,RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0
[INPUT]
KeyOverlay=36,0,0,0
'@ | Set-Content "$base/ReShade.ini" -Encoding utf8
    "Techniques=MartysMods_MXAO@MartysMods_MXAO.fx`nTechniqueSorting=MartysMods_MXAO@MartysMods_MXAO.fx" | Set-Content "$target/Private.ini" -Encoding utf8
    "Private direct download from the author's official repository: $revision" | Set-Content "$base/KAMI_MXAO_PRIVATE.enabled" -Encoding utf8
    Write-Host 'Installed for your personal use. Restart KAMI, disable native AO, and enable Private MXAO in Display.'
    Write-Host 'Press Home to check shader compilation and depth selection. First-use shader compilation can pause briefly.'
    Write-Host 'Keep this personal shader installation out of public packages.'
    Write-Host 'Close this installer, then launch KAMI Viewer.'
} finally {
    if (Test-Path $cache) { Remove-Item -LiteralPath $cache -Recurse -Force }
}
