#include "kami/protocol/seed_caps.hpp"
#include <windows.h>
#include <objbase.h>
#include <string>
#include <iostream>

using namespace kami::protocol::caps;
int main(){
    const auto request=seed_request_xml();
    if(!request.starts_with("<?xml version=\"1.0\"?>")||
       request.find("<llsd><array>")==std::string::npos||
       request.find("<string>EventQueueGet</string>")==std::string::npos||
       request.find("<string>ViewerAsset</string>")==std::string::npos||
       request.find("<string>GetMesh</string>")==std::string::npos)
        return 1;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(com))return 2;
    const std::string good=
        "<?xml version=\"1.0\"?><llsd><map>"
        "<key>EventQueueGet</key><string>"
        "https://sim1234.agni.lindenlab.com:12345/cap/eventqueue"
        "</string>"
        "<key>SimulatorFeatures</key><string>"
        "https://sim1234.agni.lindenlab.com:12345/cap/features"
        "</string></map></llsd>";
    const auto valid=parse_seed_reply(good);
    if(!valid.queue_granted()||valid.granted_names!=2U||
       valid.diagnostic().find(L"GRANTED")==std::wstring::npos){
        CoUninitialize();return 3;
    }
    const auto missing=parse_seed_reply(
        "<llsd><map><key>SimulatorFeatures</key><string>"
        "https://sim1234.agni.lindenlab.com:12345/cap/features"
        "</string></map></llsd>");
    if(missing.status!=SeedStatus::NoEventQueue){
        CoUninitialize();return 4;
    }
    const auto unsafe=parse_seed_reply(
        "<llsd><map><key>EventQueueGet</key><string>"
        "https://example.org/cap/bad"
        "</string></map></llsd>");
    if(unsafe.status!=SeedStatus::NoEventQueue){
        CoUninitialize();return 5;
    }
    const auto duplicate=parse_seed_reply(
        "<llsd><map><key>EventQueueGet</key><string>"
        "https://sim1234.agni.lindenlab.com/cap/one"
        "</string><key>EventQueueGet</key><string>"
        "https://sim1234.agni.lindenlab.com/cap/two"
        "</string></map></llsd>");
    if(duplicate.status!=SeedStatus::InvalidReply){
        CoUninitialize();return 6;
    }
    const auto dtd=parse_seed_reply(
        "<?xml version=\"1.0\"?><!DOCTYPE llsd ["
        "<!ENTITY x SYSTEM \"file:///C:/Windows/win.ini\">]>"
        "<llsd><map><key>EventQueueGet</key>"
        "<string>&x;</string></map></llsd>");
    if(dtd.status!=SeedStatus::InvalidReply){
        CoUninitialize();return 7;
    }
    const auto malformed=parse_seed_reply("<llsd><map>");
    if(malformed.status!=SeedStatus::InvalidReply){
        CoUninitialize();return 8;
    }
    CoUninitialize();
    std::cout<<"PASS: offline LLSD seed negotiation and URL trust gates\n";
    return 0;
}
