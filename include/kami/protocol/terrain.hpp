#pragma once
#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace kami::protocol::terrain {
// Independent, bounded decoder for the simulator's land LayerData
// compressed 16x16/32x32 DCT height patches. Coordinates/heights are
// simulator-sourced; this does not decode SL textures or prim meshes.
struct Patch {
    std::uint8_t size=0;
    std::uint8_t x=0, y=0;
    std::array<float,1024> elevation{};
};
// Transactional: malformed data leaves 'out' empty. Terrain layer only.
[[nodiscard]] bool decode_land_layer(const lludp::Packet& packet,
                                     std::vector<Patch>& out);
} // namespace kami::protocol::terrain
