#!/usr/bin/env python3
"""Use KAMIVIEWER for user data, daily logs, crash files and default cache.

Build-time integration only. Existing BlackDragon folders are never moved,
deleted or automatically imported. In particular, potentially broken settings
from an older install are not carried into the new startup automatically.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(text, before, after, path):
    if text.count(before) != 1:
        raise RuntimeError(f"KAMIVIEWER data-path source drift: {path}: {before!r}")
    return text.replace(before, after, 1)


def patch(path, changes):
    target = ROOT / path
    text = target.read_text(encoding="utf-8-sig")
    for before, after in changes:
        text = replace_once(text, before, after, path)
    target.write_text(text, encoding="utf-8")


viewer = "indra/newview/llappviewer.cpp"
viewer_text = (ROOT / viewer).read_text(encoding="utf-8-sig")
# The base Kombatant patch runs before branding in the Windows workflow.
# Accept the pristine upstream fixture or that exact intermediate identity.
old_app_names = [name for name in ("BlackDragon", "KombatantViewer")
                 if f'gDirUtilp->initAppDirs("{name}");' in viewer_text]
if len(old_app_names) != 1:
    raise RuntimeError("Expected exactly one known pre-branding viewer data-root identity")
patch(viewer, [
    (f'gDirUtilp->initAppDirs("{old_app_names[0]}");', 'gDirUtilp->initAppDirs("KAMIVIEWER");'),
    ('const std::string MARKER_FILE_NAME("BlackDragon.exec_marker");',
     'const std::string KAMIVIEWER_LOG_PREFIX("KAMIVIEWER_");\n'
     'const std::string MARKER_FILE_NAME("KAMIVIEWER.exec_marker");'),
    ('const std::string START_MARKER_FILE_NAME("BlackDragon.start_marker");',
     'const std::string START_MARKER_FILE_NAME("KAMIVIEWER.start_marker");'),
    ('const std::string ERROR_MARKER_FILE_NAME("BlackDragon.error_marker");',
     'const std::string ERROR_MARKER_FILE_NAME("KAMIVIEWER.error_marker");'),
    ('const std::string LOGOUT_MARKER_FILE_NAME("BlackDragon.logout_marker");',
     'const std::string LOGOUT_MARKER_FILE_NAME("KAMIVIEWER.logout_marker");'),
    ('LLDirIterator dir_iter(dir, "BlackDragon_*.log");',
     'LLDirIterator dir_iter(dir, KAMIVIEWER_LOG_PREFIX + "*.log");'),
    ('//BD - Extract everything past "BlackDragon_", yieding us "1.1.2024.log"\n'
     '\t\tstd::string date_str = file.substr(12);',
     '// Extract the date using the actual prefix length, not the old brand length.\n'
     '\t\tstd::string date_str = file.substr(KAMIVIEWER_LOG_PREFIX.size());'),
    ('S32 year, month, day = 0;', 'S32 year = 0, month = 0, day = 0;'),
    ('sscanf(date_str.c_str(), "%d.%d.%d", &month, &day, &year);',
     'if (sscanf(date_str.c_str(), "%d.%d.%d", &month, &day, &year) != 3 ||\n'
     '\t\t\tmonth < 1 || month > 12 || day < 1 || day > 31 || year < 1970)\n'
     '\t\t{\n'
     '\t\t\tcontinue; // Do not age/delete an unrecognized log filename.\n'
     '\t\t}'),
    ('"BlackDragon_" + log_date + ".log"', 'KAMIVIEWER_LOG_PREFIX + log_date + ".log"'),
    ('// Set the log file to SecondLife.log', '// Set the daily KAMIVIEWER log file'),
])

# Update exact diagnostic filenames in every viewer path, including crash
# rotation and dump metadata. Keep the server-facing diagnostic schema keys.
p = ROOT / viewer
text = p.read_text(encoding="utf-8")
for old in ("BlackDragon.log", "BlackDragon.old", "BlackDragon.crash"):
    if f'"{old}"' not in text:
        raise RuntimeError(f"Expected viewer diagnostic filename missing: {old}")
    text = text.replace(f'"{old}"', f'"{old.replace("BlackDragon", "KAMIVIEWER")}"')
p.write_text(text, encoding="utf-8")

crash = "indra/llcrashlogger/llcrashlogger.cpp"
p = ROOT / crash
text = p.read_text(encoding="utf-8-sig")
text = replace_once(text, 'gDirUtilp->initAppDirs("BlackDragon");',
                    'gDirUtilp->initAppDirs("KAMIVIEWER");', crash)
for old in ("BlackDragon.log", "BlackDragon.old", "BlackDragonCrashReport"):
    if f'"{old}"' not in text:
        raise RuntimeError(f"Expected crash-reporter filename missing: {old}")
    text = text.replace(f'"{old}"', f'"{old.replace("BlackDragon", "KAMIVIEWER")}"')
# "BlackDragonLog" and "BDLog" are compatibility metadata fields, not paths.
# Prefer the exact active log recorded by the viewer and support old metadata.
text = replace_once(text,
    'mFileMap["BlackDragonLog"] = mDebugLog["BDLog"].asString();',
    'mFileMap["BlackDragonLog"] = mDebugLog["SLLog"].asString();\n'
    '        if (mFileMap["BlackDragonLog"].empty())\n'
    '            mFileMap["BlackDragonLog"] = mDebugLog["BDLog"].asString();', crash)
p.write_text(text, encoding="utf-8")

# Snapshot the actual daily filename instead of a nonexistent fixed log name.
# This metadata is consumed by crash collection and does not change its schema.
p = ROOT / viewer
text = p.read_text(encoding="utf-8")
old = 'gDebugInfo["SLLog"] = gDirUtilp->getExpandedFilename(LL_PATH_DUMP,"KAMIVIEWER.log");'
text = replace_once(text, old,
                    'gDebugInfo["SLLog"] = LLError::logFileName();', viewer)
p.write_text(text, encoding="utf-8")

patch("indra/llfilesystem/lldir.cpp", [
    ('res = add(getOSCacheDir(), "Black Dragon");',
     'res = add(getOSCacheDir(), "KAMIVIEWER");'),
])
print("PASS: KAMIVIEWER user data, default cache, daily-log retention and crash-reporter paths integrated.")
print("Existing BlackDragon folders are retained. No previous settings, chat logs or credentials are copied.")
