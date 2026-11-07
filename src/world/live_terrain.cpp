#include "kami/world/live_terrain.hpp"
#include "kami/world/terrain_palette.hpp"
#include "kami/world/terrain_quality.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace kami::world {
namespace {
struct Position {
    float x,y,z;
};
[[nodiscard]] bool read_corner(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    unsigned size,unsigned gx,unsigned gz,float agent_x,float agent_y,
    float agent_z,Position& out) noexcept {
    const unsigned px=gx/size,pz=gz/size;
    if(px>=32U||pz>=32U)return false;
    const auto id=static_cast<std::uint16_t>(pz*32U+px);
    const auto found=decoded.find(id);
    if(found==decoded.end()||found->second.size!=size||
       found->second.x!=px||found->second.y!=pz)return false;
    const auto lx=gx%size,lz=gz%size;
    const float elevation=found->second.elevation[lz*size+lx];
    if(!std::isfinite(elevation)||elevation< -4096.f||
       elevation>8192.f)return false;
    out={static_cast<float>(gx)-agent_x,elevation-agent_z,
         static_cast<float>(gz)-agent_y};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&
           std::isfinite(out.z);
}
void triangle(SceneMesh& out,Position a,Position b,Position c,
              float agent_x,float agent_y,float agent_z,
              float verified_water_z) {
    const float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
    const float vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
    const float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
    const float length=std::sqrt(nx*nx+ny*ny+nz*nz);
    if(!std::isfinite(length)||length<1.e-6f)return;
    const float inv=1.f/length;
    auto emit=[&](Position p){
        // Height and surface normal are authentic SL land samples.
        // RGB is KAMI's native DX12 terrain styling, NOT an SL texture.
        const auto color=terraincolor::sample(
            p.x+agent_x,p.y+agent_z,p.z+agent_y,
            ny*inv,verified_water_z);
        out.vertices.push_back({p.x,p.y,p.z,
           nx*inv,ny*inv,nz*inv,
           color.r,color.g,color.b,0.f,.86f,
           (p.x+agent_x)*.025f,(p.z+agent_y)*.025f});
    };
    emit(a);emit(b);emit(c);
}
} // namespace

bool sample_live_terrain_height(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    float ox,float oy,float oz,float lx,float lz,float& height) noexcept {
    if(!std::isfinite(ox)||!std::isfinite(oy)||!std::isfinite(oz)||
       !std::isfinite(lx)||!std::isfinite(lz))return false;
    const double wx=static_cast<double>(ox)+lx,wz=static_cast<double>(oy)+lz;
    if(wx<0.0||wz<0.0||wx>=1024.0||wz>=1024.0)return false;
    const auto x=static_cast<unsigned>(std::floor(wx));
    const auto z=static_cast<unsigned>(std::floor(wz));
    const float fx=static_cast<float>(wx-x),fz=static_cast<float>(wz-z);
    unsigned size=0U;
    for(const unsigned candidate:{16U,32U}){
        const auto px=x/candidate,pz=z/candidate;
        const auto it=decoded.find(static_cast<std::uint16_t>(pz*32U+px));
        if(it!=decoded.end()&&it->second.size==candidate&&
           it->second.x==px&&it->second.y==pz){size=candidate;break;}
    }
    if(size==0U||x+1U>=1024U||z+1U>=1024U)return false;
    const auto read=[&](unsigned gx,unsigned gz,float& out) noexcept {
        const unsigned px=gx/size,pz=gz/size;
        if(px>=32U||pz>=32U)return false;
        const auto it=decoded.find(static_cast<std::uint16_t>(pz*32U+px));
        if(it==decoded.end()||it->second.size!=size||it->second.x!=px||it->second.y!=pz)return false;
        out=it->second.elevation[(gz%size)*size+(gx%size)];
        return std::isfinite(out)&&out>=-4096.f&&out<=8192.f;
    };
    float a=0.f,b=0.f,c=0.f,d=0.f;
    if(!read(x,z,a)||!read(x+1U,z,b)||!read(x,z+1U,c)||!read(x+1U,z+1U,d))return false;
    const float top=a+(b-a)*fx,bottom=c+(d-c)*fx;
    const float result=top+(bottom-top)*fz-oz;
    if(!std::isfinite(result))return false;
    height=result;return true;
}

LiveTerrainMesh build_live_terrain_mesh(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    const std::vector<const protocol::terrain::Patch*>& selected,
    float agent_x,float agent_y,float agent_z,
    float verified_water_z) {
    LiveTerrainMesh result{};
    if(!std::isfinite(agent_x)||!std::isfinite(agent_y)||
       !std::isfinite(agent_z)||selected.size()>64U)return result;
    // 64 selected patches * 32*32 cells * 6 vertices < 750k budget.
    result.scene.vertices.reserve(selected.size()*32U*32U*6U);
    for(const auto* patch:selected){
        if(!patch||(patch->size!=16U&&patch->size!=32U)||
           patch->x>=32U||patch->y>=32U)continue;
        const unsigned size=patch->size;
        for(unsigned z=0;z<size;++z)for(unsigned x=0;x<size;++x){
            const unsigned gx=static_cast<unsigned>(patch->x)*size+x;
            const unsigned gz=static_cast<unsigned>(patch->y)*size+z;
            Position a{},b{},c{},d{};
            if(!read_corner(decoded,size,gx,gz,agent_x,agent_y,agent_z,a)||
               !read_corner(decoded,size,gx,gz+1U,agent_x,agent_y,agent_z,b)||
               !read_corner(decoded,size,gx+1U,gz+1U,agent_x,agent_y,agent_z,c)||
               !read_corner(decoded,size,gx+1U,gz,agent_x,agent_y,agent_z,d)){
                ++result.skipped_quads;
                continue;
            }
            // A bad decoder coefficient can create a 100m+ slope in
            // one 1m cell. Never smooth or invent elevations; skip the
            // dangerous faces and report how many were omitted.
            if(!terrainquality::renderable_cell(
                a.y,b.y,c.y,d.y)){
                ++result.skipped_quads;
                ++result.skipped_discontinuity_quads;
                continue;
            }
            const auto old=result.scene.vertices.size();
            triangle(result.scene,a,b,c,
                     agent_x,agent_y,agent_z,verified_water_z);
            triangle(result.scene,a,c,d,
                     agent_x,agent_y,agent_z,verified_water_z);
            if(result.scene.vertices.size()!=old+6U){
                result.scene.vertices.resize(old);
                ++result.skipped_quads;
                continue;
            }
            ++result.quads;
            if(x+1U==size||z+1U==size)++result.stitched_quads;
        }
    }
    result.scene.terrain_count=result.scene.vertices.size();
    result.scene.avatar_count=0;
    return result;
}
} // namespace kami::world
