# KAMI bundled offline test update. Installs only an inert marker; no executable changes.
# The existing portable viewer is copied, then swapped after it exits. Backup is retained.
# SPDX-License-Identifier: LGPL-2.1-only
param([Parameter(Mandatory=$true)][string]$InstallDir,
      [Parameter(Mandatory=$true)][int]$ParentPid)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$work = $null
$backup = $null
$switched = $false
$success = $false
try {
    if ($ParentPid -le 0) { throw 'Invalid viewer process ID' }
    $InstallDir = [IO.Path]::GetFullPath($InstallDir).TrimEnd([char]'\')
    if (-not (Test-Path -LiteralPath (Join-Path $InstallDir 'KAMIViewer.exe') -PathType Leaf)) {
        throw 'This folder does not contain KAMIViewer.exe'
    }
    $package = Join-Path $InstallDir 'app_settings/KAMI-Test-Update.zip'
    $manifestPath = Join-Path $InstallDir 'app_settings/KAMI-Test-Update.json'
    if (-not (Test-Path -LiteralPath $package -PathType Leaf) -or
        -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw 'KAMI test update missing'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ([int]$manifest.schema -ne 1) { throw 'Unsupported test update' }
    $actual = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
    if ($actual -cne [string]$manifest.sha256) { throw 'Test update SHA256 mismatch' }
    if ([string]$manifest.overlay -cne 'app_settings/KAMI-TEST-UPDATE-INSTALLED.txt') {
        throw 'Unexpected update target'
    }
    Write-Host 'KAMI test package passed SHA256 verification.'
    try { Wait-Process -Id $ParentPid -Timeout 180 -ErrorAction Stop }
    catch {
        if (Get-Process -Id $ParentPid -ErrorAction SilentlyContinue) {
            throw 'KAMI did not exit. No update installed.'
        }
    }
    if (Get-Process -Name 'KAMIViewer' -ErrorAction SilentlyContinue) {
        throw 'Another KAMI process is running'
    }
    $parent = [IO.Path]::GetDirectoryName($InstallDir)
    if ([string]::IsNullOrEmpty($parent) -or $parent -eq $InstallDir) {
        throw 'Cannot update root directory'
    }
    $work = Join-Path $parent ('.kami-test-' + [guid]::NewGuid().ToString('N'))
    $stage = Join-Path $work 'stage'
    $backup = Join-Path $work 'previous-build'
    New-Item -ItemType Directory -Path $work | Out-Null
    Write-Host 'Staging complete viewer; original remains intact.'
    Copy-Item -LiteralPath $InstallDir -Destination $stage -Recurse -Force -ErrorAction Stop
    $overlay = Join-Path $work 'overlay'
    New-Item -ItemType Directory -Path $overlay | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($package, $overlay)
    $marker = Join-Path $overlay 'app_settings/KAMI-TEST-UPDATE-INSTALLED.txt'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { throw 'Missing test marker' }
    if ([IO.File]::ReadAllText($marker) -cne ('KAMI TEST UPDATE INSTALLED - NO EXECUTABLE CHANGES' + [char]10)) {
        throw 'Incorrect test marker'
    }
    Copy-Item -LiteralPath $marker -Destination (Join-Path $stage 'app_settings/KAMI-TEST-UPDATE-INSTALLED.txt') -Force
    foreach ($required in @('KAMIViewer.exe','skins/default/xui/en/panel_login.xml',
                             'app_settings/settings.xml','licenses.txt','KAMI-CREDITS.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $required) -PathType Leaf)) {
            throw "Incomplete staged viewer: $required"
        }
    }
    Move-Item -LiteralPath $InstallDir -Destination $backup -ErrorAction Stop
    $switched = $true
    Move-Item -LiteralPath $stage -Destination $InstallDir -ErrorAction Stop
    Start-Process -FilePath (Join-Path $InstallDir 'KAMIViewer.exe') -WorkingDirectory $InstallDir -ErrorAction Stop
    $success = $true
    Write-Host 'KAMI TEST UPDATE INSTALLED. Viewer restarted.'
    Write-Host "Previous build backed up at $backup"
}
catch {
    Write-Host ("KAMI test update failed: " + $_.Exception.Message) -ForegroundColor Red
    if ($switched) {
        try {
            if (Test-Path -LiteralPath $InstallDir) {
                Move-Item -LiteralPath $InstallDir -Destination (Join-Path $work 'failed') -ErrorAction Stop
            }
            if (Test-Path -LiteralPath $backup) {
                Move-Item -LiteralPath $backup -Destination $InstallDir -ErrorAction Stop
                Write-Host 'Previous installation restored.'
            }
        }
        catch { Write-Host ("Manual rollback required: " + $_.Exception.Message) -ForegroundColor Red }
    }
}
finally {
    if ($work -and (Test-Path -LiteralPath $work)) {
        if ($success) {
            foreach ($p in @((Join-Path $work 'overlay'),(Join-Path $work 'stage'))) {
                if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction SilentlyContinue }
            }
        }
        elseif (-not $switched -or (Test-Path -LiteralPath $InstallDir)) {
            Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
if (-not $success) { exit 1 }
