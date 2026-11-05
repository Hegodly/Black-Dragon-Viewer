#include "kami/protocol/reliable_circuit.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <array>
#include <chrono>
#include <cstdint>
using kami::protocol::lludp::Packet;
using kami::protocol::lludp::kReliable;
using kami::protocol::lludp::kResent;
using kami::protocol::reliable::Window;
int main(){
    Window q;
    const auto time=Window::Time{}+std::chrono::seconds(20);
    Packet a{};
    a.flags=kReliable|kami::protocol::lludp::kZerocoded;
    a.sequence=3U;
    a.body={0xff,0x03,0x01};
    if(!q.record_sent(a,time)||q.pending()!=1U)return 1;
    if(!q.due(time+std::chrono::milliseconds(399),8).empty())return 2;
    auto due=q.due(time+std::chrono::milliseconds(400),8);
    if(due.size()!=1U||due[0].sequence!=3U||
       !(due[0].flags&kResent)||due[0].body!=a.body||
       q.resent_packets()!=1U)return 3;
    if(!q.due(time+std::chrono::milliseconds(1100),8).empty())return 4;
    due=q.due(time+std::chrono::milliseconds(1200),8);
    if(due.size()!=1U||q.resent_packets()!=2U)return 5;
    q.acknowledge(3U);
    if(q.pending()||!q.due(time+std::chrono::seconds(12),8).empty())
        return 6;
    if(!q.accept_incoming(21U)||q.accept_incoming(21U)||
       !q.accept_incoming(22U)||q.accept_incoming(0U))return 7;
    for(std::uint32_t seq=23U;seq<535U;++seq)
        if(!q.accept_incoming(seq))return 8;
    if(!q.accept_incoming(21U))return 9; // ring evicts old replay IDs
    q.clear();
    if(!q.accept_incoming(21U)||q.resent_packets())return 10;
    // Must not admit unlimited reliable messages while server is silent.
    for(std::uint32_t seq=1U;seq<=128U;++seq){
        Packet p{};p.flags=kReliable;p.sequence=seq;
        if(!q.record_sent(p,time))return 11;
    }
    Packet overflow{};overflow.flags=kReliable;overflow.sequence=129U;
    if(q.can_send(overflow)||q.record_sent(overflow,time))return 12;
    // Retries retain their original sequence, original body and ACK scope.
    auto batch=q.due(time+std::chrono::milliseconds(400),8);
    if(batch.size()!=8U||q.pending()!=128U)return 13;
    q.acknowledge(10U);
    if(!q.can_send(overflow)||!q.record_sent(overflow,time))return 14;
    q.clear();
    Packet exp{};exp.sequence=991;exp.flags=kReliable;
    if(!q.record_sent(exp,time))return 15;
    auto next=time;
    for(unsigned i=0;i<5;++i) {
        next+=std::chrono::seconds(8);
        if(q.due(next,1).size()!=1U)return 16;
    }
    next+=std::chrono::seconds(8);
    if(!q.due(next,1).empty()||q.pending()!=0U||
       q.expired_packets()!=1U)return 17;
    // Non-reliable packets must never consume the pending window.
    Packet noack{};noack.sequence=1000U;
    if(!q.record_sent(noack,time)||q.pending()!=0U)return 18;
    // An actual SL PacketAck body uses a template-level little-endian
    // U32 sequence (distinct from big-endian LLUDP header/piggyback ACK).
    const std::array<std::uint32_t,2U> ids{0x11223344U,17U};
    Packet ack{};
    if(!kami::protocol::sim::packet_ack(ids,99U,ack))return 19;
    std::vector<std::uint8_t> wire;
    if(!kami::protocol::lludp::encode(ack,wire))return 20;
    Packet decoded{};
    if(!kami::protocol::lludp::decode(wire,decoded)||
       decoded.body.size()!=13U||decoded.body[4]!=2U||
       decoded.body[5]!=0x44U||decoded.body[6]!=0x33U||
       decoded.body[7]!=0x22U||decoded.body[8]!=0x11U)return 21;
    // Real appended ACKs in the LLUDP packet header are network order.
    Packet piggy{};
    piggy.sequence=88U;
    piggy.flags=kami::protocol::lludp::kAppendedAcks;
    piggy.body={0xff,0xff,0xff,0xfb,0};
    piggy.appended_acks={0x11223344U,17U};
    if(!kami::protocol::lludp::encode(piggy,wire)||
       !kami::protocol::lludp::decode(wire,decoded)||
       decoded.appended_acks.size()!=2U||
       decoded.appended_acks[0]!=0x11223344U||
       decoded.appended_acks[1]!=17U)return 22;
    return 0;
}
