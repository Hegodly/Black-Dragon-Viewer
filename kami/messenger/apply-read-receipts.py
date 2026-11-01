#!/usr/bin/env python3
"""KAMI-to-KAMI beta: opt-in, per-chat read acknowledgments over existing SL IM.

This protocol does NOT produce read receipts from other viewers. It sends an
ordinary, legible invitation only when a user explicitly opts in, and sends
private-format read acknowledgments only after a reciprocal invitation has been
seen from that same avatar. Read is observed when the recipient actually views
the conversation. No account-wide tracking, no server upload, no read inference
from typing or session establishment. No out-of-band service required.

Limitations: consent lives only in viewer memory and must be renewed after
relogin. Identical repeated message text is ambiguous (digest does not include
a server message ID). A peer switching viewers may see the receipt protocol IM
as text. This feature must remain off until both peers explicitly enable it.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
V=ROOT/"indra/newview"

def replace_once(path, old, new):
    s=path.read_text(encoding="utf-8-sig")
    n=s.count(old)
    if n!=1: raise RuntimeError(f"KAMI receipts anchor drift {path}: {n} for {old[:100]!r}")
    path.write_text(s.replace(old,new,1),encoding="utf-8")

im_h=V/"llimview.h"
expose=r"""
// Optional, per-conversation KAMI-to-KAMI receipt handshake.
// No Second Life read acknowledgment is claimed or synthesized.
namespace KAMIReadReceipts
{
    bool observe(const LLUUID& session, const LLUUID& peer, const std::string& text);
    bool enabled(const LLUUID& session, const LLUUID& peer);
    void toggle(const LLUUID& session, const LLUUID& peer);
    void outgoing(const LLUUID& session, const LLUUID& peer, const std::string& text);
    void viewed(const LLUUID& session, const LLUUID& peer);
    std::string label(const LLUUID& session, const LLUUID& peer);
}
"""
replace_once(im_h, "class LLIMModel :",expose+"\nclass LLIMModel :")
im_cpp=V/"llimview.cpp"
source=r"""
// KAMI-to-KAMI read receipts beta: explicit two-person consent.
// The opt-in invite is a readable ordinary IM; ordinary viewers can ignore it.
// Receipt control messages are only emitted after BOTH have sent the invite.
// A control message might still be visible if someone switches viewers after
// consenting. NEVER hide content from any other viewer.
namespace KAMIReadReceipts
{
    static const std::string kInvite =
        "KAMI Messenger: I opt in to read receipts for our conversation. "
        "If you use KAMI, click Enable Receipts in this chat to opt in too.";
    static const std::string kAckPrefix = "[KAMI_READ_V1] ";

    struct State
    {
        LLUUID peer;
        bool local = false;
        bool peer_invited = false;
        std::string inbound_digest;
        std::string last_acked_digest;
        std::string outgoing_digest;
        bool read = false;
    };
    static std::map<LLUUID, State> sSessions;

    static State* getState(const LLUUID& session, const LLUUID& peer, bool create)
    {
        if (session.isNull() || peer.isNull() || peer == gAgentID)
            return nullptr;
        auto it = sSessions.find(session);
        if (it != sSessions.end())
        {
            return it->second.peer == peer ? &it->second : nullptr;
        }
        if (!create || sSessions.size() >= 128) return nullptr;
        State& state = sSessions[session];
        state.peer = peer;
        return &state;
    }

    static std::string digest(const std::string& msg)
    {
        // Deterministic UUID digest of exact message text in this session.
        // The grid has no user-visible stable unique IM message ID.
        return LLUUID::generateNewID("KAMI_READ_V1:" + msg).asString();
    }

    bool enabled(const LLUUID& session, const LLUUID& peer)
    {
        State* state = getState(session, peer, false);
        return state && state->local;
    }

    void toggle(const LLUUID& session, const LLUUID& peer)
    {
        State* state = getState(session, peer, true);
        if (!state) return;
        state->local = !state->local;
        state->inbound_digest.clear();
        state->last_acked_digest.clear();
        state->outgoing_digest.clear();
        state->read = false;
        if (state->local)
        {
            // The user must knowingly initiate this visible invitation. Do
            // not automatically probe strangers or other viewers.
            LLIMModel::sendMessage(kInvite, session, peer, IM_NOTHING_SPECIAL);
        }
    }

    void outgoing(const LLUUID& session, const LLUUID& peer, const std::string& text)
    {
        State* state = getState(session, peer, false);
        if (!state || !state->local || !state->peer_invited ||
            text == kInvite || text.compare(0, kAckPrefix.size(), kAckPrefix) == 0)
            return;
        state->outgoing_digest = digest(text);
        state->read = false;
    }

    // Consume protocol ACK before it reaches normal chat/history/toasts.
    // The sender must have explicitly opted in and observed a reciprocal
    // invitation from the *same avatar* before accepting anything.
    bool observe(const LLUUID& session, const LLUUID& peer, const std::string& text)
    {
        State* state = getState(session, peer, true);
        if (!state) return false;
        if (text == kInvite)
        {
            state->peer_invited = true;
            return false; // Consent invitation is deliberately visible.
        }
        if (text.compare(0, kAckPrefix.size(), kAckPrefix) == 0)
        {
            if (!(state->local && state->peer_invited))
                return false;
            const std::string claimed = text.substr(kAckPrefix.size());
            const bool valid = claimed.size() == 36 &&
                LLUUID(claimed).notNull();
            if (valid && !state->outgoing_digest.empty() &&
                claimed == state->outgoing_digest)
            {
                state->read = true;
                if (LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session))
                    floater->updateKAMIMessengerStatus();
            }
            // We recognize control packets only from the opted-in partner.
            return valid;
        }
        if (state->local && state->peer_invited && !text.empty())
        {
            state->inbound_digest = digest(text);
        }
        return false;
    }

    void viewed(const LLUUID& session, const LLUUID& peer)
    {
        State* state = getState(session, peer, false);
        if (!state || !state->local || !state->peer_invited ||
            state->inbound_digest.empty() ||
            state->last_acked_digest == state->inbound_digest)
            return;
        state->last_acked_digest = state->inbound_digest;
        LLIMModel::sendMessage(kAckPrefix + state->inbound_digest,
                               session, peer, IM_NOTHING_SPECIAL);
    }

    std::string label(const LLUUID& session, const LLUUID& peer)
    {
        const State* state = getState(session, peer, false);
        if (!state || !state->local) return "Receipts: Off";
        if (!state->peer_invited) return "Receipts: Waiting for peer";
        if (!state->outgoing_digest.empty())
            return state->read ? "READ (KAMI confirmed)" : "Read: Awaiting view";
        return "Receipts: On (both opted in)";
    }
}
"""
replace_once(im_cpp, "// Add a message to a session.\nvoid LLIMMgr::addMessage(",source+"\n// Add a message to a session.\nvoid LLIMMgr::addMessage(")
# Guard early against popups from ordinary P2P receipt packets.
replace_once(im_cpp,
"""    if (new_session_id.isNull())
    {
        //no session ID...compute new one
        new_session_id = computeSessionID(dialog, other_participant_id);
    }

    //*NOTE session_name is empty in case of incoming P2P sessions""",
"""    if (new_session_id.isNull())
    {
        //no session ID...compute new one
        new_session_id = computeSessionID(dialog, other_participant_id);
    }
    if (dialog == IM_NOTHING_SPECIAL &&
        msg.compare(0, 15, "[KAMI_READ_V1] ") == 0 &&
        KAMIReadReceipts::observe(new_session_id, other_participant_id, msg))
        return; // Do not log, notify or display a recognized peer ACK.

    //*NOTE session_name is empty in case of incoming P2P sessions""")
# Central model catches IMs delivered by paths other than the legacy UDP
# dispatcher and observes original message text without any new network hooks.
replace_once(im_cpp,
"""void LLIMModel::addMessage(const LLUUID& session_id, const std::string& from, const LLUUID& from_id,
                           const std::string& utf8_text, bool log2file /* = true */, bool is_region_msg, /* = false */ U32 time_stamp /* = 0 */)
{
    if (gSavedSettings.getBOOL("TranslateChat")""",
"""void LLIMModel::addMessage(const LLUUID& session_id, const std::string& from, const LLUUID& from_id,
                           const std::string& utf8_text, bool log2file /* = true */, bool is_region_msg, /* = false */ U32 time_stamp /* = 0 */)
{
    LLIMSession* kami_session = findIMSession(session_id);
    if (kami_session && kami_session->isP2PSessionType() && !is_region_msg &&
        from_id.notNull() && from_id != gAgentID &&
        kami_session->mOtherParticipantID == from_id &&
        KAMIReadReceipts::observe(session_id, from_id, utf8_text))
        return; // In-band receipt never reaches translation, history or notifications.
    if (gSavedSettings.getBOOL("TranslateChat")""")
# Close no gap in outgoing path; send only regular IMs, not queued/unsent.
session_cpp=V/"llfloaterimsession.cpp"
replace_once(session_cpp,
"""    if (mSessionInitialized)
    {
        LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog);
    }""",
"""    if (mSessionInitialized)
    {
        if (mIsP2PChat && mDialog == IM_NOTHING_SPECIAL)
            KAMIReadReceipts::outgoing(mSessionID, mOtherParticipantUUID, utf8_text);
        LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog);
    }""")
replace_once(session_cpp,
"""            LLIMModel::sendMessage(iter->asString(), mSessionID,
                mOtherParticipantUUID, mDialog);""",
"""            if (mIsP2PChat && mDialog == IM_NOTHING_SPECIAL)
                KAMIReadReceipts::outgoing(mSessionID, mOtherParticipantUUID, iter->asString());
            LLIMModel::sendMessage(iter->asString(), mSessionID,
                mOtherParticipantUUID, mDialog);""")
# Receipt button is only actionable for a P2P, initialized session; activating
# gives a visible invitation, and partner must opt in separately on their side.
session_h=V/"llfloaterimsession.h"
replace_once(session_h,"    void updateKAMIMessengerStatus();",
             "    void updateKAMIMessengerStatus();\n    void toggleKAMIReadReceipts();\n    void maybeAcknowledgeKAMIRead();")
read_methods=r"""
void LLFloaterIMSession::toggleKAMIReadReceipts()
{
    if (!mSessionInitialized || !mIsP2PChat ||
        mDialog != IM_NOTHING_SPECIAL || mOtherParticipantUUID.isNull())
    {
        LLSD args;
        args["MESSAGE"] = "KAMI receipts require an initialized one-to-one SL conversation. Group chats and disconnected sessions are not supported.";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }
    KAMIReadReceipts::toggle(mSessionID, mOtherParticipantUUID);
    updateKAMIMessengerStatus();
}

void LLFloaterIMSession::maybeAcknowledgeKAMIRead()
{
    if (!mSessionInitialized || !mIsP2PChat || mDialog != IM_NOTHING_SPECIAL ||
        mOtherParticipantUUID.isNull() || !isInVisibleChain())
        return;
    LLFloaterIMContainer* box = LLFloaterIMContainer::findInstance();
    const bool active = hasFocus() ||
        (box && box->getVisible() && box->hasFocus() &&
         box->getSelectedSession() == mSessionID);
    if (active)
        KAMIReadReceipts::viewed(mSessionID, mOtherParticipantUUID);
}

"""
replace_once(session_cpp, "// KAMI Messenger reports SL session initialization, never delivery/read.\nvoid LLFloaterIMSession::updateKAMIMessengerStatus()",
             read_methods+"// KAMI Messenger reports SL session initialization and opt-in KAMI peer receipts.\nvoid LLFloaterIMSession::updateKAMIMessengerStatus()")
# Keep separate connection statuses, display receipt state in own label.
replace_once(session_cpp,
"""    if (!status) return;

    if (mKAMIMessengerTimedOut)""",
"""    if (!status) return;

    if (LLButton* receipts = findChild<LLButton>("kami_messenger_receipts_btn", true))
    {
        receipts->setLabel(KAMIReadReceipts::enabled(mSessionID, mOtherParticipantUUID)
                           ? "Receipts: ON" : "Enable Receipts");
        receipts->setEnabled(mSessionInitialized && mIsP2PChat);
    }

    if (mKAMIMessengerTimedOut)""")
replace_once(session_cpp,
"""        status->setText("SESSION READY | SL session initialized. Delivery/read status unavailable.");""",
"""        status->setText("SESSION READY | " +
            KAMIReadReceipts::label(mSessionID, mOtherParticipantUUID));""")
replace_once(session_cpp,
"""    mInputEditor->setCommitCallback(boost::bind(onSendMsg, _1, this));

    setDocked(true);""",
"""    mInputEditor->setCommitCallback(boost::bind(onSendMsg, _1, this));
    getChild<LLButton>("kami_messenger_receipts_btn")->setClickedCallback(
        boost::bind(&LLFloaterIMSession::toggleKAMIReadReceipts, this));

    setDocked(true);""")
replace_once(session_cpp,
"""    else
    {
        LLFloaterReg::hideInstance("voice_call", mSessionID);
    }
}

void LLFloaterIMSession::onSendMsg(""",
"""    else
    {
        LLFloaterReg::hideInstance("voice_call", mSessionID);
    }
    if (visible)
        maybeAcknowledgeKAMIRead();
}

void LLFloaterIMSession::onSendMsg(""")
replace_once(session_cpp,
"""    // should not reset unread message counters if IM floater doesn't have focus""",
"""    // should not reset unread message counters if IM floater doesn't have focus""") if False else None
replace_once(session_cpp,
"""    }
}

void LLFloaterIMSession::reloadMessages(bool clean_messages""",
"""    }
    // Only acknowledge after message history has been appended and the
    // conversation is actively visible/focused. Not on mere network receipt.
    maybeAcknowledgeKAMIRead();
    updateKAMIMessengerStatus();
}

void LLFloaterIMSession::reloadMessages(bool clean_messages""")
replace_once(session_cpp,
"""    if( im_session && im_session->mTextIMPossible && !self->mInputEditor->getReadOnly())
    {
        //in disconnected state IM input editor should be disabled
        self->mInputEditor->setEnabled(!gDisconnected);
    }
}

// static
void LLFloaterIMSession::onInputEditorFocusLost""",
"""    if( im_session && im_session->mTextIMPossible && !self->mInputEditor->getReadOnly())
    {
        //in disconnected state IM input editor should be disabled
        self->mInputEditor->setEnabled(!gDisconnected);
    }
    self->maybeAcknowledgeKAMIRead();
}

// static
void LLFloaterIMSession::onInputEditorFocusLost""")
# Distinct visual button next to status. Do not alter existing chat/voice UI.
ui=V/"skins/default/xui/en/floater_im_session.xml"
replace_once(ui,
"""                 right="-8"
                 top="2"
                 height="18"
                 font="SansSerifSmall"
                 text_color="0.68 0.86 1 1"
                 mouse_opaque="false">CONNECTING | Waiting for Second Life to initialize this conversation.</text>""",
"""                 right="-146"
                 top="2"
                 height="18"
                 font="SansSerifSmall"
                 text_color="0.68 0.86 1 1"
                 mouse_opaque="false">CONNECTING | Waiting for Second Life to initialize this conversation.</text>
                <button
                 name="kami_messenger_receipts_btn"
                 layout="topleft"
                 follows="right|top"
                 right="-4"
                 top="1"
                 height="20"
                 width="137"
                 label="Enable Receipts"
                 tool_tip="Opt in for this session only. Sends a visible consent IM. Both people must opt in using KAMI for read acknowledgments."/>""")
ET.parse(ui)

print("Installed opt-in KAMI-to-KAMI read acknowledgments: per-session consent, incoming digest matching, only viewed-focused ACK, original SL transport.")
