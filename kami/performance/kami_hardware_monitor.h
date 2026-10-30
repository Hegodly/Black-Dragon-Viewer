// KAMI Beyond Reality | Fire Garden | compact Windows hardware monitor.
// Copyright (c) 2026 KAMI contributors. SPDX-License-Identifier: LGPL-2.1-only
// Metrics are sampled ONLY while this floater is visible, once per second.
// GPU engine uses Windows PDH "busiest engine" sampling; it is NOT a synthetic
// total GPU percent. Counter unavailable -> N/A (never invent a reading).
#pragma once

#include "llfloater.h"
#include "lltextbox.h"
#include "lltimer.h"
#include "llviewerstats.h"
#include "lltrace.h"
#include "llviewercontrol.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <dxgi1_4.h>
#include <string>
#include <cmath>
#include <vector>
#pragma comment(lib,"pdh.lib")
#pragma comment(lib,"psapi.lib")
#pragma comment(lib,"dxgi.lib")

class KAMIHardwareMonitor final : public LLFloater
{
public:
    explicit KAMIHardwareMonitor(const LLSD& key): LLFloater(key) {}
    ~KAMIHardwareMonitor() override
    {
        if (mGPUQuery) PdhCloseQuery(mGPUQuery);
    }

    bool postBuild() override
    {
        mTimer.setTimerExpirySec(0.1f);
        // This counter may not exist on some GPUs, integrated graphics or
        // Windows setups. Hardware availability is explicit in the UI.
        if (PdhOpenQueryW(nullptr, 0, &mGPUQuery)==ERROR_SUCCESS)
        {
            if (PdhAddEnglishCounterW(mGPUQuery, L"\\GPU Engine(*)\\Utilization Percentage",
                                      0, &mGPUCounter)==ERROR_SUCCESS)
            {
                PdhCollectQueryData(mGPUQuery);
            }
            else
            {
                PdhCloseQuery(mGPUQuery);
                mGPUQuery = nullptr;
                mGPUCounter = nullptr;
            }
        }
        return true;
    }

    void draw() override
    {
        if (mTimer.hasExpired())
        {
            sample();
            mTimer.setTimerExpirySec(1.0f);
        }
        LLFloater::draw();
    }
private:
    static unsigned long long ticks(const FILETIME& f)
    {
        return (static_cast<unsigned long long>(f.dwHighDateTime)<<32) |
               static_cast<unsigned long long>(f.dwLowDateTime);
    }
    void label(const char* name,const std::string& value)
    {
        if (LLTextBox* text=findChild<LLTextBox>(name))
            text->setText(LLStringExplicit(value));
    }
    void sample()
    {
        const F64 fps = LLTrace::get_frame_recording().getPeriodMedianPerSec(
            LLStatViewer::FPS, 50);
        if (fps > 0.1 && std::isfinite(fps))
        {
            label("kami_hw_fps", llformat("FPS: %.1f (real frames)",fps));
            label("kami_hw_frame", llformat("Frame time: %.2f ms (approx.)",1000.0/fps));
        }
        else
        {
            label("kami_hw_fps","FPS: collecting...");
            label("kami_hw_frame","Frame time: collecting...");
        }

        FILETIME idle={}, kernel={}, user={};
        if (GetSystemTimes(&idle,&kernel,&user))
        {
            const auto idleNow=ticks(idle), kernelNow=ticks(kernel), userNow=ticks(user);
            const auto total=(kernelNow-mPrevKernel)+(userNow-mPrevUser);
            const auto idleElapsed=idleNow-mPrevIdle;
            if(mPrevKernel && total>0 && total>=idleElapsed)
                label("kami_hw_cpu", llformat("CPU (system): %.1f%%",
                      100.0*(1.0-static_cast<double>(idleElapsed)/static_cast<double>(total))));
            else label("kami_hw_cpu","CPU (system): collecting...");
            mPrevIdle=idleNow;mPrevKernel=kernelNow;mPrevUser=userNow;
        }
        else label("kami_hw_cpu","CPU (system): N/A");

        MEMORYSTATUSEX memory={sizeof(memory)};
        if(GlobalMemoryStatusEx(&memory))
            label("kami_hw_ram", llformat("RAM (system): %.1f / %.1f GB (%.0f%%)",
                  static_cast<double>(memory.ullTotalPhys-memory.ullAvailPhys)/1073741824.0,
                  static_cast<double>(memory.ullTotalPhys)/1073741824.0,
                  100.0*static_cast<double>(memory.ullTotalPhys-memory.ullAvailPhys) /
                  static_cast<double>(memory.ullTotalPhys)));
        else label("kami_hw_ram","RAM (system): N/A");
        PROCESS_MEMORY_COUNTERS pmc={};
        if(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            label("kami_hw_process", llformat("KAMI process RAM: %.1f GB",
                                           static_cast<double>(pmc.WorkingSetSize)/1073741824.0));
        else label("kami_hw_process","KAMI process RAM: N/A");

        if(mGPUQuery && mGPUCounter && PdhCollectQueryData(mGPUQuery)==ERROR_SUCCESS)
        {
            DWORD bytes=0,count=0;
            PDH_STATUS result=PdhGetFormattedCounterArrayW(
                mGPUCounter,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
            if(result==PDH_MORE_DATA && bytes>0 && bytes<(32u*1024u*1024u))
            {
                std::vector<unsigned char> data(bytes);
                auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data());
                if(PdhGetFormattedCounterArrayW(mGPUCounter,PDH_FMT_DOUBLE,
                                                &bytes,&count,items)==ERROR_SUCCESS)
                {
                    double highest=-1;
                    for(DWORD i=0;i<count;++i)
                        if(items[i].FmtValue.CStatus==PDH_CSTATUS_VALID_DATA &&
                           std::isfinite(items[i].FmtValue.doubleValue))
                            highest=std::max(highest,items[i].FmtValue.doubleValue);
                    if(highest>=0)
                        label("kami_hw_gpu",llformat("GPU busiest engine: %.1f%%",llclamp(highest,0.0,100.0)));
                    else label("kami_hw_gpu","GPU busiest engine: N/A");
                }
                else label("kami_hw_gpu","GPU busiest engine: N/A");
            }
            else label("kami_hw_gpu","GPU busiest engine: N/A");
        }
        else label("kami_hw_gpu","GPU busiest engine: N/A");

        // QueryVideoMemoryInfo reports allocation for DXGI adapter #0 only;
        // label this honestly, as the active OpenGL GPU may be another adapter.
        IDXGIFactory1* factory=nullptr;
        IDXGIAdapter1* adapter=nullptr;
        IDXGIAdapter3* vramAdapter=nullptr;
        if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                      reinterpret_cast<void**>(&factory))) && factory &&
           SUCCEEDED(factory->EnumAdapters1(0,&adapter)) && adapter &&
           SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),
                      reinterpret_cast<void**>(&vramAdapter))) && vramAdapter)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO info={};
            if(SUCCEEDED(vramAdapter->QueryVideoMemoryInfo(0,
                         DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info)))
                label("kami_hw_vram",llformat("VRAM (adapter 0): %.1f / %.1f GB (allocation/budget)",
                      static_cast<double>(info.CurrentUsage)/1073741824.0,
                      static_cast<double>(info.Budget)/1073741824.0));
            else label("kami_hw_vram","VRAM (adapter 0): N/A");
        }
        else label("kami_hw_vram","VRAM (adapter 0): N/A");
        if(vramAdapter)vramAdapter->Release();
        if(adapter)adapter->Release();
        if(factory)factory->Release();

        label("kami_hw_resolution",gSavedSettings.getString("KAMILiveResolutionStatus"));
        label("kami_hw_note","Updates 1x/sec only while this window is open. "
              "GPU engine and adapter-0 VRAM are Windows hardware counters. "
              "They may differ from total GPU usage or KAMI's OpenGL adapter.");
    }

    LLTimer mTimer;
    unsigned long long mPrevIdle=0,mPrevKernel=0,mPrevUser=0;
    PDH_HQUERY mGPUQuery=nullptr;
    PDH_HCOUNTER mGPUCounter=nullptr;
};
