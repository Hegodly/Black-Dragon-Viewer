#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace kami::protocol::lludp {
inline constexpr std::uint8_t kZerocoded=0x80U;
inline constexpr std::uint8_t kReliable=0x40U;
inline constexpr std::uint8_t kResent=0x20U;
inline constexpr std::uint8_t kAppendedAcks=0x10U;
// Original wire envelope, not the message_template.msg block decoder.
struct Packet {
    std::uint8_t flags=0;
    std::uint32_t sequence=0;
    std::vector<std::uint8_t> extra_header;
    std::vector<std::uint8_t> body; // DECODED message ID and message fields
    std::vector<std::uint32_t> appended_acks;
};
[[nodiscard]] bool decode(std::span<const std::uint8_t> datagram,
                          Packet& output);
[[nodiscard]] bool encode(const Packet& packet,
                          std::vector<std::uint8_t>& output);
// Parses only variable-length message ID; no template data blocks yet.
// Caller must handle the message template for fields and block validation.
[[nodiscard]] bool message_number(std::span<const std::uint8_t> body,
                                  std::uint32_t& message_id,
                                  unsigned& id_bytes) noexcept;
} // namespace kami::protocol::lludp
