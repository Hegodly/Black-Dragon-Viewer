#include "kami/protocol/login.hpp"
#include <objbase.h>
#include <msxml6.h>
#include <wrl/client.h>
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
    if(!parse_account_name(L"first.last",f,l)||f!="first"||l!="last"||
       !parse_account_name(L"First Last",f,l)||f!="First"||l!="Last"||
       parse_account_name(L"first..last",f,l)||
       parse_account_name(L".last",f,l)||
       parse_account_name(L"first.",f,l))return 13;
    const std::array<std::uint8_t,6> mac_bytes{0,1,2,3,4,5};
    if(hash_mac_bytes(mac_bytes)!="d15ae53931880fd7b724dd7888b4b4ed")return 16;
    if(classify_login_http_body("<html><body>unknown</body></html>")!=
       L"HTML error page"||
       classify_login_http_body("<?xml")!=
       L"XML-RPC response"||
       classify_login_http_body("")!=L"empty response")return 17;
    const std::string hashed="$1$0123456789abcdef0123456789abcdef";
    const std::string valid=build_login_xmlrpc("First","Last",hashed,
                                              "abcdef0123456789abcdef0123456789",true);
    // Validate the complete serialized XML-RPC request with the same
    // security-conscious MSXML DOM engine used for login responses.
    Microsoft::WRL::ComPtr<IXMLDOMDocument2> request_doc;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,
       CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&request_doc)))||!request_doc)
       return 18;
    (void)request_doc->put_async(VARIANT_FALSE);
    (void)request_doc->put_resolveExternals(VARIANT_FALSE);
    VARIANT prohibit; VariantInit(&prohibit);
    prohibit.vt=VT_BOOL; prohibit.boolVal=VARIANT_TRUE;
    const BSTR property=SysAllocString(L"ProhibitDTD");
    const HRESULT property_status=request_doc->setProperty(property,prohibit);
    SysFreeString(property);
    if(FAILED(property_status))return 19;
    const int characters=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        valid.data(),static_cast<int>(valid.size()),nullptr,0);
    if(characters<=0)return 20;
    std::wstring wide_xml(static_cast<std::size_t>(characters),L'\\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        valid.data(),static_cast<int>(valid.size()),
        wide_xml.data(),characters)!=characters)return 21;
    const BSTR xml_source=SysAllocStringLen(wide_xml.data(),
                                             static_cast<UINT>(wide_xml.size()));
    VARIANT_BOOL parsed=VARIANT_FALSE;
    const HRESULT parsed_status=request_doc->loadXML(xml_source,&parsed);
    SysFreeString(xml_source);
    if(FAILED(parsed_status)||parsed!=VARIANT_TRUE)return 22;
    // Expected single struct parameter with the correct root XML-RPC method.
    const BSTR method_path=SysAllocString(
        L"/methodCall/methodName[text()='login_to_simulator']");
    Microsoft::WRL::ComPtr<IXMLDOMNode> method_node;
    (void)request_doc->selectSingleNode(method_path,&method_node);
    SysFreeString(method_path);
    if(!method_node)return 23;
    if(valid.empty()||valid.find("<methodName>login_to_simulator</methodName>")==
       std::string::npos||
       valid.find("<name>passwd</name>")==std::string::npos||
       valid.find("KAMI Beyond Reality Dev")==std::string::npos||
       !build_login_xmlrpc("First","Last",hashed,"abcdef0123456789abcdef0123456789",false).empty())
       return 3;
    // Confirm required field, valid XML-RPC boolean and no impersonation of
    // an official viewer. Legacy KAMI missed id0 on every login request.
    if(valid.find("<name>id0</name>") == std::string::npos||
       valid.find("00000000-0000-0000-0000-000000000000")==std::string::npos||
       valid.find("<name>extended_errors</name><value><boolean>1</boolean>")==
           std::string::npos||
       valid.find("<name>options</name><value><array>")==std::string::npos||
       valid.find("<name>address_size</name><value><int>64</int>")==std::string::npos||
       valid.find("<name>last_exec_event</name><value><int>0</int>")==std::string::npos||
       valid.find("<name>mfa_hash</name>")==std::string::npos||
       valid.find("<name>token</name>")==std::string::npos||
       valid.find("<name>viewer_digest</name>")!=std::string::npos||
       valid.find("<name>agree_to_tos</name><value><boolean>0</boolean>")==std::string::npos||
       valid.find("<name>read_critical</name><value><boolean>0</boolean>")==std::string::npos||
       valid.find("<name>agree_to_tos</name><value><string>")!=std::string::npos)
       return 12;
    // Current protocol submits a 32-hex pseudonymous machine marker, not
    // a raw physical MAC address, and rejects old/invalid MAC inputs.
    if(valid.find("AB:CD:EF:12:34:56")!=std::string::npos||
       !build_login_xmlrpc("First","Last",hashed,
          "AB:CD:EF:12:34:56",true).empty()||
       !build_login_xmlrpc("First","Last",hashed,
          "not-a-hashed-identifier-12345678",true).empty())
       return 14;
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
    // HTTP accepts up to 8 MiB, including large optional inventory data.
    const auto large_inventory=wrap(
        m("login","true")+m("agent_id","12345678-90ab-4cde-8fab-000000000001")+
        m("session_id","12345678-90ab-4cde-8fab-000000000002")+
        m("sim_ip","sim1234.agni.lindenlab.com")+m("sim_port","13000","int")+
        m("circuit_code","190345234","int")+
        m("seed_capability","https://sim1234.agni.lindenlab.com:12345/cap/abc")+
        "<member><name>inventory-skeleton</name><value><array><data><value><string>"+
        std::string(1100000,'A')+"</string></value></data></array></value></member>");
    if(!parse_login_xmlrpc(large_inventory).accepted())return 15;
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
    if(login_http_diagnostic(400).find(L"KAMI DIAG R2")==std::wstring::npos||
       login_http_diagnostic(400).find(L"check protocol")!=std::wstring::npos)
       return 24;
    if(login_http_diagnostic(403).find(L"refused")==std::wstring::npos||
       login_http_diagnostic(429).find(L"retry later")==std::wstring::npos||
       login_http_diagnostic(302).find(L"redirect blocked")==std::wstring::npos)
        return 11;
    CoUninitialize();
    return 0;
}
