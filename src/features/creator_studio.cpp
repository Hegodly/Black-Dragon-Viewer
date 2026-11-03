#include "kami/features/creator_studio.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace kami::creator {
namespace {
struct Position { float x,y,z; };
[[nodiscard]] bool position_index(const std::string& token, std::size_t count,
                                  std::size_t& index) {
    const auto slash=token.find('/');
    const std::string_view head(token.data(),slash==std::string::npos?
                                  token.size():slash);
    int value=0;
    if(head.empty())return false;
    const auto parsed=std::from_chars(head.data(),head.data()+head.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=head.data()+head.size() ||
       !value)return false;
    const std::int64_t actual=value>0 ? std::int64_t(value)-1:
                                                   std::int64_t(count)+value;
    if(actual<0||actual>=std::int64_t(count))return false;
    index=static_cast<std::size_t>(actual);
    return true;
}
[[nodiscard]] bool triangle(std::vector<world::Vertex>& out,
                            Position a,Position b,Position c) {
    const float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
    const float vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
    const float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
    const float n=std::sqrt(nx*nx+ny*ny+nz*nz);
    if(!(n>1e-7f)||!std::isfinite(n))return false;
    for(const Position p:{a,b,c})
        out.push_back({p.x,p.y,p.z,nx/n,ny/n,nz/n,
                       .52f,.67f,.84f,.24f,.46f,p.x*.18f,p.z*.18f});
    return true;
}
}
bool import_obj(std::istream& input, Preview& output, std::string& error) {
    output={};
    error.clear();
    std::vector<Position> positions;
    positions.reserve(1024);
    std::vector<world::Vertex> vertices;
    std::string line;
    std::size_t line_number=0,polygon_count=0;
    constexpr std::size_t kMaxPositions=400000;
    constexpr std::size_t kMaxTriangles=500000;
    while(std::getline(input,line)) {
        ++line_number;
        if(line_number>1500000||line.size()>16384) {
            error="OBJ exceeded KAMI's local preview input limits";return false;
        }
        std::istringstream scanner(line);
        std::string tag;
        if(!(scanner>>tag)||tag[0]=='#')continue;
        if(tag=="v") {
            Position p{};
            if(!(scanner>>p.x>>p.y>>p.z) ||
               !std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
               std::abs(p.x)>1000000.f||std::abs(p.y)>1000000.f||
               std::abs(p.z)>1000000.f||positions.size()>=kMaxPositions) {
                error="Invalid position or vertex count exceeded";return false;
            }
            positions.push_back(p);
        } else if(tag=="f") {
            std::vector<std::size_t> face;
            std::string token;
            while(scanner>>token) {
                if(token[0]=='#')break;
                std::size_t index=0;
                if(face.size()>=128||!position_index(token,positions.size(),index)) {
                    error="Invalid polygon position index";return false;
                }
                face.push_back(index);
            }
            if(face.size()<3||vertices.size()/3+face.size()-2>kMaxTriangles) {
                error="Invalid polygon or OBJ preview triangle limit exceeded";
                return false;
            }
            ++polygon_count;
            for(std::size_t j=1;j+1<face.size();++j)
                if(!triangle(vertices,positions[face[0]],positions[face[j]],
                             positions[face[j+1]])) {
                    error="Degenerate or non-finite polygon";return false;
                }
        }
    }
    if(!input.eof()&&input.fail()) {
        error="Could not read OBJ stream";return false;
    }
    if(vertices.empty()) {
        error="OBJ has no renderable triangles";return false;
    }
    Position mn{vertices[0].px,vertices[0].py,vertices[0].pz},mx=mn;
    for(const auto& v:vertices) {
        mn.x=std::min(mn.x,v.px);mn.y=std::min(mn.y,v.py);mn.z=std::min(mn.z,v.pz);
        mx.x=std::max(mx.x,v.px);mx.y=std::max(mx.y,v.py);mx.z=std::max(mx.z,v.pz);
    }
    const float dx=mx.x-mn.x,dy=mx.y-mn.y,dz=mx.z-mn.z;
    const float biggest=std::max({dx,dy,dz});
    if(!(biggest>1e-6f)||!std::isfinite(biggest)) {
        error="OBJ mesh has no measurable size";return false;
    }
    const float scale=3.2f/biggest;
    const float cx=(mn.x+mx.x)*.5f,cz=(mn.z+mx.z)*.5f;
    for(auto& v:vertices) {
        v.px=(v.px-cx)*scale;
        v.py=(v.py-mn.y)*scale;
        v.pz=(v.pz-cz)*scale;
    }
    Preview candidate;
    candidate.scene.terrain_count=vertices.size();
    candidate.scene.vertices=std::move(vertices);
    candidate.source_positions=positions.size();
    candidate.source_polygons=polygon_count;
    candidate.source_height=dy;
    candidate.audit=inspect(candidate.scene);
    if(!candidate.audit.valid) {
        error=candidate.audit.problem;return false;
    }
    output=std::move(candidate);
    return true;
}
bool import_obj_file(const std::wstring& path,Preview& output,std::string& error) {
    if(path.empty()) {error="No OBJ path selected";return false;}
    std::ifstream file(std::filesystem::path(path),std::ios::binary);
    if(!file) {error="OBJ file could not be opened";return false;}
    return import_obj(file,output,error);
}
} // namespace kami::creator
