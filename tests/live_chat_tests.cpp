#include "kami/protocol/live_chat.hpp"
#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>
#include <vector>
using namespace kami::protocol;
namespace {
LoginResult login(){
    LoginResult result{};
    result.status=LoginStatus::Accepted;
    result.handoff.agent_id="12345678-90ab-4cde-8fab-000000000001";
    result.handoff.session_id="12345678-90ab-4cde-8fab-000000000002";
    result.handoff.simulator_host="127.0.0.1";
    result.handoff.simulator_port=12345U;
    result.handoff.seed_capability=L"https://sim1234.agni.lindenlab.com:12345/cap/abc";
    result.circuit_code=1234U;
    return result;
}
}
int main(){
    lludp::Packet p{};
    auto auth=login();
    if(!sim::build_nearby_chat(auth,L"Hello KAMI",42U,p)||
       p.sequence!=42U||p.flags!=lludp::kReliable||
       p.body.size()!=4U+32U+2U+11U+1U+4U||
       p.body[3]!=80U||p.body[38]!='H'||p.body.back()!=0U)
        return 1;
    std::vector<std::uint8_t> bytes;
    lludp::Packet unpacked{};
    if(!lludp::encode(p,bytes)||!lludp::decode(bytes,unpacked)||
       unpacked.body!=p.body)return 2;
    if(sim::build_nearby_chat(auth,L"  ",43U,p)||
       sim::build_nearby_chat(auth,L"contains\nnewline",43U,p)||
       sim::build_nearby_chat(auth,std::wstring(513,L'A'),43U,p))return 3;
    lludp::Packet live{};
    live.body={0xff,0xff,0x00,139,6,'A','l','i','c','e',0};
    live.body.insert(live.body.end(),32U,0U); // source and owner UUIDs
    live.body.push_back(1U);live.body.push_back(1U);live.body.push_back(1U);
    live.body.insert(live.body.end(),12U,0U); // position
    live.body.push_back(6U);live.body.push_back(0U);
    live.body.insert(live.body.end(),{'H','e','l','l','o',0});
    sim::ChatLine found{};
    if(!sim::decode_simulator_chat(live,found)||
       found.sender!=L"Alice"||found.message!=L"Hello"||
       found.source_type!=1U)return 4;
    auto broken=live;broken.body.pop_back();
    if(sim::decode_simulator_chat(broken,found))return 5;
    broken=live;broken.body[4]=255U;
    if(sim::decode_simulator_chat(broken,found))return 6;
    broken=live;broken.body[3]=80U;
    if(sim::decode_simulator_chat(broken,found))return 7;
    return 0;
}
