#pragma once
#include "kami/protocol/lludp.hpp"
#include <cstdint>
#include <string>
#include <span>
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
    bool volume_params_seen=false; // full 23-byte SL path/profile available
    bool default_texture_present=false; // TE reference only, NOT a downloaded texture
};
// Apply a compressed update without incorrectly restoring stale mesh and
// primitive metadata from an earlier full update. Offline unit-testable.
[[nodiscard]] bool same_spatial_transform(const Transform&,const Transform&) noexcept;
[[nodiscard]] Transform merge_compressed_transform(
    const Transform& incoming,const Transform& previous);
// Aggregate only counts, never simulator IDs or user payloads.
struct FullDecodeAudit {
    std::uint32_t volume=0,non_volume=0,linked=0;
    std::uint32_t unsupported_motion=0,invalid_transform=0;
    std::uint32_t accepted_roots=0;
    std::uint32_t accepted_children=0, unsupported_shapes=0;
    std::uint32_t texture_references=0,mesh_references=0;
    std::uint32_t extra_records=0,sculpt_records=0,mesh_sculpt_records=0;
    // Validated High 12 ExtraParams breakdown; counters only, no asset IDs.
    std::uint32_t extra_envelopes=0,malformed_extra_envelopes=0;
    std::uint32_t flexible_records=0,light_records=0;
    std::uint32_t projector_records=0,other_extra_records=0;
};
struct CompressedDecodeAudit {
    std::uint32_t extras_seen=0,extra_records=0,sculpt_records=0;
    std::uint32_t shape_fields=0,supported_shapes=0;
    std::uint32_t mesh_references=0,unsupported_optional=0;
    std::uint32_t legacy_particle_blocks=0;
};
[[nodiscard]] PrimitiveKind classify_simple_shape(
    std::span<const std::uint8_t> shape) noexcept;
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
    std::vector<Transform>* child_transforms=nullptr,
    CompressedDecodeAudit* audit=nullptr) noexcept;
// ObjectUpdateCached is an inventory of local IDs, NOT full object data.
[[nodiscard]] bool decode_cached_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
[[nodiscard]] bool decode_killed_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
} // namespace kami::protocol::objectspatial
