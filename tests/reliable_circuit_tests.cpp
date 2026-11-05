#include "kami/protocol/reliable_circuit.hpp"
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
    return 0;
}
