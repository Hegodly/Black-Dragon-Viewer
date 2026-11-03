#Requires -Version 5.1
[CmdletBinding()]
param([string]$ViewerPath)
$ErrorActionPreference = 'Stop'
if (-not $ViewerPath) { $ViewerPath = Read-Host 'Folder containing your KAMIViewer.exe' }
$target = [IO.Path]::GetFullPath($ViewerPath).TrimEnd([IO.Path]::DirectorySeparatorChar)
$payload = Join-Path $PSScriptRoot 'payload'
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
$exe = Join-Path $target 'KAMIViewer.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'KAMIViewer.exe was not found in that folder.' }
if (Get-Process -Name KAMIViewer -ErrorAction SilentlyContinue) { throw 'Close KAMI before installing the hotfix.' }
$current = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($current -eq $manifest.target_exe_sha256) { Write-Host 'This hotfix is already installed.'; exit 0 }
if ($manifest.supported_base_sha256 -notcontains $current) { throw 'This executable is not a supported base build. Use the full new viewer download instead.' }
function Resolve-SafeFile([string]$Root, [string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative.Contains('..') -or $Relative.Contains(':')) { throw 'Invalid package path.' }
    if ($Relative -ne 'KAMIViewer.exe' -and -not $Relative.StartsWith('skins/')) { throw 'Unexpected hotfix file.' }
    $full = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (-not $full.StartsWith($Root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escapes its folder.' }
    return $full
}
# Validate the complete payload before touching any installed viewer files.
foreach ($entry in $manifest.files) {
    $source = Resolve-SafeFile $payload $entry.path
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw ('Missing payload: ' + $entry.path) }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw ('Payload checksum failed: ' + $entry.path) }
    $null = Resolve-SafeFile $target $entry.path
}
$backup = Join-Path $target ('KAMI-hotfix-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
$records = @()
foreach ($entry in $manifest.files) {
    $destination = Resolve-SafeFile $target $entry.path
    $existed = Test-Path -LiteralPath $destination -PathType Leaf
    $records += [pscustomobject]@{ path = $entry.path; existed = $existed }
    if ($existed) {
        $save = Resolve-SafeFile $backup $entry.path
        $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $save)
        Copy-Item -LiteralPath $destination -Destination $save -Force
    }
}
$null = New-Item -ItemType Directory -Force -Path $backup
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup 'restore-record.json') -Encoding UTF8
try {
    foreach ($entry in $manifest.files) {
        $destination = Resolve-SafeFile $target $entry.path
        $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination)
        Copy-Item -LiteralPath (Resolve-SafeFile $payload $entry.path) -Destination $destination -Force
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw 'Installed file checksum failed.' }
    }
} catch {
    $failure = $_
    foreach ($record in $records) {
        $destination = Resolve-SafeFile $target $record.path
        if ($record.existed) { Copy-Item -LiteralPath (Resolve-SafeFile $backup $record.path) -Destination $destination -Force }
        elseif (Test-Path -LiteralPath $destination -PathType Leaf) { Remove-Item -LiteralPath $destination -Force }
    }
    throw ('Installation failed; original files restored. ' + $failure)
}
Write-Host ('KAMI hotfix installed. Backup: ' + $backup)
Write-Host 'Start KAMI normally. Both clients need the new build for Delivered acknowledgments.'
