param([switch]$StageOnly)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$source = Join-Path $root 'indra/third_party/reshade'
$revision = '7bf9de8b33bcc76c3177007e65d73c72dd0f34c0'
function Check { if ($LASTEXITCODE -ne 0) { throw "Native command failed: $LASTEXITCODE" } }
if (!$StageOnly) {
    if (!(Test-Path "$source/.git")) { git clone --no-checkout https://github.com/crosire/reshade.git $source; Check }
    git -C $source checkout --detach $revision; Check
    git -C $source submodule update --init --recursive; Check
    if ((git -C $source rev-parse HEAD) -ne $revision) { throw 'Wrong ReShade revision' }
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
    if (!$msbuild) { throw 'Visual Studio MSBuild required' }
    & $msbuild "$source/ReShade.sln" /t:ReShade /m:2 /p:Configuration=Release '/p:Platform=64-bit' /verbosity:minimal; Check
} else {
    $output = Join-Path $root 'build-vc170-64/newview/Release'
    if (!(Test-Path $output)) { throw 'Viewer output missing' }
    Copy-Item "$source/bin/x64/Release/ReShade64.dll" "$output/KAMIMXAO64.dll" -Force
    Copy-Item "$source/LICENSE.md" "$output/KAMI-MXAO-ReShade-LICENSE.md" -Force
    $notices = Join-Path $output 'KAMI-MXAO-ReShade-licenses'
    Get-ChildItem "$source/deps" -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE)' } | ForEach-Object {
        $relative = $_.FullName.Substring($source.Length + 1)
        $destination = Join-Path $notices $relative
        New-Item -ItemType Directory -Force (Split-Path $destination -Parent) | Out-Null
        Copy-Item $_.FullName $destination -Force
    }
    Copy-Item "$PSScriptRoot/Install-Private-MXAO.ps1" $output -Force
    Copy-Item "$PSScriptRoot/README.md" "$output/MXAO-PRIVATE-SETUP.md" -Force
    # Ship our original KAMI effect source and presets. The private author's
    # MXAO shader is intentionally NOT redistributed, but its setup button is.
    $fxsource = Join-Path $root 'kami/gshade/effects'
    $fxout = Join-Path $output 'KAMI-FX'
    New-Item -ItemType Directory -Path (Join-Path $fxout 'Shaders') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $fxout 'Textures') -Force | Out-Null
    Copy-Item (Join-Path $fxsource 'KAMI-FX.fx') (Join-Path $fxout 'Shaders/KAMI-FX.fx') -Force
    foreach ($preset in @('FireGarden.ini', 'Cinematic.ini', 'Clean4K.ini', 'FX-LICENSE.txt')) {
        Copy-Item (Join-Path $fxsource $preset) (Join-Path $fxout $preset) -Force
    }
    Copy-Item (Join-Path $root 'kami/gshade/Enable-KAMI-FX.ps1') $output -Force
    Copy-Item (Join-Path $root 'kami/gshade/Import-GShade-Preset.ps1') $output -Force
    Copy-Item (Join-Path $root 'kami/gshade/README.md') (Join-Path $output 'KAMI-GSHADE-FX-README.md') -Force
    # Deliberately do not fetch/stage the proprietary MXAO or enable any
    # personal runtime marker in CI/public artifacts.
}
