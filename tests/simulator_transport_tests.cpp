#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <ws2tcpip.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace kami::protocol;
// Preserve decoded root objects for real meshes even with R65's 2,048-prim cap.
static_assert(sim::Circuit::kMaxRootObjects>=4096U);
namespace {
LoginResult make_login(unsigned short port) {
    LoginResult result{};
    result.status=LoginStatus::Accepted;
    result.handoff.agent_id="12345678-90ab-4cde-8fab-000000000001";
    result.handoff.session_id="12345678-90ab-4cde-8fab-000000000002";
    result.handoff.simulator_host="127.0.0.1";
    result.handoff.simulator_port=port;
    result.handoff.seed_capability=L"https://sim1234.agni.lindenlab.com:12345/cap/abc";
    result.circuit_code=0x12345678U;
    return result;
}
bool send_test(SOCKET server,const sockaddr_in& viewer,int viewer_size,
               lludp::Packet packet) {
    std::vector<std::uint8_t> encoded;
    if(!lludp::encode(packet,encoded))return false;
    return sendto(server,reinterpret_cast<const char*>(encoded.data()),
                  static_cast<int>(encoded.size()),0,
                  reinterpret_cast<const sockaddr*>(&viewer),viewer_size)==
           static_cast<int>(encoded.size());
}
}
int main() {
    // Catch regressions to the old 512/1024 object retention limits.
    static_assert(sim::Circuit::kMaxRootObjects>=2048U);
    static_assert(sim::Circuit::kMaxLinkedObjects>=8192U);
    static_assert(sim::Circuit::kMaxMissingObjectRequests==4096U);
    static_assert(sim::Circuit::kMaxMissingObjectRequests>=
                  sim::Circuit::kMaxRootObjects);
    // The mesh fetch pool may never exceed the shared authenticated
    // scheduler's upper bound on active downloads.
    static_assert(sim::Circuit::kMaxConcurrentMeshWorkers==4U);
    static_assert(sim::Circuit::kMaxConcurrentMeshWorkers<=
                  load::Scheduler::kMaxConcurrentDownloads);
    const auto now=std::chrono::steady_clock::now();
    const auto recent=now-std::chrono::seconds(30);
    const auto stale=now-std::chrono::seconds(95);
    if(sim::detail::reclaim_missing_request(4U,stale,now)||
       sim::detail::reclaim_missing_request(5U,recent,now)||
       sim::detail::reclaim_missing_request(5U,{},now)||
       !sim::detail::reclaim_missing_request(5U,stale,now))
        return 27;
    // R65: the full queue in decorated regions must not let a speculative
    // terse update take precedence over a verified cache miss/linkset root.
    if(!sim::detail::prefer_missing_request(true,4U,stale,false,0U,recent)||
       sim::detail::prefer_missing_request(false,0U,recent,true,4U,stale)||
       !sim::detail::prefer_missing_request(true,0U,recent,true,1U,stale)||
       sim::detail::prefer_missing_request(true,2U,recent,true,1U,stale)||
       !sim::detail::prefer_missing_request(true,1U,stale,true,1U,recent))
        return 28;
    // R68: unresolved linked-object parents MUST outrank even earlier
    // cached-object requests and speculative movement records; stable
    // attempt/backoff ordering is retained within each category.
    if(!sim::detail::prefer_parent_missing_request(
            true,true,4U,stale,false,true,0U,recent)||
       sim::detail::prefer_parent_missing_request(
            false,true,0U,recent,true,true,4U,stale)||
       !sim::detail::prefer_parent_missing_request(
            false,true,2U,stale,false,false,0U,recent)||
       !sim::detail::prefer_parent_missing_request(
            true,true,0U,recent,true,true,2U,stale))
        return 56;
    WSADATA wsa{};
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0)return 1;
    const SOCKET server=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(server==INVALID_SOCKET){WSACleanup();return 2;}
    DWORD timeout=3000;
    setsockopt(server,SOL_SOCKET,SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    sockaddr_in local{};
    local.sin_family=AF_INET;
    local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    local.sin_port=0;
    if(bind(server,reinterpret_cast<const sockaddr*>(&local),
            sizeof(local))!=0){closesocket(server);WSACleanup();return 3;}
    int length=sizeof(local);
    if(getsockname(server,reinterpret_cast<sockaddr*>(&local),&length)!=0) {
        closesocket(server);WSACleanup();return 4;
    }
    const auto login=make_login(ntohs(local.sin_port));
    std::atomic<int> server_result{0};
    std::thread mock([&] {
        sockaddr_in viewer{};
        int viewer_len=sizeof(viewer);
        std::array<std::uint8_t,2048> bytes{};
        for(int i=0;i<2;++i){
            const int n=recvfrom(server,reinterpret_cast<char*>(bytes.data()),
                static_cast<int>(bytes.size()),0,
                reinterpret_cast<sockaddr*>(&viewer),&viewer_len);
            if(n<=0){server_result=5;return;}
            lludp::Packet p;
            if(!lludp::decode(
                std::span<const std::uint8_t>(bytes.data(),static_cast<std::size_t>(n)),p)){
                server_result=6;return;
            }
            std::uint32_t id=0;unsigned id_bytes=0;
            if(!lludp::message_number(p.body,id,id_bytes)||
               id!=(i==0?0xffff0003U:0xffff00f9U)){
                server_result=7;return;
            }
        }
        // All replies originate at the same connected loopback socket.
        lludp::Packet ping{};
        ping.sequence=20;
        ping.body={0x01,0x2a,0,0,0,0};
        if(!send_test(server,viewer,viewer_len,ping)){server_result=8;return;}
        lludp::Packet handshake{};
        handshake.flags=lludp::kReliable;
        handshake.sequence=21;
        handshake.body.assign(215U,0);
        handshake.body[0]=0xff;handshake.body[1]=0xff;
        handshake.body[3]=0x94;handshake.body[9]=4;
        handshake.body[10]='T';handshake.body[11]='e';
        handshake.body[12]='s';handshake.body[13]='t';
        if(!send_test(server,viewer,viewer_len,handshake)) {
            server_result=9;return;
        }
        lludp::Packet movement{};
        movement.sequence=22;
        movement.body={0xff,0xff,0x00,0xfa};
        std::array<std::uint8_t,16> agent{},session{};
        if(!sim::parse_uuid(login.handoff.agent_id,agent)||
           !sim::parse_uuid(login.handoff.session_id,session)){
            server_result=10;return;
        }
        movement.body.insert(movement.body.end(),agent.begin(),agent.end());
        movement.body.insert(movement.body.end(),session.begin(),session.end());
        movement.body.insert(movement.body.end(),38U,0);
        // Realistically formatted but synthetic avatar location.
        const std::array<float,3> xyz={130.f,98.f,25.f};
        std::memcpy(movement.body.data()+36,xyz.data(),sizeof(xyz));
        if(!send_test(server,viewer,viewer_len,movement)){
            server_result=11;return;
        }
        // Wait for the viewer's session-authorized neutral AgentUpdate.
        bool got_update=false;
        for(int attempt=0;attempt<12;++attempt){
            const int n=recvfrom(server,reinterpret_cast<char*>(bytes.data()),
                static_cast<int>(bytes.size()),0,
                reinterpret_cast<sockaddr*>(&viewer),&viewer_len);
            if(n<=0){server_result=14;return;}
            lludp::Packet p{};
            if(!lludp::decode(
                std::span<const std::uint8_t>(bytes.data(),
                    static_cast<std::size_t>(n)),p)){
                server_result=15;return;
            }
            std::uint32_t id=0;unsigned width=0;
            if(!lludp::message_number(p.body,id,width))continue;
            if(id==4U){
                if(p.body.size()!=115U||p.body[114]!=0U||
                   p.body[110]!=0U||p.body[111]!=0U||
                   p.body[112]!=0U||p.body[113]!=0U){
                    server_result=16;return;
                }
                got_update=true;break;
            }
        }
        if(!got_update){server_result=17;return;}
        lludp::Packet layer{};
        layer.sequence=23;
        layer.body={11,0x4c,4,0,1,2,3,4};
        lludp::Packet killed{};
        killed.sequence=24;
        killed.body={16,1,9,0,0,0};
        // An independently encoded 16x16 constant-height land
        // patch: patch (3,4), 20m DC offset + 2m range midpoint.
        lludp::Packet good_land{};
        good_land.sequence=25;
        good_land.body={
            11,76,14,0,16,0,16,76,64,0,0,160,65,4,0,100,38,16
        };
        if(!send_test(server,viewer,viewer_len,layer)||
           !send_test(server,viewer,viewer_len,killed)||
           !send_test(server,viewer,viewer_len,good_land)){
            server_result=18;return;
        }
        // Once movement is requested, verify actual server-directed
        // control flag bytes, then verify the focused UI can release them.
        constexpr auto expected=sim::kAtPos|sim::kFastAt|sim::kTurnRight;
        bool got_movement=false,got_release=false;
        for(unsigned attempt=0;attempt<30U;++attempt){
            const int n=recvfrom(server,reinterpret_cast<char*>(bytes.data()),
                static_cast<int>(bytes.size()),0,
                reinterpret_cast<sockaddr*>(&viewer),&viewer_len);
            if(n<=0){server_result=19;return;}
            lludp::Packet p{};
            if(!lludp::decode(std::span<const std::uint8_t>(
                bytes.data(),static_cast<std::size_t>(n)),p))continue;
            if(p.body.size()!=115U||p.body[0]!=4U)continue;
            std::uint32_t sent=0;
            std::memcpy(&sent,p.body.data()+110U,sizeof(sent));
            if(sent==expected)got_movement=true;
            if(got_movement&&sent==0U){got_release=true;break;}
        }
        if(!got_movement||!got_release)server_result=20;
    });
    sim::Circuit circuit;
    const bool started=circuit.start(login);
    if(started){
        const auto begin=std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now()-begin<
              std::chrono::seconds(3)&&
              !(circuit.movement_seen()&&
                circuit.agent_updates_sent()>0U&&
                circuit.layer_packets()>=2U&&
                circuit.object_update_packets()>0U&&
                circuit.decoded_land_packets()>0U)){
            circuit.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    // The UI requests real movement only after the session is confirmed.
    // The simulator receives it as an authenticated AgentUpdate. Release
    // inputs and verify an explicit all-zero update follows.
    if(started&&circuit.movement_seen()){
        constexpr auto motion=sim::kAtPos|sim::kFastAt|sim::kTurnRight;
        const auto until=std::chrono::steady_clock::now()+
            std::chrono::milliseconds(650);
        while(std::chrono::steady_clock::now()<until){
            circuit.set_requested_controls(motion);
            circuit.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        const auto release_until=std::chrono::steady_clock::now()+
            std::chrono::milliseconds(650);
        while(std::chrono::steady_clock::now()<release_until){
            circuit.set_requested_controls(0U);
            circuit.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    mock.join();
    const bool sent_nonzero_controls=
        circuit.movement_control_packets_sent()>0U;
    const bool ok=started&&server_result==0&&circuit.region_seen()&&
                  sent_nonzero_controls&&
                  circuit.agent_updates_sent()>=1U&&
                  circuit.layer_packets()==2U&&
                  circuit.terrain_packets()==2U&&
                  circuit.decoded_land_packets()==1U&&
                  circuit.rejected_land_packets()==1U&&
                  circuit.decoded_land_patches().size()==1U&&
                  circuit.land_revision()>0U&&
                  circuit.wind_packets()==0U&&
                  circuit.cloud_packets()==0U&&
                  circuit.object_update_packets()==1U&&
                  circuit.killed_object_packets()==1U&&
                  circuit.full_object_packets()==0U&&
                  circuit.cached_object_packets()==0U&&
                  circuit.region_metadata().verified&&
                  circuit.region_metadata().name=="Test"&&
                  circuit.movement_position().verified&&
                  circuit.movement_position().x==130.f&&
                  circuit.movement_position().z==25.f&&
                  circuit.movement_seen()&&
                  circuit.state()==sim::CircuitState::MovementConfirmed&&
                  circuit.packets_received()>=3;
    circuit.stop();closesocket(server);WSACleanup();
    if(!ok||circuit.agent_updates_sent()!=0U||
       circuit.movement_control_packets_sent()!=0U)return 12;
    sim::Circuit rejected{};
    auto invalid=login;
    invalid.status=LoginStatus::Rejected;
    if(rejected.start(invalid)||rejected.state()!=sim::CircuitState::Closed)
        return 13;
    return 0;
}
