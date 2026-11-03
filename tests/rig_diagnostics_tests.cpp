#include "kami/features/rig_diagnostics.hpp"
#include "kami/world/scene.hpp"
#include <array>
using namespace kami;
int main() {
    const auto scene=world::build_test_scene(8);
    auto audit=rig::inspect(scene.vertices);
    if(audit.status!=rig::Compatibility::PhysicsReady||
       !audit.left_pec||!audit.right_pec||!audit.belly||!audit.butt||
       !audit.butt_open_close_ready||!audit.left_butt||!audit.right_butt||
       audit.weighted_vertices==0)return 1;
    std::array<world::Vertex,3> bare{};
    for(auto& v:bare) {v.ny=1.f;}
    if(rig::inspect(bare).status!=rig::Compatibility::Unrigged)return 2;
    auto partial=bare;
    rig::Packed packed{};
    if(!rig::pack_influences({{static_cast<std::uint8_t>(rig::Joint::Belly),0,0,0},
                             {1.f,0.f,0.f,0.f}},packed))return 3;
    partial[0].bone_indices=packed.joints;
    partial[0].bone_weights=packed.weights;
    if(rig::inspect(partial).status!=rig::Compatibility::Partial)return 4;
    partial[0].bone_indices=200;
    if(rig::inspect(partial).status!=rig::Compatibility::Invalid)return 5;
    partial[0].bone_weights=0;
    partial[0].body_region=1.f;
    if(rig::inspect(partial).status!=rig::Compatibility::Invalid)return 6;
    return 0;
}
