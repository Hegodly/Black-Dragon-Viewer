#!/usr/bin/env python3
"""Package compiled viewer + skins with checksums and a local-only installer."""
from pathlib import Path
import argparse,hashlib,json,zipfile
ROOT=Path(__file__).resolve().parents[2]
BASE='5e1aea41e508623f7490f9d8dc354159be81b1441fed21b4e113e602265355a9' # actual successful 55feac4 binary
p=argparse.ArgumentParser();p.add_argument('viewer',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
assert (a.viewer/'KAMIViewer.exe').is_file()
assert (a.viewer/'skins/default/xui/en/floater_im_session.xml').is_file()
files=[a.viewer/'KAMIViewer.exe']+sorted(p for p in (a.viewer/'skins').rglob('*') if p.is_file())
manifest={'version':1,'supported_base_sha256':[BASE],'files':[]}
for f in files:manifest['files'].append({'path':f.relative_to(a.viewer).as_posix(),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
manifest['target_exe_sha256']=manifest['files'][0]['sha256']
a.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.output,'w',zipfile.ZIP_DEFLATED) as z:
 for f in files:z.write(f,'payload/'+f.relative_to(a.viewer).as_posix())
 z.writestr('manifest.json',json.dumps(manifest,indent=2))
 z.write(ROOT/'kami/messenger/Install-KAMIHotfix.ps1','Install-KAMIHotfix.ps1')
 z.writestr('READ-ME.txt', '''KAMI Windows Messenger + UI hotfix\n\nFor the original 55feac4 test build only. If the installer rejects your base executable, download the complete new viewer instead.\n\n1. Close KAMI and extract this entire ZIP.\n2. Open PowerShell in the extracted folder.\n3. Run: .\\Install-KAMIHotfix.ps1 -ViewerPath "C:\\path\\to\\your\\KAMI-folder"\n   If Windows blocks the downloaded script, review it, then use Unblock-File .\\Install-KAMIHotfix.ps1 before running. No administrator privileges or permanent system policy changes are needed.\n4. Start KAMI normally.\n\nThe script verifies all payload hashes, backs up replaced files inside KAMI-hotfix-backups, and restores them if copying fails. To undo after a successful installation: close KAMI and copy KAMIViewer.exe and skins from that backup over your viewer folder.\n\nSent means handed to the existing transport, not server delivery. Delivered means the receiving compatible, opted-in KAMI acknowledged receipt. Read means that client displayed the message under the existing foreground/focus guards. No claim of human attention. Network delays apply. Older compatible KAMI retains read receipts; unsupported viewers show Sent only.\n\nWorld Menu is available in the Workspace Navigator. DX12 remains an experimental preview, not a complete renderer migration.\n\nThe hotfix includes an executable: apply only after the Windows build succeeds. No renderer setting defaults are changed by this installer.\n''')
with zipfile.ZipFile(a.output) as z:assert z.testzip() is None
print(f'PASS packaged {len(files)} compiled viewer/skin files with SHA256 manifest: {a.output}')
