// KAMI Beyond Reality | Atmosphere Engine
// Copyright (C) 2026 KAMI contributors. SPDX-License-Identifier: LGPL-2.1-only
//
// Viewer-local EEP sky previews. No region/parcel edits, no messages to the
// simulator, no extra GL passes and no alteration to the user's FPS cap or
// world-render resolution. All effect changes are opt-in and reversible.
#pragma once
#include "llenvironment.h"
#include "llsettingssky.h"
#include "llviewercontrol.h"
#include <cmath>
#include <string>

namespace KAMIAtmosphere
{
    struct SavedState
    {
        bool active=false;
        bool hadLocal=false;
        bool hadGlow=false;
        bool hadGlowStrength=false;
        bool glow=false;
        LLSD glowStrength;
        LLEnvironment::EnvSelection_t previous=LLEnvironment::ENV_LOCAL;
        LLSettingsDay::ptr_t day;
        // Strongly typed LLUnit seconds has no implicit default constructor.
        // Initialize both fields for the baseline snapshot and reset path.
        LLSettingsDay::Seconds dayLength{0};
        LLSettingsDay::Seconds dayOffset{0};
        LLEnvironment::fixedEnvironment_t fixed;
        LLSettingsSky::ptr_t sourceSky;
        LLSettingsSky::ptr_t activeSky;
    };

    inline SavedState& state()
    {
        static SavedState memory;
        return memory;
    }

    inline F32 amount()
    {
        return llclamp(gSavedSettings.getF32("KAMIAtmosphereStrength"), 0.25f, 1.f);
    }

    inline LLSettingsBase::Seconds fade()
    {
        return LLSettingsBase::Seconds(llclamp(
            gSavedSettings.getF32("KAMIAtmosphereFade"), 0.f, 10.f));
    }

    inline void status(const std::string& value)
    {
        gSavedSettings.setString("KAMIAtmosphereStatus",value);
    }

    inline LLColor3 color(F32 r,F32 g,F32 b) { return LLColor3(r,g,b); }
    inline LLColor3 mixed(const LLColor3& source,const LLColor3& target,F32 mix)
    {
        return color(source.mV[0]+(target.mV[0]-source.mV[0])*mix,
                     source.mV[1]+(target.mV[1]-source.mV[1])*mix,
                     source.mV[2]+(target.mV[2]-source.mV[2])*mix);
    }
    inline void setSkyRotation(const LLSettingsSky::ptr_t& sky,F32 tilt,F32 azimuth)
    {
        LLQuaternion rot, yaw;
        rot.setAngleAxis(tilt,0.f,1.f,0.f);
        yaw.setAngleAxis(azimuth,0.f,0.f,1.f);
        rot*=yaw;
        sky->setSunRotation(rot);
    }

    inline void tune(LLSettingsSky::ptr_t& sky, const std::string& mood)
    {
        // Values refer only to local EEP sky parameters. 'Luxury Interior' is
        // a color/ambient lighting mood, not path tracing or actual room light.
        const F32 mix=amount();
        LLColor3 ambient,sunlight,blue,cloud;
        F32 haze=sky->getHazeDensity();
        F32 star=sky->getStarBrightness();
        F32 cloudCover=sky->getCloudShadow();
        F32 tilt=0.f,yaw=0.f;
        bool reposition=false;

        if(mood=="Night Studio")
        {
            ambient=color(0.25f,0.29f,0.43f);
            sunlight=color(0.14f,0.19f,0.35f);
            blue=color(0.14f,0.19f,0.33f);
            cloud=color(0.17f,0.22f,0.34f);
            haze=0.28f;star=1.1f;cloudCover=0.08f;
            tilt=1.27f*F_PI; yaw=0.20f*F_PI; reposition=true;
        }
        else if(mood=="Golden Hour")
        {
            ambient=color(0.74f,0.53f,0.42f);
            sunlight=color(1.25f,0.83f,0.48f);
            blue=color(0.42f,0.25f,0.28f);
            cloud=color(1.00f,0.69f,0.52f);
            haze=0.52f;star=0.f;cloudCover=0.24f;
            tilt=0.47f*F_PI; yaw=0.18f*F_PI; reposition=true;
        }
        else if(mood=="Overcast Cinema")
        {
            ambient=color(0.56f,0.62f,0.74f);
            sunlight=color(0.53f,0.59f,0.70f);
            blue=color(0.27f,0.34f,0.41f);
            cloud=color(0.72f,0.75f,0.81f);
            haze=0.66f;star=0.f;cloudCover=0.78f;
            tilt=0.45f*F_PI; yaw=0.23f*F_PI; reposition=true;
        }
        else if(mood=="Luxury Interior")
        {
            ambient=color(0.46f,0.40f,0.37f);
            sunlight=color(0.99f,0.87f,0.74f);
            blue=color(0.24f,0.23f,0.26f);
            cloud=color(0.69f,0.64f,0.61f);
            haze=0.12f;star=0.f;cloudCover=0.08f;
            // Keep the original local sun orientation indoors; no forced
            // room geometry or fake world-space lights.
        }
        else
        {
            return;
        }

        sky->setAmbientColor(mixed(sky->getAmbientColor(),ambient,mix));
        sky->setSunlightColor(mixed(sky->getSunlightColor(),sunlight,mix));
        sky->setBlueHorizon(mixed(sky->getBlueHorizon(),blue,mix));
        sky->setCloudColor(mixed(sky->getCloudColor(),cloud,mix));
        sky->setHazeDensity(llclamp(sky->getHazeDensity() +
            (haze-sky->getHazeDensity())*mix,0.f,2.f));
        sky->setCloudShadow(llclamp(sky->getCloudShadow() +
            (cloudCover-sky->getCloudShadow())*mix,0.f,1.f));
        sky->setStarBrightness(llmax(0.f,sky->getStarBrightness() +
            (star-sky->getStarBrightness())*mix));
        if(reposition && mix>=0.5f)
            setSkyRotation(sky,tilt,yaw);
        sky->setName("KAMI Local: "+mood);
        sky->update();
    }

    inline void applyBloom(bool enable)
    {
        auto& s=state();
        if(auto control = gSavedSettings.getControl("RenderGlow"))
        {
            if(enable)control->setValue(true);
            else if(s.hadGlow)control->setValue(s.glow);
        }
        if(auto control = gSavedSettings.getControl("RenderGlowStrength"))
        {
            if(enable)control->setValue(0.055f);
            else if(s.hadGlowStrength)control->setValue(s.glowStrength);
        }
    }

    inline void apply()
    {
        auto& environment=LLEnvironment::instance();
        auto& s=state();
        const std::string mood=gSavedSettings.getString("KAMIAtmosphereMood");
        if(mood!="Night Studio"&&mood!="Golden Hour"&&
           mood!="Overcast Cinema"&&mood!="Luxury Interior")
        {
            status("Choose a valid local mood before applying.");
            return;
        }

        if(!s.active)
        {
            auto sky=environment.getCurrentSky();
            if(!sky)
            {
                status("World sky not initialized. Enter a region before applying.");
                return;
            }

            // Snapshot the prior *local* EEP selection and data rather than
            // assuming the user was following the shared parcel day cycle.
            s.previous=environment.getSelectedEnvironment();
            s.hadLocal=environment.hasEnvironment(LLEnvironment::ENV_LOCAL);
            if(s.hadLocal)
            {
                s.day=environment.getEnvironmentDay(LLEnvironment::ENV_LOCAL);
                s.dayLength=environment.getEnvironmentDayLength(LLEnvironment::ENV_LOCAL);
                s.dayOffset=environment.getEnvironmentDayOffset(LLEnvironment::ENV_LOCAL);
                s.fixed=environment.getEnvironmentFixed(LLEnvironment::ENV_LOCAL);
            }
            s.sourceSky=sky->buildClone();
            if(auto c = gSavedSettings.getControl("RenderGlow"))
            {
                s.hadGlow=true; s.glow=c->getValue().asBoolean();
            }
            if(auto c = gSavedSettings.getControl("RenderGlowStrength"))
            {
                s.hadGlowStrength=true;s.glowStrength=c->getValue();
            }
            s.active=true;
        }

        if(!s.sourceSky)
        {
            status("Local sky baseline missing; no changes made.");
            return;
        }
        LLSettingsSky::ptr_t sky=s.sourceSky->buildClone();
        tune(sky,mood);

        // This is strictly a LOCAL preview; it cannot edit parcel/estate
        // EEP settings or create inventory items.
        environment.clearEnvironment(LLEnvironment::ENV_LOCAL);
        environment.setEnvironment(LLEnvironment::ENV_LOCAL,sky);
        environment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,fade(),true);
        s.activeSky=sky;
        applyBloom(gSavedSettings.getBOOL("KAMIAtmosphereBloom"));
        status("Local preview: "+mood+" | "+std::to_string((S32)(amount()*100))+
               "% strength. Other residents' skies are unchanged.");
    }

    inline void restore()
    {
        auto& s=state();
        if(!s.active)
        {
            status("No KAMI atmosphere is active. Region environment untouched.");
            return;
        }
        auto& environment=LLEnvironment::instance();
        // If a different editor/viewer control has taken over ENV_LOCAL or
        // the user changed environment selection, do not overwrite it.
        const bool ownLocal=
            environment.getSelectedEnvironment()==LLEnvironment::ENV_LOCAL &&
            environment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL)==s.activeSky;
        if(ownLocal)
        {
            environment.clearEnvironment(LLEnvironment::ENV_LOCAL);
            if(s.hadLocal)
            {
                if(s.day)
                    environment.setEnvironment(LLEnvironment::ENV_LOCAL,
                        s.day,s.dayLength,s.dayOffset);
                else
                    environment.setEnvironment(LLEnvironment::ENV_LOCAL,s.fixed);
            }
            environment.setSelectedEnvironment(s.previous,fade(),true);
        }

        applyBloom(false);
        s=SavedState{};
        if(ownLocal)
            status("Atmosphere restored: original local EEP and glow settings.");
        else
            status("Atmosphere stopped. A newer local EEP selection was preserved.");
    }

    inline bool active() { return state().active; }
} // namespace KAMIAtmosphere
