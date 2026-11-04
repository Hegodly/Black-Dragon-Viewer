#pragma once
#include "kami/protocol/lludp.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace kami::protocol::objectspatial {
// The geometry is an opt-in *untextured* subset: original SL profile/path
// classes. Complex cuts, mesh, sculpt, flexible and hollow profiles
// remain unsupported until actual asset/volume tessellation exists.
enum class PrimitiveKind : std::uint8_t { Unknown, Box, Cylinder, Sphere, Prism };
struct Transform {
    std::uint32_t local_id=0;
    float x=0.f,y=0.f,z=0.f;
    float sx=0.f,sy=0.f,sz=0.f;
    std::uint32_t parent_id=0; // 0 = simulator region-space root
    // LLQuaternion x,y,z + recovered positive w (unit length).
    float rx=0.f,ry=0.f,rz=0.f,rw=1.f;
    PrimitiveKind primitive=PrimitiveKind::Unknown;
    std::string mesh_asset_id; // authenticated ExtraParams id, NOT downloaded
    bool default_texture_present=false; // TE reference only, NOT a downloaded texture
};
// Aggregate only counts, never simulator IDs or user payloads.
struct FullDecodeAudit {
    std::uint32_t volume=0,non_volume=0,linked=0;
    std::uint32_t unsupported_motion=0,invalid_transform=0;
    std::uint32_t accepted_roots=0;
    std::uint32_t accepted_children=0, unsupported_shapes=0;
    std::uint32_t texture_references=0,mesh_references=0;
};
// The full (High 12) message uses individual message-template fields,
// unlike the packed compressed (High 13) message.
[[nodiscard]] bool decode_full_transforms(
    const lludp::Packet& packet,std::vector<Transform>& out,
    FullDecodeAudit* audit=nullptr,
    std::vector<std::uint32_t>* parent_references=nullptr,
    std::vector<Transform>* child_transforms=nullptr,
    std::vector<std::uint32_t>* avatar_root_ids=nullptr) noexcept;
[[nodiscard]] bool decode_compressed_transforms(
    const lludp::Packet& packet,std::vector<Transform>& out,
    std::vector<Transform>* child_transforms=nullptr) noexcept;
// ObjectUpdateCached is an inventory of local IDs, NOT full object data.
[[nodiscard]] bool decode_cached_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
[[nodiscard]] bool decode_killed_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
} // namespace kami::protocol::objectspatial
