#!/usr/bin/env python3
"""Apply both messaging integrations in isolation; execute the real C++ protocol.

Transport, settings and notifications are stubbed. This is not an online SL
integration test or a substitute for compiling the viewer on Windows.
"""
from pathlib import Path
import ast
import os
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
FILES = [
    "app_settings/settings_per_account.xml", "llimprocessing.cpp", "llimview.cpp",
    "llfloaterimsession.cpp", "llfloaterimsession.h", "llfloaterimsessiontab.cpp",
    "llchathistory.cpp", "llchathistory.h",
    "skins/default/xui/en/floater_im_session.xml",
    "skins/default/xui/en/notifications.xml",
]

STUBS = r'''
#pragma once
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using U32 = unsigned;
class LLUUID {
    std::string value = "00000000-0000-0000-0000-000000000000";
public:
    LLUUID() = default;
    explicit LLUUID(const std::string& s) { assert(set(s, false)); }
    static LLUUID generateNewID() {
        static unsigned n = 0;
        char out[37]; std::snprintf(out, sizeof(out), "00000000-0000-0000-0000-%012u", ++n);
        return LLUUID(out);
    }
    bool set(const std::string& s, bool) {
        if (s.size() != 36) return false;
        for (size_t i=0;i<s.size();++i)
            if ((i==8 || i==13 || i==18 || i==23) ? s[i]!='-' : !std::isxdigit(s[i])) return false;
        value = s;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return std::tolower(c); });
        return true;
    }
    bool isNull() const { return value == "00000000-0000-0000-0000-000000000000"; }
    std::string asString() const { return value; }
    bool operator==(const LLUUID& rhs) const { return value==rhs.value; }
    bool operator!=(const LLUUID& rhs) const { return !(*this==rhs); }
    bool operator<(const LLUUID& rhs) const { return value<rhs.value; }
};
struct LLSD {
    LLSD& operator[](const std::string&) { return *this; }
    template<class T> LLSD& operator=(const T&) { return *this; }
};
struct Settings { bool opted_in=false; bool getBOOL(const std::string&) const { return opted_in; } };
inline Settings gSavedPerAccountSettings;
struct Agent { LLUUID id=LLUUID::generateNewID(); LLUUID getID() const { return id; } };
inline Agent gAgent;
enum EInstantMessage { IM_NOTHING_SPECIAL, IM_SESSION_SEND };
struct Packet { std::string text; LLUUID session, peer; EInstantMessage dialog; };
inline std::vector<Packet> packets;
struct LLIMModel {
    static void sendMessage(const std::string& text, const LLUUID& session,
                            const LLUUID& peer, EInstantMessage dialog) {
        packets.push_back({text, session, peer, dialog});
    }
};
inline unsigned toasts=0;
struct LLNotificationsUtil {
    static void add(const std::string& name, const LLSD&, const LLSD&) {
        assert(name=="KAMIMessageOpened"); ++toasts;
    }
};
'''

TEST = r'''
#include "llkamireadreceipts.h"
#include <iostream>
int main() {
    auto& rr=LLKamiReadReceipts::instance();
    const LLUUID peer=LLUUID::generateNewID(), stranger=LLUUID::generateNewID(), session=LLUUID::generateNewID();
    const auto dialog=IM_NOTHING_SPECIAL;
    auto receive=[&](const std::string& frame, bool allowed=true, const LLUUID* sender=nullptr) {
        auto text=frame; return rr.incoming(sender?*sender:peer, session, text, allowed, dialog);
    };
    auto connect=[&]() {
        rr.request(peer, session, dialog);
        assert(!packets.empty());
        std::string verb; LLUUID nonce;
        assert(rr.parseFrame(packets.back().text, verb, nonce) && verb=="HELLO");
        assert(receive(rr.frame("HELLO_ACK", nonce)));
    };
    std::string verb; LLUUID id;
    assert(!rr.parseFrame("[[KAMI-RR1|SEEN|bad]]", verb,id));
    assert(!rr.parseFrame(rr.frame("BOGUS",peer),verb,id));
    assert(!rr.parseFrame(rr.frame("MSG", LLUUID()),verb,id));
    assert(!rr.isControl(rr.frame("MSG",peer)));
    assert(rr.visibleText("normal text") == "normal text");
    assert(rr.visibleText("quote [[KAMI-RR1|MSG|bad]]") == "quote [[KAMI-RR1|MSG|bad]]");
    rr.request(peer,session,dialog);
    assert(packets.empty()); // off by default, no discovery traffic
    assert(rr.wrapOutgoing(peer,"hello") == "hello");
    assert(receive(rr.frame("HELLO",peer)) && packets.empty());
    gSavedPerAccountSettings.opted_in=true;
    assert(rr.wrapOutgoing(peer,"hello") == "hello"); // unknown viewer stays plaintext
    assert(receive(rr.frame("HELLO_ACK",peer)));
    assert(rr.wrapOutgoing(peer,"still plain") == "still plain"); // unsolicited ACK rejected
    connect();
    assert(toasts==0); // handshake is not a read receipt
    auto wire=rr.wrapOutgoing(peer,"hello");
    assert(wire!="hello" && rr.visibleText(wire)=="hello");
    assert(rr.parseFrame(wire.substr(wire.find('\n')+1),verb,id));
    assert(!rr.acknowledgeOpened(peer,session,dialog));
    assert(toasts==0); // opening own chat cannot notify
    assert(receive(rr.frame("SEEN",id),true,&stranger) && toasts==0);
    assert(receive(rr.frame("SEEN",LLUUID::generateNewID())) && toasts==0);
    assert(receive(rr.frame("SEEN",id),false) && toasts==0);
    assert(receive(rr.frame("SEEN",id)) && toasts==1);
    assert(rr.status(peer)=="Open and read");
    assert(receive(rr.frame("SEEN",id)) && toasts==1); // duplicate suppression
    auto old=id;
    wire=rr.wrapOutgoing(peer,"new message");
    assert(rr.parseFrame(wire.substr(wire.find('\n')+1),verb,id));
    assert(receive(rr.frame("SEEN",old)) && toasts==1); // old ACK cannot mark new text
    assert(receive(rr.frame("SEEN",id)) && toasts==2);
    const std::string long_text(1000,'x');
    assert(rr.wrapOutgoing(peer,long_text)==long_text);
    assert(rr.status(peer)=="Sent | Read status unknown"); // no stale seen on untagged text
    assert(receive(rr.frame("SEEN",id)) && toasts==2);

    const auto incoming_id=LLUUID::generateNewID();
    auto incoming=std::string("reply\n")+rr.frame("MSG",incoming_id);
    assert(!rr.incoming(peer,session,incoming,true,IM_SESSION_SEND) && incoming=="reply");
    auto before=packets.size();
    assert(toasts==2 && packets.size()==before); // arrival alone sends nothing
    assert(!rr.acknowledgeOpened(peer,LLUUID::generateNewID(),IM_SESSION_SEND));
    assert(rr.acknowledgeOpened(peer,session,IM_SESSION_SEND));
    assert(packets.size()==before+1 && packets.back().text==rr.frame("SEEN",incoming_id));
    assert(packets.back().dialog==IM_SESSION_SEND); // modern P2P dialog preserved
    assert(!rr.acknowledgeOpened(peer,session,IM_SESSION_SEND));
    incoming=std::string("reply\n")+rr.frame("MSG",incoming_id);
    rr.incoming(peer,session,incoming,true,dialog);
    assert(!rr.acknowledgeOpened(peer,session,dialog)); // duplicate MSG suppressed
    incoming=std::string("muted\n")+rr.frame("MSG",LLUUID::generateNewID());
    rr.incoming(peer,session,incoming,false,dialog);
    assert(incoming=="muted" && !rr.acknowledgeOpened(peer,session,dialog));
    auto rev=rr.revision();
    gSavedPerAccountSettings.opted_in=false;
    assert(rr.revision()!=rev);
    gSavedPerAccountSettings.opted_in=true;
    rr.revision();
    assert(rr.wrapOutgoing(peer,"reset") == "reset");
    assert(receive(rr.frame("SEEN",id)) && toasts==2);
    connect();
    auto account=gAgent.id;
    gAgent.id=LLUUID::generateNewID();
    rr.revision();
    assert(rr.wrapOutgoing(peer,"other account")=="other account");
    gAgent.id=account;
    rr.revision();
    assert(rr.wrapOutgoing(peer,"original account")=="original account");
    before=packets.size();
    assert(receive(rr.frame("HELLO",peer),false));
    assert(packets.size()==before); // offline/DND/muted transport permission denies response
    std::cout << "PASS real C++ receipt protocol: opt-in, handshake, validation, one toast,\n"
                 "plaintext fallback, stale/duplicate ACKs, focus-only send, reset and P2P dialogs\n";
}
'''


def verify_integrated(root):
    v = root / "indra/newview"
    header = (v / "llkamireadreceipts.h").read_text()
    cpp = (v / "llfloaterimsession.cpp").read_text()
    history = (v / "llchathistory.cpp").read_text()
    for guard in ("gFocusMgr.getAppHasFocus()", "mChatHistory->isAtEnd()",
                  "!isInVisibleChain()", "gAgent.isDoNotDisturb()", "LLMuteList::getInstance()->isMuted"):
        assert guard in cpp, guard
    assert cpp.count('LLNotificationsUtil::add("KAMIMessageOpened"') == 0
    assert header.count('LLNotificationsUtil::add("KAMIMessageOpened"') == 1
    assert 'LLKamiMessageBubble' in history and 'args["kami_bubbles"].asBoolean()' in history
    assert 'chat.mNotifId.isNull()' in history and '!use_plain_text_chat_history' in history
    assert 'void LLChatHistory::reshape(' in history and 'mEditor->needsReflow()' in history
    proc = (v / "llimprocessing.cpp").read_text()
    assert '!is_muted && !is_do_not_disturb && offline == IM_ONLINE' in proc
    for p in ("app_settings/settings_per_account.xml", "skins/default/xui/en/floater_im_session.xml",
              "skins/default/xui/en/notifications.xml"):
        ET.parse(v / p)
    xui = ET.parse(v / "skins/default/xui/en/floater_im_session.xml").getroot()
    stack = xui.find(".//layout_stack[@name='main_stack']")
    children = list(stack)
    footer = stack.find("layout_panel[@name='kami_messenger_status_panel']")
    body = stack.find("layout_panel[@name='body_panel']")
    assert children.index(footer) == children.index(body)+1
    assert xui.find(".//text[@name='kami_receipt_status_text']") is not None
    assert int(xui.get('min_width')) >= 380
    optin = xui.find(".//check_box[@name='kami_read_receipts_optin']")
    request = xui.find(".//button[@name='kami_read_receipts_request']")
    assert int(optin.get('width'))+int(request.get('width'))+24 <= int(xui.get('min_width'))
    note = ET.parse(v / "skins/default/xui/en/notifications.xml").getroot().find("notification[@name='KAMIMessageOpened']")
    assert note is not None and note.get('type')=='notifytip' and 'Open and read' in note.text
    assert 'KamiChatOpenedDiagnostic' not in cpp
    print('PASS integrated XML, footer placement, compact/system fallback and foreground/viewport guards')


def main():
    for p in ('apply-read-receipts.py', 'apply-imessage-style.py'):
        ast.parse((ROOT / 'kami/messenger' / p).read_text())
    with tempfile.TemporaryDirectory(prefix='kami-messenger-') as tmp:
        root = Path(tmp)
        for name in FILES:
            src = ROOT / 'indra/newview' / name
            dst = root / 'indra/newview' / name
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
        for name in ('apply-read-receipts.py', 'apply-imessage-style.py', 'native-bubble.inc'):
            dst = root / 'kami/messenger' / name
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / 'kami/messenger' / name, dst)
        for name in ('apply-read-receipts.py', 'apply-imessage-style.py'):
            subprocess.run([os.sys.executable, str(root / 'kami/messenger' / name)], check=True)
        verify_integrated(root)
        stubs = root / 'protocol-test'
        stubs.mkdir()
        (stubs / 'stubs.h').write_text(STUBS)
        for name in ('lluuid.h', 'llviewercontrol.h', 'llagent.h', 'llimview.h', 'llnotificationsutil.h'):
            (stubs / name).write_text('#include "stubs.h"\n')
        shutil.copyfile(root / 'indra/newview/llkamireadreceipts.h', stubs / 'llkamireadreceipts.h')
        (stubs / 'test.cpp').write_text(TEST)
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            raise RuntimeError('A C++ compiler is required to execute the receipt protocol tests')
        binary = stubs / 'test'
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        str(stubs / 'test.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--integrated', action='store_true')
    args = parser.parse_args()
    if args.integrated:
        verify_integrated(ROOT)
    else:
        main()
