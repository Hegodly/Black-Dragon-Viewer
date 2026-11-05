#include "kami/protocol/reliable_circuit.hpp"
#include <algorithm>
#include <chrono>
#include <utility>

namespace kami::protocol::reliable {
bool Window::can_send(const lludp::Packet& packet) const noexcept {
    if(!(packet.flags&lludp::kReliable))return true;
    if(packet.sequence==0U)return false;
    return pending_.contains(packet.sequence)||pending_.size()<kMaxPending;
}
bool Window::record_sent(const lludp::Packet& packet,Time now) {
    if(!(packet.flags&lludp::kReliable))return true;
    if(!can_send(packet))return false;
    auto it=pending_.find(packet.sequence);
    if(it!=pending_.end()){
        // Startup uses the same sequence ID for its explicit handshake
        // retries. Preserve the retry budget rather than extending it.
        it->second.packet=packet;
        it->second.sent=now;
        it->second.retries=std::min(kMaxRetries,it->second.retries+1U);
        return true;
    }
    pending_.emplace(packet.sequence,Outgoing{packet,now,0U});
    return true;
}
void Window::acknowledge(std::uint32_t sequence) noexcept {
    pending_.erase(sequence);
}
bool Window::accept_incoming(std::uint32_t sequence) noexcept {
    if(sequence==0U)return false;
    for(std::size_t i=0U;i<seen_size_;++i)
        if(seen_[i]==sequence)return false;
    seen_[seen_next_]=sequence;
    seen_next_=(seen_next_+1U)%kSeenCapacity;
    seen_size_=std::min(kSeenCapacity,seen_size_+1U);
    return true;
}
std::vector<lludp::Packet> Window::due(Time now,unsigned budget) {
    std::vector<lludp::Packet> result;
    budget=std::min(budget,8U);
    result.reserve(budget);
    for(auto it=pending_.begin();it!=pending_.end();) {
        const unsigned bounded=std::min(it->second.retries,4U);
        const auto interval=std::chrono::milliseconds(400U<<bounded);
        if(now-it->second.sent<interval){++it;continue;}
        if(it->second.retries>=kMaxRetries){
            ++expired_count_;
            it=pending_.erase(it);
            continue;
        }
        if(result.size()>=budget){++it;continue;}
        lludp::Packet resend=it->second.packet;
        resend.flags|=lludp::kResent;
        result.push_back(std::move(resend));
        it->second.sent=now;
        ++it->second.retries;
        ++resent_count_;
        ++it;
    }
    return result;
}
void Window::clear() noexcept {
    pending_.clear();
    seen_={};
    seen_size_=seen_next_=0U;
    resent_count_=expired_count_=0U;
}
} // namespace kami::protocol::reliable
