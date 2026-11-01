// KAMI Hotfix Home, early user-interface milestone.
// This panel is intentionally NOT an updater or a network/download endpoint.
// A trusted, signed update controller must be integrated before enabling
// an "Update Required" action or a return-to-login install flow.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include "llfloater.h"
#include "llbutton.h"

class KAMIHotfixCenter final : public LLFloater
{
public:
    explicit KAMIHotfixCenter(const LLSD& key) : LLFloater(key) {}

    bool postBuild() override
    {
        // Functional close action. No fake "Apply" action while no verified
        // update service exists. An active Second Life session is untouched.
        getChild<LLButton>("kami_hotfix_close")->setClickedCallback(
            [this](LLUICtrl*, const LLSD&) { closeFloater(); });
        return true;
    }
};
