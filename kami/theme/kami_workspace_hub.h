// KAMI native Workspaces launcher. Reuses actual Second Life viewer floaters;
// never substitutes screenshots, fake buttons, quality changes, or web UIs.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include "llfloater.h"
#include "llfloaterreg.h"
#include "llbutton.h"

class KAMIWorkspaceHub final : public LLFloater
{
public:
    explicit KAMIWorkspaceHub(const LLSD& key) : LLFloater(key) {}

    bool postBuild() override
    {
        // Keep the navigation on the UI thread; native windows own their
        // own permissions, state, background work and camera behavior.
        bind("hub_inventory", "inventory");
        bind("hub_outfits", "appearance");
        bind("hub_places", "places");
        bind("hub_map", "world_map");
        bind("hub_poser", "poser");
        bind("hub_pose_creator", "poser_creator");
        bind("hub_material", "material_editor");
        bind("hub_snapshot", "snapshot");
        bind("hub_camera", "camera");
        bind("hub_preferences", "preferences");
        bind("hub_hardware", "kami_hardware_monitor");
        bind("hub_toolbar", "toybox");
        return true;
    }

private:
    void bind(const char* button, const char* nativeFloater)
    {
        getChild<LLButton>(button)->setClickedCallback(
            [nativeFloater](LLUICtrl*, const LLSD&) {
                LLFloaterReg::showInstance(nativeFloater, LLSD(), true);
            });
    }
};
