#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace kami::protocol::pbr {
enum Slot : unsigned { BaseColor, Normal, MetallicRoughness, Emissive, Occlusion, Slots };
struct Transform {
    std::array<float,2> scale{1.f,1.f},offset{};
    float rotation=0.f;
    bool operator==(const Transform&) const=default;
};
struct Material {
    std::array<std::string,Slots> images{};
    std::array<Transform,Slots> uv{};
    std::array<float,4> base{1.f,1.f,1.f,1.f};
    std::array<float,3> emissive{};
    float metallic=1.f,roughness=1.f,normal_scale=1.f,occlusion_strength=1.f,cutoff=.5f;
    unsigned alpha=0U; // glTF OPAQUE=0, MASK=1, BLEND=2
    bool double_sided=false,unlit=false;
    bool operator==(const Material&) const=default;
};
struct Override {
    Material value{};
    std::uint32_t fields=0U,image_mask=0U,uv_mask=0U;
};
using Materials=std::map<std::string,Material>;
using Overrides=std::map<std::uint32_t,std::map<unsigned,Override>>;
inline constexpr unsigned kMaxMaterials=256U,kMaxGPURecords=1024U,kRows=15U;
// SL PARAMS_RENDER_MATERIAL (0x80), count + face/U8 + material/UUID.
bool decode_face_ids(std::span<const std::uint8_t>,std::array<std::string,32>&) noexcept;
// Native SL material assets are binary LLSD {version,type,data: glTF JSON}.
bool decode_asset(std::span<const std::uint8_t>,Material&) noexcept;
bool decode_json(std::string_view,Material&) noexcept;
// GenericStreamingMessage High31, Method 0x4175, LLSD notation payload.
bool decode_override(std::span<const std::uint8_t>,std::uint32_t&,
                     std::map<unsigned,Override>&) noexcept;
bool effective(std::string_view id,std::uint32_t object,unsigned face,
               const Materials&,const Overrides&,Material& out);
}
