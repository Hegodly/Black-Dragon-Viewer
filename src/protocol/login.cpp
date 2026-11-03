#include "kami/protocol/login.hpp"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace kami::protocol {
namespace {
using Microsoft::WRL::ComPtr;
class SecureBytes {
public:
    std::string data;
    ~SecureBytes() {
        if(!data.empty())SecureZeroMemory(data.data(),data.size());
    }
    SecureBytes()=default;
    SecureBytes(const SecureBytes&)=delete;
    SecureBytes& operator=(const SecureBytes&)=delete;
};
struct HttpHandle {
    HINTERNET handle=nullptr;
    explicit HttpHandle(HINTERNET value=nullptr):handle(value){}
    ~HttpHandle(){if(handle)WinHttpCloseHandle(handle);}
    HttpHandle(const HttpHandle&)=delete;
    HttpHandle& operator=(const HttpHandle&)=delete;
};
class Bstr {
public:
    BSTR value=nullptr;
    explicit Bstr(const wchar_t* p):value(SysAllocString(p)){}
    explicit Bstr(std::wstring_view p):
        value(SysAllocStringLen(p.data(),static_cast<UINT>(p.size()))){}
    ~Bstr(){SysFreeString(value);}
    Bstr(const Bstr&)=delete;
    Bstr& operator=(const Bstr&)=delete;
};
std::wstring wide(std::string_view s) {
    if(s.empty()||s.size()>1048576)return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        s.data(),static_cast<int>(s.size()),nullptr,0);
    if(n<=0)return {};
    std::wstring w(static_cast<std::size_t>(n),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        s.data(),static_cast<int>(s.size()),w.data(),n)!=n)return {};
    return w;
}
std::string utf8(std::wstring_view s){
    if(s.empty()||s.size()>256)return {};
    const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,
        s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(n<=0)return {};
    std::string bytes(static_cast<std::size_t>(n),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,
        s.data(),static_cast<int>(s.size()),bytes.data(),n,nullptr,nullptr)!=n)
        return {};
    return bytes;
}
bool xml_safe_ascii(std::string_view v) noexcept {
    return !v.empty()&&v.size()<=128 &&
       std::all_of(v.begin(),v.end(),[](unsigned char c) {
           return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                  (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';
       });
}
std::string xml_quote(std::string_view value) {
    std::string result;
    for(const char c:value){
        switch(c){
        case '&':result+="&amp;";break;
        case '<':result+="&lt;";break;
        case '>':result+="&gt;";break;
        case '"':result+="&quot;";break;
        case '\'':result+="&apos;";break;
        default:result.push_back(c);break;
        }
    }
    return result;
}
void put_string(std::string& doc,std::string_view name,std::string_view value) {
    doc+="<member><name>";
    doc+=name;
    doc+="</name><value><string>";
    doc+=xml_quote(value);
    doc+="</string></value></member>";
}
bool digest_password(std::wstring_view password,SecureBytes& hash) {
    if(password.empty()||password.size()>128)return false;
    SecureBytes raw;
    raw.data=utf8(password);
    if(raw.data.empty())return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm,
                        BCRYPT_MD5_ALGORITHM,nullptr,0)))return false;
    std::array<unsigned char,16> digest{};
    const NTSTATUS status=BCryptHash(algorithm,nullptr,0,
        reinterpret_cast<PUCHAR>(raw.data.data()),
        static_cast<ULONG>(raw.data.size()),digest.data(),16);
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(!BCRYPT_SUCCESS(status))return false;
    constexpr char digits[]="0123456789abcdef";
    hash.data="$1$";
    for(const unsigned char v:digest){
        hash.data.push_back(digits[(v>>4)&15]);
        hash.data.push_back(digits[v&15]);
    }
    SecureZeroMemory(digest.data(),digest.size());
    return true;
}
std::string actual_mac(){
    ULONG size=0;
    if(GetAdaptersAddresses(AF_UNSPEC,
       GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,
       nullptr,nullptr,&size)!=ERROR_BUFFER_OVERFLOW||size>1024*1024)return {};
    std::vector<std::uint8_t> memory(size);
    auto* first=reinterpret_cast<PIP_ADAPTER_ADDRESSES>(memory.data());
    if(GetAdaptersAddresses(AF_UNSPEC,
       GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,
       nullptr,first,&size)!=NO_ERROR)return {};
    for(auto* a=first;a;a=a->Next){
        if(a->OperStatus!=IfOperStatusUp||a->IfType==IF_TYPE_SOFTWARE_LOOPBACK||
           a->PhysicalAddressLength!=6)continue;
        constexpr char hex[]="0123456789ABCDEF";
        std::string mac;
        for(unsigned i=0;i<6;++i){
            if(i)mac.push_back(':');
            mac.push_back(hex[a->PhysicalAddress[i]>>4]);
            mac.push_back(hex[a->PhysicalAddress[i]&15]);
        }
        return mac;
    }
    return {};
}
std::wstring safe_message(std::wstring s) {
    if(s.size()>200)s.resize(200);
    for(wchar_t& c:s)if(c<32||c==127)c=L' ';
    return s;
}
std::wstring get_text(IXMLDOMNode* node){
    if(!node)return {};
    BSTR b=nullptr;
    if(FAILED(node->get_text(&b))||!b)return {};
    const std::wstring out(b,SysStringLen(b));
    SysFreeString(b);
    return out;
}
ComPtr<IXMLDOMNode> child(IXMLDOMNode* parent,const wchar_t* path){
    ComPtr<IXMLDOMNode> out;
    if(parent){Bstr xp(path);(void)parent->selectSingleNode(xp.value,&out);}
    return out;
}
bool parse_u32(std::wstring_view s,std::uint32_t& out) noexcept {
    if(s.empty()||s.size()>10)return false;
    std::uint64_t n=0;
    for(const wchar_t ch:s){
        if(ch<L'0'||ch>L'9')return false;
        n=n*10+static_cast<unsigned>(ch-L'0');
        if(n>0xffffffffULL)return false;
    }
    out=static_cast<std::uint32_t>(n);return true;
}
} // namespace

bool parse_account_name(std::wstring_view account,std::string& first,
                        std::string& last) noexcept {
    first.clear();last.clear();
    if(account.size()<2||account.size()>80)return false;
    while(!account.empty()&&account.front()==L' ')account.remove_prefix(1);
    while(!account.empty()&&account.back()==L' ')account.remove_suffix(1);
    const auto delimiter=account.find(L' ');
    auto convert=[](std::wstring_view input,std::string& out)->bool {
        if(input.empty()||input.size()>40)return false;
        for(const wchar_t ch:input){
            if(!((ch>=L'a'&&ch<=L'z')||(ch>=L'A'&&ch<=L'Z')||
                 (ch>=L'0'&&ch<=L'9')||ch==L'-'||ch==L'_'||ch==L'.'))
                 return false;
            out.push_back(static_cast<char>(ch));
        }
        return true;
    };
    if(delimiter==std::wstring_view::npos)return convert(account,first)&&
        (last="Resident",true);
    auto first_part=account.substr(0,delimiter);
    auto last_part=account.substr(delimiter+1);
    if(last_part.find(L' ')!=std::wstring_view::npos)return false;
    return convert(first_part,first)&&convert(last_part,last);
}
std::string build_login_xmlrpc(std::string_view first,std::string_view last,
                              std::string_view hashed,
                              std::string_view mac,bool accepted){
    if(!xml_safe_ascii(first)||!xml_safe_ascii(last)||
       hashed.size()!=35||!hashed.starts_with("$1$")||
       !xml_safe_ascii(hashed.substr(3))||
       mac.size()!=17||!accepted)return {};
    std::string doc="<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<methodCall><methodName>login_to_simulator</methodName>"
        "<params><param><value><struct>";
    put_string(doc,"first",first);
    put_string(doc,"last",last);
    put_string(doc,"passwd",hashed);
    put_string(doc,"start","last");
    // This viewer ID is unique to this original KAMI build, never the
    // Linden Lab or Firestorm channel.
    put_string(doc,"channel","KAMI Beyond Reality Dev");
    put_string(doc,"version","0.1.0.53");
    put_string(doc,"platform","Win");
    put_string(doc,"mac",mac);
    put_string(doc,"agree_to_tos","true");
    put_string(doc,"read_critical","false");
    doc+="<member><name>options</name><value><array><data>";
    for(const char* option:{"login-flags","inventory-root","inventory-skeleton",
                            "buddy-list","gestures","global-textures"}){
        doc+="<value><string>";doc+=option;doc+="</string></value>";
    }
    doc+="</data></array></value></member>";
    doc+="</struct></value></param></params></methodCall>";
    return doc;
}
LoginResult parse_login_xmlrpc(std::string_view xml){
    LoginResult result{};
    result.status=LoginStatus::MalformedReply;
    if(xml.empty()||xml.size()>512*1024)return result;
    const std::wstring document=wide(xml);
    if(document.empty())return result;
    ComPtr<IXMLDOMDocument2> dom;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,
       CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dom)))||!dom)return result;
    (void)dom->put_async(VARIANT_FALSE);
    (void)dom->put_resolveExternals(VARIANT_FALSE);
    (void)dom->put_validateOnParse(VARIANT_FALSE);
    Bstr prop(L"ProhibitDTD");
    VARIANT prohibit;VariantInit(&prohibit);
    prohibit.vt=VT_BOOL;prohibit.boolVal=VARIANT_TRUE;
    if(FAILED(dom->setProperty(prop.value,prohibit)))return result;
    Bstr source(document);
    VARIANT_BOOL success=VARIANT_FALSE;
    if(FAILED(dom->loadXML(source.value,&success))||success!=VARIANT_TRUE)
        return result;
    Bstr xpath(L"/methodResponse/params/param/value/struct/member");
    ComPtr<IXMLDOMNodeList> members;
    if(FAILED(dom->selectNodes(xpath.value,&members))||!members)return result;
    long count=0;
    if(FAILED(members->get_length(&count))||count<1||count>200)return result;
    std::map<std::wstring,std::wstring> fields;
    for(long i=0;i<count;++i){
        ComPtr<IXMLDOMNode> member;
        if(FAILED(members->get_item(i,&member))||!member)return result;
        auto key=get_text(child(member.Get(),L"name").Get());
        auto val=get_text(child(member.Get(),L"value").Get());
        if(key.empty()||key.size()>100||val.size()>4096||
           !fields.emplace(key,val).second)return result;
    }
    const auto lookup=[&](const wchar_t* key)->std::wstring{
        auto it=fields.find(key);
        return it==fields.end()?std::wstring{}:it->second;
    };
    const auto login=lookup(L"login");
    if(login!=L"true"&&login!=L"1"){
        result.status=login==L"indeterminate"||login==L"mfa_required"?
            LoginStatus::RequiresUserAction:LoginStatus::Rejected;
        result.message=safe_message(lookup(L"message"));
        if(result.message.empty())result.message=L"Login rejected by Second Life";
        return result;
    }
    auto agent=lookup(L"agent_id");
    auto session=lookup(L"session_id");
    auto sim=lookup(L"sim_ip");
    auto seed=lookup(L"seed_capability");
    std::uint32_t port=0,code=0;
    if(!parse_u32(lookup(L"sim_port"),port)||
       port==0||port>65535||
       !parse_u32(lookup(L"circuit_code"),code)||code==0)
       return result;
    LoginHandoff handoff{};
    // Login response identifiers are ASCII UUID/IP; reject all other bytes.
    handoff.agent_id=std::string(agent.begin(),agent.end());
    handoff.session_id=std::string(session.begin(),session.end());
    handoff.simulator_host=std::string(sim.begin(),sim.end());
    handoff.simulator_port=static_cast<std::uint16_t>(port);
    handoff.seed_capability=std::move(seed);
    if(!valid_handoff(handoff))return result;
    result.handoff=std::move(handoff);
    result.circuit_code=code;
    result.status=LoginStatus::Accepted;
    result.message=L"Second Life login accepted; simulator connection pending";
    return result;
}
LoginResult login_to_simulator(const LoginRequest& request,
                              std::wstring_view password){
    LoginResult failure{};
    failure.status=LoginStatus::InvalidInput;
    std::string first,last;
    if(!request.terms_accepted||
       !parse_account_name(request.account,first,last)||
       request.start!=L"last"||password.empty()||password.size()>128)
       return failure;
    SecureBytes hash;
    if(!digest_password(password,hash)){
        failure.status=LoginStatus::CryptoUnavailable;return failure;
    }
    const auto mac=actual_mac();
    if(mac.empty()){failure.status=LoginStatus::NetworkError;return failure;}
    SecureBytes payload;
    payload.data=build_login_xmlrpc(first,last,hash.data,mac,request.terms_accepted);
    if(payload.data.empty())return failure;
    // No credentials may ever leave this official Linden Lab host.
    const auto host=login_host(request.grid);
    HttpHandle session(WinHttpOpen(L"KAMI-Beyond-Reality-Dev/0.1.0.53",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.handle){failure.status=LoginStatus::NetworkError;return failure;}
    WinHttpSetTimeouts(session.handle,3500,3500,4500,4500);
    HttpHandle connection(WinHttpConnect(session.handle,
        std::wstring(host).c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.handle){failure.status=LoginStatus::NetworkError;return failure;}
    HttpHandle web(WinHttpOpenRequest(connection.handle,L"POST",
        L"/cgi-bin/login.cgi",nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!web.handle){failure.status=LoginStatus::NetworkError;return failure;}
    DWORD redirects=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(web.handle,WINHTTP_OPTION_DISABLE_FEATURE,
                         &redirects,sizeof(redirects))){
        failure.status=LoginStatus::NetworkError;return failure;
    }
    constexpr wchar_t headers[]=L"Content-Type: text/xml; charset=UTF-8\r\n"
                                 L"Cache-Control: no-store\r\n";
    if(!WinHttpSendRequest(web.handle,headers,static_cast<DWORD>(-1),
           payload.data.data(),static_cast<DWORD>(payload.data.size()),
           static_cast<DWORD>(payload.data.size()),0)||
       !WinHttpReceiveResponse(web.handle,nullptr)){
        failure.status=GetLastError()==ERROR_WINHTTP_SECURE_FAILURE?
            LoginStatus::TlsFailure:LoginStatus::NetworkError;
        return failure;
    }
    DWORD code=0,length=sizeof(code);
    if(!WinHttpQueryHeaders(web.handle,
       WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
       WINHTTP_HEADER_NAME_BY_INDEX,&code,&length,WINHTTP_NO_HEADER_INDEX)){
        failure.status=LoginStatus::HttpError;return failure;
    }
    failure.http_status=code;
    if(code!=200){failure.status=LoginStatus::HttpError;return failure;}
    std::string body;
    while(body.size()<=512*1024){
        char chunk[8192]{};
        DWORD read=0;
        if(!WinHttpReadData(web.handle,chunk,sizeof(chunk),&read)){
            failure.status=LoginStatus::NetworkError;return failure;
        }
        if(!read)break;
        body.append(chunk,read);
    }
    if(body.size()>512*1024){
        failure.status=LoginStatus::MalformedReply;return failure;
    }
    auto reply=parse_login_xmlrpc(body);
    reply.http_status=code;
    return reply;
}
const wchar_t* login_status_label(LoginStatus state) noexcept {
    switch(state){
    case LoginStatus::NotAttempted:return L"LOGIN NOT ATTEMPTED";
    case LoginStatus::InvalidInput:return L"CHECK ACCOUNT NAME / TERMS";
    case LoginStatus::CryptoUnavailable:return L"PASSWORD HASHING UNAVAILABLE";
    case LoginStatus::NetworkError:return L"LOGIN NETWORK ERROR";
    case LoginStatus::TlsFailure:return L"LOGIN TLS CERTIFICATE ERROR";
    case LoginStatus::HttpError:return L"SL LOGIN HTTP ERROR";
    case LoginStatus::MalformedReply:return L"SL LOGIN REPLY INVALID";
    case LoginStatus::Rejected:return L"SL LOGIN REJECTED";
    case LoginStatus::RequiresUserAction:return L"SL ACCOUNT ACTION NEEDED";
    case LoginStatus::Accepted:return L"SL LOGIN ACCEPTED (NOT IN-WORLD)";
    }
    return L"UNKNOWN LOGIN STATE";
}
} // namespace kami::protocol
