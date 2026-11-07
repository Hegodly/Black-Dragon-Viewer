#pragma once
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/texture_entry.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <set>
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
    bool volume_params_full_layout=false; // High 12 fields; false = packed High 13
    // Preserve the exact simulator path/profile bytes, including cuts,
    // taper, twist, hollow and curve settings. Geometry must tessellate
    // these values rather than guessing from PrimitiveKind alone.
    std::array<std::uint8_t,23U> volume_params{};
    bool default_texture_present=false; // TE reference only, NOT a downloaded texture
    std::string default_texture_asset_id; // UUID from verified server TE
    std::array<std::string,45U> face_texture_asset_ids{}; // face-specific TE UUIDs
    std::array<bool,45U> face_texture_override{}; // explicit null distinct from default
    // Authentic SL diffuse TextureEntry parameters per numbered face.
    std::array<texture::UVTransform,45U> face_uv{};
    std::array<std::array<float,4U>,45U> face_rgba{};
    bool face_rgba_present=false;
    // Authentic default TextureEntry tint (per-face overrides unavailable).
    // RGB is baked into DX12 untextured preview; alpha retained for future
    // translucent material pass. No texture image is implied to be loaded.
    bool base_tint_present=false;
    float base_r=1.f,base_g=1.f,base_b=1.f,base_a=1.f;
    std::array<std::string,45U> face_material_ids{};
    bool material_params_seen=false;
    bool reflection_probe=false;
    float probe_ambiance=1.f,probe_clip=.1f;
    std::uint8_t probe_flags=0U;
};
// Apply a compressed update without incorrectly restoring stale mesh and
// primitive metadata from an earlier full update. Offline unit-testable.
[[nodiscard]] bool same_spatial_transform(const Transform&,const Transform&) noexcept;
[[nodiscard]] Transform merge_compressed_transform(
    const Transform& incoming,const Transform& previous);
// Resolve an authenticated linked child's actual region-space position,
// composing each verified parent rotation and translation. Do not use
// the remote linkset root position as a substitute for the child when
// scheduling nearby mesh, JPEG2000 or PBR assets. Fail closed for missing
// parents, avatars, hierarchy cycles, bad poses and excessive depth.
[[nodiscard]] bool resolve_world_position(
    const Transform& object,
    const std::map<std::uint32_t,Transform>& roots,
    const std::map<std::uint32_t,Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    std::array<float,3>& world) noexcept;
// Authenticated full High12 PCode=47 avatar identity and region pose.
// The simulator UUID is compared to the authenticated login AgentID
// before this can ever advance the own-avatar camera/world origin.
struct AvatarFullPose {
    std::uint32_t local_id=0U;
    std::array<std::uint8_t,16U> full_id{};
    float x=0.f,y=0.f,z=0.f;
};
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
    // Modern official LLNetworkData parameter types, not mesh UUIDs.
    std::uint32_t extended_mesh_records=0,render_material_records=0;
    std::uint32_t reflection_probe_records=0;
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
    std::vector<std::uint32_t>* avatar_root_ids=nullptr,
    std::vector<AvatarFullPose>* avatar_poses=nullptr) noexcept;
[[nodiscard]] bool decode_compressed_transforms(
    const lludp::Packet& packet,std::vector<Transform>& out,
    std::vector<Transform>* child_transforms=nullptr,
    CompressedDecodeAudit* audit=nullptr) noexcept;
// High 15 ImprovedTerseObjectUpdate: update pose of already-known objects.
// Does not create objects, invent geometry, or interpret avatar outfits.
struct TersePose {
    std::uint32_t local_id=0;
    float x=0.f,y=0.f,z=0.f;
    float rx=0.f,ry=0.f,rz=0.f,rw=1.f;
};
[[nodiscard]] bool decode_terse_poses(
    const lludp::Packet& packet,std::vector<TersePose>& out) noexcept;
[[nodiscard]] bool apply_terse_pose(
    Transform& existing,const TersePose& update) noexcept;
// Appends the change only when the pose differs enough to warrant a GPU
// geometry upload. Unknown objects should be ignored by the caller.
// ObjectUpdateCached is an inventory of local IDs, NOT full object data.
[[nodiscard]] bool decode_cached_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
[[nodiscard]] bool decode_killed_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
} // namespace kami::protocol::objectspatial

