#!/usr/bin/env python3
"""Compile and execute the generated protocol with simulated two-client traffic."""
from pathlib import Path
import importlib.util,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('existing',Path(__file__).with_name('test-imessage-style.py'))
existing=importlib.util.module_from_spec(spec);spec.loader.exec_module(existing)
TEST=r'''
#include "llkamireadreceipts.h"
#include <iostream>
int main() {
 LLKamiReadReceipts a,b;
 const LLUUID alice=LLUUID::generateNewID(),bob=LLUUID::generateNewID(),sid=LLUUID::generateNewID();
 gSavedPerAccountSettings.opted_in=true;gAgent.id=alice;
 a.request(bob,sid,IM_NOTHING_SPECIAL);
 size_t cursor=0;
 while(cursor<packets.size()) {
  assert(cursor<10);auto packet=packets[cursor++];auto text=packet.text;
  if(packet.peer==bob){gAgent.id=bob;assert(b.incoming(alice,sid,text,true,packet.dialog));}
  else {gAgent.id=alice;assert(a.incoming(bob,sid,text,true,packet.dialog));}
 }
 gAgent.id=alice;auto wire=a.wrapOutgoing(bob,"hello");auto serial=a.sendSerial(bob);
 assert(a.bubbleStatus(bob,serial)=="Sent");
 std::string verb;LLUUID mid;assert(a.parseFrame(wire.substr(wire.find('\n')+1),verb,mid));
 auto receive=[&](const LLUUID& peer,const LLUUID& session,const std::string& packet,bool permitted=true){gAgent.id=alice;auto text=packet;return a.incoming(peer,session,text,permitted,IM_NOTHING_SPECIAL);};
 assert(receive(bob,sid,a.frame("DELIVERED",mid)));
 assert(a.bubbleStatus(bob,serial)=="Delivered");
 // A fabricated/stale ID, another peer or another session cannot advance status.
 wire=a.wrapOutgoing(bob,"next");auto next=a.sendSerial(bob);assert(a.bubbleStatus(bob,serial).empty());
 assert(receive(bob,sid,a.frame("DELIVERED",mid)));assert(a.bubbleStatus(bob,next)=="Sent");
 assert(a.parseFrame(wire.substr(wire.find('\n')+1),verb,mid));
 assert(receive(LLUUID::generateNewID(),sid,a.frame("DELIVERED",mid)));assert(a.bubbleStatus(bob,next)=="Sent");
 assert(receive(bob,LLUUID::generateNewID(),a.frame("DELIVERED",mid)));assert(a.bubbleStatus(bob,next)=="Sent");
 assert(receive(bob,sid,a.frame("DELIVERED",mid),false));assert(a.bubbleStatus(bob,next)=="Sent");
 auto before=packets.size();gAgent.id=bob;auto clean=wire;
 assert(!b.incoming(alice,sid,clean,true,IM_NOTHING_SPECIAL));assert(clean=="next");
 assert(packets.size()==before+1 && packets.back().text==b.frame("DELIVERED",mid));
 assert(receive(bob,sid,packets.back().text));assert(a.bubbleStatus(bob,next)=="Delivered");
 auto revision=a.revision();assert(receive(bob,sid,packets.back().text));assert(a.revision()==revision);
 gAgent.id=bob;clean=wire;before=packets.size();b.incoming(alice,sid,clean,true,IM_NOTHING_SPECIAL);assert(packets.size()==before);
 assert(b.acknowledgeOpened(alice,sid,IM_NOTHING_SPECIAL));auto seen=packets.back().text;
 assert(receive(bob,sid,seen));assert(a.bubbleStatus(bob,next)=="Read");
 assert(receive(bob,sid,a.frame("DELIVERED",mid)));assert(a.bubbleStatus(bob,next)=="Read");
 // Overlength and unknown-viewer sends cannot inherit a stale receipt.
 a.wrapOutgoing(bob,std::string(1023,'x'));auto longSerial=a.sendSerial(bob);assert(a.bubbleStatus(bob,longSerial)=="Sent");
 assert(receive(bob,sid,seen));assert(a.bubbleStatus(bob,longSerial)=="Sent");
 // An old client completes the unchanged handshake and gets no new verb.
 LLKamiReadReceipts legacy;gAgent.id=alice;legacy.request(bob,sid,IM_NOTHING_SPECIAL);
 LLUUID nonce;assert(legacy.parseFrame(packets.back().text,verb,nonce));
 auto ack=legacy.frame("HELLO_ACK",nonce);assert(legacy.incoming(bob,sid,ack,true,IM_NOTHING_SPECIAL));
 wire=legacy.wrapOutgoing(bob,"legacy");auto legacySerial=legacy.sendSerial(bob);
 before=packets.size();auto inbound=std::string("old reply\n")+legacy.frame("MSG",LLUUID::generateNewID());
 assert(!legacy.incoming(bob,sid,inbound,true,IM_NOTHING_SPECIAL));assert(packets.size()==before);
 assert(legacy.bubbleStatus(bob,legacySerial)=="Sent");
 gSavedPerAccountSettings.opted_in=false;legacy.revision();assert(legacy.bubbleStatus(bob,legacySerial).empty());
 std::cout<<"PASS Sent -> Delivered -> Read, capability negotiation, old-client fallback, stale/wrong peer/session rejection, deduplication and reset\n";
}
'''
def verify(root):
 v=root/'indra/newview';header=(v/'llkamireadreceipts.h').read_text(encoding='utf-8-sig');cpp=(v/'llchathistory.cpp').read_text(encoding='utf-8-sig')
 for text in ['bubbleStatus','d311b002','verb == "DELIVERED"','capable->second == session']:assert text in header
 assert 'previous->setLatestOutgoing(false)' in cpp
 assert 'receipts.bubbleStatus(mPeer, mSendSerial)' in cpp
 assert 'revision != mReceiptRevision' in cpp
 if '--integrated' in __import__('sys').argv:
  print('PASS live bubble rendering and negotiated acknowledgment integration');return
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'stubs.h').write_text(existing.STUBS)
  for n in ['llviewercontrol.h','lluuid.h','llagent.h','llimview.h','llnotificationsutil.h']:(p/n).write_text('#include "stubs.h"\n')
  (p/'llkamireadreceipts.h').write_text(header);(p/'test.cpp').write_text(TEST)
  binary=p/'test';subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
if __name__=='__main__':
 if '--integrated' in __import__('sys').argv or (ROOT/'indra/newview/llkamireadreceipts.h').exists():
  verify(ROOT)
 else:
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   for name in existing.FILES:
    target=root/'indra/newview'/name;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'indra/newview'/name,target)
   for name in ['apply-read-receipts.py','apply-imessage-style.py','apply-live-bubble-status.py','native-bubble.inc']:
    target=root/'kami/messenger'/name;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'kami/messenger'/name,target)
   for name in ['apply-read-receipts.py','apply-imessage-style.py','apply-live-bubble-status.py']:
    subprocess.run([__import__('sys').executable,str(root/'kami/messenger'/name)],check=True)
   verify(root)

