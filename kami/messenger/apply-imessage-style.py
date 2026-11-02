#!/usr/bin/env python3
"""Finish native Kami Messenger presentation and validated receipt notifications.

Run after apply-read-receipts.py. Uses existing SL IM transport only; a
notification is emitted on a matching peer acknowledgement, never local focus.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
V = ROOT / "indra/newview"
XUI = V / "skins/default/xui/en"


def splice(path, before, after):
    text = path.read_text(encoding="utf-8-sig")
    if text.count(before) != 1:
        raise RuntimeError(f"Kami Messenger source drift: {path}: {before[:80]!r}")
    path.write_text(text.replace(before, after, 1), encoding="utf-8")


# Reset protocol state on opt-out as well as account changes. Old receipts
# must never become valid again after disabling and re-enabling the feature.
h = V / "llkamireadreceipts.h"
splice(h, '#include "llimview.h"',
       '#include "llimview.h"\n#include "llnotificationsutil.h"')
splice(h, "    U32 revision() const { return mRevision; }",
       "    U32 revision() { resetAccountIfNeeded(); return mRevision; }")
splice(h, "    // Called when a P2P chat is foregrounded. Never send on mere arrival.",
       "    bool hasPending(const LLUUID& peer, const LLUUID& session)\n"
       "    {\n        resetAccountIfNeeded();\n"
       "        if (!enabled()) return false;\n"
       "        auto pending = mPendingRead.find(peer);\n"
       "        return pending != mPendingRead.end() && pending->second.session == session;\n"
       "    }\n\n"
       "    // Called when a P2P chat is foregrounded. Never send on mere arrival.")
splice(h, "        const LLUUID account = gAgent.getID();\n        if (account == mAccount) return;\n        mAccount = account;",
       "        const LLUUID account = gAgent.getID();\n"
       "        const bool opted_in = enabled();\n"
       "        if (account == mAccount && opted_in == mWasEnabled) return;\n"
       "        mAccount = account;\n        mWasEnabled = opted_in;")
splice(h, "    LLUUID mAccount;", "    LLUUID mAccount;\n    bool mWasEnabled = false;")
splice(h, "        mPendingRead.clear();", "        mPendingRead.clear();\n        mLastAcknowledged.clear();\n        mHasSent.clear();")
splice(h, "    std::map<LLUUID, Pending> mPendingRead;",
       "    std::map<LLUUID, Pending> mPendingRead;\n"
       "    std::map<LLUUID, LLUUID> mLastAcknowledged;\n"
       "    std::map<LLUUID, bool> mHasSent;")

# Every send supersedes the old status, including untagged/overlength text.
splice(h, "        if (!enabled() || mVerified.find(peer) == mVerified.end() ||\n            text.empty()) return text;",
       "        mHasSent[peer] = true;\n        mLastSent.erase(peer);\n"
       "        mLastSeen.erase(peer);\n        ++mRevision;\n"
       "        if (!enabled() || mVerified.find(peer) == mVerified.end() ||\n            text.empty()) return text;")

# Notify the sender exactly once, only for the most recent tagged message.
splice(h, "                    mVerified.find(peer) != mVerified.end())\n                {\n                    mLastSeen[peer] = id; // Validated match, not a guessed receipt.\n                    ++mRevision;",
       "                    mVerified.find(peer) != mVerified.end() &&\n"
       "                    (mLastSeen.find(peer) == mLastSeen.end() || mLastSeen[peer] != id))\n"
       "                {\n"
       "                    mLastSeen[peer] = id; // Validated match, not a guessed receipt.\n"
       "                    ++mRevision;\n"
       "                    LLSD args;\n"
       "                    args[\"NAME\"] = \"secondlife:///app/agent/\" + peer.asString() + \"/about\";\n"
       "                    LLSD payload;\n"
       "                    payload[\"session_id\"] = session;\n"
       "                    LLNotificationsUtil::add(\"KAMIMessageOpened\", args, payload);")
splice(h, '                mPendingRead[peer] = {session, received_id};\n                ++mRevision;',
       '                auto acknowledged = mLastAcknowledged.find(peer);\n'
       '                if (acknowledged == mLastAcknowledged.end() || acknowledged->second != received_id)\n'
       '                {\n                    mPendingRead[peer] = {session, received_id};\n                    ++mRevision;\n                }')
splice(h, "        mPendingRead.erase(pending); // exactly once, no repeat sending",
       "        mPendingRead.erase(pending); // exactly once, no repeat sending\n"
       "        mLastAcknowledged[peer] = id;")
splice(h, '        if (!enabled()) return "Read receipts OFF";',
       '        if (!enabled()) return "Read status unknown";')
splice(h, '            return "Request Kami receipts";',
       '            return mHasSent.find(peer) != mHasSent.end() ?\n'
       '                "Sent | Read status unknown" : "Read status unknown";')
splice(h, '            return "SEEN (peer opened chat)";', '            return "Open and read";')
splice(h, '            return "Sent; awaiting Seen";', '            return "Sent | Awaiting opened confirmation";')
splice(h, '        return "Kami receipts READY";',
       '        return mHasSent.find(peer) != mHasSent.end() ?\n'
       '            "Sent | Read status unknown" : "Kami receipts connected";')

# Offline replay and DND must not initiate or acknowledge receipt exchanges.
splice(V / "llimprocessing.cpp",
       "from_id, kami_conversation_id, message, !is_muted, dialog)",
       "from_id, kami_conversation_id, message,\n"
       "                !is_muted && !is_do_not_disturb && offline == IM_ONLINE, dialog)")

f = V / "llfloaterimsession.cpp"
splice(f, '#include "llcheckboxctrl.h"',
       '#include "llcheckboxctrl.h"\n#include "llfocusmgr.h"\n#include "llmutelist.h"')
splice(f, "    if (focus && getVisible() && mIsP2PChat)\n        LLKamiReadReceipts::instance().acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);",
       "    if (focus) acknowledgeKAMIMessages();")
splice(f, "        if (hasFocus() && mIsP2PChat)\n            LLKamiReadReceipts::instance().acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);",
       "        acknowledgeKAMIMessages();")
splice(f, "    LLKamiReadReceipts& rr = LLKamiReadReceipts::instance();\n    // A cheap generation comparison; no IM traffic or scanning per frame.",
       "    LLKamiReadReceipts& rr = LLKamiReadReceipts::instance();\n"
       "    // Constant UI guards and one peer lookup; sends only a pending receipt.\n"
       "    // Also handles returning from another app or scrolling to new messages.\n"
       "    acknowledgeKAMIMessages();\n"
       "    // A cheap generation comparison; no IM traffic or scanning per frame.")
splice(f, "        if (getVisible() && hasFocus() && mIsP2PChat)\n            rr.acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);\n", "")
splice(f, "void LLFloaterIMSession::onKAMIRequestReceipts()",
       "void LLFloaterIMSession::acknowledgeKAMIMessages()\n"
       "{\n"
       "    if (!mIsP2PChat || !mSessionInitialized ||\n"
       "        !LLKamiReadReceipts::instance().hasPending(mOtherParticipantUUID, mSessionID))\n"
       "        return; // no viewport/layout work when no receipt is pending\n"
       "    if (!getVisible() ||\n"
       "        !isInVisibleChain() || !hasFocus() || !gFocusMgr.getAppHasFocus() ||\n"
       "        gAgent.isDoNotDisturb() || !mChatHistory || !mChatHistory->isAtEnd() ||\n"
       "        LLMuteList::getInstance()->isMuted(mOtherParticipantUUID, LLMute::flagTextChat))\n"
       "        return;\n"
       "    LLKamiReadReceipts::instance().acknowledgeOpened(mOtherParticipantUUID, mSessionID, mDialog);\n"
       "}\n\n"
       "void LLFloaterIMSession::onKAMIRequestReceipts()")

# Queued messages use the same protocol as messages sent after session init.
splice(f, "            LLIMModel::sendMessage(iter->asString(), mSessionID,\n                mOtherParticipantUUID, mDialog);",
       "            const std::string wire = mIsP2PChat ?\n"
       "                LLKamiReadReceipts::instance().wrapOutgoing(mOtherParticipantUUID, iter->asString()) :\n"
       "                iter->asString();\n"
       "            LLIMModel::sendMessage(wire, mSessionID, mOtherParticipantUUID, mDialog);")
splice(f, '    if (mIsP2PChat)\n        label += " | " + LLKamiReadReceipts::instance().status(mOtherParticipantUUID);\n    status->setText(label);',
       '    status->setText(label);\n'
       '    LLKamiReadReceipts& rr = LLKamiReadReceipts::instance();\n'
       '    if (LLTextBox* receipt = findChild<LLTextBox>("kami_receipt_status_text", true))\n'
       '        receipt->setText(mIsP2PChat ? rr.status(mOtherParticipantUUID) : "Group conversation");\n'
       '    if (LLCheckBoxCtrl* optin = findChild<LLCheckBoxCtrl>("kami_read_receipts_optin", true))\n'
       '    {\n        optin->setValue(rr.enabled());\n        optin->setEnabled(mIsP2PChat);\n    }\n'
       '    if (LLButton* request = findChild<LLButton>("kami_read_receipts_request", true))\n'
       '        request->setEnabled(mIsP2PChat && mSessionInitialized && rr.enabled());')
splice(f, '        gSavedPerAccountSettings.setBOOL("KAMIReadReceiptsEnabled", value.asBoolean());\n        updateKAMIMessengerStatus();',
       '        gSavedPerAccountSettings.setBOOL("KAMIReadReceiptsEnabled", value.asBoolean());\n'
       '        LLKamiReadReceipts::instance().revision(); // clear stale state on opt-out\n'
       '        updateKAMIMessengerStatus();')
splice(V / "llfloaterimsession.h", "    void onKAMIRequestReceipts();",
       "    void onKAMIRequestReceipts();\n    void acknowledgeKAMIMessages();")

# Native transcript remains the authority for copy/search/history. Bubbles
# are inline native text widgets, no embedded browser or animated effects.
splice(V / "llfloaterimsessiontab.cpp",
       '    chat_args["show_time"] = gSavedSettings.getBOOL("IMShowTime");',
       '    chat_args["kami_bubbles"] = mIsP2PChat;\n'
       '    chat_args["show_time"] = gSavedSettings.getBOOL("IMShowTime");')
ch = V / "llchathistory.h"
splice(ch, '        /*virtual*/ void clear();',
       '        /*virtual*/ void clear();\n'
       '        void reshape(S32 width, S32 height, bool called_from_parent = true) override;\n'
       '        bool isAtEnd() const { return mEditor && mEditor->scrolledToEnd(); }')
splice(ch, '        std::string mLastFromName;',
       '        std::vector<LLHandle<LLView>> mKamiBubbles;\n        std::string mLastFromName;')
splice(ch, '#include "lltexteditor.h"', '#include <vector>\n#include "lltexteditor.h"')
splice(ch, '        LLTextEditor*   mEditor;', '        LLTextEditor*   mEditor = nullptr;')
cpp = V / "llchathistory.cpp"
bubble_code = (ROOT / "kami/messenger/native-bubble.inc").read_text(encoding="utf-8")
splice(cpp, 'LLChatHistory::LLChatHistory(const LLChatHistory::Params& p)',
       bubble_code + '\nLLChatHistory::LLChatHistory(const LLChatHistory::Params& p)')
splice(cpp, 'void LLChatHistory::clear()\n{\n    mLastFromName.clear();',
       'void LLChatHistory::clear()\n{\n    mKamiBubbles.clear();\n    mLastFromName.clear();')
splice(cpp, '    bool prependNewLineState = mEditor->getText().size() != 0;',
       '''    // Personal IM only. Compact mode, /me, offers and system chat keep
    // the mature native renderer and all its existing actions.
    if (args["kami_bubbles"].asBoolean() && !use_plain_text_chat_history &&
        chat.mFromID.notNull() && chat.mNotifId.isNull() &&
        !teleport_separator && !irc_me && chat.mChatStyle != CHAT_STYLE_IRC &&
        chat.mSourceType != CHAT_SOURCE_SYSTEM && chat.mSourceType != CHAT_SOURCE_OBJECT &&
        chat.mSourceType != CHAT_SOURCE_REGION)
    {
        auto* bubble = new LLKamiMessageBubble(chat, body_message_params, from_me, args);
        bubble->fit(llmax(80, mEditor->getRect().getWidth() - 40));
        LLInlineViewSegment::Params p;
        p.view = bubble;
        p.force_newline = true;
        p.left_pad = 4;
        p.right_pad = 4;
        p.top_pad = 4;
        p.bottom_pad = 4;
        // Associated text preserves transcript search and whole-message copy.
        mEditor->appendWidget(p, "\\n[" + chat.mTimeStr + "] " +
            chat.mFromName + ": " + chat.mText + "\\n", false);
        mKamiBubbles.push_back(bubble->getHandle());
        mLastFromName.clear(); // force a header for the next non-bubble entry
        mLastFromID = LLUUID::null;
        mEditor->blockUndo();
        if (from_me) mEditor->setCursorAndScrollToEnd();
        return;
    }

    bool prependNewLineState = mEditor->getText().size() != 0;''')
splice(cpp, 'void LLChatHistory::draw()\n{',
       '''void LLChatHistory::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLUICtrl::reshape(width, height, called_from_parent);
    if (!mEditor || mKamiBubbles.empty()) return;
    const S32 available = llmax(80, mEditor->getRect().getWidth() - 40);
    for (auto it = mKamiBubbles.begin(); it != mKamiBubbles.end(); )
    {
        auto* bubble = dynamic_cast<LLKamiMessageBubble*>(it->get());
        if (!bubble) { it = mKamiBubbles.erase(it); continue; }
        bubble->fit(available);
        ++it;
    }
    mEditor->needsReflow();
}

void LLChatHistory::draw()
{''')

# Receipt footer immediately below the transcript, above the composer.
path = XUI / "floater_im_session.xml"
tree = ET.parse(path)
root = tree.getroot()
root.set("min_width", "380")
root.set("min_height", "360")
root.set("height", "440")
stack = root.find(".//layout_stack[@name='main_stack']")
panel = stack.find("layout_panel[@name='kami_messenger_status_panel']")
stack.remove(panel)
body = stack.find("layout_panel[@name='body_panel']")
stack.insert(list(stack).index(body) + 1, panel)
panel.set("height", "76")
session = panel.find("text[@name='kami_messenger_status_text']")
session.set("top", "25")
session.set("height", "16")
session.set("use_ellipses", "true")
session.set("tool_tip", "Second Life session state. This is not a delivery or read confirmation.")
optin = panel.find("check_box[@name='kami_read_receipts_optin']")
optin.set("label", "Read receipts")
optin.set("width", "164")
optin.set("top", "48")
request = panel.find("button[@name='kami_read_receipts_request']")
request.set("label", "Connect Kami receipts")
request.set("width", "170")
request.set("top", "46")
ET.SubElement(panel, "text", {
    "name": "kami_receipt_status_text", "layout": "topleft",
    "follows": "left|top|right", "left": "8", "right": "-8",
    "top": "3", "height": "20", "font": "SansSerifSmall",
    "halign": "right", "text_color": "0.68 0.86 1 1",
    "use_ellipses": "true", "mouse_opaque": "false",
    "tool_tip": "Open and read means the peer's opted-in Kami displayed the latest message. It cannot prove human attention. Other viewers: read status unknown."
}).text = "Read status unknown"
ET.indent(tree, space=" ")
tree.write(path, encoding="utf-8", xml_declaration=True)

notifications = XUI / "notifications.xml"
assert ET.parse(notifications).getroot().find("notification[@name='KAMIMessageOpened']") is None
splice(notifications, "</notifications>", '''  <notification
   name="KAMIMessageOpened" type="notifytip" icon="notifytip.tga" label="Kami Messenger">
[NAME]
Open and read
  </notification>
</notifications>''')
ET.parse(notifications)
print("PASS: Kami native chat bubbles, read-status footer and validated sender toast integrated.")
