#include "kami/protocol/transport.hpp"
#include <windows.h>
#include <winhttp.h>
#include <cwctype>
#include <string>

namespace kami::protocol {
namespace {
struct HttpHandle {
    HINTERNET value=nullptr;
    ~HttpHandle(){if(value)WinHttpCloseHandle(value);}
    HttpHandle()=default;
    explicit HttpHandle(HINTERNET h):value(h){}
    HttpHandle(const HttpHandle&)=delete;
    HttpHandle& operator=(const HttpHandle&)=delete;
};
bool unsafe_hostname(std::wstring_view host) noexcept {
    if(host.empty()||host.size()>253||host.front()==L'.'||host.back()==L'.')
        return true;
    if(host==L"localhost"||host.ends_with(L".localhost")||
       host.ends_with(L".local")||host.ends_with(L".internal"))return true;
    bool only_address_chars=true;
    for(wchar_t c:host) {
        if(!(std::iswdigit(c)||c==L'.'||c==L':'))only_address_chars=false;
        if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||
             (c>=L'0'&&c<=L'9')||c==L'-'||c==L'.'))return true;
    }
    return only_address_chars; // disallow literal IPs and locally routed hostnames
}
} // namespace
std::wstring_view login_host(Grid grid) noexcept {
    return grid==Grid::Aditi?L"login.aditi.lindenlab.com":
           L"login.agni.lindenlab.com";
}
bool valid_capability_url(std::wstring_view url) noexcept {
    if(url.size()<16||url.size()>2048||!url.starts_with(L"https://")||
       url.find_first_of(L"\r\n\t")!=std::wstring_view::npos||
       url.find(wchar_t{0})!=std::wstring_view::npos||
       url.find(L'#')!=std::wstring_view::npos||
       url.find(L'@')!=std::wstring_view::npos) return false;
    // URLs are learned from authenticated simulator responses, never from
    // arbitrary chat messages, attachments or a copy/paste URL.
    const auto beginning=url.substr(8);
    const auto slash=beginning.find_first_of(L"/?");
    const auto authority=beginning.substr(0,slash);
    const auto colon=authority.rfind(L':');
    std::wstring_view hostname=authority;
    if(colon!=std::wstring_view::npos){
        hostname=authority.substr(0,colon);
        const auto port=authority.substr(colon+1);
        if(port.empty()||port.size()>5)return false;
        unsigned n=0;
        for(wchar_t c:port) {
            if(c<L'0'||c>L'9')return false;
            n=n*10U+static_cast<unsigned>(c-L'0');
        }
        if(n==0||n>65535)return false;
    }
    if(unsafe_hostname(hostname))return false;
    // A capability must be a URL with an explicit path (often /cap/UUID).
    if(slash==std::wstring_view::npos||beginning[slash]!=L'/')return false;
    return beginning.size()>slash+1;
}
ProbeResult probe_login_tls(Grid grid) noexcept {
    const auto host=login_host(grid);
    HttpHandle session(WinHttpOpen(L"KAMI-Original-Core/0.1 (TLS check only)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value)return {ProbeStatus::NetworkUnavailable,0};
    // Bounds on user-triggered probes. No login or cookies are involved.
    WinHttpSetTimeouts(session.value,1300,1300,1700,1700);
    HttpHandle connection(WinHttpConnect(session.value,
        std::wstring(host).c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value)return {ProbeStatus::NetworkUnavailable,0};
    HttpHandle request(WinHttpOpenRequest(connection.value,L"HEAD",
        L"/cgi-bin/login.cgi",nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.value)return {ProbeStatus::NetworkUnavailable,0};
    DWORD disable=WINHTTP_DISABLE_REDIRECTS;
    (void)WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,
                           &disable,sizeof(disable));
    if(!WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,
                          WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request.value,nullptr)) {
        const DWORD code=GetLastError();
        switch(code) {
        case ERROR_WINHTTP_SECURE_FAILURE:
        case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
            return {ProbeStatus::TlsError,0};
        default:return {ProbeStatus::NetworkUnavailable,0};
        }
    }
    DWORD status=0,length=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,
        WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX))
        return {ProbeStatus::BadResponse,0};
    // An HTTP rejection of HEAD is still evidence of a completed secure
    // TLS HTTP exchange, NOT evidence of login or protocol compatibility.
    return status>=100&&status<=599?
        ProbeResult{ProbeStatus::TlsEndpointReached,status}:
        ProbeResult{ProbeStatus::BadResponse,status};
}
} // namespace kami::protocol
