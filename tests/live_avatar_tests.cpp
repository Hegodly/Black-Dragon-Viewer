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
