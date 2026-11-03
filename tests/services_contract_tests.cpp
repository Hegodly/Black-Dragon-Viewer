#include "kami/protocol/services.hpp"
#include <limits>
using namespace kami::protocol;
static constexpr auto agent="12345678-90ab-4cde-8fab-000000000001";
static constexpr auto session="12345678-90ab-4cde-8fab-000000000002";
static constexpr auto region="12345678-90ab-4cde-8fab-000000000003";
static constexpr auto object="12345678-90ab-4cde-8fab-000000000004";
static constexpr auto mesh="12345678-90ab-4cde-8fab-000000000005";
int main() {
    Bootstrap link;
    Services services(link);
    LiveData data(link);
    if(services.prepare(Service::NearbyChat,{},L"Hello")!=0||
       data.apply_objects({{object,mesh,1.f,2.f,3.f}},true))return 1;
    if(!link.endpoint_checked({ProbeStatus::TlsEndpointReached,405})||
       !link.login_reply({agent,session,"sim1234.agni.lindenlab.com",13000,
           L"https://sim1234.agni.lindenlab.com:12345/cap/first"},true)||
       !link.circuit_ack(agent,true)||
       !link.capability_reply(L"https://sim1234.agni.lindenlab.com/cap/second",true)||
       !link.region_snapshot({region,agent,128.f,128.f,40.f},true))return 2;
    const auto id=services.prepare(Service::NearbyChat,{},L"Hello");
    if(id==0||services.pending_count()!=1||
       services.prepare(Service::InstantMessage,"invalid",L"hi")!=0||
       services.prepare(Service::NearbyChat,{},L"")!=0)return 3;
    if(services.accept_server_result(id,true,true)||
       !services.mark_sent(id)||
       services.accept_server_result(id,false,true)||
       services.entries().front().state!=IntentState::Sent||
       !services.accept_server_result(id,true,true)||
       services.entries().front().state!=IntentState::ServerConfirmed)return 4;
    if(services.pending_count()!=0||
       services.accept_server_result(id,true,true))return 5;
    const auto teleport=services.prepare(Service::Teleport,{},L"");
    if(teleport==0||!services.mark_sent(teleport))return 6;
    if(data.apply_objects({{object,mesh,1.f,2.f,3.f}},false) ||
       !data.apply_objects({{object,mesh,1.f,2.f,3.f}},true) ||
       data.objects().size()!=1)return 7;
    const std::vector<WorldObject> duplicate={{object,mesh,1.f,2.f,3.f},
                                              {object,mesh,2.f,3.f,4.f}};
    if(data.apply_objects(duplicate,true)||data.objects().size()!=1)return 8;
    if(data.apply_objects({{object,mesh,std::numeric_limits<float>::infinity(),0,0}},
                          true))return 9;
    if(data.apply_inventory({{object,"",mesh,L"Hat",1U}},false)||
       !data.apply_inventory({{object,"",mesh,L"Hat",1U}},true)||
       data.inventory().size()!=1)return 10;
    if(data.apply_inventory({{object,"",mesh,L"",1U}},true)||
       data.inventory().size()!=1)return 11;
    if(data.apply_avatar({agent,mesh,object,{mesh},{{object,mesh,true}}},false))
        return 14;
    if(!data.apply_avatar({agent,mesh,object,{mesh},{{object,mesh,true}}},true)||
       !data.avatar()||data.avatar()->attachments.size()!=1)return 15;
    if(data.apply_avatar({agent,mesh,object,{mesh,mesh},{}},true) ||
       data.avatar()->texture_asset_ids.size()!=1)return 16;
    link.reset();
    if(data.avatar()||!data.objects().empty()||!data.inventory().empty())
        return 17;
    if(services.mark_sent(teleport)||services.pending_count()!=0||
       services.accept_server_result(teleport,true,true)||
       services.prepare(Service::Building,object,L"edit")!=0)return 12;
    data.clear();services.clear();
    if(!data.objects().empty()||!data.inventory().empty()||
       !services.entries().empty())return 13;
    return 0;
}
