// KAMI Viewer native Performance Mode. Applied using the regular settings listener.
// Values are captured before modification and serialized as LLSD/XML so the
// original user preferences survive restarts. No ReShade/GShade runtime.
#pragma once
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include <sstream>

namespace KAMIPerformance
{
    // Exclude FPS limit / VSync: the player remains in control of pacing.
    inline const char* const keys[] = {
        "RenderFarClip", "RenderMaxTextureResolution",
        "RenderFSAAType", "RenderShadowDetail", "RenderDeferredSSAO",
        "RenderGlow", "RenderScreenSpaceReflections", "RenderReflectionProbeDetail",
        "RenderVolumeLODFactor", "RenderTreeLODFactor", "RenderTerrainLODFactor",
        "RenderAvatarLODFactor", "RenderDepthOfField", "RenderMotionBlur",
        "RenderVolumetricLighting", "RenderResolutionDivisor",
        "RenderResolutionMultiplier", "KombatantRenderWidth",
        "KombatantUpscalerProvider", "KombatantRayAO", "KombatantNativeFX"
    };

    inline void set(const char* key, const LLSD& value)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(key))
            control->setValue(value);
    }

    inline bool changed(const LLSD& enabled)
    {
        const std::string backupKey = "KAMIPerformanceBackup";
        if (enabled.asBoolean())
        {
            // Only snapshot ONCE. Reapplying on startup must not overwrite
            // the original graphics configuration with performance settings.
            if (gSavedSettings.getString(backupKey).empty())
            {
                LLSD before = LLSD::emptyMap();
                for (const char* key : keys)
                {
                    if (LLControlVariable* c = gSavedSettings.getControl(key))
                        before[key] = c->getValue();
                }
                std::ostringstream out;
                LLSDSerialize::toXML(before, out);
                gSavedSettings.setString(backupKey, out.str());
            }
            // Use native OpenGL rendering. GPU/CPU SDK copies are too costly
            // to risk in a high-FPS mode.
            set("KombatantUpscalerProvider", 0);
            set("KombatantRayAO", false);
            set("KombatantRenderWidth", 0);
            set("RenderResolutionDivisor", 1);
            set("RenderResolutionMultiplier", 0.75);
            set("RenderFarClip", 96.0);
            set("RenderMaxTextureResolution", 1024);
            set("RenderFSAAType", 1); // Lightweight FXAA
            set("RenderShadowDetail", 0);
            set("RenderDeferredSSAO", false);
            set("RenderGlow", false);
            set("RenderScreenSpaceReflections", false);
            set("RenderReflectionProbeDetail", 0);
            set("RenderDepthOfField", false);
            set("RenderMotionBlur", false);
            set("RenderVolumetricLighting", false);
            set("RenderVolumeLODFactor", 1.0);
            set("RenderTreeLODFactor", 1.0);
            set("RenderTerrainLODFactor", 1.0);
            set("RenderAvatarLODFactor", 1.0);
            set("KombatantNativeFX", false);
        }
        else
        {
            const std::string backup = gSavedSettings.getString(backupKey);
            if (!backup.empty())
            {
                LLSD previous;
                std::istringstream in(backup);
                if (LLSDSerialize::fromXML(previous, in) > 0 && previous.isMap())
                {
                    for (const char* key : keys)
                        if (previous.has(key)) set(key, previous[key]);
                    gSavedSettings.setString(backupKey, "");
                }
                else
                {
                    LL_WARNS("KAMIPerformance") << "Saved performance backup was unreadable; it was retained." << LL_ENDL;
                }
            }
        }
        return true;
    }

    inline void registerListener()
    {
        setting_setup_signal_listener(gSavedSettings, "KAMIPerformanceMode", changed);
        // Preserves ON across restarts without losing the pre-mode backup.
        if (gSavedSettings.getBOOL("KAMIPerformanceMode"))
            changed(LLSD(true));
    }
}
