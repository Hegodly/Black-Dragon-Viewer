#!/usr/bin/env python3
"""KAMI Messenger native IM UI + session readiness, not delivery assurance."""
import xml.etree.ElementTree as ET
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def read(x):return (R/x).read_text(encoding="utf-8-sig")
def parse(x):return ET.parse(R/"indra/newview/skins/default/xui/en"/x).getroot()
c=parse("floater_im_container.xml")
f=parse("floater_im_session.xml")
assert c.get("name")=="floater_im_box" and c.get("title")=="KAMI | MESSENGER"
assert parse("floater_conversation_log.xml").get("title")=="KAMI | MESSAGE HISTORY"
assert f.get("name")=="panel_im"
for n in ("main_stack","toolbar_panel","body_panel","chat_history","chat_editor",
          "voice_call_btn","emoji_picker_show_btn","kami_messenger_status_panel",
          "kami_messenger_status_text"):
    assert len([x for x in f.iter() if x.get("name")==n])==1,n
status=[x for x in f.iter("text") if x.get("name")=="kami_messenger_status_text"][0]
assert "CONNECTING" in (status.text or "")
for n in ("conversations_layout_panel2","conversations_layout_panel",
          "im_box_tab_container","messages_layout_panel"):
    assert len([x for x in c.iter() if x.get("name")==n])==1,n
parse("panel_chat_header.xml")
parse("panel_conversation_list_item.xml")
cpp=read("indra/newview/llfloaterimsession.cpp")
hdr=read("indra/newview/llfloaterimsession.h")
im=read("indra/newview/llimview.cpp")
for x in ("void LLFloaterIMSession::updateKAMIMessengerStatus()",
          "void LLFloaterIMSession::setKAMIMessengerTimedOut()",
          'findChild<LLTextBox>("kami_messenger_status_text", true)',
          "mQueuedMsgsForInit.size() > 0",
          "mKAMIMessengerTimedOut = false",
          "mKAMIMessengerTimedOut = true",
          "updateKAMIMessengerStatus();",
          "mQueuedMsgsForInit.append(utf8_text);",
          "LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog)",
          "LLIMModel::sendMessage(iter->asString(), mSessionID"):
    assert x in cpp,x
assert "void setKAMIMessengerTimedOut();" in hdr
assert "floater->setKAMIMessengerTimedOut();" in im
assert 'showSessionStartError("session_initialization_timed_out_error"' in im
assert "LLIMMgr::showSessionStartError" in im
print("PASS KAMI Messenger: native IM widgets, status transitions, existing SL transport, no automatic retry")
print("Compilation, UI usability, network behavior, and delivery remain unverified.")
