#include "kami/world/region_geometry.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <cmath>
#include <limits>

using namespace kami;
namespace {
constexpr auto agent="12345678-90ab-4cde-8fab-000000000001";
constexpr auto sid="12345678-90ab-4cde-8fab-000000000002";
constexpr auto region="12345678-90ab-4cde-8fab-000000000003";
world::SceneMesh triangle(){
    world::SceneMesh s{};
    s.vertices={
        {0.f,0.f,0.f, 0.f,1.f,0.f, .4f,.5f,.6f, 0.f,.8f, 0.f,0.f},
        {1.f,0.f,0.f, 0.f,1.f,0.f, .4f,.5f,.6f, 0.f,.8f, 1.f,0.f},
        {0.f,0.f,1.f, 0.f,1.f,0.f, .4f,.5f,.6f, 0.f,.8f, 0.f,1.f}
    };
    s.terrain_count=s.vertices.size();
    return s;
}
}
int main(){
    protocol::Bootstrap online{};
    const auto mesh=triangle();
    using E=world::RegionGeometryError;
    if(world::validate_decoded_region(online,mesh,true)!=E::OfflineOrUnverified)
        return 1;
    if(!online.endpoint_checked({protocol::ProbeStatus::TlsEndpointReached,405})||
       !online.login_reply({agent,sid,"sim1234.agni.lindenlab.com",13000,
           L"https://sim1234.agni.lindenlab.com:12345/cap/abc"},true)||
       !online.circuit_ack(agent,true)||
       !online.capability_reply(
           L"https://sim1234.agni.lindenlab.com:12345/cap/other",true)||
       !online.region_snapshot({region,agent,128.f,128.f,40.f},true))
        return 2; // Only synthetic protocol state in this offline unit test.
    if(world::validate_decoded_region(online,mesh,false)!=E::OfflineOrUnverified||
       world::validate_decoded_region(online,mesh,true)!=E::None)return 3;
    auto bad=mesh;
    bad.terrain_count=2;
    if(world::validate_decoded_region(online,bad,true)!=E::InvalidSections)
        return 4;
    bad=mesh;
    bad.vertices[0].px=std::numeric_limits<float>::quiet_NaN();
    if(world::validate_decoded_region(online,bad,true)!=E::InvalidPosition)
        return 5;
    bad=mesh;
    bad.vertices[0].nx=std::numeric_limits<float>::infinity();
    if(world::validate_decoded_region(online,bad,true)!=E::InvalidNormal)
        return 6;
    bad=mesh;bad.vertices[0].roughness=-.5f;
    if(world::validate_decoded_region(online,bad,true)!=E::InvalidMaterial)
        return 7;
    bad=mesh;bad.vertices[0].bone_indices=1;
    if(world::validate_decoded_region(online,bad,true)!=E::UnsupportedSkinning)
        return 8;
    bad=mesh;bad.vertices.clear();bad.terrain_count=0;
    if(world::validate_decoded_region(online,bad,true)!=E::TooLarge)
        return 9;
    if(!world::region_geometry_error(E::UnsupportedSkinning) ||
       !world::region_geometry_error(E::None))
        return 10;
    online.reset();
    if(world::validate_decoded_region(online,mesh,true)!=E::OfflineOrUnverified)
        return 11;
    return 0;
}
