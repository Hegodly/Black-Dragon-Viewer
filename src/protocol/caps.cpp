#include "kami/protocol/caps.hpp"
#include "kami/protocol/transport.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <windows.h>
#include <winhttp.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace kami::protocol::caps {
namespace {
using Microsoft::WRL::ComPtr;
struct HttpHandle {
    HINTERNET value=nullptr;
    explicit HttpHandle(HINTERNET h=nullptr):value(h){}
    ~HttpHandle(){if(value)WinHttpCloseHandle(value);}
    HttpHandle(const HttpHandle&)=delete;
    HttpHandle& operator=(const HttpHandle&)=delete;
};
struct BStr {
    BSTR value=nullptr;
    explicit BStr(const wchar_t* p):value(SysAllocString(p)){}
    explicit BStr(std::wstring_view s):value(
        SysAllocStringLen(s.data(),static_cast<UINT>(s.size()))){}
    ~BStr(){if(value)SysFreeString(value);}
    BStr(const BStr&)=delete;
    BStr& operator=(const BStr&)=delete;
};
struct Parts {
    std::wstring host,path;
    INTERNET_PORT port=0;
    bool secure=true;
};
bool suffix(std::wstring_view s,std::wstring_view end) noexcept {
    return s.size()>end.size()&&s.ends_with(end);
}
// Second Life CAPS can legitimately point to separately hosted HTTPS
// CDNs. The official protocol says capability URLs do not imply a fixed
// host, port or "/cap/" path. Accept those mesh endpoints ONLY when
// granted by authenticated seed LLSD, never from arbitrary chat/UI input.
// Continue to enforce the narrower Linden-owned rule for EventQueue.
bool public_mesh_host(std::wstring_view host) noexcept {
    if(host.size()<4U||host.size()>253U||host.front()==L'.'||
       host.back()==L'.')return false;
    for(const auto forbidden:{
        L".localhost",L".local",L".internal",L".lan",L".home",
        L".test",L".invalid",L".example",L".onion",
        L".example.com",L".example.net",L".example.org"}){
        if(host.ends_with(forbidden))return false;
    }
    for(const auto exact:{L"example.com",L"example.net",L"example.org"})
        if(host==exact)return false;
    unsigned dots=0;
    bool any_letter=false;
    std::size_t begin=0U;
    for(std::size_t i=0;i<=host.size();++i){
        if(i!=host.size()&&host[i]!=L'.'){
            const auto ch=host[i];
            if(ch>=L'a'&&ch<=L'z')any_letter=true;
            else if(!(ch>=L'0'&&ch<=L'9')&&ch!=L'-')return false;
        }
        if(i==host.size()||host[i]==L'.'){
            const auto length=i-begin;
            if(length==0U||length>63U||host[begin]==L'-'||
               host[i-1U]==L'-')return false;
            if(i!=host.size())++dots;
            begin=i+1U;
        }
    }
    // Reject IP literals and single-label/private machine names.
    return dots>=1U&&any_letter;
}
bool parse_url(std::wstring_view input,Parts& result,
               bool authenticated_mesh=false) noexcept {
    if(authenticated_mesh){
        if(input.size()<12U||input.size()>2048U||
           !(input.starts_with(L"https://")||
             input.starts_with(L"http://"))||
           input.find_first_of(L"\r\n\t")!=std::wstring_view::npos||
           input.find(wchar_t{0})!=std::wstring_view::npos||
           input.find_first_of(L"#@\\")!=std::wstring_view::npos)
            return false;
    }else if(!valid_capability_url(input))return false;
    URL_COMPONENTS u{};
    u.dwStructSize=sizeof(u);
    u.dwSchemeLength=DWORD(-1);
    u.dwHostNameLength=DWORD(-1);
    u.dwUrlPathLength=DWORD(-1);
    u.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(input.data(),static_cast<DWORD>(input.size()),0,&u)||
       !(u.nScheme==INTERNET_SCHEME_HTTPS||
         (authenticated_mesh&&u.nScheme==INTERNET_SCHEME_HTTP))||
       !u.lpszHostName||u.dwHostNameLength==0U)return false;
    std::wstring host(u.lpszHostName,u.dwHostNameLength);
    std::transform(host.begin(),host.end(),host.begin(),[](wchar_t c){
        return c>=L'A'&&c<=L'Z'?static_cast<wchar_t>(c+32):c;
    });
    if(authenticated_mesh){
        if(!public_mesh_host(host))return false;
        // Legacy SL GetMesh/ViewerAsset CDN grants can be plaintext HTTP.
        // Permit HTTP only to Linden-controlled asset-cdn.*.lindenlab.com
        // on port 80. All other mesh CDN grants still require HTTPS.
        if(u.nScheme==INTERNET_SCHEME_HTTP&&
           (!host.starts_with(L"asset-cdn.")||
            !suffix(host,L".lindenlab.com")||
            (u.nPort!=0U&&u.nPort!=INTERNET_DEFAULT_HTTP_PORT)))
            return false;
    }else if(!(suffix(host,L".lindenlab.com")||
               suffix(host,L".secondlife.io")||
               suffix(host,L".secondlife.com")))return false;
    std::wstring path;
    if(u.lpszUrlPath&&u.dwUrlPathLength)
        path.assign(u.lpszUrlPath,u.dwUrlPathLength);
    if(path.empty())path=L"/"; // mesh CDN grants may have no path
    if(u.lpszExtraInfo&&u.dwExtraInfoLength)
        path.append(u.lpszExtraInfo,u.dwExtraInfoLength);
    if(path.size()>2048U||path[0]!=L'/')return false;
    for(const auto ch:path)
        if(ch<32||ch==127)return false;
    result.host=std::move(host);
    result.path=std::move(path);
    result.port=u.nPort?u.nPort:
        (u.nScheme==INTERNET_SCHEME_HTTPS?
            INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT);
    result.secure=u.nScheme==INTERNET_SCHEME_HTTPS;
    return true;
}
// A bounded, non-sensitive rejection category for a server-provided URL.
// Never expose a capability hostname, signed path, query, or credential.
std::wstring mesh_url_problem(std::wstring_view url,
                              std::wstring_view type) noexcept {
    if(type!=L"uri"&&type!=L"string")return L"unsupported LLSD grant type";
    if(url.empty())return L"empty grant";
    if(!url.starts_with(L"https://"))return L"non-HTTPS scheme";
    if(url.size()>2048U||url.size()<12U||
       url.find_first_of(L"\r\n\t#@\\")!=std::wstring_view::npos||
       url.find(wchar_t{0})!=std::wstring_view::npos)
        return L"unsafe URL syntax";
    URL_COMPONENTS u{};
    u.dwStructSize=sizeof(u);
    u.dwSchemeLength=DWORD(-1);
    u.dwHostNameLength=DWORD(-1);
    u.dwUrlPathLength=DWORD(-1);
    u.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.data(),static_cast<DWORD>(url.size()),0,&u))
        return L"unparseable URL";
    if(u.nScheme!=INTERNET_SCHEME_HTTPS)return L"non-HTTPS scheme";
    if(!u.lpszHostName||u.dwHostNameLength==0U)return L"missing hostname";
    std::wstring host(u.lpszHostName,u.dwHostNameLength);
    std::transform(host.begin(),host.end(),host.begin(),[](wchar_t x){
        return x>=L'A'&&x<=L'Z'?static_cast<wchar_t>(x+32):x;
    });
    if(!public_mesh_host(host))return L"hostname not public";
    std::wstring path;
    if(u.lpszUrlPath&&u.dwUrlPathLength)
        path.assign(u.lpszUrlPath,u.dwUrlPathLength);
    if(path.empty())path=L"/";
    if(u.lpszExtraInfo&&u.dwExtraInfoLength)
        path.append(u.lpszExtraInfo,u.dwExtraInfoLength);
    if(path.size()>2048U||path.empty()||path[0]!=L'/')
        return L"invalid URL path";
    for(wchar_t ch:path)
        if(ch<32||ch==127)return L"invalid URL path";
    return L"unsupported URL layout";
}
bool to_wide(std::string_view source,std::wstring& output) {
    if(source.empty()||source.size()>4U*1024U*1024U)return false;
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        source.data(),static_cast<int>(source.size()),nullptr,0);
    if(n<=0)return false;
    output.resize(static_cast<std::size_t>(n));
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        source.data(),static_cast<int>(source.size()),
        output.data(),n)==n;
}
bool parse_dom(std::string_view xml,ComPtr<IXMLDOMDocument2>& dom) {
    std::wstring wide;
    if(!to_wide(xml,wide))return false;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,
        CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dom.GetAddressOf())))||!dom)
        return false;
    (void)dom->put_async(VARIANT_FALSE);
    (void)dom->put_resolveExternals(VARIANT_FALSE);
    (void)dom->put_validateOnParse(VARIANT_FALSE);
    BStr prohibited(L"ProhibitDTD");
    VARIANT option;VariantInit(&option);
    option.vt=VT_BOOL;option.boolVal=VARIANT_TRUE;
    if(FAILED(dom->setProperty(prohibited.value,option)))return false;
    BStr document(wide);
    VARIANT_BOOL accepted=VARIANT_FALSE;
    return SUCCEEDED(dom->loadXML(document.value,&accepted))&&
           accepted==VARIANT_TRUE;
}
bool child_elements(IXMLDOMDocument2* dom,ComPtr<IXMLDOMNodeList>& list) {
    BStr xpath(L"/llsd/map/*");
    return SUCCEEDED(dom->selectNodes(xpath.value,list.GetAddressOf()))&&
           list!=nullptr;
}
std::wstring node_name(IXMLDOMNode* node){
    BSTR b=nullptr;
    if(!node||FAILED(node->get_nodeName(&b))||!b)return {};
    std::wstring s(b,SysStringLen(b));
    SysFreeString(b);return s;
}
std::wstring node_text(IXMLDOMNode* node){
    BSTR b=nullptr;
    if(!node||FAILED(node->get_text(&b))||!b)return {};
    std::wstring s(b,SysStringLen(b));
    SysFreeString(b);return s;
}
struct HttpReply {
    unsigned status=0,error=0;
    std::string body;
};
HttpReply send_signed_post(std::wstring_view url,std::string_view body,
                           DWORD receive_timeout_ms,std::size_t max_bytes) {
    HttpReply reply{};
    Parts target;
    if(!parse_url(url,target)){reply.error=ERROR_INVALID_PARAMETER;return reply;}
    HttpHandle session(WinHttpOpen(L"KAMI CAPS/0.1 (experimental)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value){reply.error=GetLastError();return reply;}
    WinHttpSetTimeouts(session.value,10000,10000,receive_timeout_ms,
                       receive_timeout_ms);
    HttpHandle connection(WinHttpConnect(session.value,target.host.c_str(),
                                        target.port,0));
    if(!connection.value){reply.error=GetLastError();return reply;}
    HttpHandle req(WinHttpOpenRequest(connection.value,L"POST",
        target.path.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!req.value){reply.error=GetLastError();return reply;}
    DWORD redirects=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(req.value,WINHTTP_OPTION_DISABLE_FEATURE,
                         &redirects,sizeof(redirects))){
        reply.error=GetLastError();return reply;
    }
    constexpr wchar_t headers[]=
        L"Content-Type: application/llsd+xml\r\n"
        L"Accept: application/llsd+xml\r\n";
    if(!WinHttpSendRequest(req.value,headers,static_cast<DWORD>(-1),
         const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),
         static_cast<DWORD>(body.size()),0)){
        reply.error=GetLastError();return reply;
    }
    if(!WinHttpReceiveResponse(req.value,nullptr)){
        reply.error=GetLastError();return reply;
    }
    DWORD status=0,len=sizeof(status);
    if(!WinHttpQueryHeaders(req.value,
        WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&len,WINHTTP_NO_HEADER_INDEX)){
        reply.error=GetLastError();return reply;
    }
    reply.status=status;
    if(status!=200)return reply;
    std::array<char,8192> buffer{};
    while(reply.body.size()<=max_bytes){
        DWORD read=0;
        if(!WinHttpReadData(req.value,buffer.data(),
            static_cast<DWORD>(buffer.size()),&read)){
            reply.error=GetLastError();reply.body.clear();
            SecureZeroMemory(buffer.data(),buffer.size());
            return reply;
        }
        if(!read)break;
        if(reply.body.size()+read>max_bytes){
            reply.error=ERROR_INSUFFICIENT_BUFFER;reply.body.clear();
            SecureZeroMemory(buffer.data(),buffer.size());
            return reply;
        }
        reply.body.append(buffer.data(),read);
    }
    SecureZeroMemory(buffer.data(),buffer.size());
    return reply;
}
}
bool retryable_event_queue_http_error(unsigned code) noexcept {
    switch(code){
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_RESEND_REQUEST:
    case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
        return true;
    default:
        return false;
    }
}
bool possible_idle_event_queue_http_error(unsigned code,
                                          bool held_long) noexcept {
    // Some EventQueueGet long polls end in an HTTP/1.0 502/upstream
    // response. If WinHTTP cannot parse that response, keep polling
    // conservatively. The response was NOT successfully processed,
    // and the last ACK must remain unchanged.
    return held_long &&
        (code==ERROR_WINHTTP_INVALID_SERVER_RESPONSE||
         code==ERROR_WINHTTP_TIMEOUT);
}
bool trusted_caps_url(std::wstring_view url) noexcept {
    Parts dummy;
    return parse_url(url,dummy);
}
bool trusted_mesh_caps_url(std::wstring_view url) noexcept {
    Parts dummy;
    if(!parse_url(url,dummy,true))return false;
    // Never put a signed token, request path, or query string on a
    // plaintext HTTP connection. Only the old, public CDN root may
    // be used; fetch_mesh_asset later adds the asset UUID itself.
    return dummy.secure||dummy.path==L"/";
}
bool parse_seed_llsd(std::string_view xml,std::wstring& event_queue_url,
                     std::wstring* mesh_capability_url,
                     SeedParseReport* report,
                     std::wstring* texture_capability_url) {
    event_queue_url.clear();
    if(mesh_capability_url)mesh_capability_url->clear();
    if(texture_capability_url)texture_capability_url->clear();
    if(report)*report={};
    ComPtr<IXMLDOMDocument2> dom;
    if(!parse_dom(xml,dom))return false;
    ComPtr<IXMLDOMNodeList> nodes;
    if(!child_elements(dom.Get(),nodes))return false;
    long count=0;
    if(FAILED(nodes->get_length(&count))||count>500||count%2!=0)
        return false;
    if(report)report->xml_map=true;
    std::wstring viewer_asset,getmesh2,getmesh;
    bool event_seen=false;
    for(long i=0;i<count;i+=2){
        ComPtr<IXMLDOMNode> key,value;
        if(FAILED(nodes->get_item(i,key.GetAddressOf()))||!key||
           node_name(key.Get())!=L"key")return false;
        if(FAILED(nodes->get_item(i+1,value.GetAddressOf()))||!value)
            return false;
        const auto name=node_text(key.Get());
        const bool event=(name==L"EventQueueGet");
        const bool mesh=(name==L"ViewerAsset"||
                         name==L"GetMesh2"||name==L"GetMesh");
        if(!event&&!mesh)continue;
        if(report){
            if(event)report->event_named=true;
            else report->mesh_named=true;
        }
        // LLSD can legally grant <undef/> for unsupported capabilities.
        // An invalid or absent optional mesh grant must not destroy a
        // separate valid EventQueueGet grant.
        const auto type=node_name(value.Get());
        const auto url=node_text(value.Get());
        const bool valid=(type==L"string"||type==L"uri")&&
                         (event?trusted_caps_url(url):
                                trusted_mesh_caps_url(url));
        if(event){
            if(event_seen){ // reject ambiguous duplicate EventQueueGet
                event_queue_url.clear();
                if(report)report->event_rejected=true;
                continue;
            }
            event_seen=true;
            if(valid)event_queue_url=url;
            else if(report)report->event_rejected=true;
        }else if(valid){
            if(name==L"ViewerAsset")viewer_asset=url;
            else if(name==L"GetMesh2")getmesh2=url;
            else getmesh=url;
        }else if(report){
            report->mesh_rejected=true;
            if(report->mesh_issue.empty())
                report->mesh_issue=mesh_url_problem(url,type);
        }
    }
    if(texture_capability_url)
        *texture_capability_url=viewer_asset;
    if(mesh_capability_url){
        *mesh_capability_url=!viewer_asset.empty()?std::move(viewer_asset):
                             !getmesh2.empty()?std::move(getmesh2):
                             std::move(getmesh);
    }
    return !event_queue_url.empty()||
           (mesh_capability_url&&!mesh_capability_url->empty())||
           (texture_capability_url&&!texture_capability_url->empty());
}
bool parse_event_queue_llsd(std::string_view xml,unsigned& event_count) {
    event_count=0;
    ComPtr<IXMLDOMDocument2> dom;
    if(!parse_dom(xml,dom))return false;
    ComPtr<IXMLDOMNodeList> nodes;
    if(!child_elements(dom.Get(),nodes))return false;
    long count=0;
    if(FAILED(nodes->get_length(&count))||count<4||count>50)return false;
    bool id_found=false,events_found=false;
    for(long i=0;i<count-1;++i){
        ComPtr<IXMLDOMNode> key,value;
        if(FAILED(nodes->get_item(i,key.GetAddressOf()))||!key||
           node_name(key.Get())!=L"key")continue;
        const auto key_name=node_text(key.Get());
        if(key_name!=L"id"&&key_name!=L"events")continue;
        if(FAILED(nodes->get_item(i+1,value.GetAddressOf()))||!value)
            return false;
        const auto type=node_name(value.Get());
        if(key_name==L"id") {
            if(id_found||(type!=L"integer"&&type!=L"string"))return false;
            if(node_text(value.Get()).size()>32)return false;
            id_found=true;
        }else {
            if(events_found||type!=L"array")return false;
            ComPtr<IXMLDOMNodeList> children;
            if(FAILED(value->get_childNodes(children.GetAddressOf()))||
               !children)return false;
            long n=0;
            if(FAILED(children->get_length(&n))||n<0||n>512)return false;
            event_count=static_cast<unsigned>(n);
            events_found=true;
        }
    }
    return id_found&&events_found;
}
namespace {
// Only the trusted TeleportFailed LLSD event has a bounded UI handler.
// The official SL event body contains Info/Reason. Traverse no more than
// four LLSD map/array levels. Never echo URLs, capability paths, markup or
// arbitrary binary fields to the user interface.
std::string safe_failure_reason(std::wstring_view input){
    if(input.empty()||input.size()>120U)return {};
    std::string result;
    result.reserve(input.size());
    for(const wchar_t ch:input){
        const bool allowed=(ch>=L'A'&&ch<=L'Z')||
            (ch>=L'a'&&ch<=L'z')||
            (ch>=L'0'&&ch<=L'9')||
            ch==L' '||ch==L'.'||ch==L','||ch==L':'||
            ch==L';'||ch==L'?'||ch==L'!'||ch==L'-'||
            ch==L'\''||ch==L'('||ch==L')';
        if(!allowed)return {};
        result.push_back(static_cast<char>(ch));
    }
    auto lowercase=result;
    std::transform(lowercase.begin(),lowercase.end(),lowercase.begin(),
                   [](char c){return c>='A'&&c<='Z'?char(c+32):c;});
    if(lowercase.find("http")!=std::string::npos||
       lowercase.find("token")!=std::string::npos||
       lowercase.find("capability")!=std::string::npos)return {};
    return result;
}
std::string failure_reason_in(IXMLDOMNode* node,unsigned depth){
    if(!node||depth>4U)return {};
    const auto kind=node_name(node);
    if(kind!=L"map"&&kind!=L"array")return {};
    ComPtr<IXMLDOMNodeList> members;
    if(FAILED(node->get_childNodes(members.GetAddressOf()))||!members)
        return {};
    long n=0;
    if(FAILED(members->get_length(&n))||n<0||n>128L)return {};
    for(long i=0;i<n;++i){
        ComPtr<IXMLDOMNode> child;
        if(FAILED(members->get_item(i,child.GetAddressOf()))||!child)
            return {};
        if(node_name(child.Get())==L"key"&&i+1<n){
            const auto name=node_text(child.Get());
            ComPtr<IXMLDOMNode> value;
            if(FAILED(members->get_item(i+1,value.GetAddressOf()))||!value)
                return {};
            if((name==L"Reason"||name==L"reason")&&
               node_name(value.Get())==L"string"){
                const auto reason=safe_failure_reason(node_text(value.Get()));
                if(!reason.empty())return reason;
            }
            const auto nested=failure_reason_in(value.Get(),depth+1U);
            if(!nested.empty())return nested;
            ++i;
        }else{
            const auto nested=failure_reason_in(child.Get(),depth+1U);
            if(!nested.empty())return nested;
        }
    }
    return {};
}
std::string extract_teleport_failure(IXMLDOMNode* entry){
    if(!entry||node_name(entry)!=L"map")return {};
    ComPtr<IXMLDOMNodeList> members;
    if(FAILED(entry->get_childNodes(members.GetAddressOf()))||!members)
        return {};
    long n=0;
    if(FAILED(members->get_length(&n))||n<0||n>80L)return {};
    for(long i=0;i+1<n;++i){
        ComPtr<IXMLDOMNode> key,value;
        if(FAILED(members->get_item(i,key.GetAddressOf()))||!key||
           node_name(key.Get())!=L"key"||
           node_text(key.Get())!=L"body")continue;
        if(FAILED(members->get_item(i+1,value.GetAddressOf()))||!value)
            return {};
        return failure_reason_in(value.Get(),0U);
    }
    return {};
}
} // namespace
bool parse_event_queue_frame(std::string_view xml,EventQueueFrame& out) {
    out={};
    ComPtr<IXMLDOMDocument2> dom;
    if(!parse_dom(xml,dom))return false;
    ComPtr<IXMLDOMNodeList> nodes;
    if(!child_elements(dom.Get(),nodes))return false;
    long count=0;
    if(FAILED(nodes->get_length(&count))||count<4||count>50)return false;
    bool found_id=false,found_events=false;
    for(long i=0;i+1<count;++i){
        ComPtr<IXMLDOMNode> k,v;
        if(FAILED(nodes->get_item(i,k.GetAddressOf()))||!k||
           node_name(k.Get())!=L"key")continue;
        const auto key=node_text(k.Get());
        if(key!=L"id"&&key!=L"events")continue;
        if(FAILED(nodes->get_item(i+1,v.GetAddressOf()))||!v)return false;
        const auto type=node_name(v.Get());
        if(key==L"id"){
            if(found_id||(type!=L"integer"&&type!=L"string"&&
                          type!=L"undef"))return false;
            found_id=true;
            if(type==L"undef")continue;
            const auto id=node_text(v.Get());
            if(id.empty()||id.size()>64)return false;
            std::string safe_id;
            safe_id.reserve(id.size());
            for(std::size_t j=0;j<id.size();++j){
                const wchar_t ch=id[j];
                // Restrict IDs to printable non-markup ASCII. Integers
                // preserve signed-decimal LLSD type, strings are opaque.
                if(type==L"integer"){
                    if(!(ch>=L'0'&&ch<=L'9')&&
                       !(j==0&&ch==L'-'))return false;
                }else if(!(ch>=L'0'&&ch<=L'9')&&
                         !(ch>=L'A'&&ch<=L'Z')&&
                         !(ch>=L'a'&&ch<=L'z')&&
                         ch!=L'-'&&ch!=L'_'&&ch!=L'.')return false;
                safe_id.push_back(static_cast<char>(ch));
            }
            if(safe_id=="-")return false;
            out.id.token=std::move(safe_id);
            out.id.integer=(type==L"integer");
        }else {
            if(found_events||type!=L"array")return false;
            found_events=true;
            ComPtr<IXMLDOMNodeList> events;
            if(FAILED(v->get_childNodes(events.GetAddressOf()))||!events)
                return false;
            long n=0;
            if(FAILED(events->get_length(&n))||n<0||n>512)return false;
            // Capture only bounded message *names*, not opaque event bodies.
            // This helps identify which SL event decoders are needed next,
            // without logging bearer URLs or avatar message contents.
            for(long index=0;index<n;++index){
                ComPtr<IXMLDOMNode> entry;
                if(FAILED(events->get_item(index,entry.GetAddressOf()))||
                   !entry||node_name(entry.Get())!=L"map")return false;
                if(out.event_types.size()>=8U)continue;
                ComPtr<IXMLDOMNodeList> members;
                if(FAILED(entry->get_childNodes(members.GetAddressOf()))||
                   !members)return false;
                long mcount=0;
                if(FAILED(members->get_length(&mcount))||mcount>80)return false;
                std::string safe;
                for(long m=0;m+1<mcount;++m){
                    ComPtr<IXMLDOMNode> member_key,data;
                    if(FAILED(members->get_item(m,member_key.GetAddressOf()))||
                       !member_key||node_name(member_key.Get())!=L"key"||
                       node_text(member_key.Get())!=L"message")continue;
                    if(FAILED(members->get_item(m+1,data.GetAddressOf()))||
                       !data||node_name(data.Get())!=L"string")break;
                    const auto raw=node_text(data.Get());
                    if(raw.empty()||raw.size()>48U)break;
                    for(const wchar_t ch:raw){
                        if(!((ch>=L'A'&&ch<=L'Z')||
                             (ch>=L'a'&&ch<=L'z')||
                             (ch>=L'0'&&ch<=L'9')||
                             ch==L'_'||ch==L'-')){
                            safe.clear();break;
                        }
                        safe.push_back(static_cast<char>(ch));
                    }
                    break;
                }
                if(safe=="TeleportFailed"&&
                   out.notifications.size()<8U){
                    auto reason=extract_teleport_failure(entry.Get());
                    std::string notice="Teleport failed";
                    if(!reason.empty())notice+=": "+reason;
                    out.notifications.push_back(std::move(notice));
                    ++out.handled_events;
                }
                out.event_types.push_back(safe.empty()?
                    std::string("UnknownEvent"):std::move(safe));
            }
            out.event_count=static_cast<unsigned>(n);
        }
    }
    // A nonempty batch without an acknowledgement ID cannot be advanced
    // safely; avoid blindly retrying it with an undefined cursor.
    return found_id&&found_events&&
           (out.event_count==0U||!out.id.token.empty());
}
std::string build_event_queue_post(const QueueCursor& previous) {
    std::string body="<llsd><map><key>ack</key>";
    if(previous.token.empty()){
        body+="<undef />";
    }else{
        if(previous.token.size()>64)return {};
        for(std::size_t i=0;i<previous.token.size();++i){
            const auto ch=previous.token[i];
            if(previous.integer){
                if(!(ch>='0'&&ch<='9')&&!(i==0&&ch=='-'))
                    return {};
            }else if(!((ch>='0'&&ch<='9')||
                         (ch>='A'&&ch<='Z')||
                         (ch>='a'&&ch<='z')||
                         ch=='-'||ch=='_'||ch=='.'))return {};
        }
        if(previous.token=="-")return {};
        body+=previous.integer?"<integer>":"<string>";
        body+=previous.token;
        body+=previous.integer?"</integer>":"</string>";
    }
    body+="<key>done</key><boolean>false</boolean></map></llsd>";
    return body;
}
PollResponse observe_event_queue_response(std::string_view xml) {
    PollResponse result{};
    EventQueueFrame frame{};
    if(!parse_event_queue_frame(xml,frame)){
        result.diagnostic=L"EventQueueGet invalid LLSD envelope";
        return result;
    }
    result.event_count=frame.event_count;
    result.handled_events=frame.handled_events;
    result.unsupported_events=frame.event_count-frame.handled_events;
    result.notifications=std::move(frame.notifications);
    result.event_types=std::move(frame.event_types);
    result.structurally_valid=true;
    // Commit notifications on the UI thread before advancing the ACK.
    // Only the bounded TeleportFailed notice is handled; no region
    // transition or other LLSD event payload executes.
    result.safe_to_ack=true;
    result.next_ack=std::move(frame.id);
    result.diagnostic=result.event_count?
        L"EventQueueGet live: "+
            std::to_wstring(result.event_count)+
            L" event(s) observed; "+std::to_wstring(result.handled_events)+
            L" teleport failure notice(s) handled":
        L"EventQueueGet heartbeat; no events";
    return result;
}

MeshReply fetch_mesh_asset(std::wstring capability_url,std::string mesh_id){
    MeshReply response{};
    // The mesh endpoint comes from authenticated seed LLSD. Most
    // capabilities are HTTPS; legacy Linden-owned mesh CDN roots can
    // use HTTP with no bearer path/query. Their mesh asset bytes are
    // unencrypted and unauthenticated: limit size and validate decode.
    // Never use the plaintext exception for seed/EventQueue requests.
    if(!valid_uuid(mesh_id)||mesh_id.size()!=36U||
       !trusted_mesh_caps_url(capability_url))return response;
    std::wstring suffix;
    suffix.reserve(mesh_id.size());
    for(char ch:mesh_id)suffix.push_back(static_cast<wchar_t>(ch));
    // Match the official Second Life mesh repository URL convention.
    // The capability URL is authenticated and never logged.
    const bool query=capability_url.find(L'?')!=std::wstring::npos;
    if(query)capability_url+=L"&mesh_id=";
    else {
        if(!capability_url.empty()&&capability_url.back()!=L'/')
            capability_url.push_back(L'/');
        capability_url+=L"?mesh_id=";
    }
    capability_url+=suffix;
    Parts target{};
    if(!parse_url(capability_url,target,true))return response;
    HttpHandle session(WinHttpOpen(L"KAMI mesh asset/0.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value)return response;
    WinHttpSetTimeouts(session.value,5000,5000,7000,7000);
    HttpHandle connection(WinHttpConnect(session.value,target.host.c_str(),
                                        target.port,0));
    if(!connection.value)return response;
    HttpHandle req(WinHttpOpenRequest(connection.value,L"GET",
        target.path.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        target.secure?WINHTTP_FLAG_SECURE:0));
    if(!req.value)return response;
    DWORD redirects=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(req.value,WINHTTP_OPTION_DISABLE_FEATURE,
                         &redirects,sizeof(redirects)))return response;
    if(!WinHttpSendRequest(req.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,
            WINHTTP_NO_REQUEST_DATA,0,0,0)||
       !WinHttpReceiveResponse(req.value,nullptr))return response;
    DWORD status=0,length=sizeof(status);
    if(!WinHttpQueryHeaders(req.value,
        WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,
        WINHTTP_NO_HEADER_INDEX))return response;
    response.status=status;
    if(status!=200U)return response;
    constexpr std::size_t max_bytes=8U*1024U*1024U;
    std::array<std::uint8_t,8192> buffer{};
    for(;;){
        DWORD count=0U;
        if(!WinHttpReadData(req.value,buffer.data(),
              static_cast<DWORD>(buffer.size()),&count)){
            response.bytes.clear();return response;
        }
        if(!count)break;
        if(response.bytes.size()+count>max_bytes){
            response.bytes.clear();return response;
        }
        response.bytes.insert(response.bytes.end(),
                              buffer.begin(),buffer.begin()+count);
    }
    response.fetched=!response.bytes.empty();
    return response;
}
SeedResponse request_seed(std::wstring seed_url) {
    SeedResponse result{};
    if(!trusted_caps_url(seed_url)){
        result.diagnostic=L"CAPS seed URL rejected locally";return result;
    }
    static constexpr char body[]=
        "<llsd><array><string>EventQueueGet</string>"
        "<string>ViewerAsset</string><string>GetMesh2</string>"
        "<string>GetMesh</string></array></llsd>";
    auto reply=send_signed_post(seed_url,body,20000,1024U*1024U);
    // Clear credential-like bearer URL; do not log.
    if(!seed_url.empty())
        SecureZeroMemory(seed_url.data(),seed_url.size()*sizeof(wchar_t));
    result.http_status=reply.status;
    if(reply.error){
        result.diagnostic=L"CAPS seed HTTPS error "+
            std::to_wstring(reply.error);return result;
    }
    if(reply.status!=200){
        result.diagnostic=L"CAPS seed HTTP "+
            std::to_wstring(reply.status);return result;
    }
    SeedParseReport report{};
    const bool parsed=parse_seed_llsd(reply.body,result.event_queue_url,
                                     &result.mesh_capability_url,&report,
                                     &result.texture_capability_url);
    result.mesh_named=report.mesh_named;
    result.mesh_rejected=report.mesh_rejected;
    if(!parsed){
        result.diagnostic=!report.xml_map?
            L"CAPS seed: invalid or unsupported LLSD XML map":
            report.event_rejected?
                L"CAPS seed: EventQueue URL rejected by HTTPS/host policy":
                L"CAPS seed: no usable EventQueueGet or mesh capability";
        return result;
    }
    result.event_queue_granted=!result.event_queue_url.empty();
    result.mesh_granted=!result.mesh_capability_url.empty();
    result.texture_granted=!result.texture_capability_url.empty();
    result.granted=result.event_queue_granted||result.mesh_granted||
                   result.texture_granted;
    if(result.event_queue_granted&&result.mesh_granted)
        result.diagnostic=L"CAPS seed OK: EventQueue + mesh granted";
    else if(result.event_queue_granted)
        result.diagnostic=report.mesh_rejected?
            L"CAPS: EventQueue OK; mesh rejected ("+
                (report.mesh_issue.empty()?std::wstring(L"unknown reason"):
                                           report.mesh_issue)+L")":
            report.mesh_named?
            L"CAPS: EventQueue OK; mesh grant missing usable URL":
            L"CAPS: EventQueue OK; mesh grant not offered";
    else
        result.diagnostic=report.event_rejected?
            L"CAPS mesh granted; EventQueue URL rejected":
            L"CAPS mesh granted; EventQueue unavailable";
    return result;
}
PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous) {
    PollResponse result{};
    if(!trusted_caps_url(queue_url)){
        result.diagnostic=L"EventQueue URL rejected locally";return result;
    }
    const std::string body=build_event_queue_post(previous);
    if(body.empty()){
        result.diagnostic=L"EventQueueGet invalid local acknowledgement";
        SecureZeroMemory(queue_url.data(),queue_url.size()*sizeof(wchar_t));
        return result;
    }
    const auto start=std::chrono::steady_clock::now();
    auto reply=send_signed_post(queue_url,body,45000,4U*1024U*1024U);
    if(!queue_url.empty())
        SecureZeroMemory(queue_url.data(),queue_url.size()*sizeof(wchar_t));
    result.http_status=reply.status;
    const auto elapsed=std::chrono::steady_clock::now()-start;
    // Long-poll may return a timeout when there are no new messages.
    // Treat long-held 502–504 as expected idle; short-lived 502–504
    // and WinHTTP timeouts are transient with a bounded UI-side retry.
    const bool held_long=elapsed>=std::chrono::seconds(10);
    if(((reply.status==502||reply.status==503||reply.status==504)&&
         held_long) ||
       possible_idle_event_queue_http_error(reply.error,held_long)){
        result.idle=true;
        result.diagnostic=reply.error==ERROR_WINHTTP_INVALID_SERVER_RESPONSE?
            L"EventQueueGet HTTP 12152 after long poll; retrying (not ACKed)":
            L"EventQueueGet long poll ended; polling again";
        return result;
    }
    if(reply.error){
        result.transient=retryable_event_queue_http_error(reply.error);
        result.diagnostic=L"EventQueueGet HTTPS error "+
            std::to_wstring(reply.error);return result;
    }
    if(reply.status!=200){
        result.transient=reply.status==429U||reply.status==500U||
            reply.status==502U||reply.status==503U||reply.status==504U;
        result.diagnostic=L"EventQueueGet HTTP "+
            std::to_wstring(reply.status);return result;
    }
    auto observed=observe_event_queue_response(reply.body);
    observed.http_status=reply.status;
    return observed;
}
} // namespace kami::protocol::caps
