#include "kami/protocol/login.hpp"
#include <objbase.h>
#include <string>
#include <iostream>
using namespace kami::protocol;
namespace {
std::string m(std::string key,std::string value,std::string type="string") {
    return "<member><name>"+key+"</name><value><"+type+">"+
            value+"</"+type+"></value></member>";
}
std::string wrap(std::string fields) {
    return "<?xml version=\"1.0\"?><methodResponse><params><param>"
           "<value><struct>"+fields+"</struct></value></param></params></methodResponse>";
}
} // namespace
int main(){
    const HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(init))return 1;
    std::string f,l;
    if(!parse_account_name(L"Mira",f,l)||f!="Mira"||l!="Resident"||
       !parse_account_name(L"First Last",f,l)||f!="First"||l!="Last"||
       parse_account_name(L"Invalid Name Here",f,l))return 2;
    const std::string hashed="$1$0123456789abcdef0123456789abcdef";
    const std::string valid=build_login_xmlrpc("First","Last",hashed,
                                              "AB:CD:EF:12:34:56",true);
    if(valid.empty()||valid.find("<methodName>login_to_simulator</methodName>")==
       std::string::npos||
       valid.find("<name>passwd</name>")==std::string::npos||
       valid.find("KAMI Beyond Reality Dev")==std::string::npos||
       !build_login_xmlrpc("First","Last",hashed,"AB:CD:EF:12:34:56",false).empty())
       return 3;
    const auto accepted=wrap(
        m("login","true")+m("agent_id","12345678-90ab-4cde-8fab-000000000001")+
        m("session_id","12345678-90ab-4cde-8fab-000000000002")+
        m("sim_ip","sim1234.agni.lindenlab.com")+m("sim_port","13000","int")+
        m("circuit_code","190345234","int")+
        m("seed_capability","https://sim1234.agni.lindenlab.com:12345/cap/abc")+
        // Large non-scalar inventory data must not be mistaken for a giant
        // displayable string and reject an otherwise valid login response.
        "<member><name>inventory-skeleton</name><value><array><data><value><string>"+
        std::string(6000,'A')+"</string></value></data></array></value></member>");
    const auto ok=parse_login_xmlrpc(accepted);
    if(!ok.accepted()||ok.circuit_code!=190345234U||
       ok.handoff.simulator_port!=13000||
       ok.handoff.agent_id!="12345678-90ab-4cde-8fab-000000000001")return 4;
    const auto mfa=parse_login_xmlrpc(wrap(
        m("login","false")+m("reason","mfa_challenge")+
        m("message","Enter a second factor")));
    if(mfa.status!=LoginStatus::RequiresUserAction)return 5;
    const auto refused=parse_login_xmlrpc(wrap(
        m("login","false")+m("reason","key")+
        m("message","Incorrect password")));
    if(refused.status!=LoginStatus::Rejected)return 6;
    const auto bad_seed=parse_login_xmlrpc(wrap(
        m("login","true")+m("agent_id","12345678-90ab-4cde-8fab-000000000001")+
        m("session_id","12345678-90ab-4cde-8fab-000000000002")+
        m("sim_ip","example.org")+m("sim_port","13000")+
        m("circuit_code","1")+
        m("seed_capability","http://localhost/cap/evil")));
    if(bad_seed.status!=LoginStatus::MalformedReply)return 7;
    const auto xml_entity=parse_login_xmlrpc(
        "<?xml version=\"1.0\"?><!DOCTYPE foo [<!ENTITY x SYSTEM \"file:///C:/Windows/win.ini\">]>"
        "<methodResponse><params><param><value><struct>"
        "<member><name>login</name><value><string>&x;</string></value></member>"
        "</struct></value></param></params></methodResponse>");
    if(xml_entity.status!=LoginStatus::MalformedReply)return 8;
    if(parse_login_xmlrpc("").status!=LoginStatus::MalformedReply||
       parse_login_xmlrpc("<methodResponse>").status!=LoginStatus::MalformedReply)
       return 9;
    // No secrets/redirect URLs or untrusted response text in diagnostics.
    for(unsigned code:{301U,302U,303U,307U,308U,400U,401U,403U,
                       404U,405U,408U,429U,500U,502U,503U,504U,418U}){
        const auto message=login_http_diagnostic(code);
        if(message.find(L"HTTP "+std::to_wstring(code))==std::wstring::npos||
           message.find(L"passwd")!=std::wstring::npos||message.size()>120U)
            return 10;
    }
    if(login_http_diagnostic(403).find(L"refused")==std::wstring::npos||
       login_http_diagnostic(429).find(L"retry later")==std::wstring::npos||
       login_http_diagnostic(302).find(L"redirect blocked")==std::wstring::npos)
        return 11;
    CoUninitialize();
    return 0;
}
