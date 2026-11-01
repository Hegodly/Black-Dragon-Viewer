#!/usr/bin/env python3
"""Check the paired viewer/crash-reporter naming contracts after integration."""
import argparse
import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--integrated", action="store_true")
args = parser.parse_args()
source = (ROOT / "kami/branding/apply-userdata-branding.py").read_text(encoding="utf-8")
ast.parse(source)
assert 'unlink(' not in source and 'rmtree(' not in source and 'shutil.move(' not in source
workflow = (ROOT / ".github/workflows/kombatant-windows.yaml").read_text(encoding="utf-8")
assert 'run: python kami/branding/apply-userdata-branding.py' in workflow
assert workflow.index('run: python kami/branding/apply-userdata-branding.py') < workflow.index('      - name: Configure and compile')
if args.integrated:
    viewer = (ROOT / "indra/newview/llappviewer.cpp").read_text(encoding="utf-8")
    crash = (ROOT / "indra/llcrashlogger/llcrashlogger.cpp").read_text(encoding="utf-8")
    cache = (ROOT / "indra/llfilesystem/lldir.cpp").read_text(encoding="utf-8")
    for text in (viewer, crash):
        assert 'initAppDirs("KAMIVIEWER")' in text
        assert 'initAppDirs("BlackDragon")' not in text
        for old in ('"BlackDragon.log"', '"BlackDragon.old"', '"BlackDragon.crash"'):
            assert old not in text
    assert 'const std::string KAMIVIEWER_LOG_PREFIX("KAMIVIEWER_")' in viewer
    assert 'file.substr(KAMIVIEWER_LOG_PREFIX.size())' in viewer
    assert 'KAMIVIEWER_LOG_PREFIX + "*.log"' in viewer
    assert 'KAMIVIEWER_LOG_PREFIX + log_date + ".log"' in viewer
    for suffix in ('exec_marker', 'start_marker', 'error_marker', 'logout_marker'):
        assert f'"KAMIVIEWER.{suffix}"' in viewer
    assert 'gDebugInfo["SLLog"] = LLError::logFileName();' in viewer
    assert 'mFileMap["BlackDragonLog"] = mDebugLog["SLLog"].asString();' in crash
    assert 'mDebugLog["BDLog"].asString()' in crash
    assert 'res = add(getOSCacheDir(), "KAMIVIEWER");' in cache
    assert '"KAMIVIEWERCrashReport"' in crash
    print("PASS integrated viewer/crash paths, daily prefix parsing, markers, cache and active-log metadata.")
else:
    print("PASS user-data branding integration syntax and build ordering.")
