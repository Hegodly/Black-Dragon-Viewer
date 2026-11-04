#include "kami/protocol/event_dispatch.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <windows.h>
#include <msxml6.h>
#include <wrl/client.h>
#include <oleauto.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace kami::protocol::caps {
namespace {
using Microsoft::WRL::ComPtr;
struct ScopedBstr {
    BSTR value=nullptr;
    explicit ScopedBstr(const wchar_t* s):value(SysAllocString(s)){}
    explicit ScopedBstr(std::wstring_view s):
        value(SysAllocStringLen(s.data(),static_cast<UINT>(s.size()))){}
    ~ScopedBstr(){if(value)SysFreeString(value);}
};
std::wstring text_of(IXMLDOMNode* node){
    BSTR value=nullptr;
    if(!node||FAILED(node->get_text(&value))||!value)return {};
    std::wstring result(value,SysStringLen(value));
    SysFreeString(value);
    return result;
}
std::wstring name_of(IXMLDOMNode* node){
    BSTR value=nullptr;
    if(!node||FAILED(node->get_nodeName(&value))||!value)return {};
    std::wstring result(value,SysStringLen(value));
    SysFreeString(value);
    return result;
}
bool children(IXMLDOMNode* node,ComPtr<IXMLDOMNodeList>& out,
              long& length,long max) {
    length=0;
    return node&&SUCCEEDED(node->get_childNodes(out.GetAddressOf()))&&
       out&&SUCCEEDED(out->get_length(&length))&&
       length>=0&&length<=max;
}
bool map_value(IXMLDOMNode* map,std::wstring_view wanted,
               ComPtr<IXMLDOMNode>& out){
    if(name_of(map)!=L"map")return false;
    ComPtr<IXMLDOMNodeList> list;
    long n=0;
    if(!children(map,list,n,256)||n%2!=0)return false;
    for(long i=0;i<n;i+=2){
        ComPtr<IXMLDOMNode> key,value;
        if(FAILED(list->get_item(i,key.GetAddressOf()))||
           FAILED(list->get_item(i+1,value.GetAddressOf()))||
           name_of(key.Get())!=L"key")return false;
        if(text_of(key.Get())==wanted){
            out=value;return true;
        }
    }
    return false;
}
bool array_at(IXMLDOMNode* array,long index,long limit,
              ComPtr<IXMLDOMNode>& out,long* count=nullptr){
    if(name_of(array)!=L"array")return false;
    ComPtr<IXMLDOMNodeList> list;
    long n=0;
    if(!children(array,list,n,limit))return false;
    if(count)*count=n;
    return index>=0&&index<n&&
        SUCCEEDED(list->get_item(index,out.GetAddressOf()))&&out;
}
bool is_boolean(IXMLDOMNode* node,bool& out){
    if(name_of(node)!=L"boolean")return false;
    const auto value=text_of(node);
    if(value==L"true"||value==L"1"){out=true;return true;}
    if(value==L"false"||value==L"0"){out=false;return true;}
    return false;
}
bool ascii_uuid(IXMLDOMNode* node,std::string& result,
                bool allow_null=false){
    auto type=name_of(node);
    if(type!=L"uuid"&&type!=L"string")return false;
    const auto value=text_of(node);
    if(value.size()!=36)return false;
    result.clear();result.reserve(36);
    for(wchar_t x:value){
        if(x>127)return false;
        result.push_back(static_cast<char>(
            x>=L'A'&&x<=L'F'?x+32:x));
    }
    // A group insignia may be the null UUID; agent and group IDs may not.
    return valid_uuid(result)||
       (allow_null&&result=="00000000-0000-0000-0000-000000000000");
}
bool integer(IXMLDOMNode* node,int& result){
    if(name_of(node)!=L"integer")return false;
    const auto t=text_of(node);
    if(t.empty()||t.size()>12)return false;
    std::string temp;
    for(std::size_t i=0;i<t.size();++i){
        const wchar_t c=t[i];
        if((c>=L'0'&&c<=L'9')||(i==0&&c==L'-'))
            temp.push_back(static_cast<char>(c));
        else return false;
    }
    try {
        const long long n=std::stoll(temp);
        if(n<-2147483647LL||n>2147483647LL)return false;
        result=static_cast<int>(n);
        return true;
    }catch(...){return false;}
}
bool bounded_powers(IXMLDOMNode* node,std::string& result){
    const auto type=name_of(node);
    if(type!=L"binary"&&type!=L"integer"&&type!=L"string")
        return false;
    const auto text=text_of(node);
    if(text.size()>64)return false;
    result.clear();
    for(wchar_t c:text){
        if(c>127)return false;
        const bool ascii=(c>=L'0'&&c<=L'9')||
            (c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||
            c==L'+'||c==L'/'||c==L'='||c==L'-';
        if(!ascii)return false;
        result.push_back(static_cast<char>(c));
    }
    return true;
}
bool group_entry(IXMLDOMNode* map,GroupSnapshot& group){
    ComPtr<IXMLDOMNode> uuid,insignia,powers,notices,name,contribution;
    if(!map_value(map,L"GroupID",uuid)||
       !map_value(map,L"GroupInsigniaID",insignia)||
       !map_value(map,L"GroupPowers",powers)||
       !map_value(map,L"AcceptNotices",notices)||
       !map_value(map,L"GroupName",name)||
       !map_value(map,L"Contribution",contribution))
        return false;
    if(!ascii_uuid(uuid.Get(),group.id)||
       !ascii_uuid(insignia.Get(),group.insignia,true)||
       !bounded_powers(powers.Get(),group.powers)||
       !is_boolean(notices.Get(),group.notices)||
       !integer(contribution.Get(),group.contribution)||
       name_of(name.Get())!=L"string")return false;
    const auto displayed=text_of(name.Get());
    if(displayed.size()>128)return false;
    group.name=displayed;
    return true;
}
bool event_body(IXMLDOMNode* event,std::string_view expected_agent,
                EventCommit& commit){
    ComPtr<IXMLDOMNode> type,body;
    if(!map_value(event,L"message",type)||
       !map_value(event,L"body",body)||
       name_of(type.Get())!=L"string")return false;
    const auto name=text_of(type.Get());
    if(name==L"AgentStateUpdate"){
        if(commit.navmesh_state_seen)return false;
        if(name_of(body.Get())!=L"map")return false;
        ComPtr<IXMLDOMNode> can;
        if(!map_value(body.Get(),L"can_modify_navmesh",can)||
           !is_boolean(can.Get(),commit.can_modify_navmesh))
            return false;
        commit.navmesh_state_seen=true;
    }else if(name==L"AgentGroupDataUpdate"){
        if(commit.groups_seen)return false;
        // Same nested-body convention as Linden Lab's LLAgentGroupDataUpdateViewerNode.
        ComPtr<IXMLDOMNode> nested;
        if(map_value(body.Get(),L"body",nested))body=nested;
        ComPtr<IXMLDOMNode> agentdata,groupdata;
        if(!map_value(body.Get(),L"AgentData",agentdata)||
           !map_value(body.Get(),L"GroupData",groupdata))return false;
        ComPtr<IXMLDOMNode> agent,agent_id;
        if(!array_at(agentdata.Get(),0,4,agent)|| 
           !map_value(agent.Get(),L"AgentID",agent_id))return false;
        std::string owner;
        if(!ascii_uuid(agent_id.Get(),owner)||owner!=expected_agent)
            return false;
        if(name_of(groupdata.Get())!=L"array")return false;
        ComPtr<IXMLDOMNodeList> all;
        long n=0;
        if(!children(groupdata.Get(),all,n,128))return false;
        commit.groups.clear();commit.groups.reserve(n);
        for(long i=0;i<n;++i){
            ComPtr<IXMLDOMNode> record;
            if(FAILED(all->get_item(i,record.GetAddressOf()))||
               name_of(record.Get())!=L"map")return false;
            GroupSnapshot group;
            if(!group_entry(record.Get(),group))return false;
            commit.groups.push_back(std::move(group));
        }
        commit.groups_seen=true;
    }else{
        return false; // Never ACK unknown event types
    }
    ++commit.recognized_events;
    return true;
}
}
bool validate_session_events(std::string_view xml,
                             std::string_view authenticated_agent_uuid,
                             EventCommit& output){
    output={};
    if(xml.empty()||xml.size()>4U*1024U*1024U||
       !valid_uuid(authenticated_agent_uuid))return false;
    std::string expected(authenticated_agent_uuid);
    std::transform(expected.begin(),expected.end(),expected.begin(),
        [](char x){return x>='A'&&x<='F'?char(x+32):x;});
    const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        xml.data(),static_cast<int>(xml.size()),nullptr,0);
    if(length<=0)return false;
    std::wstring wide(static_cast<std::size_t>(length),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        xml.data(),static_cast<int>(xml.size()),wide.data(),length)!=length)
        return false;
    ComPtr<IXMLDOMDocument2> document;
    if(FAILED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,
       CLSCTX_INPROC_SERVER,IID_PPV_ARGS(document.GetAddressOf())))||
       !document)return false;
    (void)document->put_async(VARIANT_FALSE);
    (void)document->put_resolveExternals(VARIANT_FALSE);
    (void)document->put_validateOnParse(VARIANT_FALSE);
    ScopedBstr property(L"ProhibitDTD");
    VARIANT option;VariantInit(&option);
    option.vt=VT_BOOL;option.boolVal=VARIANT_TRUE;
    if(FAILED(document->setProperty(property.value,option)))return false;
    ScopedBstr source(wide);
    VARIANT_BOOL parsed=VARIANT_FALSE;
    if(FAILED(document->loadXML(source.value,&parsed))||
       parsed!=VARIANT_TRUE)return false;
    ComPtr<IXMLDOMNode> root,event_array;
    ScopedBstr xpath(L"/llsd/map");
    if(FAILED(document->selectSingleNode(xpath.value,root.GetAddressOf()))||
       !map_value(root.Get(),L"events",event_array))return false;
    if(name_of(event_array.Get())!=L"array")return false;
    ComPtr<IXMLDOMNodeList> all;
    long n=0;
    if(!children(event_array.Get(),all,n,128))return false;
    EventCommit pending;
    for(long i=0;i<n;++i){
        ComPtr<IXMLDOMNode> event;
        if(FAILED(all->get_item(i,event.GetAddressOf()))||
           !event_body(event.Get(),expected,pending))return false;
    }
    pending.accepted=true;
    output=std::move(pending);
    return true;
}
}
