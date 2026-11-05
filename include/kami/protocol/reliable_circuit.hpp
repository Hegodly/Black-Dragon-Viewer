#pragma once
#include "kami/protocol/lludp.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <vector>

namespace kami::protocol::reliable {
// A bounded LLUDP reliable-packet window modeled after the Linden Lab
// LLCircuitData ACK/retransmit lifecycle. The packet bytes and sequence
// are preserved; retries set the LLUDP RESENT flag, never resequence.
// No networking or simulator secrets are held by this helper.
class Window final {
public:
    using Clock=std::chrono::steady_clock;
    using Time=Clock::time_point;
    static constexpr std::size_t kMaxPending=128U;
    static constexpr std::size_t kSeenCapacity=512U;
    static constexpr unsigned kMaxRetries=5U;

    [[nodiscard]] bool can_send(const lludp::Packet& packet) const noexcept;
    // Record before socket send. If a nonblocking socket temporarily fails,
    // the same sequence can still be retransmitted. May allocate memory.
    [[nodiscard]] bool record_sent(const lludp::Packet& packet,Time now);
    void acknowledge(std::uint32_t sequence) noexcept;
    [[nodiscard]] bool accept_incoming(std::uint32_t sequence) noexcept;
    [[nodiscard]] std::vector<lludp::Packet> due(Time now,unsigned budget);
    void clear() noexcept;
    [[nodiscard]] std::size_t pending() const noexcept {return pending_.size();}
    [[nodiscard]] std::uint64_t resent_packets() const noexcept {
        return resent_count_;
    }
    [[nodiscard]] std::uint64_t expired_packets() const noexcept {
        return expired_count_;
    }
private:
    struct Outgoing {
        lludp::Packet packet;
        Time sent{};
        unsigned retries=0U;
    };
    std::map<std::uint32_t,Outgoing> pending_;
    std::array<std::uint32_t,kSeenCapacity> seen_{};
    std::size_t seen_size_=0U,seen_next_=0U;
    std::uint64_t resent_count_=0U,expired_count_=0U;
};
} // namespace kami::protocol::reliable
