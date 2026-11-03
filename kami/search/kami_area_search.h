/**
 * KAMI Area Search. LGPL-2.1-only, consistent with the viewer.
 * Design reference: Firestorm fsareasearch.cpp, Copyright (c) 2012
 * Techwolf Lupindo, Phoenix Firestorm Project (LGPL-2.1-only).
 * Adapted for KAMI using ObjectPropertiesFamily without changing selection.
 */
#ifndef KAMI_AREA_SEARCH_H
#define KAMI_AREA_SEARCH_H
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llscrolllistctrl.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llbutton.h"
#include "llframetimer.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llselectmgr.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "lltracker.h"
#include "llavatarnamecache.h"
#include "llavatarname.h"
#include "message.h"
#include <map>
#include <vector>
#include <algorithm>

class KAMIAreaSearch final : public LLFloater
{
    struct Entry { std::string name, description; LLUUID owner; bool ready=false; };
    std::map<LLUUID, Entry> mEntries;
    std::vector<LLUUID> mQueue;
    size_t mNext=0;
    LLUUID mRegion;
    LLFrameTimer mTimer;
    LLScrollListCtrl* mList=nullptr;
    bool mScanning=false, mDirty=false;
    static bool matches(std::string text, std::string query)
    {
        LLStringUtil::toLower(text); LLStringUtil::toLower(query);
        return text.find(query)!=std::string::npos;
    }
    bool eligible(LLViewerObject* o) const
    {
        return o && !o->isDead() && o->getPCode()==LL_PCODE_VOLUME &&
            !o->isAttachment() && (o->getRootEdit()==o) && o->getRegion()==gAgent.getRegion();
    }
    LLViewerObject* selected()
    {
        auto row=mList->getFirstSelected();
        auto o=row ? gObjectList.findObject(row->getUUID()) : nullptr;
        return eligible(o) ? o : nullptr;
    }
    void locate(bool focus)
    {
        if(auto o=selected())
        {
            if(focus) gAgentCamera.setFocusGlobal(o->getPositionGlobal(),o->getID());
            else LLTracker::trackLocation(o->getPositionGlobal(),"Area Search object","KAMI Area Search");
        }
    }
    void refresh()
    {
        mEntries.clear(); mQueue.clear(); mNext=0;
        auto region=gAgent.getRegion(); mRegion=region ? region->getRegionID() : LLUUID::null;
        for(S32 i=0;i<gObjectList.getNumObjects() && mQueue.size()<4096;++i)
        {
            auto o=gObjectList.getObject(i);
            if(eligible(o)) { mQueue.push_back(o->getID()); mEntries.emplace(o->getID(),Entry()); }
        }
        mScanning=true; mDirty=true; mTimer.reset(); rebuild();
    }
    void rebuild()
    {
        LLUUID selection;
        if(auto row=mList->getFirstSelected()) selection=row->getUUID();
        mList->deleteAllItems();
        auto name=getChild<LLLineEditor>("name_filter")->getText();
        auto desc=getChild<LLLineEditor>("description_filter")->getText();
        auto owner=getChild<LLLineEditor>("owner_filter")->getText();
        size_t received=0, shown=0;
        for(const auto& pair:mEntries)
        {
            const auto& e=pair.second; auto o=gObjectList.findObject(pair.first);
            if(!e.ready || !eligible(o)) continue;
            ++received;
            LLAvatarName av; std::string owner_name=e.owner.asString();
            if(LLAvatarNameCache::get(e.owner,&av)) owner_name=av.getUserName();
            if(!matches(e.name,name)||!matches(e.description,desc)||
               (!matches(owner_name,owner)&&!matches(e.owner.asString(),owner))) continue;
            LLSD row; row["id"]=pair.first;
            row["columns"][0]["column"]="distance";
            row["columns"][0]["value"]=llformat("%.1f",dist_vec(gAgent.getPositionGlobal(),o->getPositionGlobal()));
            row["columns"][1]["column"]="name"; row["columns"][1]["value"]=e.name;
            row["columns"][2]["column"]="description"; row["columns"][2]["value"]=e.description;
            row["columns"][3]["column"]="owner"; row["columns"][3]["value"]=owner_name;
            mList->addElement(row); ++shown;
        }
        mList->sortByColumn("distance",true); mList->selectByID(selection);
        getChild<LLTextBox>("status")->setText(llformat("%u matches | %u/%u received | %u queued%s",
            (U32)shown,(U32)received,(U32)mEntries.size(),(U32)(mQueue.size()-mNext),
            mScanning ? "" : " | paused"));
        mDirty=false;
    }
public:
    explicit KAMIAreaSearch(const LLSD& key):LLFloater(key) {}
    bool postBuild() override
    {
        mList=getChild<LLScrollListCtrl>("results");
        getChild<LLButton>("refresh")->setClickedCallback([this](LLUICtrl*, const LLSD&){refresh();});
        getChild<LLButton>("stop")->setClickedCallback([this](LLUICtrl*, const LLSD&){mScanning=false; rebuild();});
        getChild<LLButton>("filter")->setClickedCallback([this](LLUICtrl*, const LLSD&){rebuild();});
        getChild<LLButton>("focus")->setClickedCallback([this](LLUICtrl*, const LLSD&){locate(true);});
        getChild<LLButton>("beacon")->setClickedCallback([this](LLUICtrl*, const LLSD&){locate(false);});
        return true;
    }
    void onOpen(const LLSD&) override { refresh(); }
    void onClose(bool) override { mScanning=false; }
    void draw() override
    {
        auto region=gAgent.getRegion();
        if((region ? region->getRegionID() : LLUUID::null)!=mRegion)
        { mScanning=false; mEntries.clear(); mQueue.clear(); mNext=0;
          mRegion=region ? region->getRegionID() : LLUUID::null; rebuild(); }
        if(mTimer.getElapsedTimeF32()>=1.f)
        {
            mTimer.reset();
            if(mScanning && region)
                for(U32 n=0;n<16 && mNext<mQueue.size();++n)
                { auto o=gObjectList.findObject(mQueue[mNext++]);
                  if(eligible(o)) LLSelectMgr::getInstance()->requestObjectPropertiesFamily(o); }
            // Update cached owner names and distances at most once per second.
            if(mDirty || !mEntries.empty()) rebuild();
        }
        LLFloater::draw();
    }
    static void receive(LLMessageSystem* msg)
    {
        auto self=LLFloaterReg::findTypedInstance<KAMIAreaSearch>("kami_area_search");
        if(!self || !self->getVisible() || !self->mScanning) return;
        LLUUID id; msg->getUUIDFast(_PREHASH_ObjectData,_PREHASH_ObjectID,id);
        auto it=self->mEntries.find(id);
        auto o=gObjectList.findObject(id);
        if(it==self->mEntries.end() || !self->eligible(o) ||
           o->getRegion()->getHost()!=msg->getSender()) return;
        auto& e=it->second;
        msg->getStringFast(_PREHASH_ObjectData,_PREHASH_Name,e.name);
        msg->getStringFast(_PREHASH_ObjectData,_PREHASH_Description,e.description);
        msg->getUUIDFast(_PREHASH_ObjectData,_PREHASH_OwnerID,e.owner);
        e.ready=true; self->mDirty=true;
    }
};
#endif
