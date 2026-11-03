#!/usr/bin/env python3
"""Check generated native session scope, not receipt delivery claims."""
from pathlib import Path
import xml.etree.ElementTree as ET
V=Path(__file__).resolve().parents[2]/'indra/newview'
s=(V/'llfloaterimsession.cpp').read_text(encoding='utf-8-sig')
a=s.index('void LLFloaterIMSession::updateKAMIMessengerStatus()');b=s.index('\nvoid ',a+5);status=s[a:b]
assert 'mSessionID.notNull() && mSession != nullptr' in status
assert 'real_session && mIsP2PChat && mOtherParticipantUUID.notNull()' in status
assert 'footer->setVisible(real_session)' in status
assert 'control->setVisible(personal_session)' in status
assert status.index('if (!real_session) return;')<status.index('CONNECTING')
nearby=(V/'llfloaterimnearbychat.cpp').read_text(encoding='utf-8-sig')
a=nearby.index('bool LLFloaterIMNearbyChat::postBuild()');b=nearby.index('return result;',a)
assert 'footer->setVisible(false)' in nearby[a:b]
tab=(V/'llfloaterimsessiontab.cpp').read_text(encoding='utf-8-sig')
assert '!mIsP2PChat && gSavedSettings.getBOOL("PlainTextChatHistory")' in tab
assert 'chat_args["kami_bubbles"] = mIsP2PChat;' in tab
root=ET.parse(V/'skins/default/xui/en/floater_im_session.xml').getroot()
assert next(e for e in root.iter() if e.get('name')=='kami_messenger_status_panel').get('visible')=='false'
print('PASS Nearby footer hidden, real-session status guard and personal-IM receipt/bubble scope')
