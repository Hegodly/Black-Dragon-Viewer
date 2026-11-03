#include "kami/protocol/seed_caps.hpp"
#include "kami/protocol/transport.hpp"
#include <windows.h>
#include <winhttp.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <string>
#include <array>
#include <algorithm>

namespace kami::protocol::caps {
namespace {
using Microsoft::WRL::ComPtr;
struct Http {
    HINTERNET h=nullptr;
    explicit Http(HINTERNET v=nullptr):h(v){}
    ~Http(){if(h)WinHttpCloseHandle(h);}
    Http(const Http&)=delete;
    Http& operator=(const Http&)=delete;
};
struct BStr {
    BSTR b=nullptr;
    explicit BStr(const wchar_t* text):b(SysAllocString(text)){}
    explicit BStr(std::wstring_view text):
        b(SysAllocStringLen(text.data(),static_cast<UINT>(text.size()))){}
    ~BStr(){SysFreeString(b);}
};
std::wstring content(IXMLDOMNode* node){
    if(!node)return {};
    BSTR str=nullptr;
    if(FAILED(node->get_text(&str))||!str)return {};
    std::wstring value(str,SysStringLen(str));
    SysFreeString(str);
    return value;
}
ComPtr<IXMLDOMNode> select(IXMLDOMNode* parent,const wchar_t* xpath){
    ComPtr<IXMLDOMNode> result;
    if(parent){BStr path(xpath);(void)parent->selectSingleNode(path.b,&result);}
    return result;
}
bool allowed_simulator_host(std::wstring_view host) noexcept {
    // Signed URLs can authorize access to an account's session. Send
    // them only to Linden Lab/Second Life infrastructure.
    const auto ends=[&](std::wstring_view suffix){
        return host.size()>suffix.size()&&host.ends_with(suffix);
    };
    if(host==L"secondlife.com"||host==L"lindenlab.com")return true;
    return ends(L".secondlife.io")||ends(L".secondlife.com")||
           ends(L".lindenlab.com");
}
bool crack(std::wstring_view url,std::wstring& host,
           INTERNET_PORT& port,std::wstring& path) {
    if(!valid_capability_url(url))return false;
    URL_COMPONENTS parts{};
    parts.dwStructSize=sizeof(parts);
    parts.dwSchemeLength=static_cast<DWORD>(-1);
    parts.dwHostNameLength=static_cast<DWORD>(-1);
    parts.dwUrlPathLength=static_cast<DWORD>(-1);
    parts.dwExtraInfoLength=static_cast<DWORD>(-1);
    if(url.size()>2048||!WinHttpCrackUrl(url.data(),
       static_cast<DWORD>(url.size()),0,&parts)||
       parts.nScheme!=INTERNET_SCHEME_HTTPS||
       !parts.lpszHostName||!parts.lpszUrlPath||
       parts.dwHostNameLength==0||parts.dwUrlPathLength==0)return false;
    host.assign(parts.lpszHostName,parts.dwHostNameLength);
    std::transform(host.begin(),host.end(),host.begin(),
        [](wchar_t x){return x>=L'A'&&x<=L'Z'?x+32:x;});
    if(!allowed_simulator_host(host))return false;
    port=parts.nPort;
    path.assign(parts.lpszUrlPath,parts.dwUrlPathLength);
    if(parts.lpszExtraInfo&&parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    return path.starts_with(L"/")&&path.size()<2048;
}
}
std::string seed_request_xml(){
    // LLViewerRegionImpl sends a list of names to its Seed capability;
    // LLSD serializes a string array under <llsd><array>.
    return "<?xml version=\"1.0\"?><llsd><array>"
           "<string>EventQueueGet</string>"
           "<string>SimulatorFeatures</string>"
           "<string>ViewerAsset</string>"
           "<string>GetMesh</string>"
           "</array></llsd>";
}
SeedResult parse_seed_reply(std::string_view xml){
    SeedResult result{};
    result.status=SeedStatus::InvalidReply;
    if(xml.empty()||xml.size()>1024U*1024U)return result;
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        xml.data(),static_cast<int>(xml.size()),nullptr,0);
    if(n<1)return result;
    std::wstring utf16(static_cast<std::size_t>(n),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        xml.data(),static_cast<int>(xml.size()),utf16.data(),n)!=n)return result;
    ComPtr<IXMLDOMDocument2> doc;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,
       CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&doc)))||!doc)return result;
    (void)doc->put_async(VARIANT_FALSE);
    (void)doc->put_resolveExternals(VARIANT_FALSE);
    (void)doc->put_validateOnParse(VARIANT_FALSE);
    BStr prop(L"ProhibitDTD");
    VARIANT yes;VariantInit(&yes);yes.vt=VT_BOOL;yes.boolVal=VARIANT_TRUE;
    if(FAILED(doc->setProperty(prop.b,yes)))return result;
    BStr body(utf16);
    VARIANT_BOOL parsed=VARIANT_FALSE;
    if(FAILED(doc->loadXML(body.b,&parsed))||parsed!=VARIANT_TRUE)return result;
    auto map=select(doc.Get(),L"/llsd/map");
    if(!map)return result;
    ComPtr<IXMLDOMNodeList> items;
    BStr keypath(L"./key");
    if(FAILED(map->selectNodes(keypath.b,&items))||!items)return result;
    long count=0;
    if(FAILED(items->get_length(&count))||count<1||count>256)return result;
    for(long i=0;i<count;++i){
        ComPtr<IXMLDOMNode> key;
        if(FAILED(items->get_item(i,&key))||!key)return result;
        auto name=content(key.Get());
        if(name.empty()||name.size()>90)return result;
        auto value=select(key.Get(),L"following-sibling::*[1]");
        if(!value)return result;
        BSTR type=nullptr;
        if(FAILED(value->get_nodeName(&type))||!type)return result;
        const bool is_string=std::wstring_view(type,SysStringLen(type))==L"string";
        SysFreeString(type);
        if(!is_string)continue;
        const auto url=content(value.Get());
        if(url.size()>2048)return result;
        if(name==L"EventQueueGet"&&valid_capability_url(url))
            result.event_queue_url=url;
        if(valid_capability_url(url))
            ++result.granted_names;
    }
    result.status=result.event_queue_url.empty()?
        SeedStatus::NoEventQueue:SeedStatus::Granted;
    return result;
}
std::wstring SeedResult::diagnostic() const{
    switch(status){
    case SeedStatus::NotRequested:return L"SEED CAPS NOT REQUESTED";
    case SeedStatus::InvalidUrl:return L"SEED CAPS URL REJECTED";
    case SeedStatus::NetworkError:return L"SEED CAPS HTTPS NETWORK ERROR";
    case SeedStatus::HttpError:return L"SEED CAPS HTTP "+std::to_wstring(http_status);
    case SeedStatus::InvalidReply:return L"SEED CAPS REPLY INVALID";
    case SeedStatus::NoEventQueue:return L"SEED CAPS RECEIVED; EVENT QUEUE NOT GRANTED";
    case SeedStatus::Granted:return L"SEED CAPS RECEIVED; EVENT QUEUE GRANTED (NOT POLLING)";
    }
    return L"UNKNOWN SEED CAPS STATE";
}
SeedResult request_seed_capabilities(std::wstring_view seed_url){
    SeedResult result{};
    result.status=SeedStatus::InvalidUrl;
    std::wstring host,path;INTERNET_PORT port=0;
    if(!crack(seed_url,host,port,path))return result;
    result.status=SeedStatus::NetworkError;
    Http session(WinHttpOpen(L"KAMI Beyond Reality Dev 0.1.0 (caps)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.h)return result;
    WinHttpSetTimeouts(session.h,6000,6000,10000,12000);
    Http connection(WinHttpConnect(session.h,host.c_str(),port,0));
    if(!connection.h)return result;
    Http request(WinHttpOpenRequest(connection.h,L"POST",path.c_str(),nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.h)return result;
    DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(request.h,WINHTTP_OPTION_DISABLE_FEATURE,
                         &disabled,sizeof(disabled)))return result;
    const auto payload=seed_request_xml();
    constexpr wchar_t headers[]=L"Content-Type: application/llsd+xml\r\n";
    if(!WinHttpSendRequest(request.h,headers,static_cast<DWORD>(-1),
           const_cast<char*>(payload.data()),
           static_cast<DWORD>(payload.size()),
           static_cast<DWORD>(payload.size()),0)||
       !WinHttpReceiveResponse(request.h,nullptr))return result;
    DWORD status=0,bytes=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,
       WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
       WINHTTP_HEADER_NAME_BY_INDEX,&status,&bytes,WINHTTP_NO_HEADER_INDEX))
        return result;
    result.http_status=status;
    if(status!=200){result.status=SeedStatus::HttpError;return result;}
    std::string reply;
    while(reply.size()<=1024U*1024U){
        std::array<char,4096> chunk{};
        DWORD got=0;
        if(!WinHttpReadData(request.h,chunk.data(),
              static_cast<DWORD>(chunk.size()),&got))return result;
        if(!got)break;
        reply.append(chunk.data(),got);
    }
    if(reply.size()>1024U*1024U){result.status=SeedStatus::InvalidReply;return result;}
    return parse_seed_reply(reply);
}
} // namespace kami::protocol::caps
