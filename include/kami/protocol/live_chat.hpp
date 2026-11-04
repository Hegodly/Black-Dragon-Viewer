#pragma once
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/login.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace kami::protocol::sim {
struct ChatLine {
    std::wstring sender;
    std::wstring message;
    std::uint8_t source_type=0;
    std::uint8_t chat_type=0;
};
// Decode authenticated-socket simulator chat (Low 139). Caller must ensure
// this packet came from the connected simulator; decoder alone is not auth.
[[nodiscard]] bool decode_simulator_chat(const lludp::Packet& packet,
                                         ChatLine& out);
// Compose real public nearby chat (Low 80, normal type, channel 0).
// Successful encoding or sending does NOT prove delivery/read.
[[nodiscard]] bool build_nearby_chat(const LoginResult& login,
                                     std::wstring_view text,
                                     std::uint32_t sequence,
                                     lludp::Packet& out);
} // namespace kami::protocol::sim
