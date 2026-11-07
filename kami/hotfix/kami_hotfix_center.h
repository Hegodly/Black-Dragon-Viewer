// KAMI Hotfix Home: native notification surface and preview UI.
// This code displays a notification ONLY when a future trusted update
// controller explicitly signals a verified optimization hotfix.
// No remote check, forced logout, download or install occurs here.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include "llfloater.h"
#include "llfloaterreg.h"
#include "llbutton.h"
#include "llnotificationsutil.h"
#include "llsd.h"

#include <string>

class KAMIHotfixCenter final : public LLFloater
{
public:
    explicit KAMIHotfixCenter(const LLSD& key) : LLFloater(key) {}

    bool postBuild() override
    {
        getChild<LLButton>("kami_hotfix_close")->setClickedCallback(
            [this](LLUICtrl*, const LLSD&) { closeFloater(); });
        return true;
    }

    // Future updater integration point. Must be called from the UI thread
    // only AFTER manifest publisher signature, patch type, channel, and
    // version are independently verified by a trusted controller.
    // This preview cannot invoke itself: no update feed is wired in.
    static void notifyVerifiedHotfix(const std::string& verified_version)
    {
        if (verified_version.empty())
        {
            return;
        }

        // Prevent repeated banners for the same pending release during one
        // viewer session. This is not a permanent dismissal policy.
        static std::string last_notified_version;
        if (verified_version == last_notified_version)
        {
            return;
        }
        last_notified_version = verified_version;

        LLSD substitutions;
        substitutions["HOTFIX_VERSION"] = verified_version;
        LLNotificationsUtil::add(
            "KamiOptimizationHotfixAvailable", substitutions, LLSD(),
            [](const LLSD& notification, const LLSD& response) {
                if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                {
                    // View details is safe while logged in; full-session
                    // Home/installer is still a separately gated feature.
                    LLFloaterReg::showInstance("kami_hotfix_home", LLSD(), true);
                }
            });
    }
};
