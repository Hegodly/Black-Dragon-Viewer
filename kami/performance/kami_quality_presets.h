// KAMI quality presets. These are render-quality choices, not FPS caps.
// All mode switches are based on actual graphics controls and restore any
// previously active automatic-mode baseline before changing quality.
#pragma once
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include <sstream>

namespace KAMIQuality
{
    inline void set(const char* name, const LLSD& value)
    {
        if (auto control = gSavedSettings.getControl(name))
        {
            if (control->getValue() != value)
                control->setValue(value);
        }
    }

    inline bool leaveOtherModes()
    {
        // This listener restores the Performance preset's original settings.
        if (gSavedSettings.getBOOL("KAMIPerformanceMode"))
            gSavedSettings.setBOOL("KAMIPerformanceMode", false);

        // Smart optimization would otherwise restore its prior settings on
        // the next rendered frame and silently erase the newly chosen preset.
        const std::string backup = gSavedSettings.getString("KAMISmartBackup");
        if (!backup.empty())
        {
            LLSD saved;
            std::istringstream in(backup);
            if (LLSDSerialize::fromXML(saved, in) <= 0 || !saved.isMap())
            {
                LL_WARNS("KAMIQuality") << "Smart graphics backup is invalid; quality preset not applied." << LL_ENDL;
                return false; // Preserve the corrupt backup for manual recovery.
            }
            for (auto it=saved.beginMap(); it!=saved.endMap(); ++it)
                set(it->first.c_str(), it->second);
            gSavedSettings.setString("KAMISmartBackup", "");
        }
        set("KAMIAutoTune", false);
        set("KAMIPotatoMode", false);
        set("KAMICrowdMode", false);
        return true;
    }

    inline bool apply(S32 q)
    {
        if (q < 0 || q > 4) return false;
        if (!leaveOtherModes()) return false;

        // 0 Low/FPS, 1 Medium, 2 High, 3 Ultra, 4 Cinematic.
        // Explicit 4K width, resolution multiplier, user-selected frame cap,
        // VSync and optical-flow state are deliberately *not modified*.
        static const F32 distance[] = {48.f, 64.f, 96.f, 128.f, 128.f};
        static const F32 objectLOD[] = {0.75f, 1.20f, 2.0f, 3.0f, 4.0f};
        static const F32 avatarLOD[] = {0.50f, 0.70f, 1.0f, 1.4f, 1.8f};
        static const F32 shadowScale[] = {0.35f, 0.50f, 0.70f, 0.85f, 1.0f};
        static const U32 textures[] = {512, 1024, 1024, 2048, 2048};
        static const U32 impostorLimit[] = {4, 6, 10, 16, 20};
        static const S32 particles[] = {256, 640, 1280, 2560, 4096};
        static const S32 lights[] = {16, 32, 64, 128, 256};
        static const U32 probes[] = {16, 32, 64, 128, 256};
        static const S32 probeDetail[] = {-1, 0, 0, 1, 2};
        static const S32 shadowDetail[] = {0, 0, 1, 2, 2};
        static const F32 glowStrength[] = {0.0f, 0.0f, 0.04f, 0.06f, 0.10f};
        static const S32 ssrSamples[] = {1, 1, 2, 3, 4};

        set("RenderFarClip", distance[q]);
        set("RenderVolumeLODFactor", objectLOD[q]);
        set("RenderTreeLODFactor", objectLOD[q]);
        set("RenderTerrainLODFactor", objectLOD[q]);
        set("RenderAvatarLODFactor", avatarLOD[q]);
        set("RenderAvatarMaxNonImpostors", impostorLimit[q]);
        set("RenderMaxPartCount", particles[q]);
        set("RenderLocalLightCount", lights[q]);
        set("RenderMaxTextureResolution", textures[q]);
        set("RenderFSAAType", q >= 2 ? 2 : 0); // Off or SMAA; FXAA currently under repair.
        set("RenderShadowDetail", shadowDetail[q]);
        set("RenderShadowResolutionScale", shadowScale[q]);
        set("RenderDeferredSSAO", q >= 2);
        set("RenderDeferredHBAO", q >= 3);
        set("RenderScreenSpaceReflections", q >= 3);
        set("RenderScreenSpaceReflectionGlossySamples", ssrSamples[q]);
        set("RenderReflectionProbeDetail", probeDetail[q]);
        set("RenderReflectionProbeCount", probes[q]);
        set("RenderGlow", q >= 2);
        set("RenderGlowStrength", glowStrength[q]);
        set("RenderDepthOfField", false); // User-directed focus effect, not auto.
        set("RenderMotionBlur", false);
        set("KAMINearFieldGI", q == 4); // Experimental, costly quality choice only.
        // Do not force-enable known unstable volumetric effects.
        // Never change FramePerSecondLimit, KombatantRenderWidth, or
        // RenderResolutionMultiplier. Quality means geometry/effect budgets.
        set("KAMIQualityPreset", q);
        LL_INFOS("KAMIQuality") << "Applied graphics quality preset " << q
            << " (0 Low, 1 Medium, 2 High, 3 Ultra, 4 Cinematic). "
            << "Frame limit and native render resolution were not changed." << LL_ENDL;
        return true;
    }
}
