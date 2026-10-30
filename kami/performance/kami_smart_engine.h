// KAMI Smart Performance Engine — native, optional, Windows-first.
// Copyright (C) 2026 KAMI contributors; SPDX-License-Identifier: LGPL-2.1-only
// This module uses EXISTING OpenGL viewer quality controls. No external DLL.
#pragma once

#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "lltimer.h"
#include <sstream>
#include <algorithm>
#include <vector>
#include <iomanip>

namespace KAMISmart
{
    // These native settings are reversible even across viewer restarts.
    inline const char* const managed[] = {
        "RenderFarClip", "RenderResolutionDivisor", "RenderResolutionMultiplier",
        "KombatantRenderWidth", "KombatantUpscalerProvider", "KombatantRayAO",
        "RenderMaxTextureResolution", "RenderMaxVRAMBudget", "RenderTextureVRAMDivisor",
        "RenderFSAAType", "RenderShadowDetail", "RenderShadowResolutionScale",
        "RenderScreenSpaceReflectionGlossySamples", "RenderDeferredHBAO",
        "RenderDeferredSSAO", "RenderGlow", "RenderScreenSpaceReflections",
        "RenderReflectionProbeDetail", "RenderReflectionProbeCount",
        "RenderVolumeLODFactor", "RenderTreeLODFactor", "RenderTerrainLODFactor",
        "RenderAvatarLODFactor", "RenderAvatarMaxNonImpostors", "RenderImpostors",
        "RenderMaxPartCount", "RenderAttachedParticles", "RenderLocalLightCount",
        "RenderDepthOfField", "RenderMotionBlur",
        "KombatantNativeFX", "KAMIAvatarFillStrength", "KAMIAvatarSoftShading",
        "KAMITargetFPS"
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
    inline void applyStage(S32 stage, bool restoreBaseline)
    {
        // Escalating quality reductions can be applied in place. Restoring
        // every managed setting before each step caused unnecessary FBO and
        // shader reallocations, which appear as large frame-time spikes.
        // Restore the saved baseline when lowering stage or editing policies.
        if (restoreBaseline) restore(false);
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
            if (reflection) atMostU32("RenderScreenSpaceReflectionGlossySamples", 2);
            // Keep manually requested SSR on during early adaptive stages.
            // Only the higher-cost emergency stages disable reflections.
            // Volumetrics remain a manual choice during Auto Tune / Crowd Mode.
            // Only the explicitly selected emergency Potato preset disables them.
            if (gSavedSettings.getBOOL("KAMIPotatoMode"))
                set("RenderVolumetricLighting", false);
            // Optional baked-avatar cosmetics yield to FPS-first settings.
            set("KAMIAvatarFillStrength", 0.f);
            set("KAMIAvatarSoftShading", 0.f);
        }
        if (stage >= 2)
        {
            // Keep the AO darkening control effective during ordinary
            // adaptation. HBAO is more costly; fall back to existing SSAO.
            // Explicit Potato Mode disables native AO for maximum speed.
            if (gSavedSettings.getBOOL("KAMIPotatoMode"))
                set("RenderDeferredSSAO", false);
            else
                set("RenderDeferredHBAO", false);
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
            if (reflection)
            {
                atMostU32("RenderReflectionProbeCount", 16);
                set("RenderScreenSpaceReflections", false);
            }
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
            // Last-resort AO reduction only after cheaper options have
            // already been exhausted and actual FPS remains below target.
            set("RenderDeferredSSAO", false);
            if (reflection) set("RenderReflectionProbeDetail", -1);
            if (avatar) atMostU32("RenderAvatarMaxNonImpostors", 2);
            if (particles) atMostU32("RenderMaxPartCount", 128);
            if (distance) atMostF32("RenderFarClip", 48.f);
            if (texture) atMostU32("RenderMaxTextureResolution", 512);
            if (resolution && !gSavedSettings.getBOOL("KAMIPotatoMode"))
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
            // Preserve real selected render pixels, including explicit 3840-wide 4K.
            // Avoid downscaling to 60% when Potato Mode is on.
            if (gSavedSettings.getBOOL("KAMIPotatoMode"))
            {
                set("RenderResolutionDivisor", 1);
                set("RenderResolutionMultiplier", 1.f);
                // 100 FPS is an advisory performance goal only.
                // Never modify the actual frame limiter here.
                set("KAMITargetFPS", 100);
            }
            else if (resolution) atMostF32("RenderResolutionMultiplier", 0.6f);
            if (texture) atMostU32("RenderMaxTextureResolution", 512);
            atMostU32("RenderLocalLightCount", 16);
            set("RenderFSAAType", 0);
            set("KombatantNativeFX", false);
        }
    }

    // Measures the *current live scene* rather than claiming a deterministic
    // GPU benchmark. User-controlled opt-in, no telemetry leaves the viewer.
    inline void benchmarkFrame(U64 now)
    {
        static U64 started = 0;
        static U64 last = 0;
        static std::vector<F64> frameMs;
        if (!gSavedSettings.getBOOL("KAMIBenchmarkCapture"))
        {
            started = 0;
            last = 0;
            frameMs.clear();
            return;
        }
        const bool focused = gFocusMgr.getAppHasFocus() &&
            gViewerWindow && gViewerWindow->getWindow() &&
            gViewerWindow->getWindow()->getVisible();
        if (!focused)
        {
            gSavedSettings.setString("KAMIBenchmarkResult", "Canceled: viewer was not focused.");
            gSavedSettings.setBOOL("KAMIBenchmarkCapture", false);
            return;
        }
        if (!started)
        {
            started = now;
            last = now;
            frameMs.clear();
            frameMs.reserve(9000);
            gSavedSettings.setString("KAMIBenchmarkResult", "Benchmark running: 30 seconds in the current scene.");
            LL_INFOS("KAMIBenchmark") << "30-second local scene FPS sample started" << LL_ENDL;
            return;
        }
        if (last && now > last && frameMs.size() < 30000)
        {
            const F64 elapsed = static_cast<F64>(now-last) / 1000.0;
            if (elapsed > 0.01 && elapsed <= 1000.0)
                frameMs.push_back(elapsed);
        }
        last = now;
        if (now - started < 30000000ULL) return;
        if (frameMs.size() < 30)
        {
            gSavedSettings.setString("KAMIBenchmarkResult", "Too few frames to score.");
            gSavedSettings.setBOOL("KAMIBenchmarkCapture", false);
            return;
        }
        std::sort(frameMs.begin(), frameMs.end());
        const size_t n = frameMs.size();
        const F64 medianMs = frameMs[n/2];
        const F64 p99Ms = frameMs[static_cast<size_t>((n-1)*0.99)];
        const F64 medianFPS = 1000.0/medianMs;
        const bool performance = gSavedSettings.getBOOL("KAMIPerformanceMode");
        const S32 qualityPreset = gSavedSettings.getS32("KAMIQualityPreset");
        const bool manualBaseline = qualityPreset < 0 && !performance &&
            !gSavedSettings.getBOOL("KAMIAutoTune") &&
            !gSavedSettings.getBOOL("KAMIPotatoMode") && !gSavedSettings.getBOOL("KAMICrowdMode");
        const F64 baselineFPS = gSavedSettings.getF32("KAMIPerformanceBaselineFPS");
        std::ostringstream out;
        out << std::fixed << std::setprecision(1)
            << "Current scene: median " << medianFPS
            << " FPS; p99 frame " << p99Ms << " ms; "
            << n << " samples. ";
        if (manualBaseline)
        {
            gSavedSettings.setF32("KAMIPerformanceBaselineFPS", static_cast<F32>(medianFPS));
            out << "Baseline saved. Turn on Performance Mode and repeat the same view.";
        }
        else if (performance)
        {
            if (baselineFPS > 1.0)
            {
                const F64 gain = (medianFPS / baselineFPS - 1.0) * 100.0;
                out << "Performance vs last baseline: " << (gain >= 0.0 ? "+" : "")
                    << gain << "% (baseline " << baselineFPS << " FPS). ";
            }
            else
                out << "No baseline yet. Turn preset off and benchmark the same scene first. ";
            out << "Scene changes, VSync and FPS cap affect comparison.";
        }
        else if (qualityPreset >= 0 && qualityPreset <= 4 &&
                 !gSavedSettings.getBOOL("KAMIAutoTune") &&
                 !gSavedSettings.getBOOL("KAMIPotatoMode") &&
                 !gSavedSettings.getBOOL("KAMICrowdMode"))
        {
            static const char* const names[] = {"Low", "Medium", "High", "Ultra", "Cinematic"};
            if (baselineFPS > 1.0)
            {
                const F64 change = (medianFPS / baselineFPS - 1.0) * 100.0;
                out << names[qualityPreset] << " quality vs manual baseline: "
                    << (change >= 0.0 ? "+" : "") << change
                    << "% (" << baselineFPS << " FPS baseline). ";
            }
            else
                out << "No manual baseline yet. Select Custom, benchmark, then reapply this preset. ";
            out << "Keep camera, region, resolution, cap and VSync the same.";
        }
        else out << "Smart optimization is active; use Custom/Off for a stable baseline. ";
        gSavedSettings.setString("KAMIBenchmarkResult", out.str());
        LL_INFOS("KAMIBenchmark") << out.str() << LL_ENDL;
        gSavedSettings.setBOOL("KAMIBenchmarkCapture", false);
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
        static bool needsBaselineRestore = true;
        static U64 lastCrowdScan = 0;
        static bool cachedCrowd = false;
        const U64 now = LLTimer::getTotalTime();
        benchmarkFrame(now);

        const bool potato = gSavedSettings.getBOOL("KAMIPotatoMode");
        const bool autoTune = gSavedSettings.getBOOL("KAMIAutoTune");
        // Avatar enumeration can be costly in busy regions: scan on a
        // 2-second interval instead of every rendering frame.
        if (autoTune && gSavedSettings.getBOOL("KAMIAutoCrowdDetection"))
        {
            if (!lastCrowdScan || now - lastCrowdScan >= 2000000ULL)
            {
                cachedCrowd = gObjectList.getAvatarCount() >=
                    llclamp(gSavedSettings.getS32("KAMICrowdAvatarThreshold"), 5, 100);
                lastCrowdScan = now;
            }
        }
        else
        {
            cachedCrowd = false;
            lastCrowdScan = 0;
        }
        const bool crowd = gSavedSettings.getBOOL("KAMICrowdMode") || cachedCrowd;
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
            needsBaselineRestore = true;
            frames = 0;
            highWindows = 0;
            windowStart = now;
            previousFrame = now;
            return;
        }

        if (!wasActive)
        {
            storeBackup();
            // The fixed visual quality levels no longer describe these
            // adaptive budgets. Avoid showing stale 'Ultra' during Potato.
            set("KAMIQualityPreset", -1);
            stage = -1;
            wasActive = true;
            needsBaselineRestore = true;
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
            needsBaselineRestore = true;
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
            const F64 target = cap > 0 ? llmin(static_cast<F64>(desired), 1000000.0 / cap) : desired;
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
            const S32 previousStage = stage;
            stage = chosen;
            applyStage(stage, needsBaselineRestore || previousStage > stage);
            needsBaselineRestore = false;
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
