#!/usr/bin/env python3
"""Integrate KAMI-to-KAMI, explicitly opt-in IM read acknowledgements.

Second Life has no general read-receipt API. A short in-band marker is only
attached to a normal IM AFTER a Kami peer confirms support in a handshake.
The receiving Kami viewer strips it before history / alerts, and sends a
separate control IM only when the conversation is actually foregrounded.

Plain SL IM stays unchanged for unverified viewers, group chats, long texts,
muted users, and when the per-account opt-in is disabled. The visible "SEEN"
means recipient's Kami client opened the chat -- NOT proof a human read it.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
V = ROOT / "indra/newview"
XUI = V / "skins/default/xui/en"

def splice(path, old, new):
    s = path.read_text(encoding="utf-8-sig")
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f"KAMI Messenger receipts source drift {path}: {count} anchors {old[:88]!r}")
    path.write_text(s.replace(old, new, 1), encoding="utf-8")

settings = V / "app_settings/settings_per_account.xml"
setting = """    <key>KAMIReadReceiptsEnabled</key>
    <map>
      <key>Comment</key>
      <string>Opt-in per account: show and send client-acknowledged read receipts to supported Kami peers only. Off by default.</string>
      <key>Persist</key><integer>1</integer>
      <key>Type</key><string>Boolean</string>
      <key>Value</key><integer>0</integer>
    </map>
"""
splice(settings, "    <key>AvatarHoverOffsetZ</key>", setting + "    <key>AvatarHoverOffsetZ</key>")
ET.parse(settings)

protocol = r"""/**
 * @file llkamireadreceipts.h
 * KAMI Messenger read acknowledgement v1: opt-in viewer-to-viewer IM protocol.
 *
 * Transport remains Second Life IM. There is NO server read receipt API.
 * No online polling, web service, database, or device fingerprinting.
 * HELLO is sent only by an explicit button action, once per request.
 * MSG is tagged only after a HELLO_ACK matching the sent nonce.
 * SEEN means the recipient's Kami viewer displayed/focused that session.
 * Read receipts are NOT proof of human attention or delivery to other viewers.
 *
 * For unmodified clients the one-off HELLO is a plain textual protocol token;
 * a later switch to a different viewer may show an old message's MSG suffix.
 * This is why receipts default OFF and are restricted to consented P2P chats.
 */
#ifndef LL_KAMI_READ_RECEIPTS_H
#define LL_KAMI_READ_RECEIPTS_H

#include "lluuid.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llimview.h"

#include <map>
#include <string>

class LLKamiReadReceipts
{
public:
    static LLKamiReadReceipts& instance()
    {
        static LLKamiReadReceipts sInstance;
        return sInstance;
    }

    bool enabled() const
    {
        return gSavedPerAccountSettings.getBOOL("KAMIReadReceiptsEnabled");
    }

    U32 revision() const { return mRevision; }

    static std::string frame(const std::string& verb, const LLUUID& id)
    {
        return "[[KAMI-RR1|" + verb + "|" + id.asString() + "]]";
    }

    static bool parseFrame(const std::string& text, std::string& verb, LLUUID& id)
    {
        const std::string prefix("[[KAMI-RR1|");
        if (text.compare(0, prefix.size(), prefix) != 0 ||
            text.size() < prefix.size() + 4 + 36 + 3 ||
            text.compare(text.size() - 2, 2, "]]") != 0)
            return false;

        const size_t mid = text.find('|', prefix.size());
        if (mid == std::string::npos || mid + 1 + 36 + 2 != text.size())
            return false;

        const std::string token = text.substr(mid + 1, 36);
        if (!id.set(token, false) || id.isNull() || id.asString() != token)
            return false;

        verb = text.substr(prefix.size(), mid - prefix.size());
        return verb == "HELLO" || verb == "HELLO_ACK" ||
               verb == "MSG" || verb == "SEEN";
    }

    static bool isControl(const std::string& text)
    {
        std::string verb;
        LLUUID id;
        return parseFrame(text, verb, id) && verb != "MSG";
    }

    static std::string visibleText(const std::string& wire)
    {
        const std::string marker("\n[[KAMI-RR1|MSG|");
        const size_t at = wire.rfind(marker);
        if (at == std::string::npos || at == 0) return wire;

        std::string verb;
        LLUUID id;
        if (!parseFrame(wire.substr(at + 1), verb, id) || verb != "MSG")
            return wire;
        return wire.substr(0, at);
    }

    void request(const LLUUID& peer, const LLUUID& session, EInstantMessage transportDialog)
    {
        resetAccountIfNeeded();
        if (!enabled() || peer.isNull() || peer == gAgent.getID() ||
            session.isNull()) return;

        LLUUID nonce = LLUUID::generateNewID();
        mPendingHello[peer] = nonce;
        mVerified.erase(peer);
        mLastSent.erase(peer);
        mLastSeen.erase(peer);
        ++mRevision;

        LLIMModel::sendMessage(frame("HELLO", nonce),
                               session, peer, transportDialog);
    }

    std::string wrapOutgoing(const LLUUID& peer, const std::string& text)
    {
        resetAccountIfNeeded();
        if (!enabled() || mVerified.find(peer) == mVerified.end() ||
            text.empty()) return text;

        const LLUUID msg_id = LLUUID::generateNewID();
        const std::string suffix = "\n" + frame("MSG", msg_id);
        // Never truncate a normal IM just to add metadata.
        if (text.size() + suffix.size() > 1023) return text;

        mLastSent[peer] = msg_id;
        mLastSeen.erase(peer);
        ++mRevision;
        return text + suffix;
    }

    // Returns true for consumed control packets (never add these to SL chat).
    // For a normal tagged message, strips metadata IN PLACE and returns false.
    bool incoming(const LLUUID& peer, const LLUUID& session,
                  std::string& text, bool permitted, EInstantMessage transportDialog)
    {
        resetAccountIfNeeded();
        std::string verb;
        LLUUID id;
        if (parseFrame(text, verb, id) && verb != "MSG")
        {
            if (!enabled() || !permitted || peer.isNull() ||
                peer == gAgent.getID() || session.isNull())
                return true; // Never respond when not opted in or muted.

            if (verb == "HELLO")
            {
                // The peer explicitly requested a connection. The receiving
                // resident has opted in via their own per-account toggle.
                mVerified[peer] = true;
                ++mRevision;
                if (mLastInboundHello[peer] != id)
                {
                    mLastInboundHello[peer] = id;
                    LLIMModel::sendMessage(frame("HELLO_ACK", id),
                                           session, peer, transportDialog);
                }
            }
            else if (verb == "HELLO_ACK")
            {
                auto hello = mPendingHello.find(peer);
                if (hello != mPendingHello.end() && hello->second == id)
                {
                    mPendingHello.erase(hello);
                    mVerified[peer] = true;
                    ++mRevision;
                }
            }
            else if (verb == "SEEN")
            {
                auto sent = mLastSent.find(peer);
                if (sent != mLastSent.end() && sent->second == id &&
                    mVerified.find(peer) != mVerified.end())
                {
                    mLastSeen[peer] = id; // Validated match, not a guessed receipt.
                    ++mRevision;
                }
            }
            return true;
        }

        // Sanitization is useful even if this account turned receipts off
        // after receiving a tagged IM: never leak a known marker into history.
        const std::string clean = visibleText(text);
        if (clean == text) return false;
        text = clean;
        if (enabled() && permitted && mVerified.find(peer) != mVerified.end() &&
            !session.isNull())
        {
            const std::string marker("\n[[KAMI-RR1|MSG|");
            const size_t at = clean.size();
            if (at < 1023)
            {
                std::string op;
                LLUUID received_id;
                if (parseFrame(std::string(text.size() ? "" : "") +
                               "", op, received_id)) {} // intentionally no-op
            }
        }
        return false;
    }

    // Called when a P2P chat is foregrounded. Never send on mere arrival.
    bool acknowledgeOpened(const LLUUID& peer, const LLUUID& session, EInstantMessage transportDialog)
    {
        resetAccountIfNeeded();
        if (!enabled() || mVerified.find(peer) == mVerified.end()) return false;
        auto pending = mPendingRead.find(peer);
        if (pending == mPendingRead.end() || pending->second.session != session)
            return false;

        const LLUUID id = pending->second.id;
        mPendingRead.erase(pending); // exactly once, no repeat sending
        ++mRevision;
        LLIMModel::sendMessage(frame("SEEN", id),
                               session, peer, transportDialog);
        return true;
    }

    std::string status(const LLUUID& peer) const
    {
        if (!enabled()) return "Read receipts OFF";
        if (mVerified.find(peer) == mVerified.end())
        {
            if (mPendingHello.find(peer) != mPendingHello.end())
                return "Kami receipt invite sent";
            return "Request Kami receipts";
        }

        auto sent = mLastSent.find(peer);
        auto seen = mLastSeen.find(peer);
        if (sent != mLastSent.end() && seen != mLastSeen.end() &&
            sent->second == seen->second)
            return "SEEN (peer opened chat)";
        if (sent != mLastSent.end())
            return "Sent; awaiting Seen";
        return "Kami receipts READY";
    }

private:
    struct Pending
    {
        LLUUID session;
        LLUUID id;
    };

    void resetAccountIfNeeded()
    {
        const LLUUID account = gAgent.getID();
        if (account == mAccount) return;
        mAccount = account;
        mPendingHello.clear();
        mLastInboundHello.clear();
        mVerified.clear();
        mLastSent.clear();
        mLastSeen.clear();
        mPendingRead.clear();
        ++mRevision;
    }

    LLUUID mAccount;
    U32 mRevision = 0;
    std::map<LLUUID, LLUUID> mPendingHello;
    std::map<LLUUID, LLUUID> mLastInboundHello;
    std::map<LLUUID, bool> mVerified;
    std::map<LLUUID, LLUUID> mLastSent;
    std::map<LLUUID, LLUUID> mLastSeen;
    std::map<LLUUID, Pending> mPendingRead;
};

#endif // LL_KAMI_READ_RECEIPTS_H
"""
# Correctly retain exact wire marker for opened acknowledgment, and do not
# accidentally call any packet parser on the stripped visible message.
old = r"""        const std::string clean = visibleText(text);
        if (clean == text) return false;
        text = clean;
        if (enabled() && permitted && mVerified.find(peer) != mVerified.end() &&
            !session.isNull())
        {
            const std::string marker("\n[[KAMI-RR1|MSG|");
            const size_t at = clean.size();
            if (at < 1023)
            {
                std::string op;
                LLUUID received_id;
                if (parseFrame(std::string(text.size() ? "" : "") +
                               "", op, received_id)) {} // intentionally no-op
            }
        }
        return false;"""
new = r"""        const std::string clean = visibleText(text);
        if (clean == text) return false;
        const std::string wire_marker = text.substr(clean.size() + 1);
        text = clean;
        if (enabled() && permitted && mVerified.find(peer) != mVerified.end() &&
            !session.isNull())
        {
            std::string op;
            LLUUID received_id;
            if (parseFrame(wire_marker, op, received_id) && op == "MSG")
            {
                mPendingRead[peer] = {session, received_id};
                ++mRevision;
            }
        }
        return false;"""
if protocol.count(old)!=1:
    raise Error("Protocol marker replacement mismatch");
protocol=protocol.replace(old,new);
write_path = V / "llkamireadreceipts.h"
if write_path.exists():
    raise RuntimeError("KAMI read-receipt header already exists; refusing duplicate")
write_path.write_text(protocol, encoding="utf-8")

# Intercept P2P packets at the *central* incoming dispatch BEFORE local
# notification/history/auto-DND so control frames cannot create ghost chats.
im_proc = V / "llimprocessing.cpp"
splice(im_proc, '#include "llimview.h"', '#include "llimview.h"\n#include "llkamireadreceipts.h"')
splice(im_proc, "    chat.mMuted = is_muted;\n",
"""    // KAMI read receipts are P2P only; never touch group/object/system IM.
    // Server-authored from_id is used as peer identity. Mutes block replies.
    // Modern WebRTC P2P chats use IM_SESSION_SEND, the same outer dialog
    // as groups. Require an existing typed P2P session before touching it.
    const LLIMModel::LLIMSession* kami_session =
        LLIMModel::getInstance()->findIMSession(session_id);
    const bool kami_direct_im =
        dialog == IM_NOTHING_SPECIAL ||
        (dialog == IM_SESSION_SEND && kami_session &&
         kami_session->isP2PSessionType());
    if (kami_direct_im && !from_group &&
        from_id.notNull() && to_id == gAgent.getID())
    {
        if (LLKamiReadReceipts::instance().incoming(
                from_id, session_id, message, !is_muted, dialog))
            return;
    }

    chat.mMuted = is_muted;
""")

# Preserve ordinary IM plaintext in our OWN history, even when wire metadata
# is appended. The control frame should have no local chat bubble at all.
model = V / "llimview.cpp"
splice(model, '#include "llimview.h"', '#include "llimview.h"\n#include "llkamireadreceipts.h"')
splice(model, """        gAgent.sendReliableMessage();
    }

    bool is_group_chat = false;""",
"""        gAgent.sendReliableMessage();
    }

    // Control frames are sent as normal IMs but have no local chat echo.
    if (LLKamiReadReceipts::isControl(utf8_text)) return;
    const std::string visible_text = LLKamiReadReceipts::visibleText(utf8_text);

    bool is_group_chat = false;""")
splice(model, "LLIMModel::getInstance()->addMessage(im_session_id, user.getDisplayName(), gAgentID, utf8_text);",
             "LLIMModel::getInstance()->addMessage(im_session_id, user.getDisplayName(), gAgentID, visible_text);")
splice(model, 'history_echo += ": " + utf8_text;', 'history_echo += ": " + visible_text;')

floater = V / "llfloaterimsession.cpp"
splice(floater, '#include "llfloaterimsession.h"',
       '#include "llfloaterimsession.h"\n#include "llkamireadreceipts.h"\n#include "llcheckboxctrl.h"')
splice(floater,
"""    if (mSessionInitialized)
    {
        LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog);
    }""",
"""    if (mSessionInitialized)
    {
        const std::string wire = mIsP2PChat ?
            LLKamiReadReceipts::instance().wrapOutgoing(mOtherParticipantUUID, utf8_text) :
            utf8_text;
        LLIMModel::sendMessage(wire, mSessionID, mOtherParticipantUUID, mDialog);
    }""")

# Acknowledge only on actual foreground-focus, never just arrival.
splice(floater,
"""void LLFloaterIMSession::setFocus(bool focus)
{
    LLFloaterIMSessionTab::setFocus(focus);
""",
"""void LLFloaterIMSession::setFocus(bool focus)
{
    LLFloaterIMSessionTab::setFocus(focus);
    if (focus && getVisible() && mIsP2PChat)
        LLKamiReadReceipts::instance().acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);
""")
splice(floater,
"""    if (visible && isInVisibleChain())
    {
        sIMFloaterShowedSignal(mSessionID);
        updateMessages();
    }""",
"""    if (visible && isInVisibleChain())
    {
        sIMFloaterShowedSignal(mSessionID);
        updateMessages();
        if (hasFocus() && mIsP2PChat)
            LLKamiReadReceipts::instance().acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);
    }""")
splice(floater,
"""void LLFloaterIMSession::draw()
{
    // add people who were added via dropPerson()""",
"""void LLFloaterIMSession::draw()
{
    LLKamiReadReceipts& rr = LLKamiReadReceipts::instance();
    // A cheap generation comparison; no IM traffic or scanning per frame.
    if (mKAMIRRLastRevision != rr.revision() ||
        mKAMIRRLastEnabled != rr.enabled())
    {
        mKAMIRRLastRevision = rr.revision();
        mKAMIRRLastEnabled = rr.enabled();
        if (getVisible() && hasFocus() && mIsP2PChat)
            rr.acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);
        updateKAMIMessengerStatus();
    }

    // add people who were added via dropPerson()""")
# Provide explicit action in native UI. No unsolicited probes.
splice(floater, "//virtual\nbool LLFloaterIMSession::postBuild()",
"""void LLFloaterIMSession::onKAMIRequestReceipts()
{
    if (!mIsP2PChat || !mSessionInitialized ||
        mOtherParticipantUUID.isNull())
        return;
    LLKamiReadReceipts::instance().request(mOtherParticipantUUID, mSessionID, mDialog);
    updateKAMIMessengerStatus();
}

//virtual
bool LLFloaterIMSession::postBuild()""")
splice(floater,
"""    mInputEditor->setCommitCallback(boost::bind(onSendMsg, _1, this));

    setDocked(true);""",
"""    mInputEditor->setCommitCallback(boost::bind(onSendMsg, _1, this));

    // Per-account opt-in, OFF by default. The peer must opt in separately.
    LLCheckBoxCtrl* receipt_optin = getChild<LLCheckBoxCtrl>("kami_read_receipts_optin");
    receipt_optin->setValue(LLKamiReadReceipts::instance().enabled());
    receipt_optin->setCommitCallback([this](LLUICtrl*, const LLSD& value)
    {
        gSavedPerAccountSettings.setBOOL("KAMIReadReceiptsEnabled", value.asBoolean());
        updateKAMIMessengerStatus();
    });
    LLButton* receipt_request = getChild<LLButton>("kami_read_receipts_request");
    receipt_request->setEnabled(mIsP2PChat);
    receipt_request->setClickedCallback(boost::bind(&LLFloaterIMSession::onKAMIRequestReceipts, this));

    setDocked(true);""")
oldstatus = """void LLFloaterIMSession::updateKAMIMessengerStatus()
{
    LLTextBox* status = findChild<LLTextBox>("kami_messenger_status_text", true);
    if (!status) return;

    if (mKAMIMessengerTimedOut)
    {
        status->setText("TIMED OUT | SL did not confirm this session. Check before resending.");
    }
    else if (mSessionInitialized)
    {
        status->setText("SESSION READY | SL session initialized. Delivery/read status unavailable.");
    }
    else if (mQueuedMsgsForInit.size() > 0)
    {
        status->setText(llformat("CONNECTING | %d message(s) queued locally, not yet sent.",
                                 (S32)mQueuedMsgsForInit.size()));
    }
    else
    {
        status->setText("CONNECTING | Waiting for Second Life to initialize this conversation.");
    }
}
"""
newstatus = """void LLFloaterIMSession::updateKAMIMessengerStatus()
{
    LLTextBox* status = findChild<LLTextBox>("kami_messenger_status_text", true);
    if (!status) return;

    std::string label;
    if (mKAMIMessengerTimedOut)
        label = "TIMED OUT | SL did not confirm this IM session";
    else if (!mSessionInitialized)
        label = mQueuedMsgsForInit.size() > 0 ?
            llformat("CONNECTING | %d queued locally, not sent", (S32)mQueuedMsgsForInit.size()) :
            "CONNECTING | Waiting for Second Life";
    else
        label = "SL SESSION READY | Not a delivery confirmation";

    if (mIsP2PChat)
        label += " | " + LLKamiReadReceipts::instance().status(mOtherParticipantUUID);
    status->setText(label);
}
"""
splice(floater, oldstatus,newstatus)

fh = V / "llfloaterimsession.h"
splice(fh,
"""    void setKAMIMessengerTimedOut();
""",
"""    void setKAMIMessengerTimedOut();
    void onKAMIRequestReceipts();
""")
splice(fh,
"""    bool mKAMIMessengerTimedOut = false;
    LLSD mQueuedMsgsForInit;""",
"""    bool mKAMIMessengerTimedOut = false;
    U32 mKAMIRRLastRevision = 0;
    bool mKAMIRRLastEnabled = false;
    LLSD mQueuedMsgsForInit;""")

session_xml = XUI / "floater_im_session.xml"
xtree=ET.parse(session_xml)
xroot=xtree.getroot()
panels=[e for e in xroot.iter("layout_panel") if e.get("name")=="kami_messenger_status_panel"]
assert len(panels)==1
panel=panels[0]
assert not any(e.get("name")=="kami_read_receipts_optin" for e in xroot.iter())
panel.set("height","52")
ET.SubElement(panel,"check_box",{
    "name":"kami_read_receipts_optin",
    "layout":"topleft","follows":"left|top",
    "label":"Allow Kami read receipts",
    "height":"20","width":"206","left":"8","top":"27",
    "tool_tip":"Off by default. Each resident opts in separately. Only Kami-to-Kami P2P chats."
})
ET.SubElement(panel,"button",{
    "name":"kami_read_receipts_request",
    "label":"Request receipts",
    "layout":"topleft","follows":"right|top",
    "height":"22","width":"142","right":"-8","top":"25",
    "tool_tip":"Send a single compatibility invitation to this resident. Non-Kami viewers may show the invitation as ordinary text."
})
ET.indent(xtree,space=" ")
xtree.write(session_xml,encoding="utf-8",xml_declaration=True)
ET.parse(session_xml)
print("PASS: KAMI-to-KAMI opt-in receipts integrated into native P2P SL messaging.")
print("Limitations: requires two Kami clients, active link, and an opened chat; marker may be visible in a different viewer.")
