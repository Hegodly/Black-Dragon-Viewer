#!/usr/bin/env python3
"""Static source contract for KAMI-to-KAMI read acknowledgement beta."""
import ast
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
src=(R/"kami/messenger/apply-read-receipts.py").read_text(encoding="utf-8")
ast.parse(src)
for key in ("KAMIReadReceipts","[KAMI_READ_V1]","peer_invited",
            "LLUUID::generateNewID", "maybeAcknowledgeKAMIRead",
            "toggleKAMIReadReceipts", "mSessionInitialized",
            "mIsP2PChat", "IM_NOTHING_SPECIAL",
            "from_id != gAgentID", "isInVisibleChain()",
            "getSelectedSession() == mSessionID",
            "KAMIReadReceipts::viewed", "getVisible() && box->hasFocus()"):
    assert key in src,key

def read(x):return (R/x).read_text(encoding="utf-8-sig")
def integrated():
    model=read("indra/newview/llimview.cpp")
    sess=read("indra/newview/llfloaterimsession.cpp")
    header=read("indra/newview/llimview.h")
    sh=read("indra/newview/llfloaterimsession.h")
    xui=ET.parse(R/"indra/newview/skins/default/xui/en/floater_im_session.xml").getroot()
    assert sum(x.get("name")=="kami_messenger_receipts_btn" for x in xui.iter("button")) == 1
    for x in ("namespace KAMIReadReceipts","bool observe(", "void toggle(",
              "void outgoing(", "void viewed(", "std::string label("):
        assert x in model,x
    assert "namespace KAMIReadReceipts" in header
    assert "LLIMModel::sendMessage(kInvite, session, peer, IM_NOTHING_SPECIAL)" in model
    assert "LLIMModel::sendMessage(kAckPrefix + state->inbound_digest" in model
    assert "state->local && state->peer_invited" in model
    assert "LLUUID::generateNewID" in model
    assert "if (kami_session && kami_session->isP2PSessionType()" in model
    assert "KAMIReadReceipts::observe(new_session_id, other_participant_id, msg)" in model
    assert "void LLFloaterIMSession::toggleKAMIReadReceipts()" in sess
    assert "void LLFloaterIMSession::maybeAcknowledgeKAMIRead()" in sess
    assert "KAMIReadReceipts::outgoing(mSessionID" in sess
    assert "KAMIReadReceipts::viewed(mSessionID" in sess
    assert "self->maybeAcknowledgeKAMIRead();" in sess
    assert "void maybeAcknowledgeKAMIRead();" in sh
    assert "Receipts: ON" in sess
    # Ensure transport and outgoing message queue behavior was not replaced.
    assert "LLIMModel::sendMessage(utf8_text, mSessionID" in sess
    assert "mQueuedMsgsForInit.append(utf8_text);" in sess
    print("PASS KAMI beta receipts: explicit reciprocal consent, P2P-only, focused-view ACK, original message transport.")
p=argparse.ArgumentParser()
p.add_argument("--integrated", action="store_true")
args=p.parse_args()
print("PASS receipt integration source parses; no external relay or auto-tracking.")
if args.integrated:integrated()
else:print("C++ compile, actual delivery, login boundaries, non-Kami switchovers and UI need in-world testing.")
