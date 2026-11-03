#include "kami/protocol/caps.hpp"
#include <objbase.h>
#include <string>
using namespace kami::protocol::caps;
namespace {
int exercise(){
    const std::wstring trusted=
        L"https://simhost-123.agni.secondlife.io:12043/cap/abc";
    if(!trusted_caps_url(trusted))return 1;
    if(!trusted_caps_url(L"https://sim1234.agni.lindenlab.com:12043/cap/x"))
        return 2;
    for(const auto* url:{
        L"http://simhost-123.agni.secondlife.io:12043/cap/abc",
        L"https://localhost/cap/abc",
        L"https://127.0.0.1/cap/abc",
        L"https://lindenlab.com.evil.example/cap/abc",
        L"https://simhost-123.agni.secondlife.io:12043/",
        L"https://simhost-123.agni.secondlife.io:12043/cap/x#frag",
        L"https://user@simhost-123.agni.secondlife.io/cap/x"
    })if(trusted_caps_url(url))return 3;
    const std::string seed=
        "<?xml version=\"1.0\"?><llsd><map>"
        "<key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io:12043/cap/abc</string>"
        "<key>ViewerAsset</key><string>https://simhost-123.agni.secondlife.io/cap/z</string>"
        "</map></llsd>";
    std::wstring cap;
    if(!parse_seed_llsd(seed,cap)||cap!=trusted)return 4;
    if(parse_seed_llsd("<llsd><map><key>EventQueueGet</key>"
        "<string>http://localhost/cap/no</string></map></llsd>",cap))
        return 5;
    if(parse_seed_llsd("<!DOCTYPE x [ <!ENTITY x SYSTEM "
         "\"file:///etc/passwd\"> ]><llsd><map></map></llsd>",cap))
        return 6;
    unsigned count=255;
    const std::string empty=
        "<llsd><map><key>id</key><integer>12345</integer>"
        "<key>events</key><array/></map></llsd>";
    if(!parse_event_queue_llsd(empty,count)||count!=0)return 7;
    const std::string events=
        "<llsd><map><key>id</key><integer>12345</integer>"
        "<key>events</key><array><map><key>message</key>"
        "<string>EnableSimulator</string></map></array></map></llsd>";
    if(!parse_event_queue_llsd(events,count)||count!=1)return 8;
    if(parse_event_queue_llsd(
        "<llsd><map><key>events</key><array/></map></llsd>",count))
        return 9;
    if(parse_event_queue_llsd(
        "<llsd><map><key>id</key><integer>1</integer>"
        "<key>events</key><string>bad</string></map></llsd>",count))
        return 10;
    return 0;
}
}
int main(){
    const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(hr))return 40;
    const auto result=exercise();
    CoUninitialize();
    return result;
}
