#include "kami/protocol/event_dispatch.hpp"
#include <objbase.h>
#include <string>
using namespace kami::protocol::caps;
namespace {
const std::string uuid="12345678-90ab-4cde-8fab-000000000001";
std::string response(std::string agent,std::string state_bool,
                     std::string event2="AgentGroupDataUpdate"){
    return "<llsd><map><key>id</key><integer>987</integer>"
      "<key>events</key><array>"
      "<map><key>message</key><string>AgentStateUpdate</string>"
      "<key>body</key><map><key>can_modify_navmesh</key>"
      "<boolean>"+state_bool+"</boolean></map></map>"
      "<map><key>message</key><string>"+event2+"</string>"
      "<key>body</key><map>"
      "<key>AgentData</key><array><map><key>AgentID</key>"
      "<uuid>"+agent+"</uuid></map></array>"
      "<key>GroupData</key><array><map>"
      "<key>GroupID</key><uuid>11111111-2222-4333-8444-555555555555</uuid>"
      "<key>GroupInsigniaID</key><uuid>00000000-0000-0000-0000-000000000000</uuid>"
      "<key>GroupPowers</key><binary>AAAAAAABAAAA</binary>"
      "<key>AcceptNotices</key><boolean>true</boolean>"
      "<key>GroupName</key><string>KAMI Test Group</string>"
      "<key>Contribution</key><integer>0</integer>"
      "</map></array></map></map></array></map></llsd>";
}
int exercise(){
    EventCommit result{};
    const auto good=response(uuid,"1");
    if(!validate_session_events(good,uuid,result)||
       !result.accepted||result.recognized_events!=2||
       !result.navmesh_state_seen||!result.can_modify_navmesh||
       !result.groups_seen||result.groups.size()!=1||
       result.groups[0].name!=L"KAMI Test Group")return 1;
    if(validate_session_events(
        response("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa","1"),
        uuid,result)||result.accepted)return 2;
    if(validate_session_events(
        response(uuid,"1","EnableSimulator"),uuid,result)||
       result.accepted)return 3;
    if(validate_session_events(response(uuid,"maybe"),uuid,result)||
       result.accepted)return 4;
    if(validate_session_events("<!DOCTYPE llsd [<!ENTITY x SYSTEM "
       "\"file:///etc/passwd\">]><llsd><map></map></llsd>",
       uuid,result)||result.accepted)return 5;
    if(validate_session_events(response(uuid,"0"),"bad-uuid",result))
       return 6;
    return 0;
}
}
int main(){
    const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(hr))return 90;
    const int result=exercise();
    CoUninitialize();
    return result;
}
