#!/usr/bin/env python3
"""Add negotiated delivery acknowledgments and live latest-bubble status.

Run after apply-imessage-style.py. Existing RR1 frames stay compatible.
HELLO nonces with a reserved prefix advertise delivery support; old clients
can echo them normally without receiving an unsupported delivery verb.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];V=ROOT/'indra/newview'
def splice(path,old,new):
 s=path.read_text(encoding='utf-8-sig');assert s.count(old)==1,(path,old[:90]);path.write_text(s.replace(old,new,1),encoding='utf-8')
h=V/'llkamireadreceipts.h'
splice(h,'               verb == "MSG" || verb == "SEEN";',
          '               verb == "MSG" || verb == "SEEN" || verb == "DELIVERED";')
splice(h,'    void request(const LLUUID& peer, const LLUUID& session, EInstantMessage transportDialog)',
'''    // A canonical UUID retains 96 random bits and advertises this extension.
    static LLUUID deliveryNonce()
    {
        std::string token = LLUUID::generateNewID().asString();
        token.replace(0, 8, "d311b002");
        return LLUUID(token);
    }
    U32 sendSerial(const LLUUID& peer)
    {
        resetAccountIfNeeded();
        auto it = mSendSerial.find(peer);
        return it == mSendSerial.end() ? 0 : it->second;
    }
    std::string bubbleStatus(const LLUUID& peer, U32 serial)
    {
        resetAccountIfNeeded();
        auto generation = mSendSerial.find(peer);
        if (!serial || generation == mSendSerial.end() || generation->second != serial)
            return "";
        auto sent = mLastSent.find(peer);
        if (sent == mLastSent.end()) return "Sent";
        auto read = mLastSeen.find(peer);
        if (read != mLastSeen.end() && read->second == sent->second) return "Read";
        auto delivered = mDelivered.find(peer);
        if (delivered != mDelivered.end() && delivered->second == sent->second) return "Delivered";
        return "Sent";
    }
    void request(const LLUUID& peer, const LLUUID& session, EInstantMessage transportDialog)''')
splice(h,'        LLUUID nonce = LLUUID::generateNewID();',
'''        LLUUID nonce = deliveryNonce();
        mAdvertised[peer] = session;
        mDeliveryPeers.erase(peer);
        mDelivered.erase(peer);''')
splice(h,'        mHasSent[peer] = true;',
'''        if (text.empty()) return text;
        mSendSerial[peer] = ++mSendCounter;
        mDelivered.erase(peer);
        mHasSent[peer] = true;''')
splice(h,'            if (verb == "HELLO")\n            {',
'''            if (verb == "HELLO")
            {
                if (id.asString().compare(0, 8, "d311b002") == 0)
                    mDeliveryPeers[peer] = session;
                // Reciprocal HELLO advertises capability without exposing new
                // verbs to old Kami clients. One advertisement per session.
                auto advertised = mAdvertised.find(peer);
                if (advertised == mAdvertised.end() || advertised->second != session)
                {
                    mAdvertised[peer] = session;
                    const LLUUID nonce = deliveryNonce();
                    mPendingHello[peer] = nonce;
                    LLIMModel::sendMessage(frame("HELLO", nonce), session, peer, transportDialog);
                }''')
splice(h,'            else if (verb == "SEEN")',
'''            else if (verb == "DELIVERED")
            {
                auto sent = mLastSent.find(peer);
                auto capable = mDeliveryPeers.find(peer);
                if (sent != mLastSent.end() && sent->second == id &&
                    mVerified.find(peer) != mVerified.end() &&
                    capable != mDeliveryPeers.end() && capable->second == session &&
                    (mDelivered.find(peer) == mDelivered.end() || mDelivered[peer] != id))
                {
                    mDelivered[peer] = id;
                    ++mRevision;
                }
            }
            else if (verb == "SEEN")''')
splice(h,'                    mPendingRead[peer] = {session, received_id};',
'''                    auto capable = mDeliveryPeers.find(peer);
                    if (capable != mDeliveryPeers.end() && capable->second == session &&
                        (mLastDelivered.find(peer) == mLastDelivered.end() || mLastDelivered[peer] != received_id))
                    {
                        mLastDelivered[peer] = received_id;
                        LLIMModel::sendMessage(frame("DELIVERED", received_id), session, peer, transportDialog);
                    }
                    mPendingRead[peer] = {session, received_id};''')
splice(h,'        mLastAcknowledged.clear();',
'''        mLastAcknowledged.clear();
        mDeliveryPeers.clear(); mAdvertised.clear(); mDelivered.clear();
        mLastDelivered.clear(); mSendSerial.clear();''')
splice(h,'    std::map<LLUUID, bool> mHasSent;',
'''    std::map<LLUUID, bool> mHasSent;
    U32 mSendCounter = 0;
    std::map<LLUUID, U32> mSendSerial;
    std::map<LLUUID, LLUUID> mDeliveryPeers, mAdvertised, mDelivered, mLastDelivered;''')
# Bubble widgets read the same live protocol state, never infer receipt from focus.
c=V/'llchathistory.cpp'
splice(c,'#include "llui.h"','#include "llui.h"\n#include "llkamireadreceipts.h"')
splice(c,'        mMeta->setText(label);',
'''        mMetaLabel = label;
        mMeta->setText(label);
        if (outgoing && chat.mChatStyle != CHAT_STYLE_HISTORY && chat.mSessionID.notNull())
        {
            mPeer = LLIMModel::getInstance()->getOtherParticipantID(chat.mSessionID);
            mSendSerial = LLKamiReadReceipts::instance().sendSerial(mPeer);
        }''')
splice(c,'    // Called on append and transcript resize, not in draw().',
'''    void setLatestOutgoing(bool latest)
    {
        mLatestOutgoing = latest;
        mStatusDirty = true;
        if (!latest) mMeta->setText(mMetaLabel);
    }
    void draw() override
    {
        if (mLatestOutgoing && mSendSerial)
        {
            auto& receipts = LLKamiReadReceipts::instance();
            U32 revision = receipts.revision();
            if (mStatusDirty || revision != mReceiptRevision)
            {
                mReceiptRevision = revision;
                mStatusDirty = false;
                const std::string state = receipts.bubbleStatus(mPeer, mSendSerial);
                // Right-aligned metadata is directly underneath the bubble.
                mMeta->setText(state.empty() ? mMetaLabel : state);
            }
        }
        LLPanel::draw();
    }
    // Called on append and transcript resize, not in draw().''')
splice(c,'    bool mOutgoing;',
'''    bool mOutgoing;
    bool mLatestOutgoing = false, mStatusDirty = true;
    U32 mSendSerial = 0, mReceiptRevision = 0;
    LLUUID mPeer;
    std::string mMetaLabel;''')
splice(c,'        bubble->fit(llmax(80, mEditor->getRect().getWidth() - 40));',
'''        if (from_me && chat.mChatStyle != CHAT_STYLE_HISTORY)
        {
            if (auto* previous = dynamic_cast<LLKamiMessageBubble*>(mKamiLatestOutgoing.get()))
                previous->setLatestOutgoing(false);
            bubble->setLatestOutgoing(true);
            mKamiLatestOutgoing = bubble->getHandle();
        }
        bubble->fit(llmax(80, mEditor->getRect().getWidth() - 40));''')
splice(c,'    mKamiBubbles.clear();','    mKamiBubbles.clear();\n    mKamiLatestOutgoing = LLHandle<LLView>();')
splice(V/'llchathistory.h','        std::vector<LLHandle<LLView>> mKamiBubbles;',
 '        std::vector<LLHandle<LLView>> mKamiBubbles;\n        LLHandle<LLView> mKamiLatestOutgoing;')
print('PASS live Sent / Delivered / Read bubble labels and negotiated delivery acknowledgments')
