#include "kami/world/live_primitive_geometry.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <limits>
using namespace kami;
namespace {
using Transform=protocol::objectspatial::Transform;
using Kind=protocol::objectspatial::PrimitiveKind;
bool valid(const world::PrimitiveBatch& b) {
    if(b.scene.avatar_count!=0U||
       b.scene.terrain_count!=b.scene.vertices.size())return false;
    for(const auto& p:b.scene.vertices){
        if(!std::isfinite(p.px)||!std::isfinite(p.py)||
           !std::isfinite(p.pz)||!std::isfinite(p.nx)||
           !std::isfinite(p.ny)||!std::isfinite(p.nz)||
           p.bone_weights!=0U)return false;
    }
    return true;
}
}
int main(){
    std::map<std::uint32_t,Transform> roots,children;
    Transform root{77U,128.f,140.f,35.f,2.f,3.f,4.f};
    root.primitive=Kind::Box;
    roots.emplace(root.local_id,root);
    auto solids=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(solids)||solids.root_shapes!=1U||
       solids.linked_shapes!=0U||solids.scene.vertices.size()!=36U)
        return 1;
    roots[77U].primitive=Kind::Cylinder;
    auto cylinder=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(cylinder)||cylinder.root_shapes!=1U||
       cylinder.scene.vertices.size()!=240U)return 2;
    roots[77U].primitive=Kind::Sphere;
    auto sphere=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(sphere)||sphere.root_shapes!=1U||
       sphere.scene.vertices.size()<1000U)return 3;
    // An unknown or mesh root has NO fabricated solid object geometry,
    // but its verified region-space pose can anchor linked children.
    roots[77U].primitive=Kind::Prism;
    auto prism=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(prism)||prism.root_shapes!=1U||
       prism.scene.vertices.size()!=24U)return 12;
    roots[77U].primitive=Kind::Unknown;
    Transform child{88U,1.f,0.f,0.f,1.f,1.f,1.f};
    child.parent_id=77U;child.primitive=Kind::Box;
    children.emplace(child.local_id,child);
    auto linked=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(linked)||linked.root_shapes!=0U||
       linked.linked_shapes!=1U||
       linked.scene.vertices.size()!=36U)return 4;
    // Root rotation +90 deg around SL Z. Child local +X must resolve to
    // world +Y (KAMI forward Z), preserving world-scale and orientation.
    constexpr float half=0.70710678118f;
    roots[77U].rz=half;roots[77U].rw=half;
    linked=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    float minz=100.f,maxz=-100.f;
    for(const auto& p:linked.scene.vertices){
        minz=std::min(minz,p.pz);maxz=std::max(maxz,p.pz);
    }
    if(!valid(linked)||linked.linked_shapes!=1U||
       std::abs(minz-.5f)>.02f||std::abs(maxz-1.5f)>.02f)
        return 5;
    // Non-root intermediate parent: compose parent local transforms,
    // while refusing an unresolved parent or hierarchy cycle.
    Transform grandchild{90U,0.f,0.f,1.f,.7f,.7f,.7f};
    grandchild.parent_id=88U;
    grandchild.primitive=Kind::Box;
    children.emplace(grandchild.local_id,grandchild);
    auto nested=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(nested)||nested.linked_shapes!=2U||
       nested.scene.vertices.size()!=72U)return 6;
    children[88U].parent_id=90U;
    auto cyclic=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(cyclic)||cyclic.linked_shapes!=0U||
       !cyclic.scene.vertices.empty())return 7;
    children[88U].parent_id=999U;
    auto missing=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(missing)||missing.linked_shapes!=0U||
       missing.unresolved_parent_shapes!=2U)return 8;
    children[88U].parent_id=77U;
    auto limited=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f,1U);
    if(!valid(limited)||limited.linked_shapes!=1U||
       limited.eligible_shapes!=2U||
       limited.budget_deferred_shapes!=1U)return 9;
    // A supported root outside the view gate is classified, not drawn.
    children.clear();
    roots[77U].primitive=Kind::Box;
    roots[77U].x=240.f;
    const auto far=world::build_live_primitive_geometry(
        roots,children,0.f,140.f,35.f);
    if(far.root_shapes!=0U||far.out_of_range_shapes!=1U||
       far.eligible_shapes!=0U)return 11;
    if(world::build_live_primitive_geometry(roots,children,
           std::numeric_limits<float>::infinity(),0.f,0.f)
           .scene.terrain_count!=0U)return 10;
    return 0;
}
