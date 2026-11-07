#include "kami/world/live_primitive_geometry.hpp"
#include "kami/world/texture_uv.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <limits>
#include <cstdio>
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
int run_primitive_tests(){
    std::map<std::uint32_t,Transform> roots,children;
    Transform root{77U,128.f,140.f,35.f,2.f,3.f,4.f};
    root.primitive=Kind::Box;
    roots.emplace(root.local_id,root);
    roots[77U].base_tint_present=true;
    roots[77U].base_r=.25f;roots[77U].base_g=.5f;roots[77U].base_b=.75f;
    auto solids=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f);
    if(!valid(solids)||solids.root_shapes!=1U||
       solids.linked_shapes!=0U||solids.scene.vertices.size()!=36U)
        return 1;
    if(std::abs(solids.scene.vertices[0].r-.25f)>.001f||
       std::abs(solids.scene.vertices[0].g-.5f)>.001f||
       std::abs(solids.scene.vertices[0].b-.75f)>.001f)return 18;
    // SL path profile scale is part of the shape, not object dimensions.
    // High 12 and High 13 encode the same scale at different byte offsets.
    {
        Transform scaled{78U,128.f,140.f,35.f,2.f,3.f,4.f};
        scaled.primitive=Kind::Box;scaled.volume_params_seen=true;
        scaled.volume_params_full_layout=true;
        scaled.volume_params[4U]=0x50U;scaled.volume_params[5U]=0xc3U;
        scaled.volume_params[19U]=0x50U;scaled.volume_params[20U]=0xc3U;
        scaled.volume_params[6U]=50U;  // 1.5x profile X
        scaled.volume_params[7U]=150U; // 0.5x profile Y
        const auto high12=world::build_live_primitive_geometry(
            {{78U,scaled}},{},128.f,140.f,35.f);
        float max_x=0.f,max_z=0.f;
        for(const auto& v:high12.scene.vertices){
            max_x=std::max(max_x,std::abs(v.px));
            max_z=std::max(max_z,std::abs(v.pz));
        }
        if(high12.root_shapes!=1U||std::abs(max_x-1.5f)>.001f||
           std::abs(max_z-.75f)>.001f)return 73;
        // Anisotropic profile scale must not flip side/top normals inward.
        for(const auto& v:high12.scene.vertices){
            const float outward=v.px*v.nx+v.py*v.ny+v.pz*v.nz;
            if(!(outward>0.f))return 83;
        }
        scaled.volume_params_full_layout=false;
        scaled.volume_params[3U]=0x50U;scaled.volume_params[4U]=0xc3U;
        scaled.volume_params[19U]=0x50U;scaled.volume_params[20U]=0xc3U;
        scaled.volume_params[5U]=50U;scaled.volume_params[6U]=150U;
        const auto high13=world::build_live_primitive_geometry(
            {{78U,scaled}},{},128.f,140.f,35.f);
        max_x=0.f;max_z=0.f;
        for(const auto& v:high13.scene.vertices){
            max_x=std::max(max_x,std::abs(v.px));
            max_z=std::max(max_z,std::abs(v.pz));
        }
        if(high13.root_shapes!=1U||std::abs(max_x-1.5f)>.001f||
           std::abs(max_z-.75f)>.001f)return 74;
    }
    // Linear square and triangular profiles must produce their own
    // polygonal silhouettes, not the legacy cylinder fallback.
    {
        Transform square{86U,128.f,140.f,35.f,2.f,2.f,2.f};
        square.primitive=Kind::Box;square.volume_params_seen=true;
        square.volume_params[0U]=0x10U;square.volume_params[16U]=0x01U;
        square.volume_params[3U]=0x50U;square.volume_params[4U]=0xc3U;
        square.volume_params[19U]=0x50U;square.volume_params[20U]=0xc3U;
        const auto box_sweep=world::build_live_primitive_geometry(
            {{86U,square}},{},128.f,140.f,35.f);
        if(box_sweep.root_shapes!=1U||box_sweep.scene.vertices.size()<=240U)
            return 86;
        // Distinct TextureEntry images on each LLVolume face must map
        // to the official path-cap and outer-side face IDs.
        auto textured_square=square;
        const std::array<std::string,6U> face_names{{
            "cap-begin","cap-end","side-0","side-1","side-2","side-3"}};
        const std::array<unsigned,6U> face_ids{{0U,1U,5U,6U,7U,8U}};
        std::map<std::string,std::uint16_t> face_layers;
        for(unsigned i=0U;i<face_ids.size();++i){
            textured_square.face_texture_override[face_ids[i]]=true;
            textured_square.face_texture_asset_ids[face_ids[i]]=face_names[i];
            face_layers.emplace(face_names[i],static_cast<std::uint16_t>(i+1U));
        }
        const auto mapped=world::build_live_primitive_geometry(
            {{86U,textured_square}},{},128.f,140.f,35.f,128U,{},
            face_layers);
        std::array<std::size_t,7U> face_vertex_counts{};
        for(const auto& vertex:mapped.scene.vertices){
            const auto layer=static_cast<unsigned>(vertex.texture_layer);
            if(layer<1U||layer>6U)return 88;
            face_vertex_counts[layer]++;
        }
        for(unsigned layer=1U;layer<=6U;++layer)
            if(face_vertex_counts[layer]==0U)return 89;
        // Every emitted vertex must retain a finite outward-facing normal
        // on the swept square, including its two path caps.
        for(const auto& vertex:mapped.scene.vertices){
            const float facing=vertex.px*vertex.nx+
                vertex.py*vertex.ny+vertex.pz*vertex.nz;
            if(!(facing>0.f))return 90;
        }
        auto tapered=square;
        tapered.volume_params[13U]=0x50U; // +0.50 X taper
        const auto tapered_mesh=world::build_live_primitive_geometry(
            {{86U,tapered}},{},128.f,140.f,35.f);
        float plain_width=0.f,tapered_width=0.f;
        for(const auto& v:box_sweep.scene.vertices)
            plain_width=std::max(plain_width,std::abs(v.px));
        for(const auto& v:tapered_mesh.scene.vertices)
            tapered_width=std::max(tapered_width,std::abs(v.px));
        if(!(tapered_width<plain_width))return 91;
        auto sheared=square;
        sheared.volume_params[11U]=0x20U;
        const auto sheared_mesh=world::build_live_primitive_geometry(
            {{86U,sheared}},{},128.f,140.f,35.f);
        float plain_center=0.f,sheared_center=0.f;
        for(const auto& v:box_sweep.scene.vertices)plain_center+=v.px;
        for(const auto& v:sheared_mesh.scene.vertices)sheared_center+=v.px;
        if(std::abs(sheared_center-plain_center)<.01f)return 92;
        auto prism=square;prism.local_id=87U;prism.primitive=Kind::Prism;
        prism.volume_params[16U]=0x03U;
        const auto prism_sweep=world::build_live_primitive_geometry(
            {{87U,prism}},{},128.f,140.f,35.f);
        if(prism_sweep.root_shapes!=1U||prism_sweep.scene.vertices.size()<=180U||
           prism_sweep.scene.vertices.size()>=box_sweep.scene.vertices.size())
            return 87;
    }
    // A real High 13 linear/circular LLVolume now follows profile and
    // path cuts, hollow radius, taper, twist and shear through tessellation.
    {
        Transform volume{79U,128.f,140.f,35.f,2.f,2.f,2.f};
        volume.primitive=Kind::Cylinder;
        volume.volume_params_seen=true;
        volume.volume_params[0U]=0x10U; // LL_PCODE_PATH_LINE
        volume.volume_params[16U]=0x00U; // circular profile, no hole
        volume.volume_params[3U]=0x50U;volume.volume_params[4U]=0xc3U;
        volume.volume_params[19U]=0x50U;volume.volume_params[20U]=0xc3U;
        for(unsigned i=7U;i<=15U;++i)volume.volume_params[i]=0U;
        const auto plain=world::build_live_primitive_geometry(
            {{79U,volume}},{},128.f,140.f,35.f);
        if(plain.root_shapes!=1U||plain.scene.vertices.size()<=240U)
            return 76;
        // A large twist delta must receive denser path sampling than the
        // untwisted baseline, rather than producing visibly faceted sides.
        auto twisted=volume;
        twisted.volume_params[9U]=0x80U;
        twisted.volume_params[10U]=0x7fU;
        const auto twist_detail=world::build_live_primitive_geometry(
            {{79U,twisted}},{},128.f,140.f,35.f);
        if(twist_detail.root_shapes!=1U||
           twist_detail.scene.vertices.size()<=plain.scene.vertices.size())
            return 85;
        // Hollow circle: profile hole code and 25% hollow opening.
        volume.volume_params[16U]=0x10U;
        volume.volume_params[21U]=0x20U;
        volume.volume_params[22U]=0x4eU; // 0.4 hollow fraction
        const auto hollow=world::build_live_primitive_geometry(
            {{79U,volume}},{},128.f,140.f,35.f);
        if(hollow.root_shapes!=1U||
           hollow.scene.vertices.size()<=plain.scene.vertices.size())
            return 77;
        // Profile cut must reduce emitted side surface, not silently fall
        // back to the old uncut 20-sided cylinder.
        volume.volume_params[17U]=0x00U;volume.volume_params[18U]=0x00U;
        volume.volume_params[19U]=0x50U;volume.volume_params[20U]=0x61U;
        const auto cut=world::build_live_primitive_geometry(
            {{79U,volume}},{},128.f,140.f,35.f);
        if(cut.root_shapes!=1U||cut.scene.vertices.size()>=hollow.scene.vertices.size())
            return 78;
        // A partial hollow profile needs two longitudinal radial walls.
        // Twelve path segments × two cut planes × two quads × six vertices.
        if(cut.scene.vertices.size()<144U)return 81;
        // The same partial profile without a hole closes to the centerline.
        volume.volume_params[16U]=0x00U;
        volume.volume_params[21U]=0U;volume.volume_params[22U]=0U;
        const auto solid_cut=world::build_live_primitive_geometry(
            {{79U,volume}},{},128.f,140.f,35.f);
        if(solid_cut.root_shapes!=1U||solid_cut.scene.vertices.size()<144U||
           solid_cut.scene.vertices.size()>=cut.scene.vertices.size())return 82;
        // A path cut trims the sweep along its length while retaining both
        // generated end caps; it must not silently revert to a full cylinder.
        volume.volume_params[1U]=0x10U;volume.volume_params[2U]=0x27U;
        volume.volume_params[3U]=0x40U;volume.volume_params[4U]=0x9cU;
        const auto path_cut=world::build_live_primitive_geometry(
            {{79U,volume}},{},128.f,140.f,35.f);
        float full_min=std::numeric_limits<float>::max();
        float full_max=std::numeric_limits<float>::lowest();
        float path_min=std::numeric_limits<float>::max();
        float path_max=std::numeric_limits<float>::lowest();
        for(const auto& v:solid_cut.scene.vertices){
            if(v.py<full_min)full_min=v.py;
            if(v.py>full_max)full_max=v.py;
        }
        bool low_cap=false,high_cap=false;
        for(const auto& v:path_cut.scene.vertices){
            if(v.py<path_min)path_min=v.py;
            if(v.py>path_max)path_max=v.py;
            if(std::abs(v.ny)>.99f){
                if(std::abs(v.py-path_min)<.0001f)low_cap=true;
                if(std::abs(v.py-path_max)<.0001f)high_cap=true;
            }
        }
        const float full_extent=full_max-full_min;
        const float path_extent=path_max-path_min;
        if(path_cut.root_shapes!=1U||path_cut.scene.vertices.empty()||
           full_extent<=0.f||path_min<=full_min+.1f||
           path_max>=full_max-.1f||path_extent>=full_extent*.75f||
           !low_cap||!high_cap)return 84;
        // High 12 full ObjectUpdate uses a distinct 23-byte field layout.
        Transform full_volume{80U,128.f,140.f,35.f,2.f,2.f,2.f};
        full_volume.primitive=Kind::Cylinder;
        full_volume.volume_params_seen=true;
        full_volume.volume_params_full_layout=true;
        full_volume.volume_params[0U]=0x10U;
        full_volume.volume_params[1U]=0x10U;
        full_volume.volume_params[4U]=0x50U;
        full_volume.volume_params[5U]=0xc3U;
        full_volume.volume_params[6U]=100U;
        full_volume.volume_params[7U]=100U;
        for(unsigned i=8U;i<=16U;++i)full_volume.volume_params[i]=0U;
        full_volume.volume_params[19U]=0x50U;
        full_volume.volume_params[20U]=0xc3U;
        full_volume.volume_params[21U]=0x20U;
        full_volume.volume_params[22U]=0x4eU;
        const auto full_sweep=world::build_live_primitive_geometry(
            {{80U,full_volume}},{},128.f,140.f,35.f);
        if(full_sweep.root_shapes!=1U||
           full_sweep.scene.vertices.size()<=240U)return 79;
        // Complex swept cylinders must be admitted atomically: their
        // tessellation is larger than the legacy 240-vertex cylinder.
        const auto too_small=world::build_live_primitive_geometry(
            {{80U,full_volume}},{},128.f,140.f,35.f,8U,{},{},{},1000U);
        if(!valid(too_small)||too_small.root_shapes!=0U||
           too_small.budget_deferred_shapes!=1U||
           !too_small.scene.vertices.empty())return 80;
    }
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
    // A known avatar parent represents an attachment, not a missing root.
    std::set<std::uint32_t> avatars={999U};
    auto avatar_bound=world::build_live_primitive_geometry(
        roots,children,128.f,140.f,35.f,128U,avatars);
    if(avatar_bound.avatar_attachment_shapes!=2U||
       avatar_bound.missing_anchor_shapes!=0U)return 13;
    if(missing.unique_missing_ids!=1U||
       missing.missing_anchor_shapes!=2U)return 14;
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
    // Authenticated static mesh LODs are rendered only with valid root
    // transforms. Avatar-parented or unknown assets never fake a world pose.
    protocol::mesh::LOD lod{};
    protocol::mesh::Triangle triangle{};
    triangle.pos={-.5f,-.5f,0.f,.5f,-.5f,0.f,0.f,.5f,0.f};
    lod.triangles.push_back(triangle);
    const std::string asset_id="12345678-90ab-4cde-8fab-000000000077";
    std::map<std::string,protocol::mesh::LOD> assets={{asset_id,lod}};
    roots[77U].x=128.f;roots[77U].primitive=Kind::Unknown;
    roots[77U].mesh_asset_id=asset_id;
    const auto mesh_root=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U);
    if(mesh_root.root_meshes!=1U||mesh_root.linked_meshes!=0U||
       mesh_root.emitted_triangles!=1U||
       mesh_root.scene.vertices.size()!=3U)return 15;
    if(std::abs(mesh_root.scene.vertices[0].r-.25f)>.001f||
       std::abs(mesh_root.scene.vertices[0].g-.5f)>.001f||
       std::abs(mesh_root.scene.vertices[0].b-.75f)>.001f)return 19;
    // Only genuine SL mesh UVs and a resident default TE texture UUID
    // may select a non-procedural DX12 texture array layer.
    assets[asset_id].triangles[0].has_uv=true;
    assets[asset_id].triangles[0].uv={0.f,1.f,1.f,1.f,0.f,0.f};
    const std::string texture_id="12345678-90ab-4cde-8fab-000000000099";
    roots[77U].default_texture_asset_id=texture_id;
    const auto textured=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U,
        {{texture_id,7U}});
    if(textured.textured_triangles!=1U||
       textured.scene.vertices.size()!=3U||
       textured.scene.vertices[0].texture_layer!=7.f||
       textured.scene.vertices[1].texture_layer!=7.f||
       textured.scene.vertices[2].texture_layer!=7.f||
       std::abs(textured.scene.vertices[1].v-0.f)>.001f||
       std::abs(textured.scene.vertices[2].u-1.f)>.001f)return 20;
    const auto fallback=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U);
    if(fallback.textured_triangles!=0U||
       fallback.scene.vertices[0].texture_layer!=0.f)return 21;
    assets[asset_id].triangles[0].has_uv=false;
    const auto no_uv=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U,
        {{texture_id,7U}});
    if(no_uv.textured_triangles!=0U||
       no_uv.scene.vertices[0].texture_layer!=0.f)return 22;
    // Restore the valid UV before testing the independent material IDs.
    // The preceding no-UV test intentionally disabled it.
    assets[asset_id].triangles[0].has_uv=true;
    // A face-specific override must take precedence over the default
    // image UUID. Unloaded overrides safely fall back to layer zero.
    roots[77U].face_texture_override[0]=true;
    roots[77U].face_texture_asset_ids[0]="12345678-90ab-4cde-8fab-000000000098";
    const auto per_face=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U,
        {{texture_id,7U},{roots[77U].face_texture_asset_ids[0],11U}});
    if(per_face.textured_triangles!=1U ||
       per_face.scene.vertices[0].texture_layer!=11.f)return 23;
    const auto missing_face=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U,
        {{texture_id,7U}});
    if(missing_face.textured_triangles!=0U||
       missing_face.scene.vertices[0].texture_layer!=0.f)return 24;
    roots[77U].face_texture_asset_ids[0].clear();
    const auto null_face=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U,
        {{texture_id,7U}});
    if(null_face.textured_triangles!=0U||
       null_face.scene.vertices[0].texture_layer!=0.f)return 25;
    roots[77U].mesh_asset_id.clear();
    Transform mesh_child_t{88U,1.f,0.f,0.f,1.f,1.f,1.f};
    mesh_child_t.parent_id=77U;
    mesh_child_t.mesh_asset_id=asset_id;
    children.clear();
    children.emplace(mesh_child_t.local_id,mesh_child_t);
    const auto mesh_child=world::build_live_mesh_geometry(
        roots,children,{},assets,128.f,140.f,35.f,10U);
    if(mesh_child.root_meshes!=0U||mesh_child.linked_meshes!=1U||
       mesh_child.emitted_triangles!=1U)return 16;
    std::set<std::uint32_t> mesh_avatar_parents={77U};
    const auto attachment=world::build_live_mesh_geometry(
        {},children,mesh_avatar_parents,assets,128.f,140.f,35.f,10U);
    if(attachment.root_meshes||attachment.linked_meshes||
       attachment.emitted_triangles||attachment.waiting_for_parent==0U||
       attachment.parent_avatar_instances==0U)
       return 17;
    // R53: simple SL primitives must use the exact same decoded-J2C
    // GPU array as mesh LODs, with independently numbered material faces.
    Transform cuboid{201U,128.f,140.f,35.f,2.f,3.f,4.f};
    cuboid.primitive=Kind::Box;
    cuboid.default_texture_asset_id=texture_id;
    std::map<std::uint32_t,Transform> textured_prim{{201U,cuboid}};
    const auto textured_box=world::build_live_primitive_geometry(
        textured_prim,{},128.f,140.f,35.f,128U,{},{{texture_id,5U}});
    if(!valid(textured_box)||textured_box.scene.vertices.size()!=36U||
       textured_box.textured_triangles!=12U)return 31;
    for(const auto& v:textured_box.scene.vertices)
        if(v.texture_layer!=5.f||!std::isfinite(v.u)||
           !std::isfinite(v.v))return 32;
    // Independent published Second Life prim face ordering, not an
    // arbitrary face-number assignment based on the triangles array.
    // KAMI converts source +Z into DirectX Y-up at the scene boundary.
    const std::array<std::array<float,3U>,6U> box_face_normals{{
        {0.f,1.f,0.f}, {0.f,0.f,-1.f}, {1.f,0.f,0.f},
        {0.f,0.f,1.f}, {-1.f,0.f,0.f}, {0.f,-1.f,0.f}
    }};
    for(unsigned face=0U;face<6U;++face){
        const auto& v=textured_box.scene.vertices[face*6U];
        const auto& n=box_face_normals[face];
        if(std::abs(v.nx-n[0])>.01f||
           std::abs(v.ny-n[1])>.01f||
           std::abs(v.nz-n[2])>.01f)return 44;
    }
    // Distinct per-face overrides and explicit zero image must not
    // accidentally inherit the default image from an adjacent face.
    const std::string alternate="aaaa1111-bbbb-4ccc-8ddd-eeeeeeee0001";
    textured_prim[201U].face_texture_override[2U]=true;
    textured_prim[201U].face_texture_asset_ids[2U]=alternate;
    const auto box_with_override=world::build_live_primitive_geometry(
        textured_prim,{},128.f,140.f,35.f,128U,{},
        {{texture_id,5U},{alternate,7U}});
    if(box_with_override.scene.vertices.size()!=36U||
       box_with_override.textured_triangles!=12U||
       box_with_override.scene.vertices[12U].texture_layer!=7.f||
       box_with_override.scene.vertices[0U].texture_layer!=5.f||
       box_with_override.scene.vertices[35U].texture_layer!=5.f)return 33;
    textured_prim[201U].face_texture_asset_ids[2U].clear();
    const auto explicit_null=world::build_live_primitive_geometry(
        textured_prim,{},128.f,140.f,35.f,128U,{},
        {{texture_id,5U},{alternate,7U}});
    if(explicit_null.textured_triangles!=10U||
       explicit_null.scene.vertices[12U].texture_layer!=0.f||
       explicit_null.scene.vertices[0U].texture_layer!=5.f)return 34;
    textured_prim[201U].face_texture_override[2U]=false;
    // Real SL UV transform: repeat, offset, rotation from each TE face,
    // not screen-space or post-generated world-space guesses.
    auto& face_uv=textured_prim[201U].face_uv[0U];
    face_uv.repeat_u=2.f;
    face_uv.repeat_v=.5f;
    face_uv.offset_u=.25f;
    face_uv.offset_v=-.25f;
    face_uv.rotation=0.f;
    textured_prim[201U].face_rgba_present=true;
    for(auto& color:textured_prim[201U].face_rgba)color={1.f,1.f,1.f,1.f};
    textured_prim[201U].face_rgba[0U]={.15f,.35f,.55f,1.f};
    const auto uv_box=world::build_live_primitive_geometry(
        textured_prim,{},128.f,140.f,35.f,128U,{},{{texture_id,5U}});
    if(uv_box.scene.vertices.size()!=36U||
       std::abs(uv_box.scene.vertices[0].u+.25f)>1.e-4f||
       std::abs(uv_box.scene.vertices[0].v-1.0f)>1.e-4f||
       std::abs(uv_box.scene.vertices[0].r-.15f)>1.e-4f||
       std::abs(uv_box.scene.vertices[0].g-.35f)>1.e-4f||
       std::abs(uv_box.scene.vertices[0].b-.55f)>1.e-4f||
       std::abs(uv_box.scene.vertices[6].r-1.f)>1.e-4f)return 35;
    for(const auto kind:{Kind::Cylinder,Kind::Sphere,Kind::Prism}){
        textured_prim[201U].primitive=kind;
        const auto shape=world::build_live_primitive_geometry(
            textured_prim,{},128.f,140.f,35.f,128U,{},
            {{texture_id,5U}});
        if(!valid(shape)||shape.scene.vertices.empty()||
           shape.textured_triangles!=shape.scene.vertices.size()/3U)
            return 36;
        if(kind==Kind::Cylinder&&shape.scene.vertices.size()!=240U)
            return 37;
        if(kind==Kind::Prism&&shape.scene.vertices.size()!=24U)
            return 38;
    }
    // R49: selecting the alphabetically first decoded UUID can render
    // the wrong (distant) building and starve nearby real mesh faces.
    assets[asset_id].triangles[0].has_uv=true;
    Transform close_mesh{101U,128.f,140.f,35.f,1.f,1.f,1.f};
    close_mesh.mesh_asset_id=asset_id;
    const std::string close_id="ffffffff-ffff-4fff-8fff-ffffffffffff";
    close_mesh.default_texture_asset_id=close_id;
    Transform far_mesh=close_mesh;
    far_mesh.local_id=102U;
    far_mesh.x=200.f;
    const std::string far_id="11111111-1111-4111-8111-111111111111";
    far_mesh.default_texture_asset_id=far_id;
    std::map<std::uint32_t,Transform> visible{{101U,close_mesh},
                                               {102U,far_mesh}};
    protocol::texture::DecodedTexture one_pixel{};
    one_pixel.width=1U;one_pixel.height=1U;
    one_pixel.rgba={255U,128U,64U,255U};
    std::map<std::string,protocol::texture::DecodedTexture> bitmaps{
        {close_id,one_pixel},{far_id,one_pixel}};
    const auto nearest=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,128.f,140.f,35.f,1U);
    if(nearest.size()!=1U||nearest.at(close_id)!=1U)return 26;
    const auto moved=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,200.f,140.f,35.f,1U);
    if(moved.size()!=1U||moved.at(far_id)!=1U)return 27;
    const std::map<std::string,std::uint16_t> preferred{
        {far_id,1U},{close_id,2U}};
    const auto stable=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,128.f,140.f,35.f,2U,preferred);
    if(stable.size()!=2U||stable.at(far_id)!=1U||
       stable.at(close_id)!=2U)return 28;
    // Explicitly empty per-face image IDs must not inherit the
    // default, and malformed decoded images cannot become GPU layers.
    visible[101U].face_texture_override[0]=true;
    visible[101U].face_texture_asset_ids[0].clear();
    const auto nulled=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,128.f,140.f,35.f,2U);
    if(nulled.contains(close_id)||!nulled.contains(far_id))return 29;
    visible[101U].face_texture_override[0]=false;
    bitmaps[close_id].rgba.clear();
    const auto invalid_image=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,128.f,140.f,35.f,2U);
    if(invalid_image.contains(close_id)||!invalid_image.contains(far_id))
        return 30;
    // Official SL mesh faces extend through index 44. The residency scan
    // must not truncate its per-asset face mask at bit 31.
    assets[asset_id].triangles[0].face_index=44U;
    visible[101U].face_texture_override[44U]=true;
    visible[101U].face_texture_asset_ids[44U]=far_id;
    const auto high_face=world::select_visible_mesh_texture_layers(
        visible,{}, {},assets,bitmaps,128.f,140.f,35.f,2U);
    if(!high_face.contains(far_id))return 75;
    assets[asset_id].triangles[0].face_index=0U;
    visible[101U].face_texture_override[44U]=false;
    // LLFace::xform legacy rotation is not a glTF-style KHR texture
    // transform. Verify the exact signs in V-down GPU coordinates.
    protocol::texture::UVTransform rot{};
    rot.rotation=1.5707963267948966f;
    const auto rotated=world::transform_diffuse_uv(1.f,0.f,rot);
    if(std::abs(rotated[0]-1.f)>1.e-4f||
       std::abs(rotated[1]-1.f)>1.e-4f)return 39;
    rot.rotation=0.f;rot.repeat_u=2.f;rot.repeat_v=.5f;
    rot.offset_u=.25f;rot.offset_v=-.25f;
    const auto scaled_uv=world::transform_diffuse_uv(0.f,1.f,rot);
    if(std::abs(scaled_uv[0]+.25f)>1.e-4f||
       std::abs(scaled_uv[1]-1.f)>1.e-4f)return 40;
    // R53 atlas: supported primitive objects must compete fairly with
    // actual mesh for residency and keep previous array-layer slots.
    Transform close_prim{305U,128.f,140.f,35.f,2.f,2.f,2.f};
    close_prim.primitive=Kind::Box;
    close_prim.default_texture_asset_id=close_id;
    const std::map<std::uint32_t,Transform> just_prim{{305U,close_prim}};
    bitmaps[close_id]=one_pixel;
    const auto prim_resident=world::select_visible_mesh_texture_layers(
        just_prim,{}, {},assets,bitmaps,128.f,140.f,35.f,48U);
    if(!prim_resident.contains(close_id)||
       prim_resident.at(close_id)!=1U)return 41;
    close_prim.face_texture_override[3]=true;
    close_prim.face_texture_asset_ids[3]=far_id;
    const auto prim_face=world::select_visible_mesh_texture_layers(
        {{305U,close_prim}}, {},{},assets,bitmaps,
        128.f,140.f,35.f,48U,prim_resident);
    if(!prim_face.contains(far_id)||prim_face.at(close_id)!=1U)
        return 42;
    // Shipped 48-image GPU residency is still bounded.
    const auto bounded=world::select_visible_mesh_texture_layers(
        {{305U,close_prim}}, {},{},assets,bitmaps,
        128.f,140.f,35.f,48U);
    if(bounded.size()>48U)return 43;
    // Dense decorated regions exceed the old 48-image limit. All 192
    // unique visible assets must receive distinct stable GPU slots.
    std::map<std::uint32_t,Transform> dense;
    std::map<std::string,protocol::texture::DecodedTexture> dense_images;
    for(std::uint32_t i=0U;i<protocol::texture::kGPUImageLayers+8U;++i){
        Transform t=close_prim;t.local_id=1000U+i;
        t.face_texture_override.fill(false);
        t.default_texture_asset_id="dense-image-"+std::to_string(i);
        t.x=128.f+float(i%10U);t.y=140.f+float(i/10U);
        dense_images.emplace(t.default_texture_asset_id,one_pixel);
        dense.emplace(t.local_id,t);
    }
    const auto expanded=world::select_visible_mesh_texture_layers(
        dense,{}, {},{},dense_images,128.f,140.f,35.f,1000U);
    if(expanded.size()!=protocol::texture::kGPUImageLayers)return 45;
    std::set<std::uint16_t> slots;
    for(const auto& [id,slot]:expanded){
        if(slot==0U||slot>protocol::texture::kGPUImageLayers||
           !slots.insert(slot).second)return 46;
    }
    if(!slots.contains(protocol::texture::kGPUImageLayers))return 47;
    const auto repeated=world::select_visible_mesh_texture_layers(
        dense,{}, {},{},dense_images,128.f,140.f,35.f,
        protocol::texture::kGPUImageLayers,expanded);
    if(repeated!=expanded)return 48;
    // Fully invisible SL faces must not enter either color or shadow
    // geometry, while the neighboring opaque faces remain renderable.
    Transform alpha_box{777U,128.f,140.f,35.f,2.f,2.f,2.f};
    alpha_box.primitive=Kind::Box;alpha_box.face_rgba_present=true;
    for(auto& color:alpha_box.face_rgba)color={1.f,1.f,1.f,1.f};
    alpha_box.face_rgba[0][3]=0.f;
    const auto invisible_top=world::build_live_primitive_geometry(
        {{777U,alpha_box}},{},128.f,140.f,35.f,128U);
    if(invisible_top.scene.vertices.size()!=30U)return 49;
    for(auto& color:alpha_box.face_rgba)color[3]=0.f;
    const auto invisible_box=world::build_live_primitive_geometry(
        {{777U,alpha_box}},{},128.f,140.f,35.f,128U);
    if(!invisible_box.scene.vertices.empty())return 50;
    alpha_box.mesh_asset_id=asset_id;
    alpha_box.primitive=Kind::Unknown;
    const auto invisible_mesh=world::build_live_mesh_geometry(
        {{777U,alpha_box}}, {},{},assets,128.f,140.f,35.f,10U);
    if(invisible_mesh.emitted_triangles!=0U||
       invisible_mesh.undrawn_transparent_instances!=1U)return 51;
    for(auto& color:alpha_box.face_rgba)color[3]=1.f;
    const auto restored_mesh=world::build_live_mesh_geometry(
        {{777U,alpha_box}}, {},{},assets,128.f,140.f,35.f,10U);
    if(restored_mesh.emitted_triangles!=1U)return 52;
    // A dense live scene must render beyond the old 18k/24k preview cap,
    // while respecting the caller's explicit quality budget.
    auto dense_root=close_mesh;dense_root.x=128;dense_root.y=140;dense_root.z=35;
    dense_root.default_texture_asset_id.clear();
    protocol::mesh::LOD dense_lod;dense_lod.triangles.assign(30000U,triangle);
    const auto expanded_mesh=world::build_live_mesh_geometry({{101U,dense_root}}, {}, {},{{asset_id,dense_lod}},128,140,35,28000U);
    // Whole-object admission: an oversized decoded mesh must not be
    // truncated into partial walls, even when it is the only instance.
    if(expanded_mesh.emitted_triangles!=0U||
       !expanded_mesh.scene.vertices.empty()||
       expanded_mesh.undrawn_global_limit!=1U)return 53;
    {
        // Many nearby instances sharing a modest real LOD can exceed the
        // previous 500k cap. The explicit caller limit must still win.
        protocol::mesh::LOD shared;shared.triangles.assign(1000U,triangle);
        std::map<std::uint32_t,Transform> instances;
        for(std::uint32_t i=0;i<600U;++i){auto t=dense_root;t.local_id=1000U+i;instances.emplace(t.local_id,t);}
        const auto quality=world::build_live_mesh_geometry(instances, {}, {},{{asset_id,shared}},128,140,35,550000U);
        if(quality.emitted_triangles!=550000U||quality.scene.vertices.size()!=1650000U)return 55;
    }
    // R65: Quality falls back to 2,048 verified primitive boxes, but the
    // internal cap still defers additional eligible shapes.
    {
        std::map<std::uint32_t,Transform> crowded;
        for(std::uint32_t i=0U;i<4200U;++i){
            Transform shape{5000U+i,
                static_cast<float>(i%70U)*2.f-70.f,
                static_cast<float>(i/70U)*2.f-60.f,
                0.f,1.f,1.f,1.f};
            shape.primitive=Kind::Box;
            crowded.emplace(shape.local_id,shape);
        }
        const auto quality=world::build_live_primitive_geometry(
            crowded,{},0.f,0.f,0.f,9000U);
        if(!valid(quality)||quality.root_shapes!=world::kMaxQualityPrimitiveShapes||
           quality.eligible_shapes!=4200U||
           quality.budget_deferred_shapes!=4200U-world::kMaxQualityPrimitiveShapes||
           quality.scene.vertices.size()!=world::kMaxQualityPrimitiveShapes*36U)return 56;
        const auto balanced=world::build_live_primitive_geometry(
            crowded,{},0.f,0.f,0.f,512U);
        if(!valid(balanced)||balanced.root_shapes!=512U||
           balanced.budget_deferred_shapes!=3688U)return 57;
    }
    // Enforce the *remaining scene* vertex budget for worst-case sphere
    // geometry, without failing the entire terrain upload.
    {
        std::map<std::uint32_t,Transform> spheres;
        for(std::uint32_t i=0U;i<5U;++i){
            Transform shape{9000U+i,static_cast<float>(i),0.f,0.f,1.f,1.f,1.f};
            shape.primitive=Kind::Sphere;
            spheres.emplace(shape.local_id,shape);
        }
        const auto capped=world::build_live_primitive_geometry(
            spheres,{},0.f,0.f,0.f,5U,{},{},{},2410U);
        if(!valid(capped)||capped.root_shapes!=2U||
           capped.budget_deferred_shapes!=3U||
           capped.scene.vertices.size()>2410U)return 58;
    }
    // R64/R65 regression: render mesh assets first when dense primitives
    // compete for the same bounded DX12 world buffer. A prim-first
    // allocator could fill the buffer and leave NO visible mesh.
    {
        constexpr std::size_t existing_land_vertices=3970000U;
        protocol::mesh::LOD shared_lod;
        shared_lod.triangles.assign(1000U,triangle);
        const auto available=world::live_world_vertices_remaining(existing_land_vertices);
        if(available!=29000U)return 59;
        const auto mesh_first=world::build_live_mesh_geometry(
            {{101U,dense_root}}, {}, {}, {{asset_id,shared_lod}},
            128.f,140.f,35.f,available/3U);
        if(mesh_first.emitted_triangles!=1000U||
           mesh_first.scene.vertices.size()!=3000U)return 60;
        std::map<std::uint32_t,Transform> crowded;
        for(std::uint32_t i=0U;i<1000U;++i){
            Transform box{50000U+i,
                128.f+float(i%40U),140.f+float(i/40U),35.f,
                1.f,1.f,1.f};
            box.primitive=Kind::Box;
            crowded.emplace(box.local_id,box);
        }
        const auto remainder=world::live_world_vertices_remaining(
            existing_land_vertices+mesh_first.scene.vertices.size());
        const auto prim_second=world::build_live_primitive_geometry(
            crowded,{},128.f,140.f,35.f,world::kMaxQualityPrimitiveShapes,
            {},{},{},remainder);
        if(!valid(prim_second)||prim_second.root_shapes!=remainder/36U||
           prim_second.budget_deferred_shapes!=
               crowded.size()-prim_second.root_shapes)return 61;
        if(existing_land_vertices+mesh_first.scene.vertices.size()+
           prim_second.scene.vertices.size() >
           world::kLiveSceneVertexLimit-world::kLiveAvatarVertexReserve)
            return 62;
        if(world::live_world_vertices_remaining(
            world::kLiveSceneVertexLimit)!=0U)return 63;
    }
    // R66 classifications must explain why downloaded mesh LODs
    // don't necessarily turn into visible in-world mesh instances.
    {
        auto near=dense_root;near.local_id=70001U;
        auto absent=near;absent.local_id=70002U;
        absent.mesh_asset_id="not-loaded-in-cache";
        auto far=near;far.local_id=70003U;far.x=1000.f;
        auto invalid=near;invalid.local_id=70004U;
        invalid.rx=invalid.ry=invalid.rz=invalid.rw=0.f;
        auto orphan=near;orphan.local_id=70005U;orphan.parent_id=999999U;
        const auto sample=world::build_live_mesh_geometry(
            {{near.local_id,near},{absent.local_id,absent},
             {far.local_id,far},{invalid.local_id,invalid}},
            {{orphan.local_id,orphan}}, {},
            {{asset_id,lod}},128.f,140.f,35.f,0U);
        if(sample.eligible_instances!=1U||
           sample.missing_lod_instances!=1U||
           sample.out_of_range_instances!=1U||
           sample.invalid_pose_instances!=1U||
           sample.waiting_for_parent!=1U||
           sample.parent_missing_instances!=1U||
           sample.undrawn_eligible_instances!=1U||
           sample.undrawn_budget_instances!=1U||
           sample.emitted_triangles!=0U){
            std::fprintf(stderr,
                "R81 mesh diagnostics: eligible=%zu missing=%zu range=%zu pose=%zu waiting=%zu parent=%zu undrawn=%zu budget=%zu triangles=%zu\\n",
                sample.eligible_instances,sample.missing_lod_instances,
                sample.out_of_range_instances,sample.invalid_pose_instances,
                sample.waiting_for_parent,sample.parent_missing_instances,
                sample.undrawn_eligible_instances,
                sample.undrawn_budget_instances,sample.emitted_triangles);
            return 64;
        }
        const auto drawn=world::build_live_mesh_geometry(
            {{near.local_id,near}}, {}, {}, {{asset_id,lod}},
            128.f,140.f,35.f,1U);
        if(drawn.eligible_instances!=1U||
           drawn.undrawn_eligible_instances!=0U||
           drawn.root_meshes!=1U||
           drawn.emitted_triangles!=1U)return 65;
        // Degenerate but decoded triangle geometry must not count as a
        // budget failure or missing parent.
        auto degenerate=lod;
        if(degenerate.triangles.empty())return 66;
        degenerate.triangles[0].pos.fill(0.f);
        const auto bad_geometry=world::build_live_mesh_geometry(
            {{near.local_id,near}}, {}, {}, {{asset_id,degenerate}},
            128.f,140.f,35.f,10U);
        if(bad_geometry.emitted_triangles!=0U||
           bad_geometry.undrawn_bad_geometry_instances!=1U||
           bad_geometry.undrawn_budget_instances!=0U)return 67;
        // Every reported parent failure must be classified exactly
        // once, including cycles that a larger missing queue cannot fix.
        auto self_parent=orphan;
        self_parent.local_id=70006U;
        self_parent.parent_id=self_parent.local_id;
        const auto cycle=world::build_live_mesh_geometry(
            {},{{self_parent.local_id,self_parent}}, {},
            {{asset_id,lod}},128.f,140.f,35.f,10U);
        if(cycle.waiting_for_parent!=1U||
           cycle.parent_cycle_instances!=1U||
           cycle.parent_missing_instances!=0U)return 68;
    }
    // Dense-region regression A: object-ID ordering must never outrank
    // a closer building in the same 16m band when only one fits.
    {
        protocol::mesh::LOD building;
        building.triangles.assign(1000U,triangle);
        auto farther=dense_root;farther.local_id=100U;farther.x=139.f;
        auto nearer=dense_root;nearer.local_id=200U;nearer.x=129.f;
        const auto prioritized=world::build_live_mesh_geometry(
            {{farther.local_id,farther},{nearer.local_id,nearer}},
            {},{},{{asset_id,building}},128.f,140.f,35.f,1000U);
        if(prioritized.emitted_triangles!=1000U||
           prioritized.root_meshes!=1U||
           prioritized.undrawn_budget_instances!=1U||
           prioritized.scene.vertices.size()!=3000U)return 69;
        // Check actual emitted world-space X rather than just counters.
        float sum_x=0.f;
        for(const auto& v:prioritized.scene.vertices)sum_x+=v.px;
        if(std::abs(sum_x/3000.f)>5.f)return 70;
    }
    // Dense-region regression B: repeated instances of one asset must
    // remain distinct, but no instance may be partially emitted at a cap.
    {
        protocol::mesh::LOD shared;
        shared.triangles.assign(1000U,triangle);
        auto a=dense_root;a.local_id=300U;a.x=128.f;
        auto b=dense_root;b.local_id=301U;b.x=130.f;
        const auto both=world::build_live_mesh_geometry(
            {{a.local_id,a},{b.local_id,b}}, {},{},
            {{asset_id,shared}},128.f,140.f,35.f,2000U);
        if(both.root_meshes!=2U||both.emitted_triangles!=2000U||
           both.scene.vertices.size()!=6000U||
           both.undrawn_budget_instances!=0U)return 71;
        const auto capped=world::build_live_mesh_geometry(
            {{a.local_id,a},{b.local_id,b}}, {},{},
            {{asset_id,shared}},128.f,140.f,35.f,1500U);
        if(capped.root_meshes!=1U||capped.emitted_triangles!=1000U||
           capped.scene.vertices.size()!=3000U||
           capped.undrawn_budget_instances!=1U)return 72;
    }
    dense_root.reflection_probe=true;
    const auto invisible_probe=world::build_live_mesh_geometry({{101U,dense_root}}, {}, {},{{asset_id,lod}},128,140,35,28000U);
    if(invisible_probe.emitted_triangles!=0U)return 54;
    return 0;
}

int main(){
    const int code=run_primitive_tests();
    if(code!=0)std::fprintf(stderr,
        "KAMI prim/material regression failed: check %d\n",code);
    return code;
}


