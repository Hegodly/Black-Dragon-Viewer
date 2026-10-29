# Copyright (C) 2026 Kombatant Viewer contributors
# SPDX-License-Identifier: MIT
# Fetch pinned public FSR 3.1 and Intel XeSS SDKs; retain vendor licensing.
param([switch]$StageOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$amd = Join-Path $root '.fsr-sdk'
$intel = Join-Path $root '.xess-sdk'
function CheckExit([string]$message) { if ($LASTEXITCODE -ne 0) { throw $message } }
if (!$StageOnly) {
    if (!(Test-Path "$amd/.git")) {
        git clone --filter=blob:none --no-checkout https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git $amd
        CheckExit 'AMD FidelityFX SDK clone failed'
        git -C $amd sparse-checkout set --no-cone '/Kits/FidelityFX/api/include/' '/Kits/FidelityFX/upscalers/include/' '/Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.dll' '/Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll' '/3rdpartynotice.md' '/Kits/FidelityFX/readme.md'
        CheckExit 'AMD SDK sparse checkout failed'
    }
    git -C $amd checkout --detach 7539ae8b6568ad29ed78a753404351daf3b5cca7
    CheckExit 'AMD SDK pinned checkout failed'
    if ((git -C $amd rev-parse HEAD).Trim() -ne '7539ae8b6568ad29ed78a753404351daf3b5cca7') { throw 'AMD SDK revision mismatch' }
    if (!(Test-Path "$intel/.git")) {
        git clone --filter=blob:none --no-checkout https://github.com/intel/xess.git $intel
        CheckExit 'Intel XeSS SDK clone failed'
        git -C $intel sparse-checkout set --no-cone '/inc/xess/' '/bin/libxess.dll' '/LICENSE.txt' '/third-party-programs.txt'
        CheckExit 'Intel XeSS sparse checkout failed'
    }
    git -C $intel checkout --detach 8fe81bdbbaf00b3c1b733fd0d830c333dc84e6f0
    CheckExit 'Intel XeSS pinned checkout failed'
    if ((git -C $intel rev-parse HEAD).Trim() -ne '8fe81bdbbaf00b3c1b733fd0d830c333dc84e6f0') { throw 'Intel XeSS SDK revision mismatch' }
    foreach ($file in @("$amd/Kits/FidelityFX/api/include/ffx_api.h", "$amd/Kits/FidelityFX/upscalers/include/ffx_upscale.h", "$amd/Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.dll", "$amd/Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll", "$intel/inc/xess/xess_d3d12.h", "$intel/bin/libxess.dll")) {
        if (!(Test-Path $file)) { throw "Missing required SDK input: $file" }
    }
    Write-Host 'Pinned AMD FidelityFX and Intel XeSS SDKs fetched.'
} else {
    $out = Join-Path $root 'build-vc170-64/newview/Release'
    if (!(Test-Path "$out/blackdragon-bin.exe")) { throw 'Viewer output missing for FSR/XeSS staging' }
    Copy-Item "$amd/Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.dll" $out -Force
    Copy-Item "$amd/Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll" $out -Force
    Copy-Item "$intel/bin/libxess.dll" $out -Force
    New-Item -ItemType Directory -Force "$out/licenses" | Out-Null
    Copy-Item "$amd/3rdpartynotice.md" "$out/licenses/AMD-FidelityFX-ThirdPartyNotices.md" -Force
    Copy-Item "$intel/LICENSE.txt" "$out/licenses/Intel-XeSS-LICENSE.txt" -Force
    Copy-Item "$intel/third-party-programs.txt" "$out/licenses/Intel-XeSS-ThirdParty.txt" -Force
    foreach ($file in @('amd_fidelityfx_loader_dx12.dll','amd_fidelityfx_upscaler_dx12.dll','libxess.dll')) {
        if (!(Test-Path "$out/$file")) { throw "Portable build missing: $file" }
    }
    Write-Host 'FSR and XeSS binaries and notices staged.'
}
