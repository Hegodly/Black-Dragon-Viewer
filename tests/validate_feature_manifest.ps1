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
