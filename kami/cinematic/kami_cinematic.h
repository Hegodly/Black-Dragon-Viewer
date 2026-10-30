// KAMI Dark Cinematic: independent graphics toggles, never a grouped preset.
// Lighting uses the proven viewer-local EEP preview/restore pathway.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include "kami_atmosphere.h"
#include "llviewercontrol.h"
#include <string>

namespace KAMICinematic
{
struct LightingSession
{
    bool active=false;
    std::string previousMood;
    F32 previousStrength=0.75f;
};
inline LightingSession& lighting()
{
    static LightingSession s;
    return s;
}
inline void status(const std::string& value)
{
    if (auto c=gSavedSettings.getControl("KAMICinematicStatus"))
        if (c->getValue().asString()!=value)c->setValue(value);
}
inline void setLighting(bool enabled)
{
    auto& s=lighting();
    if (enabled)
    {
        if (s.active)
        {
            status("Cinematic directional lighting is already enabled.");
            return;
        }
        if (KAMIAtmosphere::active())
        {
            // Do not clobber a different environment/preview.
            gSavedSettings.setBOOL("KAMICinematicLighting", false);
            status("Restore the existing Atmosphere preview before enabling cinematic lighting.");
            return;
        }
        s.previousMood=gSavedSettings.getString("KAMIAtmosphereMood");
        s.previousStrength=gSavedSettings.getF32("KAMIAtmosphereStrength");
        gSavedSettings.setString("KAMIAtmosphereMood","Dark Cinematic");
        gSavedSettings.setF32("KAMIAtmosphereStrength",0.65f);
        KAMIAtmosphere::apply();
        if (!KAMIAtmosphere::active())
        {
            gSavedSettings.setString("KAMIAtmosphereMood",s.previousMood);
            gSavedSettings.setF32("KAMIAtmosphereStrength",s.previousStrength);
            gSavedSettings.setBOOL("KAMICinematicLighting",false);
            status("Cinematic lighting needs an initialized in-world sky.");
            return;
        }
        s.active=true;
        status("Directional cinematic lighting ON. PBR and avatar toggles are independent.");
        return;
    }
    if (!s.active)
    {
        status("Directional cinematic lighting OFF; PBR and avatar settings unchanged.");
        return;
    }

    // Restore only the sky we still own. Atmosphere protects a later local
    // environment editor or different selection from being overwritten.
    KAMIAtmosphere::restore();
    if (gSavedSettings.getString("KAMIAtmosphereMood")=="Dark Cinematic")
        gSavedSettings.setString("KAMIAtmosphereMood",s.previousMood);
    if (gSavedSettings.getF32("KAMIAtmosphereStrength")==0.65f)
        gSavedSettings.setF32("KAMIAtmosphereStrength",s.previousStrength);
    s=LightingSession{};
    status("Directional cinematic lighting OFF; original local EEP restored when still owned.");
}
} // namespace KAMICinematic
