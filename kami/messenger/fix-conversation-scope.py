#!/usr/bin/env python3
"""Restrict shared messenger UI to real IM sessions, never Nearby Chat."""
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2];V=ROOT/'indra/newview';X=V/'skins/default/xui/en'
def replace(path,old,new):
    s=path.read_text(encoding='utf-8-sig');assert s.count(old)==1,(path,old[:70]);path.write_text(s.replace(old,new,1),encoding='utf-8')
path=X/'floater_im_session.xml';t=ET.parse(path)
panels=[e for e in t.getroot().iter() if e.get('name')=='kami_messenger_status_panel'];assert len(panels)==1
panels[0].set('visible','false')
ET.indent(t,space=' ');t.write(path,encoding='utf-8',xml_declaration=True)
replace(V/'llfloaterimnearbychat.cpp',
 '    bool result = LLFloaterIMSessionTab::postBuild();',
 '''    bool result = LLFloaterIMSessionTab::postBuild();
    // Nearby shares the IM XML but has no peer receipt/session handshake.
    if (LLView* footer = findChild<LLView>("kami_messenger_status_panel", true))
        footer->setVisible(false);''')
replace(V/'llfloaterimsession.cpp',
 'void LLFloaterIMSession::updateKAMIMessengerStatus()\n{',
 '''void LLFloaterIMSession::updateKAMIMessengerStatus()
{
    const bool real_session = mSessionID.notNull() && mSession != nullptr;
    const bool personal_session = real_session && mIsP2PChat && mOtherParticipantUUID.notNull();
    if (LLView* footer = findChild<LLView>("kami_messenger_status_panel", true))
        footer->setVisible(real_session);
    for (const char* name : {"kami_receipt_status_text", "kami_read_receipts_optin", "kami_read_receipts_request"})
        if (LLView* control = findChild<LLView>(name, true))
            control->setVisible(personal_session);
    if (!real_session) return;
''')
# P2P uses the existing native bubble renderer; Nearby/group history keeps
# its original rendering preference. This does not change saved preferences.
replace(V/'llfloaterimsessiontab.cpp',
 '            gSavedSettings.getBOOL("PlainTextChatHistory");',
 '            !mIsP2PChat && gSavedSettings.getBOOL("PlainTextChatHistory");')
print('PASS real IM footer scope; Nearby footer hidden; P2P bubbles enabled')
