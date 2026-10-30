# User-controlled compatibility import. No GShade binary, paywalled assets,
# remote downloads, shader bypasses or auto-run third-party code are bundled.
# Compatible .ini and ReShade FX (.fx/.fxh) files are copied for personal use.
param(
    [Parameter(Mandatory=$true)][string]$ViewerDirectory,
    [string]$PresetPath = '',
    [string]$ShaderDirectory = '',
    [string]$TextureDirectory = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$base = (Resolve-Path -LiteralPath $ViewerDirectory).Path
$runtime = Join-Path $base 'KAMIMXAO64.dll'
if (!(Test-Path -LiteralPath $runtime -PathType Leaf)) {
    throw 'KAMI FX runtime is missing. Extract the complete Windows portable build first.'
}
if (Test-Path -LiteralPath (Join-Path $base 'opengl32.dll')) {
    throw 'An external GShade/ReShade opengl32.dll is already installed. Do not stack graphics hooks. Configure that external installation through its own overlay, or use a clean KAMI portable folder.'
}
$running=Get-Process -Name KAMIViewer,BlackDragonViewer,BlackDragon -ErrorAction SilentlyContinue
if ($running) {
    Write-Host 'Close KAMI Viewer to continue importing your GShade preset.' -ForegroundColor Yellow
    while (Get-Process -Name KAMIViewer,BlackDragonViewer,BlackDragon -ErrorAction SilentlyContinue) {
        Start-Sleep -Milliseconds 500
    }
}

function ChoosePreset {
    $dlg=New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter='GShade / ReShade preset (*.ini)|*.ini'
    $dlg.Title='Select your existing GShade preset (.ini)'
    $dlg.CheckFileExists=$true
    if ($dlg.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'Preset import canceled.' }
    return $dlg.FileName
}
function ChooseFolder([string]$prompt,[string]$initial='') {
    $dlg=New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description=$prompt
    if ($initial -and (Test-Path $initial -PathType Container)) {
        $dlg.SelectedPath=$initial
    }
    if ($dlg.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'Folder selection canceled.' }
    return $dlg.SelectedPath
}
if (!$PresetPath) { $PresetPath=ChoosePreset }
$preset=(Resolve-Path -LiteralPath $PresetPath).Path
if ([IO.Path]::GetExtension($preset).ToLowerInvariant() -ne '.ini') { throw 'The selected preset must be a .ini file.' }
$presetBody=[IO.File]::ReadAllText($preset)
if (($presetBody -notmatch '(?im)^\s*Techniques\s*=') -and ($presetBody -notmatch '(?im)^\s*TechniqueSorting\s*=')) {
    throw 'Not a supported GShade / ReShade FX preset: Techniques/TechniqueSorting not found.'
}

# Derive the likely GShade 5.x default folder from this .ini location.
$roots=@((Split-Path -Parent $preset), (Split-Path -Parent (Split-Path -Parent $preset)))
$candidates=@()
foreach($p in $roots) {
    $candidates+= (Join-Path $p 'gshade-shaders/Shaders')
    $candidates+= (Join-Path $p 'reshade-shaders/Shaders')
    $candidates+= (Join-Path $p 'Shaders')
}
if (!$ShaderDirectory) {
    $found=$candidates|Where-Object { Test-Path -LiteralPath $_ -PathType Container }|Select-Object -First 1
    if($found) { $ShaderDirectory=$found }
    else { $ShaderDirectory=ChooseFolder 'Select the Shaders directory of YOUR GShade installation (contains .fx files).' }
}
$shaders=(Resolve-Path -LiteralPath $ShaderDirectory).Path
if (!(Get-ChildItem -LiteralPath $shaders -File -Recurse -Filter '*.fx' | Select-Object -First 1)) {
    throw "There are no .fx files in $shaders. Please choose the actual GShade Shaders directory."
}
if (!$TextureDirectory) {
    $sparent=Split-Path -Parent $shaders
    $proposed=Join-Path $sparent 'Textures'
    if (Test-Path -LiteralPath $proposed -PathType Container) { $TextureDirectory=$proposed }
    else {
        $answer=[System.Windows.Forms.MessageBox]::Show(
            'Does this preset use custom GShade textures? Click Yes to choose your Textures directory, No to import shaders only.',
            'KAMI GShade compatibility',
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Question)
        if ($answer -eq [System.Windows.Forms.DialogResult]::Yes) {
            $TextureDirectory=ChooseFolder 'Select your GShade Textures folder'
        }
    }
}
$textures=''
if($TextureDirectory) { $textures=(Resolve-Path -LiteralPath $TextureDirectory).Path }

# A new import uses its own directory, so a failed attempt cannot erase a
# previous working preset. The user can keep existing files outside KAMI.
$import=Join-Path $base 'KAMI-GShade'
if (Test-Path -LiteralPath $import) {
    throw 'A previous KAMI-GShade import exists. Back it up and remove that folder before replacing it. The installer will not overwrite your previous imported preset.'
}
# A private MXAO user install manages a distinct ReShade.ini and shader set.
# Refuse to override it, rather than clobbering MXAO or old-user settings.
if (Test-Path -LiteralPath (Join-Path $base 'KAMI_MXAO_PRIVATE.enabled')) {
    throw 'The optional MXAO runtime is configured in this folder. Use a separate clean portable KAMI folder for GShade preset import; their single-runtime preset configs must not conflict.'
}
if (Test-Path -LiteralPath (Join-Path $base 'ReShade.ini')) {
    throw 'A ReShade.ini already exists. Back it up manually and use a clean portable KAMI folder. This importer never overwrites existing runtime configuration.'
}
New-Item -ItemType Directory -Force -Path (Join-Path $import 'Shaders') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $import 'Textures') | Out-Null
try {
    Copy-Item -LiteralPath $preset -Destination (Join-Path $import 'UserPreset.ini') -ErrorAction Stop
    # Copy the source contents instead of embedding absolute paths. The result
    # is relocatable with the user's own KAMI portable folder.
    Get-ChildItem -LiteralPath $shaders -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $import 'Shaders') -Recurse -Force
    }
    if ($textures) {
        Get-ChildItem -LiteralPath $textures -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $import 'Textures') -Recurse -Force
        }
    }
    $config=@'
[GENERAL]
EffectSearchPaths=.\KAMI-GShade\Shaders\**
TextureSearchPaths=.\KAMI-GShade\Textures\**
PresetPath=.\KAMI-GShade\UserPreset.ini
SkipLoadingDisabledEffects=0
PerformanceMode=0
PreprocessorDefinitions=RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0,RESHADE_DEPTH_INPUT_IS_REVERSED=0,RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0
[INPUT]
KeyOverlay=36,0,0,0
'@
    $config | Set-Content -LiteralPath (Join-Path $base 'ReShade.ini') -Encoding UTF8 -ErrorAction Stop
    'Imported for this viewer only; restart KAMI after import. User-provided shader assets are not shipped by KAMI.' |
        Set-Content -LiteralPath (Join-Path $base 'KAMI_GSHADE_USER.enabled') -Encoding UTF8 -ErrorAction Stop
    Write-Host ''
    Write-Host 'GShade-compatible preset imported to KAMI.' -ForegroundColor Cyan
    Write-Host 'Restart KAMI, open Preferences > Display > GShade Compatibility, enable it, then press Home for diagnostics.'
    Write-Host 'Some GShade-exclusive/paid shaders or shader extensions will not compile under ReShade FX.'
    Write-Host 'Verify depth, technique compilation and frame rate in the live scene.'
    [System.Windows.Forms.MessageBox]::Show(
        'Preset imported. Restart KAMI, enable GShade Compatibility in Display, then press Home to check compilation and depth.',
        'KAMI Fire Garden - GShade compatibility',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null
}
catch {
    # ReShade.ini is deliberately never overwritten. Clean only our brand-new
    # assets; leave original GShade files completely untouched.
    if (Test-Path -LiteralPath (Join-Path $base 'KAMI_GSHADE_USER.enabled')) {
        Remove-Item -LiteralPath (Join-Path $base 'KAMI_GSHADE_USER.enabled') -Force
    }
    if (Test-Path -LiteralPath (Join-Path $base 'ReShade.ini')) {
        Remove-Item -LiteralPath (Join-Path $base 'ReShade.ini') -Force
    }
    if (Test-Path -LiteralPath $import) { Remove-Item -LiteralPath $import -Force -Recurse }
    throw
}
