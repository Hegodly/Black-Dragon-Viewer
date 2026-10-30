// KAMI Smart Performance Engine — native, optional, Windows-first.
// Copyright (C) 2026 KAMI contributors; SPDX-License-Identifier: LGPL-2.1-only
// This module uses EXISTING OpenGL viewer quality controls. No external DLL.
#pragma once

#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "lltimer.h"
#include <sstream>
#include <algorithm>

namespace KAMISmart
{
    // These native settings are reversible even across viewer restarts.
    inline const char* const managed[] = {
        "RenderFarClip", "RenderResolutionDivisor", "RenderResolutionMultiplier",
        "KombatantRenderWidth", "KombatantUpscalerProvider", "KombatantRayAO",
        "RenderMaxTextureResolution", "RenderMaxVRAMBudget", "RenderTextureVRAMDivisor",
        "RenderFSAAType", "RenderShadowDetail", "RenderShadowResolutionScale",
        "RenderDeferredSSAO", "RenderGlow", "RenderScreenSpaceReflections",
        "RenderReflectionProbeDetail", "RenderReflectionProbeCount",
        "RenderVolumeLODFactor", "RenderTreeLODFactor", "RenderTerrainLODFactor",
        "RenderAvatarLODFactor", "RenderAvatarMaxNonImpostors", "RenderImpostors",
        "RenderMaxPartCount", "RenderAttachedParticles", "RenderLocalLightCount",
        "RenderDepthOfField", "RenderMotionBlur", "RenderVolumetricLighting",
        "KombatantNativeFX"
    };

    inline void set(const char* name, const LLSD& value)
    {
        if (auto control = gSavedSettings.getControl(name))
        {
            if (control->getValue() != value)
                control->setValue(value);
        }
    }

    inline LLSD baseline()
    {
        LLSD before = LLSD::emptyMap();
        for (const char* key : managed)
            if (auto control = gSavedSettings.getControl(key))
                before[key] = control->getValue();
        return before;
    }

    inline bool readBackup(LLSD& saved)
    {
        const std::string xml = gSavedSettings.getString("KAMISmartBackup");
        if (xml.empty()) return false;
        std::istringstream in(xml);
        return LLSDSerialize::fromXML(saved, in) > 0 && saved.isMap();
    }

    inline void storeBackup()
    {
        if (!gSavedSettings.getString("KAMISmartBackup").empty()) return;
        std::ostringstream out;
        LLSDSerialize::toXML(baseline(), out);
        gSavedSettings.setString("KAMISmartBackup", out.str());
    }

    inline void restore(bool clear)
    {
        LLSD old;
        if (readBackup(old))
        {
            for (const char* name : managed)
                if (old.has(name)) set(name, old[name]);
            if (clear) gSavedSettings.setString("KAMISmartBackup", "");
        }
        else if (!gSavedSettings.getString("KAMISmartBackup").empty())
        {
            // Keep the corrupt backup for diagnostics instead of losing the data.
            LL_WARNS("KAMISmart") << "Unable to restore performance settings backup" << LL_ENDL;
        }
    }

    inline void atMostU32(const char* key, U32 value)
    {
        if (auto c = gSavedSettings.getControl(key))
            if (c->getValue().asInteger() > static_cast<S32>(value)) set(key, value);
    }

    inline void atMostF32(const char* key, F32 value)
    {
        if (auto c = gSavedSettings.getControl(key))
            if (c->getValue().asReal() > value) set(key, value);
    }

    // One quality level at a time; capture and restore the complete manual
    // configuration on each transition so the user doesn't lose preferences.
    inline void applyStage(S32 stage)
    {
        restore(false);
        const bool avatar = gSavedSettings.getBOOL("KAMIOptimizeAvatars");
        const bool protect = gSavedSettings.getBOOL("KAMIProtectMyAvatar");
        const bool shadow = gSavedSettings.getBOOL("KAMIAdaptiveShadows");
        const bool reflection = gSavedSettings.getBOOL("KAMIAdaptiveReflections");
        const bool particles = gSavedSettings.getBOOL("KAMIAdaptiveParticles");
        const bool distance = gSavedSettings.getBOOL("KAMIAdaptiveDrawDistance");
        const bool resolution = gSavedSettings.getBOOL("KAMIAdaptiveResolution");
        const bool texture = gSavedSettings.getBOOL("KAMISmartTextures");

        if (stage >= 1)
        {
            // Cheap native effects first; no expensive CPU-to-GPU SDK copies.
            set("KombatantUpscalerProvider", 0);
            set("KombatantRayAO", false);
            set("RenderMotionBlur", false);
            set("RenderDepthOfField", false);
            if (reflection) set("RenderScreenSpaceReflections", false);
            set("RenderVolumetricLighting", false);
        }
        if (stage >= 2)
        {
            set("RenderDeferredSSAO", false);
            set("RenderGlow", false);
            if (shadow)
            {
                atMostU32("RenderShadowDetail", 1);
                atMostF32("RenderShadowResolutionScale", 0.75f);
            }
            if (reflection)
            {
                atMostU32("RenderReflectionProbeCount", 64);
                atMostU32("RenderReflectionProbeDetail", 0);
            }
            if (particles) atMostU32("RenderMaxPartCount", 1024);
        }
        if (stage >= 3)
        {
            // Impostors provide fewer distant-avatar rendering and update costs.
            if (avatar)
            {
                set("RenderImpostors", true);
                atMostU32("RenderAvatarMaxNonImpostors",
                    gSavedSettings.getBOOL("KAMIThrottleDistantAvatars") ? 6u : 10u);
                if (!protect) atMostF32("RenderAvatarLODFactor", 0.8f);
            }
            if (particles) atMostU32("RenderMaxPartCount", 512);
            if (distance) atMostF32("RenderFarClip", 96.f);
            atMostF32("RenderVolumeLODFactor", 1.5f);
            atMostF32("RenderTreeLODFactor", 1.5f);
            atMostF32("RenderTerrainLODFactor", 1.5f);
        }
        if (stage >= 4)
        {
            if (avatar)
            {
                atMostU32("RenderAvatarMaxNonImpostors", 4);
                if (!protect) atMostF32("RenderAvatarLODFactor", 0.6f);
            }
            if (distance) atMostF32("RenderFarClip", 64.f);
            if (particles) atMostU32("RenderMaxPartCount", 256);
            if (reflection) atMostU32("RenderReflectionProbeCount", 16);
            if (shadow) atMostF32("RenderShadowResolutionScale", 0.5f);
            atMostF32("RenderVolumeLODFactor", 1.0f);
            atMostF32("RenderTreeLODFactor", 1.0f);
            atMostF32("RenderTerrainLODFactor", 1.0f);
            if (texture)
            {
                // 0 = existing viewer auto VRAM budget; avoid guessing VRAM.
                set("RenderMaxVRAMBudget", 0);
                atMostU32("RenderMaxTextureResolution", 1024);
            }
        }
        if (stage >= 5)
        {
            if (shadow) set("RenderShadowDetail", 0);
            if (reflection) set("RenderReflectionProbeDetail", -1);
            if (avatar) atMostU32("RenderAvatarMaxNonImpostors", 2);
            if (particles) atMostU32("RenderMaxPartCount", 128);
            if (distance) atMostF32("RenderFarClip", 48.f);
            if (texture) atMostU32("RenderMaxTextureResolution", 512);
            if (resolution)
            {
                set("KombatantRenderWidth", 0);
                set("RenderResolutionDivisor", 1);
                atMostF32("RenderResolutionMultiplier", 0.75f);
            }
        }
        if (stage >= 6)
        {
            if (particles)
            {
                set("RenderAttachedParticles", false);
                atMostU32("RenderMaxPartCount", 64);
            }
            if (distance) atMostF32("RenderFarClip", 32.f);
            if (resolution) atMostF32("RenderResolutionMultiplier", 0.6f);
            if (texture) atMostU32("RenderMaxTextureResolution", 512);
            atMostU32("RenderLocalLightCount", 16);
            set("RenderFSAAType", 0);
            set("KombatantNativeFX", false);
        }
    }

    inline void tick()
    {
        static U64 previousFrame = 0;
        static U64 windowStart = 0;
        static S32 frames = 0;
        static S32 stage = -1;
        static S32 highWindows = 0;
        static U32 options = 0;
        static bool wasActive = false;
        const U64 now = LLTimer::getTotalTime();

        const bool potato = gSavedSettings.getBOOL("KAMIPotatoMode");
        const bool crowd = gSavedSettings.getBOOL("KAMICrowdMode");
        const bool autoTune = gSavedSettings.getBOOL("KAMIAutoTune");
        const bool active = autoTune || potato || crowd;

        // Existing older Performance Mode is mutually exclusive with the
        // Smart Engine: switch it off before capturing the user's baseline.
        if (active && gSavedSettings.getBOOL("KAMIPerformanceMode"))
            gSavedSettings.setBOOL("KAMIPerformanceMode", false);

        if (!active)
        {
            if (wasActive || !gSavedSettings.getString("KAMISmartBackup").empty())
                restore(true);
            wasActive = false;
            stage = -1;
            frames = 0;
            highWindows = 0;
            windowStart = now;
            previousFrame = now;
            return;
        }

        if (!wasActive)
        {
            storeBackup();
            stage = -1;
            wasActive = true;
            frames = 0;
            highWindows = 0;
            windowStart = now;
        }

        // Settings can change without a restart; update stage only when
        // toggles change, never churn the renderer on every frame.
        const U32 newOptions =
            (gSavedSettings.getBOOL("KAMIProtectMyAvatar") ? 1u : 0u) |
            (gSavedSettings.getBOOL("KAMIOptimizeAvatars") ? 2u : 0u) |
            (gSavedSettings.getBOOL("KAMIAdaptiveShadows") ? 4u : 0u) |
            (gSavedSettings.getBOOL("KAMIAdaptiveReflections") ? 8u : 0u) |
            (gSavedSettings.getBOOL("KAMIAdaptiveParticles") ? 16u : 0u) |
            (gSavedSettings.getBOOL("KAMIAdaptiveDrawDistance") ? 32u : 0u) |
            (gSavedSettings.getBOOL("KAMIAdaptiveResolution") ? 64u : 0u) |
            (gSavedSettings.getBOOL("KAMISmartTextures") ? 128u : 0u) |
            (gSavedSettings.getBOOL("KAMIThrottleDistantAvatars") ? 256u : 0u);
        if (options != newOptions)
        {
            options = newOptions;
            stage = -1;
        }

        // Deliberately don't use wall-clock FPS when window isn't focused:
        // background limits and task-switching aren't rendering bottlenecks.
        const bool focused = gFocusMgr.getAppHasFocus() &&
            gViewerWindow && gViewerWindow->getWindow() &&
            gViewerWindow->getWindow()->getVisible();
        if (!focused)
        {
            previousFrame = now;
            windowStart = now;
            frames = 0;
            return;
        }

        if (windowStart == 0) windowStart = now;
        ++frames;
        const U64 elapsed = now - windowStart;
        S32 chosen = stage < 0 ? 0 : stage;
        if (autoTune && elapsed >= 3500000 && frames > 0)
        {
            const F64 fps = static_cast<F64>(frames) * 1000000.0 / elapsed;
            const S32 cap = gSavedSettings.getS32("FramePerSecondLimit");
            const S32 desired = llclamp(static_cast<S32>(gSavedSettings.getU32("KAMITargetFPS")), 30, 120);
            // Never punish graphics because of a user-selected FPS cap.
            const S32 target = cap > 0 ? llmin(desired, cap) : desired;
            if (fps < target * 0.90 && chosen < 6)
            {
                ++chosen;
                highWindows = 0;
            }
            else if (fps > target * 1.15 && chosen > 0)
            {
                if (++highWindows >= 5)
                {
                    --chosen;
                    highWindows = 0;
                }
            }
            else highWindows = 0;
            frames = 0;
            windowStart = now;
        }

        if (potato) chosen = 6;
        else if (crowd) chosen = llmax(chosen, 3);

        if (stage != chosen)
        {
            stage = chosen;
            applyStage(stage);
            LL_INFOS("KAMISmart") << "Native adaptive quality stage " << stage <<
                "/6 (0=original, 6=Potato)" << LL_ENDL;
        }
        previousFrame = now;
    }

    inline void limitBackgroundFrames()
    {
        if (gFocusMgr.getAppHasFocus() && gViewerWindow &&
            gViewerWindow->getWindow() && gViewerWindow->getWindow()->getVisible())
            return;
        const S32 fps = gSavedSettings.getS32("KAMIBackgroundFPS");
        if (fps <= 0) return;
        static U64 last = 0;
        const U64 now = LLTimer::getTotalTime();
        const U64 period = 1000000ULL / static_cast<U64>(llclamp(fps, 5, 60));
        if (last && now > last && now-last < period)
            micro_sleep(period - (now-last), 0);
        last = LLTimer::getTotalTime();
    }
}
