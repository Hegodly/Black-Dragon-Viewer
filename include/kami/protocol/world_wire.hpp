#pragma once
#include "kami/protocol/lludp.hpp"
#include <cstddef>
#include <cstdint>

namespace kami::protocol::worldwire {
// Framing diagnostics for simulator-originated LayerData and ObjectUpdate
// messages. These are NOT geometry/terrain/mesh/texture decoders.
enum class Kind : std::uint8_t {
    Unrelated, Invalid, LayerData, ObjectFull,
    ObjectCompressed, ObjectCached, ObjectTerse, ObjectKilled
};
struct Sample {
    Kind kind=Kind::Unrelated;
    unsigned blocks=0;
    std::size_t payload_bytes=0;
    std::uint32_t first_local_id=0;
    bool has_local_id=false;
    bool complete_frame=false;
};
[[nodiscard]] Sample inspect(const lludp::Packet& packet) noexcept;
}
