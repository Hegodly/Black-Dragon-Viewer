// Deterministic hostile-input regression corpus. No network credentials
// or live simulator are needed. This is intentionally bounded, unlike
// coverage-guided fuzzing; it catches malformed framing/bounds crashes
// in the exact parsers used for live SL UDP, mesh and TextureEntry assets.
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/terrain.hpp"
#include "kami/protocol/texture_entry.hpp"
#include "kami/protocol/mesh_lod.hpp"
#include "kami/protocol/world_wire.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

int main(){
    using namespace kami::protocol;
    std::uint32_t seed=0xc0ffee25U;
    const auto next=[&]() mutable -> std::uint32_t {
        seed=1664525U*seed+1013904223U;
        return seed;
    };
    const std::array<std::uint8_t,5> types{12U,13U,15U,17U,18U};
    for(unsigned case_no=0U;case_no<1536U;++case_no){
        const auto length=static_cast<std::size_t>(next()%400U);
        std::vector<std::uint8_t> bytes(length);
        for(auto& b:bytes)b=static_cast<std::uint8_t>(next()>>24U);
        lludp::Packet packet{};
        packet.body=bytes;
        if(packet.body.size()>11U && case_no%3U==0U){
            packet.body[0]=types[case_no%types.size()];
            packet.body[11]=static_cast<std::uint8_t>((case_no%10U)+1U);
        }
        std::vector<objectspatial::Transform> roots{},children{};
        objectspatial::FullDecodeAudit full{};
        objectspatial::CompressedDecodeAudit compressed{};
        (void)objectspatial::decode_full_transforms(
            packet,roots,&full,nullptr,&children);
        (void)objectspatial::decode_compressed_transforms(
            packet,roots,&children,&compressed);
        std::vector<objectspatial::TersePose> terse{};
        (void)objectspatial::decode_terse_poses(packet,terse);
        std::vector<std::uint32_t> ids{};
        (void)objectspatial::decode_cached_ids(packet,ids);
        (void)objectspatial::decode_killed_ids(packet,ids);
        std::vector<terrain::Patch> land{};
        (void)terrain::decode_land_layer(packet,land);
        texture::Base entry{};
        (void)texture::decode_base(bytes,entry);
        lludp::Packet decoded{};
        (void)lludp::decode(bytes,decoded);
        if(case_no%7U==0U){
            mesh::LOD lod{};
            (void)mesh::decode_mesh_lod(bytes,lod);
        }
    }
    // A truncated packet must be rejected without writing fake geometry.
    lludp::Packet empty{};
    std::vector<objectspatial::Transform> roots{};
    if(objectspatial::decode_full_transforms(empty,roots)||
       objectspatial::decode_compressed_transforms(empty,roots))
        return 1;
    std::vector<std::uint32_t> ids{};
    if(objectspatial::decode_cached_ids(empty,ids)||
       objectspatial::decode_killed_ids(empty,ids))return 2;
    return 0;
}
