// KAMI Beyond Reality | signature native features
// SPDX-License-Identifier: LGPL-2.1-only
// No additional GPU passes. Camera keyframes update at display frame rate;
// adaptive world settings change at most once every 3 seconds.
#pragma once
#include "llviewercontrol.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llvoavatarself.h"
#include "llviewerregion.h"
#include "llsdserialize.h"
#include "llviewerstats.h"
#include "lltrace.h"
#include <chrono>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

namespace KAMISignature
{
using Clock = std::chrono::steady_clock;
inline F64 seconds()
{
    return std::chrono::duration<F64>(Clock::now().time_since_epoch()).count();
}
inline void put(const char* key,const LLSD& value)
{
    if (auto c = gSavedSettings.getControl(key))
        if (c->getValue()!=value) c->setValue(value);
}
inline std::string xml(const LLSD& data)
{
    std::ostringstream out;
    LLSDSerialize::toXML(data,out);
    return out.str();
}
inline bool parse(const std::string& data,LLSD& dst)
{
    if (data.empty()) return false;
    std::istringstream in(data);
    return LLSDSerialize::fromXML(dst,in)>0;
}
inline std::string region()
{
    auto* r=gAgent.getRegion();
    return r ? r->getRegionID().asString() : "";
}
inline F64 fps()
{
    const F64 result=LLTrace::get_frame_recording()
       .getPeriodMedianPerSec(LLStatViewer::FPS,50);
    return std::isfinite(result) && result>0.1 ? result : 0.;
}
// We intentionally exclude FPS cap, native resolution, camera state and
// graphics backends; region changes should not alter 4K or screen cropping.
inline const std::vector<std::string>& graphicKeys()
{
    static const std::vector<std::string> keys={
      "RenderFarClip","RenderAvatarMaxNonImpostors","RenderAvatarLODFactor",
      "RenderVolumeLODFactor","RenderTreeLODFactor","RenderTerrainLODFactor",
      "RenderMaxTextureResolution","RenderMaxPartCount","RenderLocalLightCount",
      "RenderShadowDetail","RenderShadowResolutionScale",
      "RenderScreenSpaceReflections","RenderDeferredSSAO",
      "RenderDeferredHBAO","RenderGlow","RenderReflectionProbeCount"
    };
    return keys;
}
inline LLSD snapshot()
{
    LLSD values=LLSD::emptyMap();
    for (const auto& key:graphicKeys())
        if (auto c = gSavedSettings.getControl(key))
            values[key]=c->getValue();
    return values;
}
inline void restore(const LLSD& saved)
{
    if (!saved.isMap()) return;
    for (const auto& key:graphicKeys())
        if (saved.has(key)) put(key.c_str(),saved[key]);
}
inline bool conflictingMode()
{
    return gSavedSettings.getBOOL("KAMIAutoTune") ||
        gSavedSettings.getBOOL("KAMIPotatoMode") ||
        gSavedSettings.getBOOL("KAMICrowdMode") ||
        gSavedSettings.getBOOL("KAMIPerformanceMode");
}

// ----------- Render Doctor (measured, reversible diagnosis) ------------
namespace Doctor
{
inline const char* recommendation()
{
    const F64 now=seconds();
    const F64 last=gSavedSettings.getF32("KAMISensorTime");
    const bool fresh=last>0 && now-last>=0 && now-last<4.5;
    if (fresh && gSavedSettings.getF32("KAMISensorVRAM")>=90.f)
        return "VRAM budget pressure (adapter 0); reduce texture resolution";
    if (fresh && gSavedSettings.getF32("KAMISensorGPU")>=90.f)
        return "Busy GPU engine; reduce shadow cost";
    if (fresh && gSavedSettings.getF32("KAMISensorCPU")>=90.f)
        return "High system CPU load; reduce fully rendered avatars";
    return "No reliable bottleneck classification; open Hardware Monitor for fresh GPU/CPU data";
}
inline void tick()
{
    static F64 last=0;
    const F64 now=seconds();
    if (!gSavedSettings.getBOOL("KAMIDoctorEnabled") || now-last<2.) return;
    last=now;
    const F64 current=fps();
    const U32 target=gSavedSettings.getU32("KAMITargetFPS");
    if (current==0)
        put("KAMIDoctorStatus","Collecting real FPS data.");
    else if (target>0 && current>=target)
        put("KAMIDoctorStatus",llformat("FPS %.1f / goal %u. At or above target.",current,target));
    else
        put("KAMIDoctorStatus",llformat("FPS %.1f / goal %u. %s",current,target,recommendation()));
}
inline void apply()
{
    if (conflictingMode()||gSavedSettings.getBOOL("KAMIWorldSmart"))
    {
        put("KAMIDoctorStatus","Pause Auto Tune/Performance/Smart World before applying a single fix.");
        return;
    }
    if (!gSavedSettings.getString("KAMIDoctorBackup").empty())
    {
        put("KAMIDoctorStatus","A Doctor fix is already applied. Restore it before another.");
        return;
    }
    if (fps()==0)
    {
        put("KAMIDoctorStatus","No measured FPS yet; no change applied.");
        return;
    }
    const F64 now=seconds();
    const F64 seen=gSavedSettings.getF32("KAMISensorTime");
    if (seen<=0 || now-seen<0 || now-seen>=4.5)
    {
        put("KAMIDoctorStatus","Open Hardware Monitor for live evidence before applying a fix.");
        return;
    }
    const F32 vram=gSavedSettings.getF32("KAMISensorVRAM");
    const F32 gpu=gSavedSettings.getF32("KAMISensorGPU");
    const F32 cpu=gSavedSettings.getF32("KAMISensorCPU");
    const char* setting=nullptr;
    LLSD value;
    if (vram>=90.f) { setting="RenderMaxTextureResolution"; value=1024; }
    else if (gpu>=90.f) { setting="RenderShadowResolutionScale"; value=0.60; }
    else if (cpu>=90.f) { setting="RenderAvatarMaxNonImpostors"; value=4; }
    if (!setting || !gSavedSettings.getControl(setting))
    {
        put("KAMIDoctorStatus","No confident, supported single-setting fix. Nothing changed.");
        return;
    }
    LLSD before=LLSD::emptyMap();
    before[setting]=gSavedSettings.getControl(setting)->getValue();
    if (setting==std::string("RenderMaxTextureResolution") &&
        before[setting].asInteger()<=1024)
    {
        put("KAMIDoctorStatus","Texture size already at or below 1024; no change applied.");
        return;
    }
    if (setting==std::string("RenderShadowResolutionScale") &&
        before[setting].asReal()<=0.60)
    {
        put("KAMIDoctorStatus","Shadow scale already low; no change applied.");
        return;
    }
    if (setting==std::string("RenderAvatarMaxNonImpostors") &&
        before[setting].asInteger()<=4)
    {
        put("KAMIDoctorStatus","Avatar count already low; no change applied.");
        return;
    }
    put("KAMIDoctorBackup",xml(before));
    put(setting,value);
    put("KAMIDoctorStatus",std::string("Applied one targeted change: ")+setting+". Compare FPS; Restore is available.");
}
inline void undo()
{
    LLSD before;
    if (parse(gSavedSettings.getString("KAMIDoctorBackup"),before) && before.isMap())
    {
        for (auto it=before.beginMap();it!=before.endMap();++it)
            put(it->first.c_str(),it->second);
        put("KAMIDoctorBackup","");
        put("KAMIDoctorStatus","Original settings restored. Doctor diagnosis remains available.");
    }
}
}

// ------ Smart World (native view-dependent impostors/budgets only) ------
namespace World
{
inline bool running=false;
inline S32 lastStage=-1;
inline void restoreWorld()
{
    LLSD baseline;
    const std::string backup=gSavedSettings.getString("KAMIWorldBackup");
    if (parse(backup,baseline) && baseline.isMap())
    {
        restore(baseline);
        put("KAMIWorldBackup","");
    }
    if (!backup.empty() && !baseline.isMap())
    {
        put("KAMIWorldStatus","Backup unreadable: retained for recovery; turn Smart World off.");
        return;
    }
    lastStage=-1;running=false;
    put("KAMIWorldStatus","Off; original scene budgets restored.");
}
inline void tick()
{
    static F64 prev=0;
    const F64 now=seconds();
    if (now-prev<3.) return;
    prev=now;
    const bool enabled=gSavedSettings.getBOOL("KAMIWorldSmart");
    if (!enabled)
    {
        if (running || !gSavedSettings.getString("KAMIWorldBackup").empty())
            restoreWorld();
        return;
    }
    if (conflictingMode() || !gSavedSettings.getString("KAMIDoctorBackup").empty())
    {
        if (running) restoreWorld();
        put("KAMIWorldStatus","Paused: Performance/Auto Tune or Render Doctor is controlling graphics.");
        return;
    }
    const F64 current=fps();
    if (current<=0) {put("KAMIWorldStatus","Collecting FPS before adaptive world changes.");return;}
    if (gSavedSettings.getString("KAMIWorldBackup").empty())
        put("KAMIWorldBackup",xml(snapshot()));
    running=true;
    const U32 goal=llclamp(gSavedSettings.getU32("KAMITargetFPS"),30u,120u);
    // Hysteresis reduces costly reconfiguration oscillations. Use no GPU
    // readbacks and never change camera resolution or users' FPS cap.
    const S32 stage=(current<goal*0.63)?2:(current<goal*0.88)?1:0;
    if (lastStage!=stage)
    {
        LLSD original;
        if (!parse(gSavedSettings.getString("KAMIWorldBackup"),original) || !original.isMap())
        {put("KAMIWorldStatus","Baseline unavailable; no world changes.");return;}
        restore(original);
        if (stage>=1)
        {
            auto c = gSavedSettings.getControl("RenderAvatarMaxNonImpostors");
            if (c && c->getValue().asInteger()>8)put("RenderAvatarMaxNonImpostors",8);
            c=gSavedSettings.getControl("RenderMaxPartCount");
            if (c && c->getValue().asInteger()>1200)put("RenderMaxPartCount",1200);
        }
        if (stage>=2)
        {
            auto c = gSavedSettings.getControl("RenderAvatarMaxNonImpostors");
            if (c && c->getValue().asInteger()>4)put("RenderAvatarMaxNonImpostors",4);
            c=gSavedSettings.getControl("RenderLocalLightCount");
            if(c && c->getValue().asInteger()>32)put("RenderLocalLightCount",32);
            c=gSavedSettings.getControl("RenderShadowResolutionScale");
            if(c && c->getValue().asReal()>0.7)put("RenderShadowResolutionScale",0.7);
        }
        lastStage=stage;
    }
    // Render impostors are already native screen/distance based. The viewer
    // retains its native foreground object and avatar selection; no custom
    // per-object culling or extra GPU render pass is introduced.
    put("KAMIWorldStatus",llformat("FPS %.1f; view-aware impostors + budget stage %d (0=original). 4K kept.",current,stage));
}
}

// ------ Intelligent region-aware custom profiles (no render-size changes) --
namespace Profiles
{
inline std::string lastRegion;
inline void applyProfile(const std::string& name)
{
    if (conflictingMode())
    {
        put("KAMIProfileStatus","Turn off Auto Tune/Performance before applying a profile.");
        return;
    }
    if (gSavedSettings.getBOOL("KAMIWorldSmart"))
    {
        put("KAMIWorldSmart",false);
        World::restoreWorld();
    }
    if (!gSavedSettings.getString("KAMIDoctorBackup").empty())
        Doctor::undo();
    if (gSavedSettings.getString("KAMIProfileManualBackup").empty())
        put("KAMIProfileManualBackup",xml(snapshot()));
    LLSD bank;
    const std::string raw=gSavedSettings.getString("KAMIProfileBank");
    if (!raw.empty() && !parse(raw,bank))
    {
        put("KAMIProfileStatus","Saved profile bank unreadable. Nothing changed.");
        return;
    }
    if (bank.isMap() && bank.has(name) && bank[name].isMap())
        restore(bank[name]);
    else
    {
        // Built-in starter profiles are intentionally conservative and change
        // neither world resolution nor FPS caps. User Save overrides each.
        if (name=="Club")
        {
            put("RenderFarClip",64.);put("RenderAvatarMaxNonImpostors",6);
            put("RenderMaxPartCount",512);put("RenderShadowDetail",0);
        }
        else if (name=="Photography")
        {
            put("RenderAvatarMaxNonImpostors",16);put("RenderShadowDetail",2);
            put("RenderDeferredSSAO",true);put("RenderScreenSpaceReflections",true);
        }
        else if (name=="Exploration")
        {
            put("RenderFarClip",96.);put("RenderAvatarMaxNonImpostors",10);
            put("RenderMaxPartCount",1280);
        }
        else if (name=="Low-end PC")
        {
            put("RenderFarClip",48.);put("RenderAvatarMaxNonImpostors",3);
            put("RenderMaxPartCount",256);put("RenderShadowDetail",0);
            put("RenderScreenSpaceReflections",false);
        }
        else {put("KAMIProfileStatus","Unknown profile; no settings changed.");return;}
    }
    put("KAMIProfileStatus",std::string("Active: ")+name+" | native resolution and FPS cap unchanged.");
}
inline void saveProfile(const std::string& name)
{
    if(name.empty())return;
    LLSD bank;
    if(!gSavedSettings.getString("KAMIProfileBank").empty() &&
       !parse(gSavedSettings.getString("KAMIProfileBank"),bank))
    {
        put("KAMIProfileStatus","Saved profiles are unreadable. No overwrite.");
        return;
    }
    if(!bank.isMap())bank=LLSD::emptyMap();
    bank[name]=snapshot();
    put("KAMIProfileBank",xml(bank));
    put("KAMIProfileStatus",std::string("Saved current graphics to ")+name+".");
}
inline void restoreManual()
{
    LLSD baseline;
    if(parse(gSavedSettings.getString("KAMIProfileManualBackup"),baseline) && baseline.isMap())
    {
        restore(baseline);
        put("KAMIProfileManualBackup","");
        put("KAMIProfileStatus","Manual graphics settings restored.");
    }
}
inline void rememberRegion(const std::string& name)
{
    const auto id=region();
    if(id.empty()) {put("KAMIProfileStatus","Not connected to a region.");return;}
    LLSD map;
    if(!gSavedSettings.getString("KAMIRegionProfileBank").empty() &&
       !parse(gSavedSettings.getString("KAMIRegionProfileBank"),map))
    {
        put("KAMIProfileStatus","Region profile map unreadable. No overwrite.");
        return;
    }
    if(!map.isMap())map=LLSD::emptyMap();
    map[id]=name;
    put("KAMIRegionProfileBank",xml(map));
    lastRegion=id;
    put("KAMIProfileStatus",std::string("Remembered ")+name+" for this region; auto switch is opt-in.");
}
inline void tick()
{
    static F64 last=0;
    const F64 now=seconds();
    if(now-last<4.)return;
    last=now;
    const auto id=region();
    if(id.empty()){lastRegion.clear();return;}
    if(!gSavedSettings.getBOOL("KAMIRegionProfilesEnabled"))
    {lastRegion.clear();return;}
    if(id==lastRegion)return;
    lastRegion=id;
    LLSD map;
    if(parse(gSavedSettings.getString("KAMIRegionProfileBank"),map) &&
       map.isMap() && map.has(id))
        applyProfile(map[id].asString());
    else
        restoreManual();
}
}

// --------- Camera Director: user-recorded A/B global camera keyframes -------
namespace Camera
{
inline bool playing=false;
inline Clock::time_point started;
inline F64 lastFrame=0;
inline LLVector3d fromPos,fromFocus,toPos,toFocus,originalPos,originalFocus;
inline LLVector3d subjectAtStart,smoothedSubject;
inline F64 trackingBlend=0.;
inline std::string sourceRegion;
inline LLSD asData(const LLVector3d& p,const LLVector3d& f)
{
    LLSD result=LLSD::emptyMap();
    LLSD pos=LLSD::emptyArray(),focus=LLSD::emptyArray();
    for(S32 i=0;i<3;++i){pos.append(p[i]);focus.append(f[i]);}
    result["position"]=pos;result["focus"]=focus;result["region"]=region();
    return result;
}
inline bool readData(const LLSD& d,LLVector3d& p,LLVector3d& f)
{
    if(!d.isMap()||!d.has("position")||!d.has("focus")||
       !d["position"].isArray()||!d["focus"].isArray()||
       d["position"].size()!=3||d["focus"].size()!=3||
       d["region"].asString()!=region())return false;
    for(S32 i=0;i<3;++i)
    {
        p[i]=d["position"][i].asReal();
        f[i]=d["focus"][i].asReal();
        if(!std::isfinite(p[i])||!std::isfinite(f[i]))return false;
    }
    return true;
}
inline void capture(const char* key)
{
    if(region().empty()||gAgentCamera.getCameraMode()==CAMERA_MODE_MOUSELOOK)
    {put("KAMICameraStatus","Enter a region and leave mouselook to capture a shot.");return;}
    put(key,xml(asData(gAgentCamera.getCameraPositionGlobal(),gAgentCamera.getFocusGlobal())));
    put("KAMICameraStatus",std::string("Recorded ")+(std::string(key).find("Start")!=std::string::npos?"A":"B")+" camera keyframe.");
}
inline void stop()
{
    if(!playing)return;
    playing=false;
    trackingBlend=0.;
    // Restore original manual camera to avoid trapping the user in playback.
    gAgentCamera.setCameraPosAndFocusGlobal(originalPos,originalFocus,LLUUID::null);
    gAgentCamera.stopCameraAnimation();
    put("KAMICameraStatus","Director stopped; original camera restored.");
}
inline void play()
{
    LLSD a,b;
    if (region().empty() || gAgentCamera.getCameraMode()==CAMERA_MODE_MOUSELOOK ||
        !parse(gSavedSettings.getString("KAMICameraStart"),a) ||
        !parse(gSavedSettings.getString("KAMICameraEnd"),b) ||
        !readData(a,fromPos,fromFocus) || !readData(b,toPos,toFocus))
    {
        put("KAMICameraStatus","Record keyframes A/B in this same region outside mouselook.");
        return;
    }
    if(playing)stop();
    originalPos=gAgentCamera.getCameraPositionGlobal();
    originalFocus=gAgentCamera.getFocusGlobal();
    sourceRegion=region();
    // The avatar render pose (rather than coarse agent physics position) is
    // what the viewer actually draws. Anchor the camera rig at playback start.
    subjectAtStart=isAgentAvatarValid()
        ? gAgent.getPosGlobalFromAgent(gAgentAvatarp->getRenderPosition())
        : gAgent.getPositionGlobal();
    smoothedSubject=subjectAtStart;
    trackingBlend=0.;
    started=Clock::now();lastFrame=0;playing=true;
    put("KAMICameraStatus",gSavedSettings.getBOOL("KAMICameraTrackSelf")
        ? "Director playing: camera follows and aims at my avatar."
        : "Cinematic A-to-B camera path playing.");
}
inline void tick()
{
    if(!playing)return;
    if(region()!=sourceRegion||gAgentCamera.getCameraMode()==CAMERA_MODE_MOUSELOOK)
    {stop();return;}
    const F64 now=seconds();
    // Do not quantize tracking to 30 Hz: on high-FPS displays a 30 Hz aim
    // causes visible stepping. Camera state is already updated once per
    // display frame; the exponential filter is independent of framerate.
    const F64 dt=lastFrame>0. ? llclamp(now-lastFrame,0.0001,0.1) : 1./60.;
    lastFrame=now;
    const F64 elapsed=std::chrono::duration<F64>(Clock::now()-started).count();
    const F64 duration=llclamp((F64)gSavedSettings.getF32("KAMICameraPathSeconds"),2.,30.);
    const F64 t=llclamp(elapsed/duration,0.,1.);
    const F64 eased=t*t*(3.-2.*t);
    const LLVector3d pathPos=fromPos+(toPos-fromPos)*eased;
    const LLVector3d pathFocus=fromFocus+(toFocus-fromFocus)*eased;

    const bool track=gSavedSettings.getBOOL("KAMICameraTrackSelf") && isAgentAvatarValid();
    if(isAgentAvatarValid())
    {
        // Use the actual rendered avatar position, not the agent's coarse
        // simulated coordinates; the mismatch used to make aim drift.
        const LLVector3d subject=gAgent.getPosGlobalFromAgent(gAgentAvatarp->getRenderPosition());
        const F64 subjectAlpha=1.-std::exp(-dt/0.10);
        smoothedSubject+=(subject-smoothedSubject)*subjectAlpha;
    }

    // Switching tracking on or off must not abruptly snap to a new focus.
    const F64 blendAlpha=1.-std::exp(-dt/0.18);
    trackingBlend+=(track?1.-trackingBlend:-trackingBlend)*blendAlpha;
    trackingBlend=llclamp(trackingBlend,0.,1.);
    if(!track && trackingBlend<0.0001)trackingBlend=0.;

    const LLVector3d followPos=pathPos+(smoothedSubject-subjectAtStart);
    const LLVector3d followFocus=smoothedSubject+LLVector3d(0.,0.,1.45);
    const LLVector3d pos=pathPos+(followPos-pathPos)*trackingBlend;
    const LLVector3d focus=pathFocus+(followFocus-pathFocus)*trackingBlend;
    gAgentCamera.setCameraPosAndFocusGlobal(pos,focus,LLUUID::null);
    gAgentCamera.stopCameraAnimation();
    if(t>=1.)
    {
        playing=false; // Hold the finished shot, without trapping user input.
        trackingBlend=0.;
        put("KAMICameraStatus","Path complete. Avatar tracking ended; manual camera available.");
    }
}
}
inline void tick()
{
    Doctor::tick();
    World::tick();
    Profiles::tick();
    Camera::tick();
}
} // namespace KAMISignature
