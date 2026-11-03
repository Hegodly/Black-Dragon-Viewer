#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <ws2tcpip.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace kami::protocol;
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
        if(!send_test(server,viewer,viewer_len,movement))
            server_result=11;
    });
    sim::Circuit circuit;
    const bool started=circuit.start(login);
    if(started){
        const auto begin=std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now()-begin<
              std::chrono::seconds(3)&&!circuit.movement_seen()){
            circuit.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    mock.join();
    const bool ok=started&&server_result==0&&circuit.region_seen()&&
                  circuit.movement_seen()&&
                  circuit.state()==sim::CircuitState::MovementConfirmed&&
                  circuit.packets_received()>=3;
    circuit.stop();closesocket(server);WSACleanup();
    if(!ok)return 12;
    sim::Circuit rejected{};
    auto invalid=login;
    invalid.status=LoginStatus::Rejected;
    if(rejected.start(invalid)||rejected.state()!=sim::CircuitState::Closed)
        return 13;
    return 0;
}
