param(
    [string]$ManifestPath = "docs/KAMI_FEATURE_ACCEPTANCE.json"
)

$ErrorActionPreference = "Stop"
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

if ($manifest.application -ne "KAMIViewer.exe") {
    throw "Feature manifest must target the independent KAMIViewer.exe application."
}
if ($manifest.release_allowed -isnot [bool]) {
    throw "release_allowed must be an explicit JSON boolean."
}

$required = @(
    "ui", "dx12_world", "terrain", "mesh_residency", "sl_login",
    "sl_transport", "avatar", "inventory", "messaging", "read_receipts",
    "camera", "graphics", "creator_studio", "photo_studio", "smart_sort",
    "accessibility", "hotfixes"
)

$script:featureMap = $manifest.features
if ($null -eq $script:featureMap) {
    throw "Feature manifest has no features object."
}
$script:featureNames = @($script:featureMap.PSObject.Properties.Name)
foreach ($name in $required) {
    if ($script:featureNames -notcontains $name) {
        throw "Missing required feature gate: $name"
    }
}

$script:visitState = @{}
$script:visitPath = [System.Collections.Generic.List[string]]::new()

function Visit-KamiFeature {
    param([string]$Name)

    if ($script:visitState.ContainsKey($Name)) {
        if ($script:visitState[$Name] -eq 1) {
            $cycle = (@($script:visitPath) + $Name) -join " -> "
            throw "Feature dependency cycle detected: $cycle"
        }
        if ($script:visitState[$Name] -eq 2) {
            return
        }
    }

    $property = $script:featureMap.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Unknown feature dependency: $Name"
    }

    $entry = $property.Value
    if ([string]::IsNullOrWhiteSpace([string]$entry.acceptance)) {
        throw "Feature '$Name' has no acceptance criteria."
    }
    if ($entry.status -notin @("unverified", "partial", "verified")) {
        throw "Feature '$Name' has an invalid status: $($entry.status)"
    }
    if ($null -eq $entry.dependencies) {
        throw "Feature '$Name' must declare a dependencies array (use [] when independent)."
    }
    if ($null -eq $entry.evidence) {
        throw "Feature '$Name' must declare an evidence array (use [] until verified)."
    }

    $script:visitState[$Name] = 1
    $script:visitPath.Add($Name)
    foreach ($dependency in @($entry.dependencies)) {
        if ([string]::IsNullOrWhiteSpace([string]$dependency)) {
            throw "Feature '$Name' contains an empty dependency."
        }
        if ($script:featureNames -notcontains [string]$dependency) {
            throw "Feature '$Name' depends on unknown feature '$dependency'."
        }
        Visit-KamiFeature -Name ([string]$dependency)
    }

    if ($entry.status -eq "verified") {
        $validEvidence = @($entry.evidence | Where-Object {
            [string]$_.commit -match '^[0-9a-f]{40}$' -and
            [string]$_.ci_url -match '^https://github\.com/[^/\s]+/[^/\s]+/actions/runs/\d+$' -and
            $_.native_app -eq $true -and
            $_.live_sl -eq $true -and
            -not [string]::IsNullOrWhiteSpace([string]$_.notes)
        })
        if ($validEvidence.Count -eq 0) {
            throw "Verified feature '$Name' requires native KAMIViewer.exe CI and authenticated live SL evidence."
        }
        foreach ($dependency in @($entry.dependencies)) {
            $dependencyProperty = $script:featureMap.PSObject.Properties[[string]$dependency]
            if ($null -eq $dependencyProperty -or $dependencyProperty.Value.status -ne "verified") {
                throw "Feature '$Name' cannot be verified while dependency '$dependency' is not verified."
            }
        }
    }

    $script:visitPath.RemoveAt($script:visitPath.Count - 1)
    $script:visitState[$Name] = 2
}

foreach ($name in $script:featureNames) {
    Visit-KamiFeature -Name $name
}

if ($manifest.release_allowed) {
    if (@($manifest.release_blockers).Count -ne 0) {
        throw "Release is enabled while release blockers remain."
    }
    foreach ($name in $script:featureNames) {
        if ($script:featureMap.$name.status -ne "verified") {
            throw "Release is enabled while '$name' is not verified."
        }
    }
}

Write-Host "KAMI feature dependency graph verified: $($script:featureNames.Count) features; $($required.Count) required viewer gates."
