#include "kami/world/region_geometry.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <cmath>

namespace kami::world {
namespace {
bool finite(float v) noexcept {return std::isfinite(v);}
bool between(float value,float lo,float hi) noexcept {
    return finite(value)&&value>=lo&&value<=hi;
}
}
RegionGeometryError validate_decoded_region(
    const protocol::Bootstrap& session,
    const SceneMesh& mesh,bool verified) noexcept {
    if(!verified||!session.online())return RegionGeometryError::OfflineOrUnverified;
    // Static world geometry only. Skinned SL avatars require a separate
    // validated rig/bone/animation asset path; never fake a local rig.
    if(mesh.terrain_count!=mesh.vertices.size()||mesh.avatar_count!=0 ||
       mesh.vertices.size()%3U!=0U)
       return RegionGeometryError::InvalidSections;
    if(mesh.vertices.empty()||mesh.vertices.size()>750000U)
       return RegionGeometryError::TooLarge;
    for(const auto& v:mesh.vertices) {
        // Positions must be region-relative to avoid float precision loss.
        if(!between(v.px,-8192.f,8192.f)||
           !between(v.py,-8192.f,8192.f)||
           !between(v.pz,-8192.f,8192.f))
           return RegionGeometryError::InvalidPosition;
        if(!between(v.nx,-1.1f,1.1f)||
           !between(v.ny,-1.1f,1.1f)||
           !between(v.nz,-1.1f,1.1f))
           return RegionGeometryError::InvalidNormal;
        const float normal_len=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;
        if(!between(normal_len,.70f,1.30f))
           return RegionGeometryError::InvalidNormal;
        if(!between(v.r,0.f,1.f)||!between(v.g,0.f,1.f)||
           !between(v.b,0.f,1.f)||!between(v.metallic,0.f,1.f)||
           !between(v.roughness,.06f,1.f)||
           !between(v.u,-100000.f,100000.f)||
           !between(v.v,-100000.f,100000.f))
           return RegionGeometryError::InvalidMaterial;
        if(v.bone_indices!=0U||v.bone_weights!=0U||
           v.body_region!=0.f)
           return RegionGeometryError::UnsupportedSkinning;
    }
    return RegionGeometryError::None;
}
const wchar_t* region_geometry_error(RegionGeometryError error) noexcept {
    switch(error) {
    case RegionGeometryError::None:return L"REGION DX12 GEOMETRY READY";
    case RegionGeometryError::OfflineOrUnverified:
        return L"No verified in-world SL session / region update";
    case RegionGeometryError::InvalidSections:
        return L"Decoded region triangles have invalid draw sections";
    case RegionGeometryError::TooLarge:
        return L"Region batch empty or exceeds DX12 vertex budget";
    case RegionGeometryError::InvalidPosition:
        return L"Region mesh contains invalid or out-of-range positions";
    case RegionGeometryError::InvalidNormal:
        return L"Region mesh has invalid surface normals";
    case RegionGeometryError::InvalidMaterial:
        return L"Region mesh has invalid material data";
    case RegionGeometryError::UnsupportedSkinning:
        return L"Rigged avatar mesh requires the SL skeleton decoder";
    }
    return L"Invalid SL region geometry";
}
} // namespace kami::world
