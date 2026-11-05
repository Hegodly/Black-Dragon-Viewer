#include "kami/world/live_avatar.hpp"
#include "kami/world/scene.hpp"
#include <cmath>
#include <limits>
int main(){
    using namespace kami;
    world::LiveAvatarPreview preview{};
    if(preview.active())return 1;
    if(preview.reset({128.f,128.f,30.f,false}))return 2;
    if(preview.reset({std::numeric_limits<float>::infinity(),128.f,30.f,true}))
        return 3;
    if(!preview.reset({124.f,129.f,30.f,true})||!preview.active())
        return 4;
    if(std::fabs(preview.pose().x)>.001f||
       std::fabs(preview.pose().z)>.001f)return 5;
    avatar::Controls c{};c.forward=1.f;
    preview.step(.1f,c);
    const auto prior=preview.pose();
    if(prior.z<=0.f||!prior.moving)return 6;
    preview.step(.1f,{});
    if(std::fabs(preview.pose().z-prior.z)>.001f||
       preview.pose().moving)return 7;
    c.turn=1.f;
    preview.step(.1f,c);
    if(preview.pose().heading<=0.f||preview.pose().x<=0.f)return 8;
    for(int n=0;n<3000;++n)preview.step(.1f,c);
    if(!std::isfinite(preview.pose().x)||
       !std::isfinite(preview.pose().z)||
       !std::isfinite(preview.pose().heading)||
       preview.pose().x>131.99f||preview.pose().z>126.99f)return 9;
    // R52: a 12-meter DX12 world rebase must not make the avatar or
    // follow camera jump relative to the buildings.
    world::LiveAvatarPreview centered{};
    if(!centered.reset({100.f,100.f,30.f,true}))return 14;
    centered.step(.1f,avatar::Controls{1.f,0.f,0.f,false,false});
    const auto prior_local=centered.pose();
    if(!centered.rebase({109.f,108.f,32.f,true}))return 15;
    if(std::abs((100.f+prior_local.x)-(109.f+centered.pose().x))>.001f||
       std::abs((100.f+prior_local.z)-(108.f+centered.pose().z))>.001f||
       std::abs((30.f+prior_local.y)-(32.f+centered.pose().y))>.001f)
        return 16;
    const auto before_invalid=centered.pose();
    if(centered.rebase({400.f,108.f,32.f,true})||
       std::abs(centered.pose().x-before_invalid.x)>.001f)return 17;
    // A long-distance teleport invalidates local dead reckoning and
    // snaps the marker back to the new authenticated region position.
    if(!centered.rebase({210.f,210.f,55.f,true})||
       std::abs(centered.pose().x)>.001f||
       std::abs(centered.pose().z)>.001f||
       std::abs(centered.pose().y+1.f)>.001f)return 18;
    preview.clear();
    if(preview.active())return 10;
    const auto mesh=world::build_live_avatar_placeholder();
    if(mesh.terrain_count!=0U||mesh.avatar_count!=324U||
       mesh.vertices.size()!=324U)return 11;
    bool front=false,back=false,visor=false;
    for(const auto& v:mesh.vertices){
        if(v.pz>.300f&&v.r>.9f&&v.g>.6f)front=true;
        if(v.pz<-.340f&&v.b>.75f&&v.r>.4f)back=true;
        if(v.pz>.330f&&v.r>.9f&&v.g>.9f)visor=true;
    }
    if(!front||!back||!visor)return 13;
    for(const auto& v:mesh.vertices){
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||
           !std::isfinite(v.pz)||v.bone_weights!=0U)return 12;
    }
    return 0;
}
