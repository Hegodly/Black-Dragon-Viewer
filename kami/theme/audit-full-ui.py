#!/usr/bin/env python3
"""Audit every native KAMI XUI screen without falsely certifying visual completion.

This is a *test-build reporting tool*, not a shortcut to 100% UI sign-off.
Actual visual verification requires screenshots, keyboard and scaling tests.
Source changes invalidate previous manual sign-offs.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

CATEGORIES = (
    ("shell", r"login|navigation|toolbar|bottomtray|status|menu_viewer|notification"),
    ("messaging", r"im_|chat|conversation|people|profile|group"),
    ("inventory_and_avatar", r"inventory|outfit|wearable|appearance|gesture|physics"),
    ("creation", r"build|edit|upload|material|script|texture|snapshot|camera|inspect"),
    ("exploration", r"map|places|teleport|search|directory|environment|sky|water|land"),
    ("settings", r"preferences|settings|controls|privacy|audio|voice|help|about"),
)
FIELDS = ("visual_dark","visual_light","functional","keyboard","scaling","performance")
HEADERS = ("path","group","sha256","xml","status",*FIELDS,"reviewer","screenshot","test_evidence","notes")
BRAND = re.compile(r"\bblack\s*dragon\b|\bblackdragon\b|\bbd\s+viewer\b", re.I)

def category(name):
    for label,expression in CATEGORIES:
        if re.search(expression,name,re.I):
            return label
    return "remaining_dialogs"

def scan(viewer_root, output, strict=False):
    directory=viewer_root/"skins/default/xui/en"
    assert directory.is_dir(),f"Missing UI directory: {directory}"
    output.mkdir(parents=True,exist_ok=True)
    csv_file=output/"KAMI-UI-COVERAGE.csv"
    prior={}
    if csv_file.exists():
        with csv_file.open(newline="",encoding="utf-8") as handle:
            prior={row["path"]:row for row in csv.DictReader(handle)}
    lines=[]
    invalid=[]
    legacy=[]
    for file in sorted(directory.rglob("*.xml")):
        path=file.relative_to(directory).as_posix()
        digest=hashlib.sha256(file.read_bytes()).hexdigest()
        current=prior.get(path,{})
        same=current.get("sha256")==digest
        valid=True
        branded=False
        try:
            root=ET.parse(file).getroot()
            if path!="floater_about.xml":
                for node in root.iter():
                    # Action IDs and legal notices are not display branding.
                    for key in ("label","title","tool_tip","description","placeholder_text"):
                        if BRAND.search(node.get(key,"")): branded=True
                    if node.tag in ("text","string") and node.text and BRAND.search(node.text):
                        if not re.search("NiranV|source credit|based on|copyright|licen[cs]e",node.text,re.I):
                            branded=True
        except ET.ParseError:
            valid=False
        if not valid: invalid.append(path)
        if branded: legacy.append(path)
        complete=(valid and not branded and same and current.get("status")=="VERIFIED"
                  and all(current.get(k)=="PASS" for k in FIELDS)
                  and bool(current.get("reviewer")) and bool(current.get("screenshot"))
                  and bool(current.get("test_evidence")))
        row={"path":path,"group":category(path),"sha256":digest,"xml":"PASS" if valid else "FAIL",
             "status":"VERIFIED" if complete else "PENDING"}
        for k in FIELDS:
            row[k]=current.get(k,"PENDING") if same else "PENDING"
        for k in ("reviewer","screenshot","test_evidence","notes"):
            row[k]=current.get(k,"") if same else ""
        lines.append(row)
    assert lines, "No UI resources discovered"
    with csv_file.open("w",newline="",encoding="utf-8") as handle:
        writer=csv.DictWriter(handle,fieldnames=HEADERS)
        writer.writeheader()
        writer.writerows(lines)
    verified=sum(x["status"]=="VERIFIED" for x in lines)
    summary={
        "screen_files":len(lines), "verified":verified,"pending":len(lines)-verified,
        "completion_percent":round(100*verified/len(lines),2),
        "invalid_xml":invalid,"visible_legacy_branding":legacy,
        "groups":{key:sum(row["group"]==key for row in lines) for key,_ in CATEGORIES},
        "remaining_dialogs":sum(row["group"]=="remaining_dialogs" for row in lines),
        "release_ui_gate":"PASS" if verified==len(lines) and not invalid and not legacy else "BLOCKED",
        "qualification":"Native XML parsing cannot prove a window looks like KAMI; require screenshots and live tests."
    }
    (output/"KAMI-UI-COVERAGE.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
    print(f"KAMI UI: {verified}/{len(lines)} verified; full UI sign-off: {summary['release_ui_gate']}")
    if strict and summary["release_ui_gate"]!="PASS":return 1
    return 0

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--viewer-root",type=Path,default=Path("indra/newview"))
    p.add_argument("--output",type=Path,default=Path("kami-ui-qa"))
    p.add_argument("--strict",action="store_true",help="Use only for release-candidate approval, not test builds")
    args=p.parse_args()
    return scan(args.viewer_root,args.output,args.strict)
if __name__=="__main__": sys.exit(main())
