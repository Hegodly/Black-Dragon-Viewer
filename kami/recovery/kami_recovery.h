// KAMI Beyond Reality | Session Recovery
// Copyright (C) 2026 KAMI contributors. SPDX-License-Identifier: LGPL-2.1-only
// Purely viewer-local. No recovery of simulator-side actions or inventories.
// Opt-in and no chat/IM text, tokens, names, SL credentials, or messages stored.
#pragma once
#include "llappviewer.h"
#include "llstartup.h"
#include "lldir.h"
#include "llfile.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "kami_signature.h"
#include <cstdio>
#include <sstream>
#include <string>

namespace KAMIRecovery
{
    // All state is on the viewer's main thread.
    inline bool startupChecked=false;
    inline bool offerPending=false;
    inline F64 lastSample=0.;

    inline void status(const std::string& message)
    {
        gSavedSettings.setString("KAMIRecoveryStatus",message);
    }
    inline std::string file()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,"kami_recovery_snapshot.xml");
    }
    inline bool readFile(const std::string& path,LLSD& data)
    {
        LLFILE* stream=LLFile::fopen(path,"rb");
        if(!stream)return false;
        std::string buffer;
        char chunk[4096];
        size_t bytes=0;
        while((bytes=std::fread(chunk,1,sizeof(chunk),stream))>0)
        {
            if(buffer.size()+bytes>131072) { std::fclose(stream);return false; }
            buffer.append(chunk,bytes);
        }
        const bool good=!std::ferror(stream);
        std::fclose(stream);
        if(!good || buffer.empty())return false;
        std::istringstream in(buffer);
        return LLSDSerialize::fromXML(data,in)>0 && data.isMap()
            && data["schema"].asInteger()==1 && data["graphics"].isMap();
    }
    inline bool load(LLSD& data)
    {
        return readFile(file(),data) || readFile(file()+".bak",data);
    }
    inline bool writeFile(const std::string& path,const std::string& body)
    {
        if(body.empty()||body.size()>131072)return false;
        LLFILE* stream=LLFile::fopen(path,"wb");
        if(!stream)return false;
        const bool wrote=std::fwrite(body.data(),1,body.size(),stream)==body.size();
        const bool closed=std::fclose(stream)==0;
        return wrote&&closed;
    }
    inline bool save()
    {
        if(!gSavedSettings.getBOOL("KAMIRecoveryEnabled"))
        {
            status("Session Recovery OFF. Turn on snapshots to save a recovery point.");
            return false;
        }
        LLSD data=LLSD::emptyMap();
        data["schema"]=1;
        data["graphics"]=KAMISignature::snapshot();
        data["profile"]=gSavedSettings.getString("KAMIProfileSelected");
        data["mood"]=gSavedSettings.getString("KAMIAtmosphereMood");
        data["atmosphere_strength"]=gSavedSettings.getF32("KAMIAtmosphereStrength");
        data["atmosphere_fade"]=gSavedSettings.getF32("KAMIAtmosphereFade");
        data["atmosphere_glow"]=gSavedSettings.getBOOL("KAMIAtmosphereBloom");
        // Never auto-capture world positions; the user must explicitly opt
        // into saving their stored camera keyframes in a local plaintext file.
        if(gSavedSettings.getBOOL("KAMIRecoveryIncludeCamera"))
        {
            const auto a=gSavedSettings.getString("KAMICameraStart");
            const auto b=gSavedSettings.getString("KAMICameraEnd");
            if(a.size()<=16384 && b.size()<=16384)
            {
                data["camera_a"]=a;
                data["camera_b"]=b;
                data["camera_duration"]=gSavedSettings.getF32("KAMICameraPathSeconds");
                data["camera_tracking"]=gSavedSettings.getBOOL("KAMICameraTrackSelf");
            }
        }
        std::ostringstream out;
        LLSDSerialize::toXML(data,out);
        const auto path=file();
        const auto temp=path+".tmp";
        const auto backup=path+".bak";
        if(!writeFile(temp,out.str()))
        {
            status("Recovery snapshot could not be written; check local folder permissions.");
            return false;
        }
        // Rotated backup means a crash between moves can still recover a
        // valid earlier snapshot. Save is deliberately infrequent (20 secs).
        LLFile::remove(backup,1);
        LLFile::rename(path,backup,1); // first save may have no primary
        if(LLFile::rename(temp,path,1)!=0)
        {
            // Keep backup intact; never overwrite it with an empty snapshot.
            LLFile::rename(backup,path,1);
            status("Could not commit snapshot safely; previous copy retained.");
            return false;
        }
        status("Recovery point saved locally. No chat or messages captured.");
        return true;
    }
    inline void discard()
    {
        LLFile::remove(file(),1);
        LLFile::remove(file()+".bak",1);
        LLFile::remove(file()+".tmp",1);
        status("Stored recovery snapshots deleted from this computer.");
    }
    inline bool restore()
    {
        LLSD data;
        if(!load(data))
        {
            status("No valid local recovery point. Current settings unchanged.");
            return false;
        }
        // Don't compete with ongoing self-adjusting modes (would overwrite
        // the player's settings, produce oscillations or a misleading result).
        if(KAMISignature::conflictingMode() ||
           gSavedSettings.getBOOL("KAMIWorldSmart"))
        {
            status("Turn off Auto Tune, Performance Mode and Smart World before restoring.");
            return false;
        }
        KAMISignature::restore(data["graphics"]);
        if(data.has("profile"))
            gSavedSettings.setString("KAMIProfileSelected",data["profile"].asString());
        if(data.has("mood"))
            gSavedSettings.setString("KAMIAtmosphereMood",data["mood"].asString());
        if(data.has("atmosphere_strength"))
            gSavedSettings.setF32("KAMIAtmosphereStrength",
                llclamp((F32)data["atmosphere_strength"].asReal(),0.25f,1.f));
        if(data.has("atmosphere_fade"))
            gSavedSettings.setF32("KAMIAtmosphereFade",
                llclamp((F32)data["atmosphere_fade"].asReal(),0.f,10.f));
        if(data.has("atmosphere_glow"))
            gSavedSettings.setBOOL("KAMIAtmosphereBloom",data["atmosphere_glow"].asBoolean());
        if(data.has("camera_a"))
        {
            gSavedSettings.setString("KAMICameraStart",data["camera_a"].asString());
            gSavedSettings.setString("KAMICameraEnd",data["camera_b"].asString());
            gSavedSettings.setF32("KAMICameraPathSeconds",
                llclamp((F32)data["camera_duration"].asReal(),2.f,30.f));
            gSavedSettings.setBOOL("KAMICameraTrackSelf",
                data["camera_tracking"].asBoolean());
        }
        // Intentionally no camera move, no EEP activation, no teleport or
        // agent asset/inventory changes. Restore only the *choices*.
        status("Recovery restored: graphics, profile and atmosphere choices"
               " (and opt-in camera keyframes). Preview sky manually.");
        return true;
    }
    inline void tick()
    {
        if(!gSavedSettings.getBOOL("KAMIRecoveryEnabled"))return;
        if(LLStartUp::getStartupState()!=STATE_STARTED)return;
        LLAppViewer* app=LLAppViewer::instance();
        if(!app || app->isSecondInstance())return;
        if(!startupChecked)
        {
            startupChecked=true;
            LLSD previous;
            const bool abnormal = gLastExecEvent != LAST_EXEC_NORMAL;
            if(abnormal && load(previous))
            {
                offerPending=true;
                status("Unexpected shutdown detected. Choose Restore or Keep Current.");
                LLNotificationsUtil::add("KAMIRecoveryOffer",LLSD(),LLSD(),
                    [](const LLSD& notification,const LLSD& response) {
                        const S32 choice=LLNotificationsUtil::getSelectedOption(notification,response);
                        if(choice==0)restore();
                        else status("Recovery skipped. Current viewer settings retained.");
                        offerPending=false;
                        lastSample=KAMISignature::seconds();
                    });
                return;
            }
        }
        if(offerPending)return; // do not overwrite the previous crash snapshot
        const F64 now=KAMISignature::seconds();
        if(lastSample==0. || now-lastSample>=20.)
        {
            lastSample=now;
            save();
        }
    }
} // namespace KAMIRecovery
