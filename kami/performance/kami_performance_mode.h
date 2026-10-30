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
        "RenderFSAAType", "RenderShadowDetail", "RenderShadowResolutionScale",
        "RenderDeferredSSAO", "RenderDeferredHBAO", "RenderGlow",
        "RenderScreenSpaceReflections", "RenderScreenSpaceReflectionGlossySamples",
        "RenderReflectionProbeDetail", "RenderReflectionProbeCount",
        "RenderVolumeLODFactor", "RenderTreeLODFactor", "RenderTerrainLODFactor",
        "RenderAvatarLODFactor", "RenderAvatarMaxNonImpostors",
        "RenderMaxPartCount", "RenderLocalLightCount",
        "RenderDepthOfField", "RenderMotionBlur",
        "RenderVolumetricLighting", "RenderResolutionDivisor",
        "RenderResolutionMultiplier", "KombatantRenderWidth",
        "KombatantUpscalerProvider", "KombatantRayAO", "KombatantNativeFX"
    };

    inline void set(const char* key, const LLSD& value)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(key))
            if (control->getValue() != value)
                control->setValue(value);
    }

    inline void atMostF32(const char* key, F32 threshold)
    {
        if (auto* control = gSavedSettings.getControl(key))
            if (control->getValue().asReal() > threshold)
                control->setValue(threshold);
    }

    // Performance Mode and Smart/Potato modes are exclusive. The original
    // implementation let Smart tick immediately turn Performance Mode off,
    // making its switch appear to have no measurable effect. Restore the
    // user's pre-Smart settings before taking the Performance baseline.
    inline void suspendSmartModes()
    {
        if (!gSavedSettings.getBOOL("KAMIAutoTune") &&
            !gSavedSettings.getBOOL("KAMIPotatoMode") &&
            !gSavedSettings.getBOOL("KAMICrowdMode"))
            return;

        const std::string backup = gSavedSettings.getString("KAMISmartBackup");
        if (!backup.empty())
        {
            LLSD saved;
            std::istringstream in(backup);
            if (LLSDSerialize::fromXML(saved, in) > 0 && saved.isMap())
            {
                for (auto it = saved.beginMap(); it != saved.endMap(); ++it)
                    set(it->first.c_str(), it->second);
                gSavedSettings.setString("KAMISmartBackup", "");
            }
            else
            {
                LL_WARNS("KAMIPerformance") << "Invalid Smart backup retained; enable Performance Mode after disabling Smart modes manually." << LL_ENDL;
                return;
            }
        }
        set("KAMIAutoTune", false);
        set("KAMIPotatoMode", false);
        set("KAMICrowdMode", false);
    }

    inline bool changed(const LLSD& enabled)
    {
        const std::string backupKey = "KAMIPerformanceBackup";
        if (enabled.asBoolean())
        {
            suspendSmartModes();
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
            // Keep the explicit live render width (including native 4K) and
            // native camera viewport. Scaled render targets crop the view in
            // the current Windows renderer; keep-resolution patch enforces 1x.
            set("RenderResolutionDivisor", 1);
            set("RenderResolutionMultiplier", 0.75);
            // Prior preset mostly changed effects and left expensive scene
            // geometry/lighting untouched. It even raised avatar LOD from
            // the default .5 to 1.0, increasing the geometry burden.
            atMostF32("RenderFarClip", 64.f);
            set("RenderMaxTextureResolution", 1024);
            set("RenderFSAAType", 0); // Avoid known-broken experimental FXAA.
            set("RenderShadowDetail", 0);
            set("RenderShadowResolutionScale", 0.5f);
            set("RenderDeferredSSAO", false);
            set("RenderDeferredHBAO", false);
            set("RenderGlow", false);
            set("RenderScreenSpaceReflections", false);
            set("RenderScreenSpaceReflectionGlossySamples", 1);
            set("RenderReflectionProbeDetail", 0);
            set("RenderReflectionProbeCount", 16);
            set("RenderAvatarMaxNonImpostors", 4);
            set("RenderMaxPartCount", 512);
            set("RenderLocalLightCount", 16);
            set("RenderDepthOfField", false);
            set("RenderMotionBlur", false);
            set("RenderVolumetricLighting", false);
            atMostF32("RenderVolumeLODFactor", 0.75f);
            atMostF32("RenderTreeLODFactor", 0.8f);
            atMostF32("RenderTerrainLODFactor", 1.0f);
            atMostF32("RenderAvatarLODFactor", 0.5f);
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
