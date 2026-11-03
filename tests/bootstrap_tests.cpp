#include "kami/protocol/bootstrap.hpp"
#include <string>
using namespace kami::protocol;
static constexpr auto agent="12345678-90ab-4cde-8fab-000000000001";
static constexpr auto session="12345678-90ab-4cde-8fab-000000000002";
static constexpr auto region="12345678-90ab-4cde-8fab-000000000003";
int main(){
    Bootstrap flow;
    if(flow.online()||flow.authenticated()||
       flow.phase()!=BootstrapPhase::Offline)return 1;
    LoginHandoff handoff{agent,session,"sim1234.agni.lindenlab.com",13000,
        L"https://sim1234.agni.lindenlab.com:12345/cap/abc"};
    if(flow.login_reply(handoff,true)||flow.circuit_ack(agent,true)||
       flow.endpoint_checked({ProbeStatus::TlsError,0}))return 2;
    if(!flow.endpoint_checked({ProbeStatus::TlsEndpointReached,405}) ||
       flow.online()||flow.authenticated())return 3;
    auto invalid=handoff;
    invalid.seed_capability=L"http://attacker.example.com/cap/something";
    if(flow.login_reply(invalid,true)||flow.login_reply(handoff,false))return 4;
    if(!valid_handoff(handoff)||!flow.login_reply(handoff,true))return 5;
    if(flow.online() || !flow.authenticated())return 6;
    if(flow.circuit_ack(session,true)||flow.circuit_ack(agent,false)||
       !flow.circuit_ack(agent,true))return 7;
    if(flow.region_snapshot({region,agent,128.f,128.f,40.f},true)||
       flow.capability_reply(L"https://localhost/cap/evil",true)||
       !flow.capability_reply(L"https://sim1234.agni.lindenlab.com:12345/cap/def",true))
        return 8;
    if(flow.region_snapshot({region,session,128.f,128.f,40.f},true)||
       flow.region_snapshot({region,agent,128.f,128.f,40.f},false)||
       !flow.region_snapshot({region,agent,128.f,128.f,40.f},true))return 9;
    if(!flow.online())return 10;
    const auto epoch=flow.generation();
    flow.reset();
    if(flow.online()||flow.authenticated()||flow.generation()!=epoch+1||
       flow.phase()!=BootstrapPhase::Offline)return 11;
    if(!valid_uuid(agent)||valid_uuid("bad")||
       valid_uuid("00000000-0000-0000-0000-000000000000"))return 12;
    return 0;
}
