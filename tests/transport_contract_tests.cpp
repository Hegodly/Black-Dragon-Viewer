#include "kami/protocol/transport.hpp"
#include <string>
using namespace kami::protocol;
int main(){
    if(login_host(Grid::Agni)!=L"login.agni.lindenlab.com"||
       login_host(Grid::Aditi)!=L"login.aditi.lindenlab.com")return 1;
    if(!valid_capability_url(L"https://sim1234.agni.lindenlab.com:12345/cap/uuid")||
       !valid_capability_url(L"https://sim1234.agni.lindenlab.com/cap/uuid"))
       return 2;
    for(const wchar_t* value:{
        L"http://sim123.agni.lindenlab.com/cap/t",
        L"https://localhost/cap/t",
        L"https://127.0.0.1/cap/t",
        L"https://example.local/cap/t",
        L"https://root:pass@example.org/cap/t",
        L"https://sim123.agni.lindenlab.com:0/cap/t",
        L"https://sim123.agni.lindenlab.com:99999/cap/t",
        L"https://sim123.agni.lindenlab.com:abc/cap/t",
        L"https://sim123.agni.lindenlab.com",
        L"https://example.org/cap/foo#fragment",
        L"https://example.org/cap/foo\r\nEvil:1"
    })if(valid_capability_url(value))return 3;
    const std::wstring too_long=std::wstring(L"https://example.org/")+std::wstring(2500,L'a');
    if(valid_capability_url(too_long))return 4;
    std::wstring with_nul=L"https://example.org/cap/uuid";
    with_nul.push_back(wchar_t{0});
    with_nul+=L"suffix";
    if(valid_capability_url(with_nul))return 5;
    // This test never performs a live network request.
    return 0;
}
