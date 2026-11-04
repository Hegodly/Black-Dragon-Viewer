#include "kami/protocol/caps.hpp"
#include "kami/protocol/transport.hpp"
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
};
bool suffix(std::wstring_view s,std::wstring_view end) noexcept {
    return s.size()>end.size()&&s.ends_with(end);
}
bool parse_url(std::wstring_view input,Parts& result) noexcept {
    if(!valid_capability_url(input))return false;
    URL_COMPONENTS u{};
    u.dwStructSize=sizeof(u);
    u.dwSchemeLength=DWORD(-1);
    u.dwHostNameLength=DWORD(-1);
    u.dwUrlPathLength=DWORD(-1);
    u.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(input.data(),static_cast<DWORD>(input.size()),0,&u)||
       u.nScheme!=INTERNET_SCHEME_HTTPS||!u.lpszHostName||
       !u.lpszUrlPath||u.dwUrlPathLength==0)return false;
    std::wstring host(u.lpszHostName,u.dwHostNameLength);
    std::transform(host.begin(),host.end(),host.begin(),[](wchar_t c){
        return c>=L'A'&&c<=L'Z'?static_cast<wchar_t>(c+32):c;
    });
    // A signed capability URL from authenticated SL login is still a bearer
    // URL. Never allow it to direct a request at private or unrelated hosts.
    if(!(suffix(host,L".lindenlab.com")||
         suffix(host,L".secondlife.io")||
         suffix(host,L".secondlife.com")))return false;
    std::wstring path(u.lpszUrlPath,u.dwUrlPathLength);
    if(u.lpszExtraInfo&&u.dwExtraInfoLength)
        path.append(u.lpszExtraInfo,u.dwExtraInfoLength);
    if(path.size()>2048||path.empty()||path[0]!=L'/')return false;
    result.host=std::move(host);
    result.path=std::move(path);
    result.port=u.nPort?u.nPort:INTERNET_DEFAULT_HTTPS_PORT;
    return true;
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
bool trusted_caps_url(std::wstring_view url) noexcept {
    Parts dummy;
    return parse_url(url,dummy);
}
bool parse_seed_llsd(std::string_view xml,std::wstring& event_queue_url) {
    event_queue_url.clear();
    ComPtr<IXMLDOMDocument2> dom;
    if(!parse_dom(xml,dom))return false;
    ComPtr<IXMLDOMNodeList> nodes;
    if(!child_elements(dom.Get(),nodes))return false;
    long count=0;
    if(FAILED(nodes->get_length(&count))||count<2||count>500)return false;
    for(long i=0;i<count-1;++i){
        ComPtr<IXMLDOMNode> key,value;
        if(FAILED(nodes->get_item(i,key.GetAddressOf()))||!key||
           node_name(key.Get())!=L"key"||
           node_text(key.Get())!=L"EventQueueGet")continue;
        if(FAILED(nodes->get_item(i+1,value.GetAddressOf()))||!value||
           node_name(value.Get())!=L"string")return false;
        const auto cap=node_text(value.Get());
        if(!trusted_caps_url(cap))return false;
        event_queue_url=cap;
        return true;
    }
    return false;
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
                    ComPtr<IXMLDOMNode> key,data;
                    if(FAILED(members->get_item(m,key.GetAddressOf()))||
                       !key||node_name(key.Get())!=L"key"||
                       node_text(key.Get())!=L"message")continue;
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
                out.event_types.push_back(safe.empty()?
                    std::string("UnknownEvent"):std::move(safe));
            }
            out.event_count=static_cast<unsigned>(n);
        }
    }
    return found_id&&found_events;
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
SeedResponse request_seed(std::wstring seed_url) {
    SeedResponse result{};
    if(!trusted_caps_url(seed_url)){
        result.diagnostic=L"CAPS seed URL rejected locally";return result;
    }
    static constexpr char body[]=
        "<llsd><array><string>EventQueueGet</string></array></llsd>";
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
    if(!parse_seed_llsd(reply.body,result.event_queue_url)){
        result.diagnostic=L"CAPS seed missing valid EventQueueGet";
        return result;
    }
    result.granted=true;
    result.diagnostic=L"CAPS seed OK; EventQueueGet granted";
    return result;
}
PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous,
    std::string_view authenticated_agent_id) {
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
    // This is an initial one-shot probe. No events are acknowledged or
    // dispatched until the complete event handler exists.
    if((reply.status==502||reply.status==503||reply.status==504||
        (reply.error==ERROR_WINHTTP_TIMEOUT))&&
       elapsed>=std::chrono::seconds(10)){
        result.idle=true;
        result.diagnostic=L"EventQueueGet reached; no events yet";
        return result;
    }
    if(reply.error){
        result.diagnostic=L"EventQueueGet HTTPS error "+
            std::to_wstring(reply.error);return result;
    }
    if(reply.status!=200){
        result.diagnostic=L"EventQueueGet HTTP "+
            std::to_wstring(reply.status);return result;
    }
    EventQueueFrame frame{};
    if(!parse_event_queue_frame(reply.body,frame)){
        result.diagnostic=L"EventQueueGet invalid LLSD envelope";
        return result;
    }
    result.event_count=frame.event_count;
    result.event_types=std::move(frame.event_types);
    result.structurally_valid=true;
    if(frame.event_count!=0){
        // R9 only ACKs if ALL events are recognized, validated, and
        // staged for local atomic commit. Unknown/malformed events
        // preserve R8's no-ACK stop behavior.
        EventCommit committed{};
        if(!validate_session_events(reply.body,authenticated_agent_id,committed)||
           !committed.accepted||
           committed.recognized_events!=frame.event_count){
            result.diagnostic=L"EventQueue: "+
                std::to_wstring(frame.event_count)+
                L" event(s) unhandled; not acknowledged";
            return result;
        }
        result.committed=std::move(committed);
        result.safe_to_ack=true;
        result.next_ack=std::move(frame.id);
        result.diagnostic=L"R9: validated "+std::to_wstring(frame.event_count)+
            L" event(s); ready for local commit";
        return result;
    }
    result.safe_to_ack=true;
    result.next_ack=std::move(frame.id);
    result.diagnostic=L"EventQueueGet heartbeat; no events";
    return result;
}
} // namespace kami::protocol::caps
