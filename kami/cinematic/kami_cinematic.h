// KAMI Dark Cinematic: original PBR material adjustment + viewer-local EEP
// + existing optimized live avatar LOD. No additional GPU render passes.
// Copyright (C) 2026 KAMI contributors. SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include "kami_atmosphere.h"
#include "llviewercontrol.h"
#include <string>

namespace KAMICinematic
{
struct Session
{
    bool active=false;
    bool previousMaterials=false;
    bool previousAvatar=false;
    std::string previousMood;
    F32 previousStrength=0.75f;
};
inline Session& session()
{
    static Session s;
    return s;
}
inline void status(const std::string& value)
{
    if (auto c=gSavedSettings.getControl("KAMICinematicStatus"))
        if (c->getValue().asString()!=value) c->setValue(value);
}
inline void apply()
{
    auto& s=session();
    if (s.active)
    {
        status("Dark Cinematic is already active; Restore before applying again.");
        return;
    }
    // Shader/EEP quality is an explicitly selected visual mode. Never fight
    // adaptive FPS settings or replace another active Atmosphere preview.
    if (gSavedSettings.getBOOL("KAMIPotatoMode") ||
        gSavedSettings.getBOOL("KAMIPerformanceMode") ||
        gSavedSettings.getBOOL("KAMIAutoTune") ||
        gSavedSettings.getBOOL("KAMICrowdMode"))
    {
        status("Turn off Potato/Performance/Auto Tune/Crowd before applying cinematic visuals.");
        return;
    }
    if (KAMIAtmosphere::active())
    {
        status("Another Atmosphere preview is active. Restore it first to protect its local EEP.");
        return;
    }

    s.previousMaterials=gSavedSettings.getBOOL("KAMICinematicMaterials");
    s.previousAvatar=gSavedSettings.getBOOL("KAMIAvatarQualityFPS");
    s.previousMood=gSavedSettings.getString("KAMIAtmosphereMood");
    s.previousStrength=gSavedSettings.getF32("KAMIAtmosphereStrength");

    // Component 1: inexpensive, existing PBR roughness material evaluation.
    gSavedSettings.setBOOL("KAMICinematicMaterials", true);
    // Component 2: direct/ambient light changes on a cloned local EEP sky.
    gSavedSettings.setString("KAMIAtmosphereMood", "Dark Cinematic");
    gSavedSettings.setF32("KAMIAtmosphereStrength", 0.65f);
    KAMIAtmosphere::apply();
    if (!KAMIAtmosphere::active())
    {
        // Missing sky/region: do not leave a half-applied preset behind.
        gSavedSettings.setBOOL("KAMICinematicMaterials",s.previousMaterials);
        gSavedSettings.setString("KAMIAtmosphereMood",s.previousMood);
        gSavedSettings.setF32("KAMIAtmosphereStrength",s.previousStrength);
        status("World sky is not ready. Enter a region and retry.");
        return;
    }
    // Component 3: keep the user's mesh LOD detailed and use native distant
    // avatar impostors to bound crowd rendering. Existing SL asset limits remain.
    gSavedSettings.setBOOL("KAMIAvatarQualityFPS", true);
    s.active=true;
    status("Active: PBR highlights + local directional EEP + avatar detail/FPS. No new pass. Compare real FPS.");
}
inline void restore()
{
    auto& s=session();
    if (!s.active)
    {
        status("Dark Cinematic is not active; current graphics preserved.");
        return;
    }
    // Do not overwrite later manual user choices when they differ from this
    // preset. Do not steal the local environment from a new EEP editor.
    if (gSavedSettings.getBOOL("KAMICinematicMaterials"))
        gSavedSettings.setBOOL("KAMICinematicMaterials",s.previousMaterials);
    if (gSavedSettings.getBOOL("KAMIAvatarQualityFPS"))
        gSavedSettings.setBOOL("KAMIAvatarQualityFPS",s.previousAvatar);
    if (gSavedSettings.getString("KAMIAtmosphereMood")=="Dark Cinematic")
    {
        KAMIAtmosphere::restore();
        gSavedSettings.setString("KAMIAtmosphereMood",s.previousMood);
        if (gSavedSettings.getF32("KAMIAtmosphereStrength")==0.65f)
            gSavedSettings.setF32("KAMIAtmosphereStrength",s.previousStrength);
    }
    s=Session{};
    status("Dark Cinematic stopped. Prior local graphics and avatar budgets restored.");
}
} // namespace KAMICinematic
