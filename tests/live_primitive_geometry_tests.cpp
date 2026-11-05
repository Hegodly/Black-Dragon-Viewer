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
       attachment.emitted_triangles||attachment.waiting_for_parent==0U)
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
    return 0;
}

int main(){
    const int code=run_primitive_tests();
    if(code!=0)std::fprintf(stderr,
        "KAMI prim/material regression failed: check %d\n",code);
    return code;
}
