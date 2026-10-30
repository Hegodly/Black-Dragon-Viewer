# KAMI Beyond Reality: install a user-provided login MP3 into the portable viewer.
# Never download or redistribute music. This script needs no administrator rights.
param([string]$SourcePath)
$ErrorActionPreference = 'Stop'
$destination = Join-Path $PSScriptRoot 'KAMI-login.mp3'
$viewer = Join-Path $PSScriptRoot 'KAMIViewer.exe'

if (-not (Test-Path -LiteralPath $viewer -PathType Leaf)) {
    throw 'KAMIViewer.exe is missing. Place this installer in the extracted viewer folder.'
}
if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker = New-Object System.Windows.Forms.OpenFileDialog
    $picker.Title = 'Select your MP3 for the KAMI login screen'
    $picker.Filter = 'MP3 audio files (*.mp3)|*.mp3'
    $picker.CheckFileExists = $true
    if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        Write-Host 'No MP3 chosen. Login music was not changed.'
        exit 0
    }
    $SourcePath = $picker.FileName
}
$source = (Resolve-Path -LiteralPath $SourcePath -ErrorAction Stop).Path
if ([System.IO.Path]::GetExtension($source) -ine '.mp3') {
    throw 'The selected file must be an MP3. This installer does not convert audio formats.'
}
if ((Get-Item -LiteralPath $source).Length -lt 128) {
    throw 'The MP3 appears empty or incomplete.'
}
if ($source -ine $destination) {
    Copy-Item -LiteralPath $source -Destination $destination -Force
}
Write-Host ('Login soundtrack installed: ' + $destination)
Write-Host 'Close and reopen KAMI to hear the soundtrack at the login screen.'
