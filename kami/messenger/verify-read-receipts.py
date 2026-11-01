#!/usr/bin/env python3
"""KAMI opt-in read receipts: static safety and integrated native hooks."""
import ast, argparse
import xml.etree.ElementTree as ET
from pathlib import Path
R=Path(__file__).resolve().parents[2]
source=(R/"kami/messenger/apply-read-receipts.py").read_text(encoding="utf-8")
ast.parse(source)
for k in ("KAMIReadReceiptsEnabled","llkamireadreceipts.h",
          "parseFrame","HELLO_ACK","wrapOutgoing","acknowledgeOpened",
          "isControl(utf8_text)","visibleText(utf8_text)",
          "IM_NOTHING_SPECIAL","!from_group","mPendingRead",
          "mLastSent","mLastSeen","mVerified","session.isNull()",
          "text.size() + suffix.size() > 1023",
          "getVisible() && hasFocus()",
          "void LLFloaterIMSession::onKAMIRequestReceipts()"):
    assert k in source,k
assert "new HTTP" not in source
def read(p):return (R/p).read_text(encoding="utf-8-sig")
def integrated():
    src="indra/newview/"
    settings=ET.parse(R/(src+"app_settings/settings_per_account.xml")).getroot()
    keys=[e.text for e in settings.findall("./map/key")]
    assert keys.count("KAMIReadReceiptsEnabled")==1
    x=ET.parse(R/(src+"skins/default/xui/en/floater_im_session.xml")).getroot()
    for name in ("kami_messenger_status_text",
                 "kami_read_receipts_optin","kami_read_receipts_request"):
        assert sum(e.get("name")==name for e in x.iter())==1,name
    assert x.find(".//check_box[@name='kami_read_receipts_optin']") is not None
    header=read(src+"llkamireadreceipts.h")
    for mark in ('frame("HELLO", nonce)','frame("HELLO_ACK", id)',
                 'frame("MSG", msg_id)','frame("SEEN", id)',
                 'mVerified.find(peer) == mVerified.end()',
                 'mPendingRead.erase(pending)',
                 'if (text.size() + suffix.size() > 1023) return text',
                 'mLastSeen[peer] = id',
                 'gSavedPerAccountSettings.getBOOL("KAMIReadReceiptsEnabled")'):
        assert mark in header,mark
    proc=read(src+"llimprocessing.cpp")
    assert 'LLKamiReadReceipts::instance().incoming(' in proc
    assert 'const bool kami_direct_im =' in proc
    assert 'dialog == IM_NOTHING_SPECIAL ||' in proc
    assert 'dialog == IM_SESSION_SEND && kami_session &&' in proc
    assert 'kami_session->isP2PSessionType()' in proc
    assert 'if (kami_direct_im && !from_group &&' in proc
    assert 'LLIMMgr::computeSessionID(dialog, from_id)' in proc
    im=read(src+"llimview.cpp")
    assert 'LLKamiReadReceipts::isControl(utf8_text)' in im
    assert 'LLKamiReadReceipts::visibleText(utf8_text)' in im
    session=read(src+"llfloaterimsession.cpp")
    for k in ("void LLFloaterIMSession::onKAMIRequestReceipts()",
              'getChild<LLCheckBoxCtrl>("kami_read_receipts_optin")',
              'getChild<LLButton>("kami_read_receipts_request")',
              'LLKamiReadReceipts::instance().wrapOutgoing(',
              'acknowledgeOpened(mOtherParticipantUUID, mSessionID)',
              'label += " | " + LLKamiReadReceipts::instance().status('):
        assert k in session,k
    assert "mKAMIRRLastRevision" in read(src+"llfloaterimsession.h")
    print("PASS native opt-in handshake, message IDs, focus-confirmed Seen, isolated SL P2P transport")
p=argparse.ArgumentParser();p.add_argument("--integrated",action="store_true")
args=p.parse_args()
print("PASS source syntax, IM safety guards, per-account privacy defaults")
if args.integrated:integrated()
else:print("Native compilation and two-account online tests still required.")
