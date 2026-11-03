# KAMI restart-safe release installer. Bundled with the native viewer; never fetched as a script.
# Requires a public, non-prerelease, three-part semver KAMI GitHub Release,
# with a KAMI-Windows-Portable.zip asset and SHA256 digest in GitHub release metadata.
# SPDX-License-Identifier: LGPL-2.1-only
param(
  [Parameter(Mandatory=$true)][string] $InstallDir,
  [Parameter(Mandatory=$true)][int] $ParentPid,
  [Parameter(Mandatory=$true)][string] $TargetTag
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$api = 'https://api.github.com/repos/Hegodly/Black-Dragon-Viewer/releases/latest'
$assetName = 'KAMI-Windows-Portable.zip'
$repo = 'https://github.com/Hegodly/Black-Dragon-Viewer/releases/download/'
$agent = 'KAMIViewer-Updater/1.0'
$work = $null
$stage = $null
$backup = $null
$renamed = $false
$installed = $false
$success = $false

function Require([bool] $condition, [string] $message) {
  if (-not $condition) { throw $message }
}
function Validate-Version([string] $version) {
  Require ($version -cmatch '^[vV]?[0-9]{1,9}\.[0-9]{1,9}\.[0-9]{1,9}$') 'Invalid target version'
}
function Check-Archive([string] $archive, [string] $destination) {
  Add-Type -AssemblyName System.IO.Compression
  $zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
  try {
    Require ($zip.Entries.Count -gt 10) 'Incomplete release archive'
    Require ($zip.Entries.Count -lt 100000) 'Unexpected archive entry count'
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $root = [IO.Path]::GetFullPath($destination).TrimEnd([char]'\') + '\'
    foreach ($entry in $zip.Entries) {
      $name = $entry.FullName
      Require (-not [string]::IsNullOrWhiteSpace($name)) 'Empty ZIP entry'
      Require (-not $name.StartsWith('/') -and -not $name.StartsWith('\') -and
        -not $name.Contains(':') -and -not $name.Contains([char]0)) 'Unsafe ZIP entry name'
      $parts = $name.Replace('\','/').Split('/')
      foreach ($segment in $parts) {
        Require ($segment -ne '.' -and $segment -ne '..') 'Parent traversal in ZIP'
      }
      $full = [IO.Path]::GetFullPath([IO.Path]::Combine($destination, $name.Replace('/', '\')))
      Require ($full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) 'ZIP entry escapes staging root'
      Require ($seen.Add($full)) 'Duplicate ZIP entry (case-insensitive)'
      # Unix symlink type (0120000) is forbidden; never follow links from archives.
      $mode = (($entry.ExternalAttributes -shr 16) -band 0xF000)
      Require ($mode -ne 0xA000) 'Symlink in release archive'
      Require ($entry.Length -le 2147483648L) 'Oversized ZIP entry'
    }
  }
  finally { $zip.Dispose() }
}
try {
  Validate-Version $TargetTag
  Require ($ParentPid -gt 0) 'Invalid parent process'
  $InstallDir = [IO.Path]::GetFullPath($InstallDir).TrimEnd([char]'\')
  Require (Test-Path -LiteralPath (Join-Path $InstallDir 'KAMIViewer.exe') -PathType Leaf) 'Not a Kami portable installation'
  Require (Test-Path -LiteralPath (Join-Path $InstallDir 'app_settings/kami-updater.ps1') -PathType Leaf) 'Updater is not in this installation'
  $parent = [IO.Path]::GetDirectoryName($InstallDir)
  Require (-not [string]::IsNullOrEmpty($parent) -and $parent -ne $InstallDir) 'Cannot replace drive root'
  $latest = Invoke-RestMethod -Uri $api -Headers @{ 'User-Agent'=$agent; Accept='application/vnd.github+json' } -TimeoutSec 25
  Require (-not $latest.draft -and -not $latest.prerelease) 'No stable release published'
  Require ($latest.tag_name -ceq $TargetTag) 'Release changed since you approved it; try checking again'
  $assets = @($latest.assets | Where-Object { $_.name -ceq $assetName })
  Require ($assets.Count -eq 1) 'This release has no unique full Windows install package'
  $asset = $assets[0]
  $expectedUrl = $repo + $TargetTag + '/' + $assetName
  Require ([string]$asset.browser_download_url -ceq $expectedUrl) 'Untrusted release file URL'
  $digest = [string]$asset.digest
  Require ($digest -cmatch '^sha256:([0-9a-fA-F]{64})$') 'Release SHA256 digest is required'
  $expectedHash = $Matches[1].ToUpperInvariant()
  $expectedBytes = [long]$asset.size
  Require ($expectedBytes -gt 1048576L -and $expectedBytes -lt 2147483648L) 'Invalid release package size'

  $work = Join-Path $parent ('.kami-update-' + [Guid]::NewGuid().ToString('N'))
  $stage = Join-Path $work 'stage'
  $backup = Join-Path $work 'backup'
  New-Item -ItemType Directory -Path $work, $stage | Out-Null
  $archive = Join-Path $work 'KAMI-Windows-Portable.zip'
  Write-Host "Downloading verified KAMI release $TargetTag..."
  Invoke-WebRequest -Uri $expectedUrl -Headers @{ 'User-Agent'=$agent } -OutFile $archive -TimeoutSec 600
  $download = Get-Item -LiteralPath $archive
  Require ($download.Length -eq $expectedBytes) 'Release size mismatch'
  $actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToUpperInvariant()
  Require ($actualHash -ceq $expectedHash) 'Release checksum mismatch - installation cancelled'
  Check-Archive $archive $stage
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [System.IO.Compression.ZipFile]::ExtractToDirectory($archive, $stage)
  foreach ($required in @('KAMIViewer.exe','app_settings/settings.xml','app_settings/kami-updater.ps1',
                           'skins/default/xui/en/panel_login.xml','licenses.txt','KAMI-CREDITS.txt')) {
    Require (Test-Path -LiteralPath (Join-Path $stage $required) -PathType Leaf) "Missing release component: $required"
  }
  Write-Host 'Package verified. Waiting for KAMIViewer.exe to close...'
  try { Wait-Process -Id $ParentPid -Timeout 180 -ErrorAction Stop }
  catch {
    # Do not touch any files while the viewer is still running.
    if (Get-Process -Id $ParentPid -ErrorAction SilentlyContinue) { throw 'KAMI did not exit; installation cancelled' }
  }
  Require (-not (Get-Process -Name 'KAMIViewer' -ErrorAction SilentlyContinue)) 'Another Kami viewer is running'
  # The whole directory is switched on one volume. Backup is kept for recovery.
  Move-Item -LiteralPath $InstallDir -Destination $backup -ErrorAction Stop
  $renamed = $true
  Move-Item -LiteralPath $stage -Destination $InstallDir -ErrorAction Stop
  $installed = $true
  Start-Process -FilePath (Join-Path $InstallDir 'KAMIViewer.exe') -WorkingDirectory $InstallDir -ErrorAction Stop
  $success = $true
  Write-Host "KAMI $TargetTag installed and restarted. Previous build backed up at $backup."
}
catch {
  Write-Host ("KAMI update could not be installed: " + $_.Exception.Message) -ForegroundColor Red
  if ($renamed) {
    try {
      if ($installed -and (Test-Path -LiteralPath $InstallDir)) {
        $failed = Join-Path $work 'failed-install'
        Move-Item -LiteralPath $InstallDir -Destination $failed -ErrorAction Stop
      }
      if (Test-Path -LiteralPath $backup) {
        Move-Item -LiteralPath $backup -Destination $InstallDir -ErrorAction Stop
        Write-Host 'Previous Kami installation restored.'
      }
    }
    catch { Write-Host ("Rollback requires manual attention: " + $_.Exception.Message) -ForegroundColor Red }
  }
}
finally {
  if ($work -and (Test-Path -LiteralPath $work)) {
    if ($success) {
      # Retain the old viewer backup; remove just downloaded archive and staging.
      foreach ($p in @($archive, $stage)) {
        if ($p -and (Test-Path -LiteralPath $p)) { Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction SilentlyContinue }
      }
    } elseif (-not $renamed -or (Test-Path -LiteralPath $InstallDir)) {
      Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
  }
  if (-not $success) { Write-Host 'No update was installed. You can still run your previous viewer.' }
}
if (-not $success) { exit 1 }
